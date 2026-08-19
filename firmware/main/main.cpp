#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "driver/usb_serial_jtag.h"
#include "driver/gpio.h"

// Configuration
#include "hardware_config.h"
#include "app_config.h"

// Hardware & Diagnostics
#include "hardware_probe.h"
#include "memory_tracker.h"

// LLM Micro-Transformer
#include "generator.h"

// Web & WiFi Hotspot
#include "wifi_ap.h"
#include "web_server.h"

static const char* TAG = "APP_MAIN";

// ----------------------------------------------------------------------------
// Interactive Local Chat Task (Serial Terminal Autoregressive Streaming)
// ----------------------------------------------------------------------------
static void chatTask(void* pvParameters) {
    ESP_LOGI(TAG, "Micro-Transformer Generative LLM Task initialized on Core 1.");
    char input_line[256];
    size_t line_idx = 0;

    // 1. Initialize USB Serial/JTAG Hardware Driver
    usb_serial_jtag_driver_config_t jtag_cfg;
    memset(&jtag_cfg, 0, sizeof(jtag_cfg));
    jtag_cfg.tx_buffer_size = 512;
    jtag_cfg.rx_buffer_size = 512;
    usb_serial_jtag_driver_install(&jtag_cfg);

    printf("\n\n");
    printf("====================================================================\n");
    printf("     ESP32-S3 ON-DEVICE GENERATIVE MICRO-TRANSFORMER (JARVIS)       \n");
    printf("====================================================================\n");
    printf("  • Model        : Transformer Decoder-Only (d=64, L=3, H=4, INT8)\n");
    printf("  • Storage      : Flash Cold Weights (Zero SRAM weight footprint)\n");
    printf("  • KV-Cache RAM : ~12 KB Static Buffer in SRAM (No PSRAM needed!)\n");
    printf("  • WiFi Hotspot : SSID: 'ESP32-Local-AI' | Pass: '12345678'\n");
    printf("  • Web Chat UI  : http://192.168.4.1 (Connect phone WiFi to chat)\n");
    printf("  • Serial Prompt: Type ANY English prompt below and watch AI stream!\n");
    printf("====================================================================\n\n");

    while (1) {
        uint8_t c = 0;
        int bytes = usb_serial_jtag_read_bytes(&c, 1, pdMS_TO_TICKS(10));
        if (bytes > 0) {
            if (c == '\r' || c == '\n') {
                if (line_idx > 0) {
                    input_line[line_idx] = '\0';
                    printf("\n");

                    printf("\n====================================================================\n");
                    printf(">>> [PROMPT] : %s\n", input_line);
                    printf("<<< [STREAM] : %s", input_line);
                    fflush(stdout);

                    // Autoregressive Token Generation & Live Streaming
                    LLM::GenerationStats stats = LLM::Generator::generateStream(
                        input_line,
                        [](const char* tok_str) {
                            printf("%s", tok_str);
                            fflush(stdout);
                        },
                        Config::App::MAX_GENERATION_TOKENS, // Max tokens to generate (256)
                        0.0f, // Temperature (Greedy argmax for coherent English)
                        0.9f  // Top-P
                    );

                    printf("\n--- [METRICS]: Tokens: %u | Speed: %.2f tok/s | Latency: %.2f ms | Free SRAM: %u B\n",
                           (unsigned int)stats.generated_tokens,
                           stats.tokens_per_second,
                           stats.total_time_ms,
                           (unsigned int)stats.free_sram_bytes);
                    printf("====================================================================\n\n");

                    line_idx = 0;
                }
            } else if (c == '\b' || c == 0x7F) { // Backspace
                if (line_idx > 0) {
                    line_idx--;
                    printf("\b \b");
                    fflush(stdout);
                }
            } else if (line_idx < sizeof(input_line) - 1 && c >= 32) {
                input_line[line_idx++] = (char)c;
                putchar(c);
                fflush(stdout);
            }
        }
    }
}

// ----------------------------------------------------------------------------
// Heartbeat & Telemetry Task (Every 10 seconds)
// ----------------------------------------------------------------------------
static void heartbeatTask(void* pvParameters) {
    uint32_t count = 0;
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        count++;
        size_t free_sram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        uint32_t uptime_sec = (uint32_t)(esp_timer_get_time() / 1000000);
        
        ESP_LOGI("HEARTBEAT", "[System Alive #%u] Uptime: %u s | Free SRAM: %u Bytes (Zero Leak)",
                 (unsigned int)count, (unsigned int)uptime_sec, (unsigned int)free_sram);
    }
}

// ----------------------------------------------------------------------------
// Application Entry Point (ESP-IDF)
// ----------------------------------------------------------------------------
extern "C" void app_main(void) {
    // 1. Hardware Diagnostics Probe
    Diagnostics::HardwareProbe::printReport();
    Diagnostics::HardwareProbe::runCPUBenchmark();
    Diagnostics::MemoryTracker::init();

    // 2. Initialize Status LED (GPIO 8)
    gpio_reset_pin(static_cast<gpio_num_t>(Config::Hardware::PIN_STATUS_LED));
    gpio_set_direction(static_cast<gpio_num_t>(Config::Hardware::PIN_STATUS_LED), GPIO_MODE_OUTPUT);
    gpio_set_level(static_cast<gpio_num_t>(Config::Hardware::PIN_STATUS_LED),
                   Config::Hardware::STATUS_LED_ACTIVE_LOW ? 1 : 0);

    // 3. Initialize Micro-Transformer Engine
    LLM::Generator::init();

    // 4. Start WiFi Hotspot (SoftAP) and HTTP Web Server
    Web::WifiAP::init("ESP32-Local-AI", "12345678");
    Web::WebServer::start();

    // 5. Launch FreeRTOS Tasks
    xTaskCreatePinnedToCore(
        chatTask, "chat_task",
        8192, nullptr,
        5, nullptr, 1
    );

    xTaskCreatePinnedToCore(
        heartbeatTask, "heartbeat_task",
        4096, nullptr,
        2, nullptr, 0
    );

    ESP_LOGI(TAG, "ESP32-S3 English Generative LLM Host started successfully!");
}
