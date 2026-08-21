# MicroQuant-ESP32 Model Quantization & Compression Engine (microquant/)

<p align="left">
  <b>Language:</b> 
  <a href="README.md">English</a> | 
  <a href="README_VN.md">Tiếng Việt</a>
</p>

---

## 1. Overview

`microquant/` is a specialized quantization and compression toolchain for ESP32-S3 microcontrollers. It provides Python tools for quantizing weights and exporting to C++ Flash DROM headers, along with micro-architecture optimized C++ kernels (INT8, Group-wise INT4, BitNet 1.58b).

---

## 2. Problem Statement & Technical Bottlenecks

1. **4MB Flash Constraint**: Storing weights in FP32 or INT8 limits parameter scaling within budget 4MB Flash boundaries.
2. **Dynamic Range Loss in Global INT4**: Outlier weights degrade global scaling precision, reducing Signal-to-Quantization-Noise Ratio (SQNR).
3. **ALU Multiplication Overhead**: Standard floating-point multiply-accumulate operations incur CPU pipeline latency.

---

## 3. Technical Solutions & Architecture

1. **Group-Wise INT4 (Group Size 32)**: Divides matrices into 32-element blocks with dedicated dynamic scales, achieving 7.7x compression with minimal accuracy loss.
2. **BitNet 1.58b Kernel**: Encodes ternary weights {-1, 0, +1}, replacing ALU multiplications with pure additions and subtractions.
3. **Zero-Copy Header Exporter**: Generates `const` Flash DROM arrays for direct memory-mapped execution.

---

## 4. Empirical Mathematical Verification (`test_quant_math.py`)

| Quantization Mode | Compression Ratio | Cosine Similarity | SQNR (Signal-to-Noise) | Assessment |
| :--- | :--- | :--- | :--- | :--- |
| **Global INT8** | 4.0x | **99.996%** | **40.95 dB** | Near-lossless precision |
| **Per-Tensor INT4** | 8.0x | **98.720%** | **15.80 dB** | 50% Flash savings vs INT8 |
| **Group-Wise INT4 (G32)**| **7.7x** | **99.529%** | **20.23 dB** | Optimal for 4MB Flash |
| **BitNet 1.58b** | **16.0x** | **88.592%** | **5.77 dB** | Multiplication-free ALU |
