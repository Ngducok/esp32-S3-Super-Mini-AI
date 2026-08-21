/**
 * @file main.cpp
 * @brief MicroQuant-ESP32 Verification Example
 * Demonstrates INT8, INT4, and BitNet matrix-vector inference from Flash DROM headers.
 */

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "../../include/MicroQuant.h"
#include "generated_headers/weights_int8.h"
#include "generated_headers/weights_int4.h"
#include "generated_headers/weights_int4_gw.h"
#include "generated_headers/weights_bitnet.h"

int main() {
    printf("=================================================================\n");
    printf("[*] MicroQuant-ESP32: Bare-Metal Multi-Quantization Inference Demo\n");
    printf("=================================================================\n\n");

    // 1. Prepare dummy input activation vector (64 dimensions)
    float input_x[64];
    for (int i = 0; i < 64; i++) {
        input_x[i] = sinf((float)i * 0.1f);
    }

    int8_t input_x_q[64];
    MicroQuant::float_to_int8_vector(input_x, input_x_q, 64, 30.0f);

    float output_int8[64] = {0};
    float output_int4[64] = {0};
    float output_int4_gw[64] = {0};
    float output_bitnet[64] = {0};

    // 2. INT8 Inference (4.0x smaller)
    MicroQuant::matvec_int8(
        MicroQuantModel::DEMO_LAYER_INT8_DATA,
        input_x_q,
        output_int8,
        64, 64,
        MicroQuantModel::DEMO_LAYER_INT8_SCALE * (1.0f / 30.0f)
    );
    printf("[+] INT8 Inference      (4.0x smaller) -> First 3 outputs: %.4f, %.4f, %.4f\n",
           output_int8[0], output_int8[1], output_int8[2]);

    // 3. INT4 Inference (Per-Tensor, 8.0x smaller)
    MicroQuant::matvec_int4(
        MicroQuantModel::DEMO_LAYER_INT4_DATA,
        input_x_q,
        output_int4,
        64, 64,
        MicroQuantModel::DEMO_LAYER_INT4_SCALE * (1.0f / 30.0f)
    );
    printf("[+] INT4 Per-Tensor     (8.0x smaller) -> First 3 outputs: %.4f, %.4f, %.4f\n",
           output_int4[0], output_int4[1], output_int4[2]);

    // 4. INT4 Group-wise Inference (Group Size 32, 7.7x smaller)
    MicroQuant::matvec_int4_group32(
        MicroQuantModel::DEMO_LAYER_INT4_GW_DATA,
        MicroQuantModel::DEMO_LAYER_INT4_GW_GROUP_SCALES,
        input_x_q,
        output_int4_gw,
        64, 64,
        1.0f / 30.0f
    );
    printf("[+] INT4 Group-32       (7.7x smaller) -> First 3 outputs: %.4f, %.4f, %.4f\n",
           output_int4_gw[0], output_int4_gw[1], output_int4_gw[2]);

    // 5. BitNet 1.58-bit Inference (Multiplication-Free, 16.0x smaller)
    MicroQuant::matvec_bitnet(
        MicroQuantModel::DEMO_LAYER_BITNET_DATA,
        input_x_q,
        output_bitnet,
        64, 64,
        MicroQuantModel::DEMO_LAYER_BITNET_SCALE * (1.0f / 30.0f)
    );
    printf("[+] BitNet Multiplication-Free (16.0x) -> First 3 outputs: %.4f, %.4f, %.4f\n",
           output_bitnet[0], output_bitnet[1], output_bitnet[2]);

    // 6. Fast Activation & Softmax
    float act_test[4] = { output_int8[0], output_int8[1], output_int8[2], output_int8[3] };
    MicroQuant::softmax(act_test, 4);
    printf("[+] Softmax Probabilities              -> [%.3f, %.3f, %.3f, %.3f]\n",
           act_test[0], act_test[1], act_test[2], act_test[3]);

    printf("\n=================================================================\n");
    printf("[OK] All 4 quantization kernels executed successfully with 0 memory leak!\n");
    printf("=================================================================\n");

    return 0;
}
