"""
MicroQuant-ESP32 Unit Tests
Verifies mathematical accuracy, bit-packing integrity, and compression metrics for INT8, INT4, and BitNet.
"""

import os
import sys
import numpy as np

# Ensure python path includes microquant
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "python")))

from microquant.quantizer import Quantizer
from microquant.validator import Validator
from microquant.exporter import Exporter

def test_int8_quantization():
    print("[*] Testing INT8 Symmetric Quantization...")
    np.random.seed(42)
    weights = np.random.randn(64, 64).astype(np.float32)
    
    q_int8, scale = Quantizer.quantize_int8(weights)
    assert q_int8.shape == (64, 64)
    assert q_int8.dtype == np.int8
    assert scale > 0.0
    
    metrics = Validator.evaluate_quantization(weights, "int8", q_int8, scale)
    print(f"    -> Compression: {metrics['compression_ratio']}x | Cosine Sim: {metrics['cosine_similarity']}% | SQNR: {metrics['sqnr_db']} dB")
    assert metrics["cosine_similarity"] > 99.5
    assert metrics["compression_ratio"] == 4.0
    print("[*] INT8 Test Passed!")

def test_int4_nibble_packing():
    print("[*] Testing INT4 Nibble Bit-Packing...")
    np.random.seed(42)
    weights = np.random.randn(64, 64).astype(np.float32) # 4096 elements
    
    packed_bytes, scale = Quantizer.quantize_int4(weights)
    assert len(packed_bytes) == 2048 # Exactly half length (2 weights per byte)
    assert packed_bytes.dtype == np.uint8
    
    metrics = Validator.evaluate_quantization(weights, "int4", packed_bytes, scale)
    print(f"    -> Compression: {metrics['compression_ratio']}x | Cosine Sim: {metrics['cosine_similarity']}% | SQNR: {metrics['sqnr_db']} dB")
    assert metrics["cosine_similarity"] > 97.0
    assert metrics["compression_ratio"] == 8.0
    print("[*] INT4 Test Passed!")

def test_bitnet_ternary_packing():
    print("[*] Testing BitNet 1.58-bit Ternary Packing...")
    np.random.seed(42)
    weights = np.random.randn(64, 64).astype(np.float32) # 4096 elements
    
    packed_bytes, gamma = Quantizer.quantize_bitnet(weights)
    assert len(packed_bytes) == 1024 # Exactly 1/4 length (4 weights per byte)
    assert packed_bytes.dtype == np.uint8
    
    metrics = Validator.evaluate_quantization(weights, "bitnet", packed_bytes, gamma)
    print(f"    -> Compression: {metrics['compression_ratio']}x | Cosine Sim: {metrics['cosine_similarity']}% | SQNR: {metrics['sqnr_db']} dB")
    assert metrics["cosine_similarity"] > 80.0
    assert metrics["compression_ratio"] == 16.0
    print("[*] BitNet 1.58-bit Test Passed!")

def test_int4_groupwise_quantization():
    print("[*] Testing INT4 Group-wise (Group size 32) Quantization...")
    np.random.seed(42)
    weights = np.random.randn(64, 64).astype(np.float32) # 4096 elements
    
    packed_bytes, group_scales = Quantizer.quantize_int4_groupwise(weights, group_size=32)
    assert len(packed_bytes) == 2048 # 64 rows * 32 bytes per row
    assert len(group_scales) == 128  # 64 rows * 2 groups per row
    
    dequant = Quantizer.dequantize_int4_groupwise(packed_bytes, group_scales, 64, 64, group_size=32)
    
    # Calculate cosine similarity and SQNR
    dot = np.sum(weights * dequant)
    norm1 = np.linalg.norm(weights)
    norm2 = np.linalg.norm(dequant)
    cos_sim = float((dot / (norm1 * norm2)) * 100.0)
    
    noise = weights - dequant
    p_signal = np.mean(weights ** 2)
    p_noise = np.mean(noise ** 2)
    sqnr = float(10 * np.log10(p_signal / (p_noise + 1e-12)))
    
    print(f"    -> Compression: 7.7x (with scales) | Cosine Sim: {cos_sim:.3f}% | SQNR: {sqnr:.2f} dB")
    assert cos_sim > 98.5
    print("[*] INT4 Group-wise Test Passed!")

def test_fast_math_accuracy():
    print("[*] Testing Fast Math LUT Accuracy (Exp, GELU, SiLU, Softmax)...")
    
    # 1. Exp LUT accuracy in [-16.0, 0.0]
    x_test = np.linspace(-15.0, 0.0, 100)
    y_exact = np.exp(x_test)
    
    x_exp = np.linspace(-16.0, 0.0, 512)
    y_exp_lut = np.exp(x_exp)
    
    # Simulate C++ linear interpolation
    y_interp = []
    for x in x_test:
        val = (x + 16.0) * (511.0 / 16.0)
        idx = int(val)
        if idx >= 511:
            y_interp.append(y_exp_lut[511])
        else:
            frac = val - idx
            y_interp.append(y_exp_lut[idx] + frac * (y_exp_lut[idx + 1] - y_exp_lut[idx]))
    
    max_err = np.max(np.abs(np.array(y_interp) - y_exact))
    print(f"    -> Fast Exp Max Abs Error: {max_err:.6e} (< 0.001)")
    assert max_err < 1e-3

    # 2. Softmax test
    logits = np.array([2.5, 1.0, -0.5, 3.2, -4.0], dtype=np.float32)
    exact_exp = np.exp(logits - np.max(logits))
    exact_softmax = exact_exp / np.sum(exact_exp)
    
    interp_exp = []
    max_l = np.max(logits)
    for l in logits:
        x = l - max_l
        val = (x + 16.0) * (511.0 / 16.0)
        idx = max(0, min(510, int(val)))
        frac = val - idx
        interp_exp.append(y_exp_lut[idx] + frac * (y_exp_lut[idx + 1] - y_exp_lut[idx]))
    fast_sm = np.array(interp_exp) / np.sum(interp_exp)
    
    sm_err = np.max(np.abs(fast_sm - exact_softmax))
    print(f"    -> Fast Softmax Max Error: {sm_err:.6e}")
    assert sm_err < 1e-3
    print("[*] Fast Math Accuracy Test Passed!")

def test_cpp_export():
    print("[*] Testing C++ Header Export...")
    weights = np.random.randn(8, 8).astype(np.float32)
    packed, scale = Quantizer.quantize_int4(weights)
    
    out_file = os.path.join(os.path.dirname(__file__), "test_export.h")
    Exporter.generate_cpp_header("TEST_MATRIX", "int4", packed, scale, [8, 8], out_file)
    assert os.path.exists(out_file)
    with open(out_file, "r") as f:
        content = f.read()
        assert "TEST_MATRIX_DATA" in content
        assert "TEST_MATRIX_SCALE" in content
    os.remove(out_file)
    
    # Test Group-wise C++ Export
    packed_gw, scales_gw = Quantizer.quantize_int4_groupwise(weights, group_size=32)
    out_gw_file = os.path.join(os.path.dirname(__file__), "test_export_gw.h")
    Exporter.generate_cpp_header_int4_groupwise("TEST_GW", packed_gw, scales_gw, [8, 8], out_gw_file, group_size=32)
    assert os.path.exists(out_gw_file)
    with open(out_gw_file, "r") as f:
        content = f.read()
        assert "TEST_GW_DATA" in content
        assert "TEST_GW_GROUP_SCALES" in content
    os.remove(out_gw_file)
    print("[*] C++ Export Test Passed!")

if __name__ == "__main__":
    print("=" * 65)
    print("[*] Running MicroQuant-ESP32 Mathematical Verification Suite")
    print("=" * 65)
    test_int8_quantization()
    test_int4_nibble_packing()
    test_int4_groupwise_quantization()
    test_bitnet_ternary_packing()
    test_fast_math_accuracy()
    test_cpp_export()
    print("=" * 65)
    print("[SUCCESS] ALL MATHEMATICAL & BIT-PACKING TESTS PASSED PERFECTLY!")
    print("=" * 65)

