#pragma once

#include <stdint.h>
#include <stddef.h>

namespace Diagnostics {

struct TelemetryData {
    uint32_t sample_id;
    int64_t  timestamp_ms;
    float    raw_sensors[4];
    float    normalized_sensors[4];
    uint8_t  predicted_class;
    float    confidence;
    float    inference_latency_us;
    float    control_actions[4];
    size_t   free_sram_bytes;
    size_t   free_psram_bytes;
};

class Telemetry {
public:
    static void logHumanReadable(const TelemetryData& data);
    static void logJson(const TelemetryData& data);
};

} // namespace Diagnostics
