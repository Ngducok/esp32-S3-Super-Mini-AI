/**
 * @file matvec_int4.h
 * @brief On-the-Fly Nibble-Unpacking INT4 Matrix-Vector Multiplication Kernel
 * Zero dynamic memory allocation - unpacks directly into CPU registers during dot product.
 */

#pragma once
#include <stdint.h>
#include <stddef.h>

namespace MicroQuant {

/**
 * @brief Sign-extends a 4-bit signed value in range [-8, 7]
 */
static inline int32_t sign_extend_4bit(uint8_t nibble) {
    if (nibble & 0x08) {
        return (int32_t)(int8_t)(nibble | 0xF0);
    }
    return (int32_t)nibble;
}

/**
 * @brief Computes Y = Scale * (W_int4 * X_int8) using on-the-fly bit unpacking
 * 
 * @param packed_mat Pointer to bit-packed uint8 array in Flash DROM (cols/2 bytes per row)
 * @param vec_in Pointer to INT8 input vector
 * @param vec_out Pointer to output float array
 * @param rows Number of rows in matrix
 * @param cols Number of columns in matrix (must be multiple of 2)
 * @param scale_factor Combined scale factor
 */
inline void matvec_int4(const uint8_t* packed_mat,
                        const int8_t* vec_in,
                        float* vec_out,
                        uint32_t rows,
                        uint32_t cols,
                        float scale_factor) {
    const uint32_t packed_cols_per_row = (cols + 1) / 2;

    for (uint32_t r = 0; r < rows; r++) {
        int32_t acc = 0;
        const uint8_t* row_packed = &packed_mat[r * packed_cols_per_row];
        
        for (uint32_t pc = 0; pc < packed_cols_per_row; pc++) {
            uint8_t byte = row_packed[pc];
            
            // Extract low nibble (w0) and high nibble (w1)
            int32_t w0 = sign_extend_4bit(byte & 0x0F);
            int32_t w1 = sign_extend_4bit((byte >> 4) & 0x0F);
            
            uint32_t c0 = pc * 2;
            uint32_t c1 = c0 + 1;
            
            acc += w0 * (int32_t)vec_in[c0];
            if (c1 < cols) {
                acc += w1 * (int32_t)vec_in[c1];
            }
        }
        
        vec_out[r] = (float)acc * scale_factor;
    }
}

/**
 * @brief Group-wise INT4 Matrix-Vector Multiplication Kernel (Group Size = 32)
 * Unpacks 32 nibbles (16 bytes) per group with per-group dynamic scale factors.
 * 
 * @param packed_mat Pointer to bit-packed uint8 array in Flash DROM
 * @param group_scales Pointer to float array of scales (rows * (cols/32))
 * @param vec_in Pointer to INT8 input vector
 * @param vec_out Pointer to output float array
 * @param rows Number of rows in matrix
 * @param cols Number of columns in matrix (must be multiple of 32)
 * @param act_scale Input activation dequant scale
 */
inline void matvec_int4_group32(const uint8_t* packed_mat,
                                const float* group_scales,
                                const int8_t* vec_in,
                                float* vec_out,
                                uint32_t rows,
                                uint32_t cols,
                                float act_scale = (1.0f / 30.0f)) {
    const uint32_t num_groups_per_row = (cols + 31) / 32;
    const uint32_t bytes_per_row = (cols + 1) / 2;

    for (uint32_t r = 0; r < rows; r++) {
        const uint8_t* row_bytes = &packed_mat[r * bytes_per_row];
        const float* row_scales = &group_scales[r * num_groups_per_row];
        float row_acc = 0.0f;

        for (uint32_t g = 0; g < num_groups_per_row; g++) {
            const uint8_t* g_bytes = &row_bytes[g * 16];
            const int8_t* g_in = &vec_in[g * 32];
            int32_t g_acc0 = 0;
            int32_t g_acc1 = 0;

            for (uint32_t b = 0; b < 16; b += 2) {
                uint8_t b0 = g_bytes[b];
                uint8_t b1 = g_bytes[b + 1];

                int32_t w0 = sign_extend_4bit(b0 & 0x0F);
                int32_t w1 = sign_extend_4bit((b0 >> 4) & 0x0F);
                int32_t w2 = sign_extend_4bit(b1 & 0x0F);
                int32_t w3 = sign_extend_4bit((b1 >> 4) & 0x0F);

                uint32_t in_idx = b * 2;
                g_acc0 += w0 * (int32_t)g_in[in_idx + 0] + w1 * (int32_t)g_in[in_idx + 1];
                g_acc1 += w2 * (int32_t)g_in[in_idx + 2] + w3 * (int32_t)g_in[in_idx + 3];
            }

            float group_sum = (float)(g_acc0 + g_acc1) * row_scales[g];
            row_acc += group_sum;
        }

        vec_out[r] = row_acc * act_scale;
    }
}

} // namespace MicroQuant
