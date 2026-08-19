#pragma once

#include <stdint.h>
#include <stddef.h>

namespace Config {
namespace App {

    // Model runtime parameters
    constexpr uint32_t MAX_GENERATION_TOKENS   = 256;
    constexpr float    DEFAULT_TEMPERATURE     = 0.0f; // Deterministic argmax
    constexpr float    DEFAULT_TOP_P           = 0.9f;

    // FreeRTOS Task Priorities & Stack Sizes
    constexpr uint32_t CHAT_TASK_PRIORITY      = 5;
    constexpr uint32_t CHAT_TASK_STACK_SIZE    = 8192;

    constexpr uint32_t TELEMETRY_TASK_PRIORITY = 2;
    constexpr uint32_t TELEMETRY_TASK_STACK_SIZE = 4096;

    // Telemetry intervals
    constexpr uint32_t HEARTBEAT_INTERVAL_MS   = 10000;
    constexpr size_t   MIN_SAFE_HEAP_BYTES     = 32 * 1024;

} // namespace App
} // namespace Config
