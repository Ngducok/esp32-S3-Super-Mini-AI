# Main Application Component (ESP-IDF)

<p align="left">
  <b>Language:</b> 
  <a href="README.md">English</a> | 
  <a href="README_VN.md">Tiếng Việt</a>
</p>

---

## Overview

The `firmware/main/` directory contains the core application entry point and component subsystems that run on the ESP32-S3 microcontroller. It bridges hardware initialization, dual-core task orchestration, neural network computation, and web serving.

---

## Architecture & Subsystem Breakdown

```
                             [firmware/main/main.cpp]
                                        │
             ┌──────────────────────────┼──────────────────────────┐
             ▼                          ▼                          ▼
       [config/]                  [diagnostics/]                 [llm/]
 • Hardware pin definitions  • Silicon probing API        • Transformer core (INT8)
 • Task priorities & stacks  • Zero-leak heap auditor    • Static SRAM KV-cache (~12KB)
 • Safety thresholds         • Telemetry & JSON stream    • Argmax token sampler
                                                                   │
                                                                   ▼
                                                                [web/]
                                                      • SoftAP WiFi ('ESP32-Local-AI')
                                                      • In-Memory Web Server (Port 80)
                                                      • REST API (/api/chat, /api/status)
```

---

## FreeRTOS Dual-Core Execution Flowchart

```
                 [app_main() Initialization]
                              │
             ┌────────────────┴────────────────┐
             ▼                                 ▼
      [CPU Core 0]                       [CPU Core 1]
 ├── Initialize NVS Flash           ├── Run Hardware Probe
 ├── Init TCP/IP Stack              ├── Initialize Memory Tracker
 ├── Start SoftAP WiFi              ├── Initialize Transformer Engine
 ├── Launch HTTP Web Server         └── Launch chat_task
 └── Run Heartbeat Daemon                 ├── Read Serial USB-JTAG
      (Logs uptime every 10s)             ├── Tokenize Input Prompt
                                          ├── Autoregressive Loop
                                          └── Stream Tokens via Serial
```

---

## Problem & Solution Breakdown

### 1. Dual-Core Load Balancing
- **Problem**: Simultaneous execution of compute-heavy Transformer matrix multiplications and WiFi packet processing on a single CPU core causes networking latency spikes and watchdog timeouts.
- **Solution**: The WiFi event loop and HTTP server run pinned to **CPU Core 0**, while the Transformer generation loop runs on **CPU Core 1**. Both cores communicate safely using thread-safe function callbacks and pre-allocated static buffers.

### 2. Zero Dynamic Memory Allocation
- **Problem**: Repeated heap allocations (`malloc`/`free`) during continuous token generation cause internal SRAM fragmentation, eventually leading to allocation failures on long-running devices.
- **Solution**: All KV-cache buffers ($3 \times 64 \times 64 = 12,288\text{ bytes}$), prompt buffers, and output arrays are statically pre-allocated at compile time.

---

## Subdirectories

- [`config/`](config/README.md): Pinout definitions, task priorities, stack boundaries, and sampling thresholds.
- [`diagnostics/`](diagnostics/README.md): Silicon introspection, heap leak tracking, and CPU matrix multiplication benchmarks.
- [`llm/`](llm/README.md): INT8 Transformer Decoder, static KV-cache manager, and autoregressive streaming generator.
- [`web/`](web/README.md): SoftAP WiFi manager, HTTP daemon, and in-memory Web UI asset serving.
