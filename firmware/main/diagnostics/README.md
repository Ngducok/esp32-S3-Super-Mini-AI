# Hardware Diagnostics & Silicon Telemetry

<p align="left">
  <b>Detailed Documentation:</b> 
  <a href="../../../../docs/en/DIAGNOSTICS.md">English Guide</a> | 
  <a href="../../../../docs/vn/DIAGNOSTICS.md">Hướng Dẫn Tiếng Việt</a>
</p>

---

## Overview

This module provides hardware probing, live micro-benchmarking, and 24-hour continuous heap memory leak tracking on ESP32-S3 silicon.

## Core Modules

- `hardware_probe.cpp` & `hardware_probe.h`: Silicon revision audit, CPU core frequency, Flash size, SRAM bounds, and live micro-benchmark (`runCPUBenchmark`).
- `memory_tracker.cpp` & `memory_tracker.h`: Real-time heap tracking and 24-hour zero-leak verification.
