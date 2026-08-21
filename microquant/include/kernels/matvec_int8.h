/**
 * @file matvec_int8.h
 * @brief Symmetric INT8 Matrix-Vector Multiplication Kernel for ESP32 / Bare-Metal
 */

#pragma once
#include <stdint.h>
#include <stddef.h>

namespace MicroQuant {

/**
 * @brief Computes Y = Scale * (W_int8 * X_int8)
 * 
 * @param mat Pointer to row-major INT8 weight matrix in Flash DROM
 * @param vec_in Pointer to INT8 input vector
 * @param vec_out Pointer to output float array
 * @param rows Number of rows in matrix
 * @param cols Number of columns in matrix
 * @param scale_factor Combined scale factor (Scale_W * Scale_X)
 */
inline void matvec_int8(const int8_t* mat,
                        const int8_t* vec_in,
                        float* vec_out,
                        uint32_t rows,
                        uint32_t cols,
                        float scale_factor) {
    for (uint32_t r = 0; r < rows; r++) {
        const int8_t* row = &mat[r * cols];
        int32_t acc0 = 0, acc1 = 0, acc2 = 0, acc3 = 0;
        uint32_t c = 0;
        
        // 16-way unrolled loop
        for (; c + 16 <= cols; c += 16) {
            acc0 += (int32_t)row[c + 0] * (int32_t)vec_in[c + 0]
                  + (int32_t)row[c + 1] * (int32_t)vec_in[c + 1]
                  + (int32_t)row[c + 2] * (int32_t)vec_in[c + 2]
                  + (int32_t)row[c + 3] * (int32_t)vec_in[c + 3];

            acc1 += (int32_t)row[c + 4] * (int32_t)vec_in[c + 4]
                  + (int32_t)row[c + 5] * (int32_t)vec_in[c + 5]
                  + (int32_t)row[c + 6] * (int32_t)vec_in[c + 6]
                  + (int32_t)row[c + 7] * (int32_t)vec_in[c + 7];

            acc2 += (int32_t)row[c + 8] * (int32_t)vec_in[c + 8]
                  + (int32_t)row[c + 9] * (int32_t)vec_in[c + 9]
                  + (int32_t)row[c + 10] * (int32_t)vec_in[c + 10]
                  + (int32_t)row[c + 11] * (int32_t)vec_in[c + 11];

            acc3 += (int32_t)row[c + 12] * (int32_t)vec_in[c + 12]
                  + (int32_t)row[c + 13] * (int32_t)vec_in[c + 13]
                  + (int32_t)row[c + 14] * (int32_t)vec_in[c + 14]
                  + (int32_t)row[c + 15] * (int32_t)vec_in[c + 15];
        }
        
        for (; c + 4 <= cols; c += 4) {
            acc0 += (int32_t)row[c + 0] * (int32_t)vec_in[c + 0];
            acc1 += (int32_t)row[c + 1] * (int32_t)vec_in[c + 1];
            acc2 += (int32_t)row[c + 2] * (int32_t)vec_in[c + 2];
            acc3 += (int32_t)row[c + 3] * (int32_t)vec_in[c + 3];
        }
        
        for (; c < cols; c++) {
            acc0 += (int32_t)row[c] * (int32_t)vec_in[c];
        }
        
        vec_out[r] = (float)(acc0 + acc1 + acc2 + acc3) * scale_factor;
    }
}

} // namespace MicroQuant
