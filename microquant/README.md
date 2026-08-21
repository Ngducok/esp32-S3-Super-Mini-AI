# MicroQuant-ESP32 Quantization Engine

<p align="left">
  <b>Detailed Documentation:</b> 
  <a href="../docs/en/MICROQUANT.md">English Guide</a> | 
  <a href="../docs/vn/MICROQUANT.md">Hướng Dẫn Tiếng Việt</a>
</p>

---

## Overview

`microquant` is an embedded quantization toolchain delivering Group-Wise INT4 (Group size 32, 7.7x compression) and BitNet 1.58b ternary quantization for microcontrollers lacking external PSRAM.

## Directory Structure

- `include/`: Standalone C++ headers for INT8, Group-32 INT4, and BitNet GEMV kernels.
- `python/microquant/`: Python quantization, verification, and C++ header export toolchain.
- `tests/test_quant_math.py`: Automated mathematical test suite verifying Cosine Similarity, SQNR, and LUT accuracy.
- `examples/`: Minimal standalone GEMV examples.
