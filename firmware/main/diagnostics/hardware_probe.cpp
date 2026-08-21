#include "hardware_probe.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "fast_math.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_log.h"

static const char* TAG = "HARDWARE_PROBE";

namespace Diagnostics {

esp_err_t HardwareProbe::runProbe(SystemInfo* out_info) {
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    SystemInfo info = {};
    if (chip_info.model == CHIP_ESP32S3) {
        info.chip_model = "ESP32-S3 (Xtensa LX7)";
    } else if (chip_info.model == CHIP_ESP32) {
        info.chip_model = "ESP32";
    } else {
        info.chip_model = "Unknown ESP32 Variant";
    }

    info.revision_major = chip_info.revision / 100;
    info.revision_minor = chip_info.revision % 100;
    info.cpu_cores = chip_info.cores;

#if CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ
    info.cpu_freq_mhz = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ;
#else
    info.cpu_freq_mhz = 240;
#endif

    uint32_t flash_size = 0;
    esp_flash_get_size(NULL, &flash_size);
    info.flash_size_bytes = flash_size;

    info.free_sram_bytes = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    info.min_free_sram_bytes = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    info.total_sram_bytes = heap_caps_get_total_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

    info.total_psram_bytes = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    info.free_psram_bytes = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    info.has_psram = (info.total_psram_bytes > 0);

    if (out_info) {
        *out_info = info;
    }

    return ESP_OK;
}

void HardwareProbe::printReport() {
    SystemInfo info;
    runProbe(&info);

    ESP_LOGI(TAG, "============================================================");
    ESP_LOGI(TAG, "              ESP32-S3 SYSTEM HARDWARE PROBE                ");
    ESP_LOGI(TAG, "============================================================");
    ESP_LOGI(TAG, "  Chip Model       : %s", info.chip_model);
    ESP_LOGI(TAG, "  Silicon Revision : v%u.%u", (unsigned int)info.revision_major, (unsigned int)info.revision_minor);
    ESP_LOGI(TAG, "  CPU Cores / Freq : %u Cores @ %u MHz", info.cpu_cores, (unsigned int)info.cpu_freq_mhz);
    ESP_LOGI(TAG, "  Flash Size       : %u MB (%u Bytes)", 
             (unsigned int)(info.flash_size_bytes / (1024 * 1024)), (unsigned int)info.flash_size_bytes);
    ESP_LOGI(TAG, "  Internal SRAM    : Free: %u B (%.2f KB) | Min Free: %u B", 
             (unsigned int)info.free_sram_bytes, info.free_sram_bytes / 1024.0f, (unsigned int)info.min_free_sram_bytes);
    if (info.has_psram) {
        ESP_LOGI(TAG, "  External PSRAM   : DETECTED -> Total: %u B (%.2f MB) | Free: %u B",
                 (unsigned int)info.total_psram_bytes, info.total_psram_bytes / (1024.0f * 1024.0f), (unsigned int)info.free_psram_bytes);
    } else {
        ESP_LOGI(TAG, "  External PSRAM   : NOT DETECTED (Running in High-Efficiency SRAM mode)");
    }
    ESP_LOGI(TAG, "============================================================");
}

void HardwareProbe::runCPUBenchmark() {
    ESP_LOGI(TAG, "Starting Micro-Architecture Hardware Benchmarks...");
    
    // 1. 64x64 INT8 GEMV Benchmark (Scalar C loop vs SIMD 4-way Unrolled)
    static int8_t mat[64 * 64];
    static int8_t vec_in[64];
    static float vec_out_scalar[64];
    static float vec_out_simd[64];
    
    for (int i = 0; i < 64 * 64; i++) mat[i] = (int8_t)((i % 25) - 12);
    for (int i = 0; i < 64; i++) vec_in[i] = (int8_t)((i % 15) - 7);

    constexpr int GEMV_ITERS = 1000;
    
    // Scalar C GEMV
    int64_t t_scalar_0 = esp_timer_get_time();
    for (int it = 0; it < GEMV_ITERS; it++) {
        for (uint32_t r = 0; r < 64; r++) {
            int32_t acc = 0;
            const int8_t* row = &mat[r * 64];
            for (uint32_t c = 0; c < 64; c++) {
                acc += (int32_t)row[c] * (int32_t)vec_in[c];
            }
            vec_out_scalar[r] = (float)acc * (1.0f / 900.0f);
        }
    }
    int64_t t_scalar_1 = esp_timer_get_time();
    float scalar_us = (float)(t_scalar_1 - t_scalar_0) / GEMV_ITERS;

    // SIMD 4-way Unrolled GEMV
    int64_t t_simd_0 = esp_timer_get_time();
    for (int it = 0; it < GEMV_ITERS; it++) {
        for (uint32_t r = 0; r < 64; r++) {
            const int8_t* row = &mat[r * 64];
            int32_t acc0 = 0, acc1 = 0, acc2 = 0, acc3 = 0;
            uint32_t c = 0;
            for (; c + 16 <= 64; c += 16) {
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
            vec_out_simd[r] = (float)(acc0 + acc1 + acc2 + acc3) * (1.0f / 900.0f);
        }
    }
    int64_t t_simd_1 = esp_timer_get_time();
    float simd_us = (float)(t_simd_1 - t_simd_0) / GEMV_ITERS;
    float gemv_speedup = scalar_us / (simd_us > 0.001f ? simd_us : 1.0f);

    ESP_LOGI(TAG, "  64x64 INT8 GEMV Baseline : %.2f us/op (%.2f ms total)", scalar_us, (t_scalar_1 - t_scalar_0) / 1000.0f);
    ESP_LOGI(TAG, "  64x64 INT8 GEMV SIMD     : %.2f us/op (%.2f ms total) -> SPEEDUP: %.2fx faster!",
             simd_us, (t_simd_1 - t_simd_0) / 1000.0f, gemv_speedup);
    
    volatile float gemv_sink = vec_out_scalar[0] + vec_out_simd[0];
    (void)gemv_sink;

    // 2. expf vs Fast Math LUT Benchmark (10,000 runs)
    constexpr int EXP_ITERS = 10000;
    float test_x[64];
    for (int i = 0; i < 64; i++) test_x[i] = -((float)(i % 16));
    
    volatile float sum_libc = 0.0f;
    int64_t t_exp_libc_0 = esp_timer_get_time();
    for (int it = 0; it < EXP_ITERS; it++) {
        sum_libc += expf(test_x[it % 64]);
    }
    int64_t t_exp_libc_1 = esp_timer_get_time();
    float libc_exp_ns = (float)(t_exp_libc_1 - t_exp_libc_0) * 1000.0f / EXP_ITERS;

    volatile float sum_lut = 0.0f;
    int64_t t_exp_lut_0 = esp_timer_get_time();
    for (int it = 0; it < EXP_ITERS; it++) {
        sum_lut += LLM::FastMath::fast_expf(test_x[it % 64]);
    }
    int64_t t_exp_lut_1 = esp_timer_get_time();
    float lut_exp_ns = (float)(t_exp_lut_1 - t_exp_lut_0) * 1000.0f / EXP_ITERS;
    float exp_speedup = libc_exp_ns / (lut_exp_ns > 0.001f ? lut_exp_ns : 1.0f);

    ESP_LOGI(TAG, "  Standard libc expf()     : %.1f ns/call", libc_exp_ns);
    ESP_LOGI(TAG, "  Fast Math Exp LUT        : %.1f ns/call -> SPEEDUP: %.2fx faster!", lut_exp_ns, exp_speedup);
}

} // namespace Diagnostics
