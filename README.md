# ESP32-S3 On-Device Micro-LLM & Edge Intelligence Host

[![Platform: ESP-IDF](https://img.shields.io/badge/Platform-ESP--IDF_v5.x_|_v6.x-blue.svg)](https://github.com/espressif/esp-idf)
[![Target: ESP32-S3](https://img.shields.io/badge/Target-ESP32--S3_Dual--Core_240MHz-red.svg)](https://www.espressif.com/en/products/socs/esp32-s3)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![Memory: Zero PSRAM](https://img.shields.io/badge/PSRAM-0KB_Required_(Super_Mini)-orange.svg)](results.md)
[![Speed: 15+ tok/s](https://img.shields.io/badge/Speed-15--20_tokens/s-brightgreen.svg)](results.md)

A 100% standalone, on-device **Generative Language Model (Micro-Transformer)** and Edge Intelligence system running on the **ESP32-S3 Super Mini** (4MB Flash, 380KB internal SRAM, **0 KB external PSRAM**).

No internet connection, external servers, or paid API keys required. Includes an embedded **WiFi SoftAP Hotspot** and **ChatGPT-style Dark Mode Web UI** running entirely from microcontroller Flash memory.

---

## 🌟 Key Features

- 🧠 **On-Device Transformer Decoder**: Multi-head self-attention engine executing INT8 quantized weights directly from Flash DROM.
- ⚡ **Zero PSRAM Requirement**: Static KV-cache engineered to consume only ~12 KB in internal SRAM, leaving >220 KB free RAM for networking and FreeRTOS.
- 🚀 **High-Speed Inference**: Generates text at **9.33 – 20.00 tokens/second** on dual-core 240 MHz Xtensa LX7 silicon.
- 📶 **Built-in WiFi Hotspot (SoftAP)**: Broadcasts SSID `ESP32-Local-AI` (Password: `12345678`) for direct smartphone/laptop browser connectivity.
- 💬 **Embedded Web Chat UI**: ChatGPT-style dark mode web interface stored in Flash DROM with zero filesystem overhead.
- 🤖 **Interactive Conversational Persona**: On-device responses for storytelling, jokes, developer humor, and system diagnostics.
- 🛠️ **Dual Runtime Support**: Fully compatible with both **ESP-IDF** (C++20/C++17) and **Arduino IDE**.

---

## 🏗️ System Architecture

```
                     ┌─────────────────────────────────────────────────────────┐
                     │          ESP32-S3 SUPER MINI HARDWARE (240MHz)          │
                     └────────────────────────────┬────────────────────────────┘
                                                  │
                   ┌──────────────────────────────┴──────────────────────────────┐
                   ▼                                                             ▼
┌──────────────────────────────────────┐                      ┌──────────────────────────────────────┐
│       NEURAL COMPUTATION CORE        │                      │        COMMUNICATION & UI CORE       │
├──────────────────────────────────────┤                      ├──────────────────────────────────────┤
│ • Micro-Transformer Decoder (INT8)   │                      │ • SoftAP WiFi: 'ESP32-Local-AI'      │
│ • Static SRAM KV-Cache (~12KB)       │                      │ • Flash-Resident Web UI (Port 80)    │
│ • Zero-Copy Flash DROM Weights       │                      │ • Dual-Core Live Streaming Engine    │
│ • Greedy Argmax Token Sampler        │                      │ • GPIO 8 Hardware Actuator Control   │
└──────────────────────────────────────┘                      └──────────────────────────────────────┘
```

---

## 📊 Technical Benchmarks

| Metric | Measured Value | Remarks |
| :--- | :--- | :--- |
| **Inference Generation Speed** | **9.33 – 20.00 tokens/sec** | Measured on Xtensa LX7 @ 240 MHz |
| **Token Latency** | **~50 ms – 107 ms / token** | Real-time interactive feel |
| **Internal SRAM Footprint** | **~12 KB (KV-Cache)** | Leaves **> 229 KB Free SRAM** |
| **Flash Binary Size** | **1.44 MB** | Easily fits inside standard 4MB Flash |
| **External PSRAM Required** | **0 KB** | Runs on sub-$3 budget boards |
| **Memory Drift (Leak)** | **0 Bytes** | Zero dynamic heap allocation in loop |

---

## 📁 Repository Structure

```text
esp32/
├── firmware/                     # ESP-IDF C++ Project
│   ├── CMakeLists.txt            # Top-level build configuration
│   ├── partitions.csv            # 3.5MB application partition layout
│   ├── sdkconfig                 # ESP32-S3 hardware & flash configuration
│   └── main/
│       ├── config/               # Hardware pinout & application settings
│       ├── diagnostics/          # CPU benchmarks, heap tracker, telemetry
│       ├── llm/                  # Transformer engine, sampler, INT8 weights
│       ├── web/                  # SoftAP WiFi driver & HTTP Web Server
│       └── main.cpp              # Multi-threaded FreeRTOS entry point
│
├── web/                          # Standalone Web Application
│   ├── index.html                # Responsive HTML5 chat template
│   ├── style.css                 # Dark Mode stylesheet
│   ├── app.js                    # Client-side JavaScript & telemetry poller
│   ├── generate_web_header.py    # Asset bundler (compiles web to C++ header)
│   └── web_ui.h                  # Flash DROM header included by firmware
│
├── esp32_ai_runtime/             # Standalone Arduino IDE Sketch
│   └── esp32_ai_runtime.ino      # Single-file Arduino IDE deployment
│
├── results.md                    # Detailed research paper & benchmark report
├── LICENSE                       # MIT License
└── README.md                     # Project documentation
```

---

## 🚀 Quick Start Guide

### Option 1: Using ESP-IDF (Recommended)

1. **Clone the repository**:
   ```bash
   git clone https://github.com/your-username/esp32-s3-micro-llm.git
   cd esp32-s3-micro-llm/firmware
   ```

2. **Build and flash**:
   ```bash
   idf.py build
   idf.py -p COM5 flash monitor
   ```

### Option 2: Using Arduino IDE

1. Open `esp32_ai_runtime/esp32_ai_runtime.ino` in Arduino IDE.
2. Select Board: **ESP32S3 Dev Module** (Flash Size: **4MB**, Partition Scheme: **Huge APP (3MB)**).
3. Click **Upload**.

---

## 💬 How to Interact

1. **Via Web Browser (Smartphone / PC)**:
   - Connect to WiFi: **`ESP32-Local-AI`** (Password: `12345678`).
   - Open browser to `http://192.168.4.1`.
   - Send prompts or tap quick action chips.

2. **Via Serial Monitor (PowerShell / Terminal)**:
   - Open Serial Terminal at `115200` baud.
   - Type prompt and press Enter to watch live streamed token generation!

---

## 📜 License

This project is licensed under the **MIT License** - see the [LICENSE](LICENSE) file for details.
