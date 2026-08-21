# Arduino IDE Standalone Runtime Sketch

<p align="left">
  <b>Detailed Documentation:</b> 
  <a href="../docs/en/ARDUINO_RUNTIME.md">English Guide</a> | 
  <a href="../docs/vn/ARDUINO_RUNTIME.md">Hướng Dẫn Tiếng Việt</a>
</p>

---

## Overview

A standalone, single-file Arduino sketch (`esp32_ai_runtime.ino`) implementing the entire Micro-Transformer pipeline, SoftAP WiFi hotspot, and Web UI for quick experimentation in Arduino IDE without setting up ESP-IDF.

## Quick Start (Arduino IDE)

1. Open `esp32_ai_runtime.ino`.
2. Under **Tools > Board**, select **ESP32S3 Dev Module**.
3. Configure settings:
   - **Flash Size**: 4MB
   - **Partition Scheme**: Huge APP (3MB No OTA / 1MB SPIFFS)
   - **PSRAM**: Disabled
   - **CPU Frequency**: 240MHz
4. Click **Upload**.
