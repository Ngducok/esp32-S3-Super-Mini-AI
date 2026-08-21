"""
MicroQuant-ESP32 Demo Weight Generator
Generates sample weight matrices in INT8, INT4, and BitNet 1.58-bit formats.
"""

import os
import sys
import numpy as np

# Add microquant python to path
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "python")))

from microquant.quantizer import Quantizer
from microquant.validator import Validator
from microquant.exporter import Exporter

def main():
    print("=" * 65)
    print("[*] MicroQuant-ESP32 Example: Generating Compressed Model Weights")
    print("=" * 65)
    
    np.random.seed(2026)
    
    # Create a 64x64 Linear Layer Weight Matrix (4,096 parameters = 16,384 Bytes in FP32)
    weights = np.random.randn(64, 64).astype(np.float32)
    out_dir = os.path.join(os.path.dirname(__file__), "generated_headers")
    os.makedirs(out_dir, exist_ok=True)
    
    # 1. INT8 Compression
    packed_int8, scale_int8 = Quantizer.quantize_int8(weights)
    metrics_int8 = Validator.evaluate_quantization(weights, "int8", packed_int8, scale_int8)
    Exporter.generate_cpp_header(
        "DEMO_LAYER_INT8", "int8", packed_int8, scale_int8, [64, 64],
        os.path.join(out_dir, "weights_int8.h")
    )
    print(f"[*] INT8 Exported  : {metrics_int8['packed_bytes']} Bytes ({metrics_int8['compression_ratio']}x) | Cosine Sim: {metrics_int8['cosine_similarity']}%")
    
    # 2. INT4 Compression (Per-Tensor)
    packed_int4, scale_int4 = Quantizer.quantize_int4(weights)
    metrics_int4 = Validator.evaluate_quantization(weights, "int4", packed_int4, scale_int4)
    Exporter.generate_cpp_header(
        "DEMO_LAYER_INT4", "int4", packed_int4, scale_int4, [64, 64],
        os.path.join(out_dir, "weights_int4.h")
    )
    print(f"[*] INT4 Exported  : {metrics_int4['packed_bytes']} Bytes ({metrics_int4['compression_ratio']}x) | Cosine Sim: {metrics_int4['cosine_similarity']}%")
    
    # 3. INT4 Group-wise Compression (Group size 32)
    packed_int4_gw, scales_int4_gw = Quantizer.quantize_int4_groupwise(weights, group_size=32)
    Exporter.generate_cpp_header_int4_groupwise(
        "DEMO_LAYER_INT4_GW", packed_int4_gw, scales_int4_gw, [64, 64],
        os.path.join(out_dir, "weights_int4_gw.h"), group_size=32
    )
    print(f"[*] INT4-GW Export : {len(packed_int4_gw)} Bytes + {len(scales_int4_gw)*4} Bytes scales (7.7x) | Group size 32")

    # 4. BitNet 1.58-bit Compression
    packed_bitnet, gamma_bitnet = Quantizer.quantize_bitnet(weights)
    metrics_bitnet = Validator.evaluate_quantization(weights, "bitnet", packed_bitnet, gamma_bitnet)
    Exporter.generate_cpp_header(
        "DEMO_LAYER_BITNET", "bitnet", packed_bitnet, gamma_bitnet, [64, 64],
        os.path.join(out_dir, "weights_bitnet.h")
    )
    print(f"[*] BitNet Exported: {metrics_bitnet['packed_bytes']} Bytes ({metrics_bitnet['compression_ratio']}x) | Cosine Sim: {metrics_bitnet['cosine_similarity']}%")
    
    print("-" * 65)
    print(f"[OK] All C++ Flash DROM headers generated in: {out_dir}")
    print("=" * 65)

if __name__ == "__main__":
    main()
