#pragma once

#include <stdint.h>
#include <stddef.h>

namespace Diagnostics {

struct MemorySnapshot {
    size_t free_sram;
    size_t min_sram;
    size_t largest_sram_block;
    size_t free_psram;
    int64_t timestamp_us;
};

class MemoryTracker {
public:
    static void init();
    static MemorySnapshot captureSnapshot();
    static void recordInferenceStart();
    static void recordInferenceEnd();
    static void printAudit(uint32_t total_inferences);

private:
    static MemorySnapshot s_initial_snapshot;
    static MemorySnapshot s_pre_inference_snapshot;
    static size_t s_baseline_sram;
};

} // namespace Diagnostics
