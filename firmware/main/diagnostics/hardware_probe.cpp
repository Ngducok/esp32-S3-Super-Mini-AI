#include "hardware_probe.h"
#include <stdio.h>
#include <string.h>
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
    ESP_LOGI(TAG, "Starting CPU & Matrix Multiplication Baseline Benchmark...");
    
    // 16x16 Float Matrix Multiplication Benchmark (1000 runs)
    static float A[16][16], B[16][16], C[16][16];
    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 16; j++) {
            A[i][j] = (float)(i + j) * 0.05f;
            B[i][j] = (float)(i - j) * 0.05f;
            C[i][j] = 0.0f;
        }
    }

    constexpr int ITERS = 1000;
    int64_t t0 = esp_timer_get_time();
    for (int it = 0; it < ITERS; it++) {
        for (int i = 0; i < 16; i++) {
            for (int k = 0; k < 16; k++) {
                float a_ik = A[i][k];
                for (int j = 0; j < 16; j++) {
                    C[i][j] += a_ik * B[k][j];
                }
            }
        }
    }
    int64_t t1 = esp_timer_get_time();
    float avg_us = (float)(t1 - t0) / ITERS;
    ESP_LOGI(TAG, "  16x16 Float MatMul : %d iterations in %.2f ms | Avg: %.2f us/op",
             ITERS, (t1 - t0) / 1000.0f, avg_us);
}

} // namespace Diagnostics
