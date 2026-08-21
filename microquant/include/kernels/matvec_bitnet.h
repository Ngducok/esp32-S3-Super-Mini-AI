/**
 * @file matvec_bitnet.h
 * @brief BitNet 1.58-Bit Ternary {-1, 0, +1} Multiplication-Free Matrix-Vector Kernel
 * Eliminates all multiplication instructions from the CPU ALU inner loop.
 * 4 weights packed per byte (16x smaller than FP32).
 */

#pragma once
#include <stdint.h>
#include <stddef.h>

namespace MicroQuant {

/**
 * @brief Computes Y = Gamma * (W_ternary * X_int8) using pure additions and subtractions
 * 
 * @param packed_mat Pointer to 2-bit packed ternary matrix in Flash DROM (cols/4 bytes per row)
 * @param vec_in Pointer to INT8 input vector
 * @param vec_out Pointer to output float array
 * @param rows Number of rows in matrix
 * @param cols Number of columns in matrix
 * @param gamma Model scale factor
 */
inline void matvec_bitnet(const uint8_t* packed_mat,
                          const int8_t* vec_in,
                          float* vec_out,
                          uint32_t rows,
                          uint32_t cols,
                          float gamma) {
    const uint32_t bytes_per_row = (cols + 3) / 4;

    for (uint32_t r = 0; r < rows; r++) {
        const uint8_t* row_bytes = &packed_mat[r * bytes_per_row];
        int32_t acc0 = 0;
        int32_t acc1 = 0;
        uint32_t b = 0;

        // Unroll 4 bytes = 16 ternary weights per iteration
        for (; b + 4 <= bytes_per_row; b += 4) {
            uint8_t byte0 = row_bytes[b + 0];
            uint8_t byte1 = row_bytes[b + 1];
            uint8_t byte2 = row_bytes[b + 2];
            uint8_t byte3 = row_bytes[b + 3];
            uint32_t c = b * 4;

            uint8_t c00 = byte0 & 0x03, c01 = (byte0 >> 2) & 0x03, c02 = (byte0 >> 4) & 0x03, c03 = (byte0 >> 6);
            if (c00 == 0x01) acc0 += (int32_t)vec_in[c + 0]; else if (c00 == 0x02) acc0 -= (int32_t)vec_in[c + 0];
            if (c01 == 0x01) acc0 += (int32_t)vec_in[c + 1]; else if (c01 == 0x02) acc0 -= (int32_t)vec_in[c + 1];
            if (c02 == 0x01) acc0 += (int32_t)vec_in[c + 2]; else if (c02 == 0x02) acc0 -= (int32_t)vec_in[c + 2];
            if (c03 == 0x01) acc0 += (int32_t)vec_in[c + 3]; else if (c03 == 0x02) acc0 -= (int32_t)vec_in[c + 3];

            uint8_t c10 = byte1 & 0x03, c11 = (byte1 >> 2) & 0x03, c12 = (byte1 >> 4) & 0x03, c13 = (byte1 >> 6);
            if (c10 == 0x01) acc1 += (int32_t)vec_in[c + 4]; else if (c10 == 0x02) acc1 -= (int32_t)vec_in[c + 4];
            if (c11 == 0x01) acc1 += (int32_t)vec_in[c + 5]; else if (c11 == 0x02) acc1 -= (int32_t)vec_in[c + 5];
            if (c12 == 0x01) acc1 += (int32_t)vec_in[c + 6]; else if (c12 == 0x02) acc1 -= (int32_t)vec_in[c + 6];
            if (c13 == 0x01) acc1 += (int32_t)vec_in[c + 7]; else if (c13 == 0x02) acc1 -= (int32_t)vec_in[c + 7];

            uint8_t c20 = byte2 & 0x03, c21 = (byte2 >> 2) & 0x03, c22 = (byte2 >> 4) & 0x03, c23 = (byte2 >> 6);
            if (c20 == 0x01) acc0 += (int32_t)vec_in[c + 8];  else if (c20 == 0x02) acc0 -= (int32_t)vec_in[c + 8];
            if (c21 == 0x01) acc0 += (int32_t)vec_in[c + 9];  else if (c21 == 0x02) acc0 -= (int32_t)vec_in[c + 9];
            if (c22 == 0x01) acc0 += (int32_t)vec_in[c + 10]; else if (c22 == 0x02) acc0 -= (int32_t)vec_in[c + 10];
            if (c23 == 0x01) acc0 += (int32_t)vec_in[c + 11]; else if (c23 == 0x02) acc0 -= (int32_t)vec_in[c + 11];

            uint8_t c30 = byte3 & 0x03, c31 = (byte3 >> 2) & 0x03, c32 = (byte3 >> 4) & 0x03, c33 = (byte3 >> 6);
            if (c30 == 0x01) acc1 += (int32_t)vec_in[c + 12]; else if (c30 == 0x02) acc1 -= (int32_t)vec_in[c + 12];
            if (c31 == 0x01) acc1 += (int32_t)vec_in[c + 13]; else if (c31 == 0x02) acc1 -= (int32_t)vec_in[c + 13];
            if (c32 == 0x01) acc1 += (int32_t)vec_in[c + 14]; else if (c32 == 0x02) acc1 -= (int32_t)vec_in[c + 14];
            if (c33 == 0x01) acc1 += (int32_t)vec_in[c + 15]; else if (c33 == 0x02) acc1 -= (int32_t)vec_in[c + 15];
        }

        for (; b < bytes_per_row; b++) {
            uint8_t byte = row_bytes[b];
            uint32_t c = b * 4;
            uint8_t c0 = byte & 0x03, c1 = (byte >> 2) & 0x03, c2 = (byte >> 4) & 0x03, c3 = (byte >> 6);
            if (c + 0 < cols) { if (c0 == 0x01) acc0 += (int32_t)vec_in[c + 0]; else if (c0 == 0x02) acc0 -= (int32_t)vec_in[c + 0]; }
            if (c + 1 < cols) { if (c1 == 0x01) acc0 += (int32_t)vec_in[c + 1]; else if (c1 == 0x02) acc0 -= (int32_t)vec_in[c + 1]; }
            if (c + 2 < cols) { if (c2 == 0x01) acc0 += (int32_t)vec_in[c + 2]; else if (c2 == 0x02) acc0 -= (int32_t)vec_in[c + 2]; }
            if (c + 3 < cols) { if (c3 == 0x01) acc0 += (int32_t)vec_in[c + 3]; else if (c3 == 0x02) acc0 -= (int32_t)vec_in[c + 3]; }
        }

        vec_out[r] = (float)(acc0 + acc1) * gamma;
    }
}

} // namespace MicroQuant
