/**
 * @file MicroQuant.h
 * @brief MicroQuant-ESP32: Complete Bare-Metal Mathematical Compression & Inference Core
 * Header-Only, Zero Dynamic Allocation (0-Leak), Embedded & Arduino Compatible.
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <math.h>

#include "kernels/matvec_int8.h"
#include "kernels/matvec_int4.h"
#include "kernels/matvec_bitnet.h"

namespace MicroQuant {

/**
 * @brief Fast floating-point to INT8 quantization for activation vectors (Unrolled)
 */
inline void float_to_int8_vector(const float* in, int8_t* out, uint32_t size, float scale = 30.0f) {
    uint32_t i = 0;
    for (; i + 4 <= size; i += 4) {
        int32_t v0 = (int32_t)roundf(in[i + 0] * scale);
        int32_t v1 = (int32_t)roundf(in[i + 1] * scale);
        int32_t v2 = (int32_t)roundf(in[i + 2] * scale);
        int32_t v3 = (int32_t)roundf(in[i + 3] * scale);
        if (v0 < -128) v0 = -128; if (v0 > 127) v0 = 127;
        if (v1 < -128) v1 = -128; if (v1 > 127) v1 = 127;
        if (v2 < -128) v2 = -128; if (v2 > 127) v2 = 127;
        if (v3 < -128) v3 = -128; if (v3 > 127) v3 = 127;
        out[i + 0] = (int8_t)v0;
        out[i + 1] = (int8_t)v1;
        out[i + 2] = (int8_t)v2;
        out[i + 3] = (int8_t)v3;
    }
    for (; i < size; i++) {
        int32_t val = (int32_t)roundf(in[i] * scale);
        if (val < -128) val = -128;
        if (val > 127)  val = 127;
        out[i] = (int8_t)val;
    }
}

/**
 * @brief GELU Activation function
 */
inline float gelu(float x) {
    if (x <= -4.0f) return 0.0f;
    if (x >= 4.0f) return x;
    return 0.5f * x * (1.0f + tanhf(0.79788456f * (x + 0.044715f * x * x * x)));
}

/**
 * @brief SiLU / Swish Activation function
 */
inline float silu(float x) {
    if (x <= -8.0f) return 0.0f;
    if (x >= 8.0f) return x;
    return x / (1.0f + expf(-x));
}

/**
 * @brief Numerically stable Softmax for probability vectors
 */
inline void softmax(float* vec, uint32_t size) {
    if (size == 0) return;
    float max_v = vec[0];
    for (uint32_t i = 1; i < size; i++) {
        if (vec[i] > max_v) max_v = vec[i];
    }
    float sum_exp = 0.0f;
    for (uint32_t i = 0; i < size; i++) {
        float e = expf(vec[i] - max_v);
        vec[i] = e;
        sum_exp += e;
    }
    float inv_sum = 1.0f / (sum_exp > 1e-7f ? sum_exp : 1.0f);
    for (uint32_t i = 0; i < size; i++) {
        vec[i] *= inv_sum;
    }
}

} // namespace MicroQuant
