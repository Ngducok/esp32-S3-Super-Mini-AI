#pragma once

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

namespace Diagnostics {

struct SystemInfo {
    const char* chip_model;
    uint32_t revision_major;
    uint32_t revision_minor;
    uint8_t cpu_cores;
    uint32_t cpu_freq_mhz;
    uint32_t flash_size_bytes;
    bool has_psram;
    size_t total_psram_bytes;
    size_t free_psram_bytes;
    size_t total_sram_bytes;
    size_t free_sram_bytes;
    size_t min_free_sram_bytes;
};

class HardwareProbe {
public:
    static esp_err_t runProbe(SystemInfo* out_info = nullptr);
    static void printReport();
    static void runCPUBenchmark();
};

} // namespace Diagnostics
