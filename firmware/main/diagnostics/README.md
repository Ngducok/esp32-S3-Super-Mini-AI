# System Diagnostics, Hardware Probing & Memory Tracking

<p align="left">
  <b>Language:</b> 
  <a href="README.md">English</a> | 
  <a href="README_VN.md">Tiếng Việt</a>
</p>

---

## Overview

The `diagnostics/` directory provides hardware introspection, real-time memory leak auditing, CPU performance benchmarking, and structured telemetry logging.

---

## Architectural Problem & Solution

### Problem
1. When running low-level neural models on microcontrollers without an operating system memory manager, memory leaks can silently exhaust internal SRAM over time, leading to sudden panics (`LoadProhibited`, `Guru Meditation Errors`).
2. Microcontroller hardware capabilities (silicon revision, CPU frequency, SRAM/PSRAM boundaries) vary across board variants and must be dynamically verified at boot time.

### Solution
1. **Zero-Leak Memory Snapshot Auditor (`MemoryTracker`)**:
   Captures internal SRAM and PSRAM high-water marks before and after inference iterations. Computes net memory drift:
   $$\text{Drift} = \text{Free SRAM}_{\text{post}} - \text{Free SRAM}_{\text{baseline}}$$
   If drift is non-zero after hundreds of generations, an immediate warning is emitted.
2. **Dynamic Hardware Probing (`HardwareProbe`)**:
   Queries the ESP-IDF silicon identification API to detect active CPU cores, clock speed (240 MHz), SPI Flash size, and internal heap caps.
3. **Fixed Matrix Multiplication Benchmark**:
   Executes a deterministic $16 \times 16$ float matrix multiplication stress test over 1,000 iterations to evaluate raw floating-point and integer compute throughput.

---

## Diagnostics Execution Flowchart

```
                 [System Boot / Hardware Init]
                               │
                               ▼
                   [Diagnostics::HardwareProbe]
                               │
            ┌──────────────────┼──────────────────┐
            ▼                  ▼                  ▼
     [Read Chip Info]   [Query SPI Flash]  [Query Heap Caps]
      • Model: ESP32-S3  • Flash: 4MB       • Total SRAM: 512KB
      • Rev: v0.2        • Speed: 80MHz     • Free SRAM: ~380KB
      • Cores: 2 @ 240M                     • PSRAM: 0 KB (None)
                               │
                               ▼
                 [Diagnostics::MemoryTracker]
                               │
                               ├── 1. Capture Initial Baseline SRAM
                               ├── 2. Hook Pre-Inference Memory Snapshot
                               ├── 3. Hook Post-Inference Memory Snapshot
                               └── 4. Verify Net Drift == 0 Bytes
                               │
                               ▼
                    [Diagnostics::Telemetry]
                               │
                 ┌─────────────┴─────────────┐
                 ▼                           ▼
       [Human Console Log]          [Structured JSON Stream]
```

---

## Source Files

- `hardware_probe.h` / `hardware_probe.cpp`: Silicon introspection, hardware summary logger, and matrix computation benchmark probe.
- `memory_tracker.h` / `memory_tracker.cpp`: Heap memory snapshot capture, watermark tracking, and long-running leak detection.
- `telemetry.h` / `telemetry.cpp`: Formats performance metrics and system statistics into human-readable logs and JSON packets.
