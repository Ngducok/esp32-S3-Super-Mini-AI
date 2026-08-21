"""
MicroQuant-ESP32: Mathematical Validation & Error Analysis Module
Evaluates:
- Mean Squared Error (MSE)
- Cosine Similarity
- Signal-to-Quantization-Noise Ratio (SQNR in dB)
- Compression Ratio vs FP32
"""

import numpy as np
from typing import Dict, Any
from .quantizer import Quantizer

class Validator:
    @staticmethod
    def evaluate_quantization(original: np.ndarray,
                              quantized_type: str,
                              packed_data: np.ndarray,
                              scale: float) -> Dict[str, Any]:
        orig = np.asarray(original, dtype=np.float32).flatten()
        orig_len = len(orig)
        
        if quantized_type == "int8":
            dequant = packed_data.astype(np.float32).flatten() * scale
        elif quantized_type == "int4":
            dequant = Quantizer.dequantize_int4(packed_data, scale, orig_len)
        elif quantized_type == "bitnet":
            dequant = Quantizer.dequantize_bitnet(packed_data, scale, orig_len)
        else:
            raise ValueError(f"Unknown quantization type: {quantized_type}")
            
        dequant = dequant[:orig_len]
        
        # 1. Mean Squared Error (MSE)
        mse = float(np.mean((orig - dequant) ** 2))
        
        # 2. Cosine Similarity
        norm_orig = np.linalg.norm(orig)
        norm_dequant = np.linalg.norm(dequant)
        if norm_orig > 1e-8 and norm_dequant > 1e-8:
            cosine_sim = float(np.dot(orig, dequant) / (norm_orig * norm_dequant))
        else:
            cosine_sim = 1.0
            
        # 3. Signal to Quantization Noise Ratio (SQNR in dB)
        signal_pwr = np.mean(orig ** 2)
        noise_pwr = np.mean((orig - dequant) ** 2)
        if noise_pwr > 1e-12 and signal_pwr > 1e-12:
            sqnr_db = float(10.0 * np.log10(signal_pwr / noise_pwr))
        else:
            sqnr_db = 99.99
            
        # 4. Storage & Compression Ratio
        orig_bytes = orig_len * 4 # Float32
        if quantized_type == "int8":
            packed_bytes = len(packed_data.flatten())
        elif quantized_type == "int4":
            packed_bytes = len(packed_data.flatten())
        elif quantized_type == "bitnet":
            packed_bytes = len(packed_data.flatten())
        else:
            packed_bytes = orig_bytes
            
        compression_ratio = float(orig_bytes / packed_bytes) if packed_bytes > 0 else 1.0
        
        return {
            "quant_type": quantized_type,
            "original_elements": orig_len,
            "original_bytes_fp32": orig_bytes,
            "packed_bytes": packed_bytes,
            "compression_ratio": round(compression_ratio, 2),
            "mse": mse,
            "cosine_similarity": round(cosine_sim * 100.0, 3), # Percentage
            "sqnr_db": round(sqnr_db, 2)
        }
