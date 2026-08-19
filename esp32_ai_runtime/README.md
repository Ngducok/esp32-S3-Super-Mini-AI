# Arduino IDE Standalone Deployment

## Overview

The `esp32_ai_runtime/` directory provides a self-contained, single-file Arduino sketch (`esp32_ai_runtime.ino`) for developers and makers who prefer using the Arduino IDE over the ESP-IDF toolchain.

---

## Architectural Problem & Solution

### Problem
Building an advanced ESP-IDF project requires configuring Python virtual environments, CMake build systems, and Ninja compilers, which presents a steep learning curve for hobbyists and educational environments.

### Solution
The entire runtime (SoftAP WiFi driver, WebServer, INT8 Micro-Transformer decoder, static KV-cache, and REST endpoints) is consolidated into an Arduino-compatible `.ino` sketch:
1. Replaces the ESP-IDF HTTP server with the standard Arduino `WebServer` library.
2. Direct-includes `model_llm_weights.h` and `web_ui.h` from the repository root.
3. Automatically manages the autoregressive token generation loop inside the standard Arduino `loop()` function.

---

## Arduino Runtime Execution Flowchart

```
                 [Arduino setup() Initialization]
                                │
                 ├── Serial.begin(115200)
                 ├── WiFi.softAP("ESP32-Local-AI", "12345678")
                 ├── server.on("/", handleRoot)
                 ├── server.on("/api/status", handleStatus)
                 ├── server.on("/api/chat", handleChatAPI)
                 └── server.begin()
                                │
                                ▼
                 [Arduino loop() Execution Cycle]
                                │
            ┌───────────────────┴───────────────────┐
            ▼                                       ▼
 [server.handleClient()]                  [Serial.available()]
   ├── Handle HTTP requests                 ├── Read incoming prompt
   ├── Execute forwardToken()               ├── Stream token-by-token
   └── Send JSON reply                      └── Print inference metrics
```

---

## Arduino IDE Configuration & Flashing

1. Install the **ESP32 by Espressif Systems** board package in the Arduino IDE Boards Manager.
2. Open `esp32_ai_runtime.ino`.
3. In **Tools > Board**, select **ESP32S3 Dev Module**.
4. Configure the following menu options:
   - **Flash Size**: 4MB (32Mb)
   - **Partition Scheme**: Huge APP (3MB No OTA / 1MB SPIFFS)
   - **USB Mode**: Hardware CDC and JTAG
   - **Upload Mode**: UART0 / Hardware CDC
   - **PSRAM**: Disabled
5. Click **Upload** and open the Serial Monitor at `115200` baud.
