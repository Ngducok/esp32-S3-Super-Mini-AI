#include "memory_tracker.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_log.h"

static const char* TAG = "MEM_TRACKER";

namespace Diagnostics {

MemorySnapshot MemoryTracker::s_initial_snapshot = {};
MemorySnapshot MemoryTracker::s_pre_inference_snapshot = {};
size_t MemoryTracker::s_baseline_sram = 0;

void MemoryTracker::init() {
    s_initial_snapshot = captureSnapshot();
    s_baseline_sram = s_initial_snapshot.free_sram;
    ESP_LOGI(TAG, "Memory Tracker Initialized. Baseline SRAM: %u Bytes", (unsigned int)s_baseline_sram);
}

MemorySnapshot MemoryTracker::captureSnapshot() {
    MemorySnapshot snap;
    snap.free_sram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    snap.min_sram = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    snap.largest_sram_block = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    snap.free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    snap.timestamp_us = esp_timer_get_time();
    return snap;
}

void MemoryTracker::recordInferenceStart() {
    s_pre_inference_snapshot = captureSnapshot();
}

void MemoryTracker::recordInferenceEnd() {
    MemorySnapshot post = captureSnapshot();
    if (post.free_sram < s_pre_inference_snapshot.free_sram) {
        size_t diff = s_pre_inference_snapshot.free_sram - post.free_sram;
        ESP_LOGW(TAG, "Warning: Heap dropped by %u bytes during single inference run!", (unsigned int)diff);
    }
}

void MemoryTracker::printAudit(uint32_t total_inferences) {
    MemorySnapshot current = captureSnapshot();
    int32_t net_sram_drift = (int32_t)current.free_sram - (int32_t)s_baseline_sram;

    ESP_LOGI(TAG, "----------------- MEMORY AUDIT (%u INFERENCES) -----------------", (unsigned int)total_inferences);
    ESP_LOGI(TAG, "  Current Free SRAM   : %u Bytes (%.2f KB)", (unsigned int)current.free_sram, current.free_sram / 1024.0f);
    ESP_LOGI(TAG, "  Min Free Watermark  : %u Bytes (%.2f KB)", (unsigned int)current.min_sram, current.min_sram / 1024.0f);
    ESP_LOGI(TAG, "  Largest Free Block  : %u Bytes", (unsigned int)current.largest_sram_block);
    ESP_LOGI(TAG, "  Net SRAM Drift      : %d Bytes (%s)", 
             (int)net_sram_drift, (net_sram_drift >= 0) ? "STABLE / ZERO LEAK" : "POTENTIAL LEAK DETECTED");
    if (current.free_psram > 0) {
        ESP_LOGI(TAG, "  Current Free PSRAM  : %u Bytes (%.2f MB)", (unsigned int)current.free_psram, current.free_psram / (1024.0f * 1024.0f));
    }
    ESP_LOGI(TAG, "------------------------------------------------------------------");
}

} // namespace Diagnostics
