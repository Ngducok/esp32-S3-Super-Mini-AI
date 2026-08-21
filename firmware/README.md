# ESP32-S3 Firmware Subsystem

<p align="left">
  <b>Detailed Documentation:</b> 
  <a href="../docs/en/FIRMWARE.md">English Guide</a> | 
  <a href="../docs/vn/FIRMWARE.md">Hướng Dẫn Tiếng Việt</a>
</p>

---

## Overview

This directory contains the production C++ ESP-IDF project implementing the on-device Micro-Transformer runtime, dual-core task scheduling, hardware diagnostics, and Flash-resident HTTP server.

## Key Files & Structure

- `CMakeLists.txt`: Root ESP-IDF build configuration.
- `partitions.csv`: 3.5MB application partition table (`0x10000` to `0x380000`).
- `sdkconfig`: Project configuration (240MHz CPU frequency, 4MB SPI Flash).
- `main/`: Core application source files:
  - `main.cpp`: FreeRTOS dual-core initialization (`app_main`).
  - `llm/`: Vectorized SIMD GEMV, FastMath LUTs, Ring-Buffer KV-cache.
  - `diagnostics/`: Silicon probe, heap audit, live CPU benchmark.
  - `config/`: System pinouts, stack sizes, and task priorities.
  - `web/`: SoftAP WiFi controller and HTTP web server daemon.

## Build & Flash

```bash
idf.py build
idf.py -p COM5 flash monitor
```
