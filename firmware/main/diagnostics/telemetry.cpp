#include "telemetry.h"
#include <stdio.h>
#include "esp_log.h"

static const char* TAG = "TELEMETRY";

namespace Diagnostics {

static const char* s_class_names[] = {"NORMAL", "WARNING", "DANGER", "UNKNOWN"};

void Telemetry::logHumanReadable(const TelemetryData& data) {
    const char* class_str = (data.predicted_class < 3) ? s_class_names[data.predicted_class] : s_class_names[3];

    ESP_LOGI(TAG, "====================== [AI INFERENCE #%u] ======================", (unsigned int)data.sample_id);
    ESP_LOGI(TAG, "  Sensors Raw    : Temp: %.1f C | Hum: %.1f %% | Light: %.0f lx | Pres: %.1f hPa",
             data.raw_sensors[0], data.raw_sensors[1], data.raw_sensors[2], data.raw_sensors[3]);
    ESP_LOGI(TAG, "  AI Prediction  : Class: %s (ID: %u) | Confidence: %.2f%%",
             class_str, data.predicted_class, data.confidence * 100.0f);
    ESP_LOGI(TAG, "  Performance    : Compute Latency: %.2f us (%.3f ms)",
             data.inference_latency_us, data.inference_latency_us / 1000.0f);
    ESP_LOGI(TAG, "  Action Vector  : LED: %.2f | PWM: %.2f | Fan: %.2f | Relay: %.2f",
             data.control_actions[0], data.control_actions[1], data.control_actions[2], data.control_actions[3]);
    ESP_LOGI(TAG, "  Memory Health  : Free SRAM: %u B | Free PSRAM: %u B",
             (unsigned int)data.free_sram_bytes, (unsigned int)data.free_psram_bytes);
    ESP_LOGI(TAG, "===============================================================");
}

void Telemetry::logJson(const TelemetryData& data) {
    // Formatted JSON string for PC / Web dashboard consumption
    printf("{\"type\":\"telemetry\",\"id\":%u,\"time_ms\":%lld,\"temp\":%.2f,\"hum\":%.2f,\"light\":%.2f,\"pres\":%.2f,\"pred\":%u,\"conf\":%.4f,\"lat_us\":%.1f,\"sram\":%u,\"psram\":%u}\n",
           (unsigned int)data.sample_id,
           (long long)data.timestamp_ms,
           data.raw_sensors[0], data.raw_sensors[1], data.raw_sensors[2], data.raw_sensors[3],
           data.predicted_class, data.confidence,
           data.inference_latency_us,
           (unsigned int)data.free_sram_bytes, (unsigned int)data.free_psram_bytes);
}

} // namespace Diagnostics
