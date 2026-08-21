"""
MicroQuant-ESP32 CLI Tool
Usage:
    python -m microquant.cli compress --input weights.npy --type int4 --output weights_int4.h --name W_LAYER0
"""

import os
import argparse
import sys
import numpy as np

try:
    from .quantizer import Quantizer
    from .validator import Validator
    from .exporter import Exporter
except ImportError:
    sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))
    from microquant.quantizer import Quantizer
    from microquant.validator import Validator
    from microquant.exporter import Exporter

def main():
    parser = argparse.ArgumentParser(description="MicroQuant-ESP32: Mathematical Model Compression Tool")
    parser.add_argument("--input", required=True, help="Path to input numpy array (.npy) or raw float file")
    parser.add_argument("--type", choices=["int8", "int4", "bitnet"], required=True, help="Quantization type")
    parser.add_argument("--output", required=True, help="Output C++ header file path (.h)")
    parser.add_argument("--name", default="WEIGHT_MATRIX", help="C++ variable name for the tensor")
    parser.add_argument("--namespace", default="MicroQuantModel", help="C++ namespace")
    
    args = parser.parse_args()
    
    print("=" * 65)
    print("[*] MicroQuant-ESP32 Mathematical Tensor Compressor")
    print("=" * 65)
    print(f"[*] Input File     : {args.input}")
    print(f"[*] Format Target  : {args.type.upper()}")
    print(f"[*] Output Header  : {args.output}")
    print(f"[*] Tensor Symbol  : {args.name}")
    print("-" * 65)
    
    try:
        if args.input.endswith(".npy"):
            weights = np.load(args.input)
        else:
            weights = np.fromfile(args.input, dtype=np.float32)
    except Exception as e:
        print(f"[!] Error reading input file: {e}")
        sys.exit(1)
        
    shape = list(weights.shape)
    print(f"[*] Tensor Shape   : {shape} ({weights.size} elements, {weights.nbytes} Bytes in FP32)")
    
    # 1. Quantize
    if args.type == "int8":
        packed, scale = Quantizer.quantize_int8(weights)
    elif args.type == "int4":
        packed, scale = Quantizer.quantize_int4(weights)
    elif args.type == "bitnet":
        packed, scale = Quantizer.quantize_bitnet(weights)
        
    # 2. Evaluate & Validate
    metrics = Validator.evaluate_quantization(weights, args.type, packed, scale)
    
    print("[+] Quantization & Bit-Packing Succeeded!")
    print(f"    - Compressed Bytes : {metrics['packed_bytes']} Bytes (vs {metrics['original_bytes_fp32']} Bytes FP32)")
    print(f"    - Compression Ratio: {metrics['compression_ratio']}x smaller")
    print(f"    - Cosine Similarity: {metrics['cosine_similarity']}%")
    print(f"    - Mean Squared Err : {metrics['mse']:.8f}")
    print(f"    - SQNR Ratio       : {metrics['sqnr_db']} dB")
    print("-" * 65)
    
    # 3. Export Header
    Exporter.generate_cpp_header(
        tensor_name=args.name,
        quant_type=args.type,
        packed_data=packed,
        scale=scale,
        original_shape=shape,
        output_path=args.output,
        namespace=args.namespace
    )
    print(f"[OK] C++ Header generated at: {args.output}")
    print("=" * 65)

if __name__ == "__main__":
    main()
