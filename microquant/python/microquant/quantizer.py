"""
MicroQuant-ESP32: Mathematical Quantization & Bit-Packing Engine
Supports:
- INT8 (Symmetric 8-bit, 1 weight/byte)
- INT4 (Symmetric 4-bit nibble-packed, 2 weights/byte)
- BitNet 1.58-bit (Ternary {-1, 0, +1}, 4 weights/byte, multiplication-free)
"""

import numpy as np
from typing import Tuple, Dict, Any, List

class Quantizer:
    @staticmethod
    def quantize_int8(weights: np.ndarray) -> Tuple[np.ndarray, float]:
        """
        Symmetric INT8 Quantization:
        Scale = max(|W|) / 127.0
        W_q = clamp(round(W / Scale), -128, 127)
        """
        w = np.asarray(weights, dtype=np.float32)
        max_val = float(np.max(np.abs(w)))
        if max_val < 1e-8:
            return np.zeros(w.shape, dtype=np.int8), 1.0
        
        scale = max_val / 127.0
        q_weights = np.clip(np.round(w / scale), -128, 127).astype(np.int8)
        return q_weights, scale

    @staticmethod
    def quantize_int4(weights: np.ndarray) -> Tuple[np.ndarray, float]:
        """
        Symmetric INT4 Quantization:
        Scale = max(|W|) / 7.0
        W_q = clamp(round(W / Scale), -8, 7)
        Packs 2 INT4 weights into 1 uint8 byte (Nibble Packing).
        """
        w = np.asarray(weights, dtype=np.float32)
        original_shape = w.shape
        w_flat = w.flatten()
        
        # Pad to even length if needed
        pad_len = 0
        if len(w_flat) % 2 != 0:
            pad_len = 1
            w_flat = np.pad(w_flat, (0, 1), mode='constant', constant_values=0)
            
        max_val = float(np.max(np.abs(w_flat)))
        if max_val < 1e-8:
            packed = np.zeros(len(w_flat) // 2, dtype=np.uint8)
            return packed, 1.0

        scale = max_val / 7.0
        q_int4 = np.clip(np.round(w_flat / scale), -8, 7).astype(np.int8)
        
        # Bit-pack: lower nibble (w0) & upper nibble (w1)
        w0 = q_int4[0::2] & 0x0F
        w1 = q_int4[1::2] & 0x0F
        packed_bytes = (w0 | (w1 << 4)).astype(np.uint8)
        return packed_bytes, scale

    @staticmethod
    def quantize_int4_groupwise(weights: np.ndarray, group_size: int = 32) -> Tuple[np.ndarray, np.ndarray]:
        """
        Group-wise Symmetric INT4 Quantization (Group size 32):
        Divides the matrix into blocks of group_size weights along cols.
        Computes per-group Scale = max(|W_group|) / 7.0
        Packs 2 INT4 weights per byte.
        Returns: (packed_bytes, group_scales)
        """
        w = np.asarray(weights, dtype=np.float32)
        rows, cols = w.shape if len(w.shape) == 2 else (1, len(w))
        
        num_groups_per_row = (cols + group_size - 1) // group_size
        group_scales = np.zeros((rows, num_groups_per_row), dtype=np.float32)
        
        packed_cols_per_row = (cols + 1) // 2
        packed_bytes = np.zeros((rows, packed_cols_per_row), dtype=np.uint8)
        
        for r in range(rows):
            row_w = w[r] if len(w.shape) == 2 else w
            for g in range(num_groups_per_row):
                start = g * group_size
                end = min(start + group_size, cols)
                block = row_w[start:end]
                
                max_val = float(np.max(np.abs(block))) if len(block) > 0 else 0.0
                scale = max_val / 7.0 if max_val >= 1e-8 else 1.0
                group_scales[r, g] = scale
                
                q_block = np.clip(np.round(block / scale), -8, 7).astype(np.int8)
                
                # Pack nibbles in this block
                for i in range(0, len(q_block), 2):
                    w0 = int(q_block[i]) & 0x0F
                    w1 = (int(q_block[i + 1]) & 0x0F) if (i + 1 < len(q_block)) else 0
                    byte_idx = (start + i) // 2
                    packed_bytes[r, byte_idx] = (w0 | (w1 << 4))
                    
        return packed_bytes.flatten(), group_scales.flatten()

    @staticmethod
    def dequantize_int4_groupwise(packed_bytes: np.ndarray, group_scales: np.ndarray, rows: int, cols: int, group_size: int = 32) -> np.ndarray:
        """
        Dequantize Group-wise INT4 packed bytes back to float32 matrix.
        """
        num_groups_per_row = (cols + group_size - 1) // group_size
        packed_cols_per_row = (cols + 1) // 2
        
        packed_mat = packed_bytes.reshape((rows, packed_cols_per_row))
        scales_mat = group_scales.reshape((rows, num_groups_per_row))
        
        out = np.zeros((rows, cols), dtype=np.float32)
        
        for r in range(rows):
            for g in range(num_groups_per_row):
                scale = float(scales_mat[r, g])
                start = g * group_size
                end = min(start + group_size, cols)
                
                for c in range(start, end):
                    byte_val = int(packed_mat[r, c // 2])
                    if c % 2 == 0:
                        nibble = byte_val & 0x0F
                    else:
                        nibble = (byte_val >> 4) & 0x0F
                        
                    # Sign extend 4-bit
                    signed_val = nibble - 16 if nibble >= 8 else nibble
                    out[r, c] = float(signed_val) * scale
                    
        return out

    @staticmethod
    def dequantize_int4(packed_bytes: np.ndarray, scale: float, length: int) -> np.ndarray:
        """
        Dequantize packed INT4 bytes back to float32.
        """
        low_nibbles = (packed_bytes & 0x0F).astype(np.int8)
        high_nibbles = ((packed_bytes >> 4) & 0x0F).astype(np.int8)
        
        # Sign-extend 4-bit signed ints
        low_nibbles = np.where(low_nibbles >= 8, low_nibbles - 16, low_nibbles)
        high_nibbles = np.where(high_nibbles >= 8, high_nibbles - 16, high_nibbles)
        
        unpacked = np.empty(len(packed_bytes) * 2, dtype=np.float32)
        unpacked[0::2] = low_nibbles * scale
        unpacked[1::2] = high_nibbles * scale
        return unpacked[:length]

    @staticmethod
    def quantize_bitnet(weights: np.ndarray) -> Tuple[np.ndarray, float]:
        """
        BitNet 1.58-bit Ternary Quantization:
        Gamma = mean(|W|)
        W_tilde = clamp(round(W / (Gamma + eps)), -1, 1) in {-1, 0, +1}
        Packs 4 ternary weights into 1 uint8 byte:
        0 -> 00, +1 -> 01, -1 -> 10
        """
        w = np.asarray(weights, dtype=np.float32)
        w_flat = w.flatten()
        
        # Pad to multiple of 4
        remainder = len(w_flat) % 4
        if remainder != 0:
            pad_len = 4 - remainder
            w_flat = np.pad(w_flat, (0, pad_len), mode='constant', constant_values=0)
            
        gamma = float(np.mean(np.abs(w_flat)))
        if gamma < 1e-8:
            return np.zeros(len(w_flat) // 4, dtype=np.uint8), 1.0
            
        ternary = np.clip(np.round(w_flat / gamma), -1, 1).astype(np.int8)
        
        # 2-bit Encoding Map:
        # 0 -> 0 (00b), +1 -> 1 (01b), -1 -> 2 (10b)
        encoded = np.zeros_like(ternary, dtype=np.uint8)
        encoded[ternary == 1] = 0x01
        encoded[ternary == -1] = 0x02
        encoded[ternary == 0] = 0x00
        
        # Pack 4 ternary weights per byte
        e0 = encoded[0::4]
        e1 = encoded[1::4] << 2
        e2 = encoded[2::4] << 4
        e3 = encoded[3::4] << 6
        
        packed_bytes = (e0 | e1 | e2 | e3).astype(np.uint8)
        return packed_bytes, gamma

    @staticmethod
    def dequantize_bitnet(packed_bytes: np.ndarray, gamma: float, length: int) -> np.ndarray:
        """
        Dequantize packed BitNet 1.58-bit bytes back to float32.
        """
        e0 = packed_bytes & 0x03
        e1 = (packed_bytes >> 2) & 0x03
        e2 = (packed_bytes >> 4) & 0x03
        e3 = (packed_bytes >> 6) & 0x03
        
        def decode_2bit(val):
            out = np.zeros_like(val, dtype=np.float32)
            out[val == 0x01] = 1.0
            out[val == 0x02] = -1.0
            return out
        
        unpacked = np.empty(len(packed_bytes) * 4, dtype=np.float32)
        unpacked[0::4] = decode_2bit(e0) * gamma
        unpacked[1::4] = decode_2bit(e1) * gamma
        unpacked[2::4] = decode_2bit(e2) * gamma
        unpacked[3::4] = decode_2bit(e3) * gamma
        return unpacked[:length]
