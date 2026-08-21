# Arduino IDE Standalone Deployment (esp32_ai_runtime/)

<p align="left">
  <b>Language:</b> 
  <a href="README.md">English</a> | 
  <a href="README_VN.md">Tiếng Việt</a>
</p>

---

## 1. Overview

The `esp32_ai_runtime/` directory provides a self-contained, single-file Arduino sketch (`esp32_ai_runtime.ino`) for developers who prefer the Arduino IDE over the ESP-IDF toolchain.

---

## 2. Problem Statement & Technical Solution

### Problem
Setting up ESP-IDF toolchains with CMake and Python environments presents a high barrier to entry for rapid prototyping and education.

### Solution
The entire generative pipeline (Transformer Decoder, SIMD GEMV, Fast Math LUTs, Sliding Window KV-cache, SoftAP WiFi, and Web Server) is consolidated into `esp32_ai_runtime.ino`:
1. Utilizes the standard Arduino `WebServer` library to host the chat UI.
2. Integrates hardware-level micro-architecture optimizations (`simd_ops.h`, `fast_math.h`).
3. Executes autoregressive generation continuously inside Arduino's `loop()` routine.

---

## 3. Arduino IDE Flashing Guide

1. Select Board: **ESP32S3 Dev Module**.
2. Configure parameters:
   - **Flash Size**: 4MB
   - **Partition Scheme**: Huge APP (3MB No OTA / 1MB SPIFFS)
   - **PSRAM**: Disabled
   - **CPU Frequency**: 240MHz
3. Click **Upload**.
