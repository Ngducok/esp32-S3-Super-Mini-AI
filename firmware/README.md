# ESP-IDF Firmware Subsystem

## Overview

The `firmware/` directory contains the core industrial C++ ESP-IDF project for the ESP32-S3 microcontroller. It orchestrates the multi-threaded execution of the Micro-Transformer language model, FreeRTOS task scheduling, standalone SoftAP WiFi networking, HTTP web server daemon, and memory telemetry.

---

## Architectural Problem & Solution

### Problem
Running an autoregressive generative model alongside a WiFi access point and an HTTP server on a microcontroller with no external PSRAM presents severe memory contention and CPU scheduling challenges:
1. WiFi and TCP/IP protocol buffers require significant dynamic memory (~100 KB - 150 KB).
2. Continuous token generation loops can easily starve the FreeRTOS idle task, triggering the Task Watchdog (`task_wdt`) on CPU cores.
3. Flash size limitations require expanding the default 1MB factory partition to accommodate large quantized embedding and weight tables.

### Solution
1. **Custom Partition Table (`partitions.csv`)**:
   Expands the `factory` app partition to 3.5 MB (`0x380000`), allowing zero-copy Flash-resident model storage.
2. **Dual-Core Task Pinning**:
   - **CPU Core 0**: Dedicated to WiFi SoftAP event loops, TCP/IP stack, and HTTP web server request handling.
   - **CPU Core 1**: Dedicated to the interactive serial shell and Transformer token generation (`chat_task`).
3. **Watchdog Yielding**:
   Inserts explicit non-blocking tick delays (`vTaskDelay(pdMS_TO_TICKS(1))`) between token generation iterations, yielding execution to FreeRTOS watchdog tasks while maintaining >15 tok/s perceived streaming speed.

---

## Task Execution Flowchart

```
                 [app_main() Boot Initialization]
                                │
       ┌────────────────────────┴────────────────────────┐
       ▼                                                 ▼
[CPU 0: Web & Network Stack]                   [CPU 1: Compute Core]
  ├── NVS & TCP/IP Init                          ├── Hardware Probing
  ├── SoftAP WiFi ('ESP32-Local-AI')             ├── Memory Tracker Init
  ├── HTTP Daemon (Port 80)                      ├── LLM Engine Init
  │     ├── GET  /       (Web UI)                └── chat_task Execution
  │     ├── GET  /status (Telemetry)                   ├── USB-JTAG Polling
  │     └── POST /chat   (Inference)                   ├── Autoregressive Loop
  └── Heartbeat Daemon (10s loop)                      └── Stream Tokens to Serial
```

---

## Subsystem Directory Structure

```text
firmware/
├── CMakeLists.txt              # Root CMake build definition
├── sdkconfig                   # Preconfigured ESP-IDF settings (240MHz, 4MB Flash)
├── partitions.csv              # Custom 3.5MB application partition layout
└── main/
    ├── CMakeLists.txt          # Main component source & include registration
    ├── main.cpp                # System entry point and FreeRTOS task creation
    ├── config/                 # Hardware pin assignments and task priority parameters
    ├── diagnostics/            # Hardware probe, heap audit, and telemetry loggers
    ├── llm/                    # Micro-Transformer core, sampler, and INT8 weights
    └── web/                    # SoftAP WiFi manager and HTTP REST API handlers
```

---

## Build and Flash Workflow

```bash
# 1. Navigate to the firmware directory
cd firmware

# 2. Build the binary
idf.py build

# 3. Flash to the microcontroller and launch serial monitor
idf.py -p COM5 flash monitor
```
