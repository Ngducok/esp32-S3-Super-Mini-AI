# MicroQuant-ESP32

<p align="center">
  <b>Ultra-Dense Mathematical Quantization & Bare-Metal Sub-Byte Inference Engine for ESP32 & MCUs</b><br>
  <i>INT8 (4x) • INT4 (8x) • BitNet 1.58-bit (16x) • Zero-Copy Flash DROM • Zero Memory Drift</i>
</p>

<p align="center">
  <a href="README.md">English</a> | 
  <a href="README_VN.md">Tiếng Việt</a>
</p>

---

## Overview

**MicroQuant-ESP32** is a high-performance mathematical compression toolkit and C++ bare-metal inference engine designed to deploy large neural networks onto resource-constrained microcontrollers (e.g., **ESP32-S3 with 4MB Flash and 0 KB external PSRAM**) without pruning or losing a single parameter.

---

## Mathematical Compression Formulations

### 1. Symmetric INT8 Quantization (4x Compression)
For weight tensor $W \in \mathbb{R}^{M \times N}$:

$$S_8 = \frac{\max(|W|)}{127.0}, \quad W_{\text{int8}} = \text{clamp}\left(\left\lfloor \frac{W}{S_8} + 0.5 \right\rfloor, -128, 127\right)$$

- **Storage**: 1 byte / weight
- **Inference**: High-speed Xtensa LX7 SIMD accumulator

---

### 2. Nibble-Packed INT4 Quantization (8x Compression)
Packs two 4-bit signed integers $[-8, 7]$ into a single `uint8_t` byte:

$$S_4 = \frac{\max(|W|)}{7.0}, \quad W_{\text{int4}} = \text{clamp}\left(\left\lfloor \frac{W}{S_4} + 0.5 \right\rfloor, -8, 7\right)$$

$$\text{PackedByte}[m] = \left(W_{\text{int4}}[2m] \ \& \ \text{0x0F}\right) \ | \ \left(\left(W_{\text{int4}}[2m+1] \ \& \ \text{0x0F}\right) \ll 4\right)$$

- **Storage**: 0.5 bytes / weight (2 weights / byte)
- **Inference**: On-the-fly register unpacking with sign extension during matrix-vector multiplication

---

### 3. BitNet 1.58-Bit Ternary Quantization (16x Compression)
Encodes weights into a ternary alphabet $\{-1, 0, +1\}$, eliminating multiplication operations:

$$\gamma = \frac{1}{M \times N} \sum_{i=1}^M \sum_{j=1}^N |W_{ij}|, \quad \widetilde{W}_{ij} = \text{clamp}\left(\text{round}\left(\frac{W_{ij}}{\gamma}\right), -1, 1\right)$$

- **Encoding**: $00_2 \to 0, \quad 01_2 \to +1, \quad 10_2 \to -1$
- **Storage**: 0.25 bytes / weight (4 weights / byte)
- **Multiplication-Free Matrix Vector Product**:
  $$Y[r] = \gamma \times \left( \sum_{w_{r, c} = +1} X[c] - \sum_{w_{r, k} = -1} X[k] \right)$$

---

## Memory & Density Comparison

| Format | Storage / Weight | 1 Million Parameters | Fits 4MB Flash? | Arithmetic Mode |
| :--- | :--- | :--- | :--- | :--- |
| **Float32 (Uncompressed)** | 4.0 Bytes | 4.0 MB | ❌ No | Standard FP32 Multiply |
| **INT8 (Symmetric)** | 1.0 Byte | 1.0 MB | ✅ Yes | Integer SIMD Multiply |
| **INT4 (Nibble-Packed)** | 0.5 Bytes | 500 KB | ✅ Yes (8x smaller) | Register-Unpack Multiply |
| **BitNet (1.58-bit Ternary)** | 0.25 Bytes | **250 KB** | ✅ Yes (**16x smaller**) | **Multiplication-Free Add/Sub** |

---

## Quickstart (C++ Bare-Metal Inference)

```cpp
#include "MicroQuant.h"
#include "model_int4_weights.h" // Exported via Python CLI

void run_inference(const float* input_activations, float* output_logits) {
    int8_t x_q[MicroQuantModel::WEIGHT_DIM_1];
    MicroQuant::float_to_int8_vector(input_activations, x_q, MicroQuantModel::WEIGHT_DIM_1);

    // Compute Matrix-Vector product with register-level on-the-fly unpacking:
    MicroQuant::matvec_int4(
        MicroQuantModel::WEIGHT_DATA,
        x_q,
        output_logits,
        MicroQuantModel::WEIGHT_DIM_0,
        MicroQuantModel::WEIGHT_DIM_1,
        MicroQuantModel::WEIGHT_SCALE * (1.0f / 30.0f)
    );
}
```

---

## License

MIT License. Designed for embedded systems and edge machine learning research.
