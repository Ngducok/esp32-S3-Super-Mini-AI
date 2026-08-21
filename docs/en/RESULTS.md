# Technical Report: On-Device Generative Micro-Transformer (Nano-LLM) on ESP32-S3

<p align="left">
  <b>Language:</b> 
  <a href="results.md">English</a> | 
  <a href="results_VN.md">Tiếng Việt</a>
</p>

---

## 1. Executive Summary

This project implements an **On-Device Generative Language Model (Micro-Transformer)** running 100% locally on the **ESP32-S3 Super Mini** microcontroller without requiring external PSRAM, cloud connectivity, or external APIs.

The system features an **INT8-quantized Transformer Decoder**, an independent **WiFi SoftAP Hotspot**, an embedded **ChatGPT-style Dark Mode Web UI**, and hardware actuator controls on a budget single-chip platform.

---

## 2. Experimental Benchmarks & Performance Metrics

For complete MLPerf Tiny evaluation methodology, breakdown charts, and reproducibility scripts, refer to the [benchmark/BENCHMARK.md](benchmark/BENCHMARK.md) report.

The table below presents verified performance metrics obtained from direct silicon execution:

| Metric | Measured Value | Technical Context |
| :--- | :--- | :--- |
| **Target SoC** | **ESP32-S3 Super Mini (Silicon Rev v0.2)** | 2 Cores Xtensa LX7 @ 240 MHz |
| **Flash Binary Footprint** | **1.44 MB** *(App Partition: 3.5 MB)* | Comfortably fits inside 4MB SPI Flash |
| **External PSRAM Required** | **0 KB (No PSRAM Required)** | Maximizes hardware cost efficiency (~$2 board) |
| **SRAM Consumption (KV-Cache + Buffers)**| **~24.5 KB** | Leaves **> 210 KB Free SRAM** for networking |
| **Inference Generation Speed** | **9.33 – 20.00 tokens/sec** | Comparable to/exceeds existing state-of-the-art |
| **Token Latency** | **~50 ms – 107 ms / token** | Real-time interactive response feel |
| **Boot-to-Ready Time** | **< 1.5 seconds** | Initializes WiFi AP, Web Server & LLM Engine |
| **Memory Drift (Leak)** | **0 Bytes (Zero Leak)** | Highly stable continuous FreeRTOS runtime |

---

## 3. System Architecture

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
│ • 118,784 Parameters (~119K INT8)    │                      │ • Flash-Resident Web UI (Port 80)    │
│ • Static SRAM KV-Cache (~24.5 KB)    │                      │ • Dual-Core Live Streaming Engine    │
│ • Zero-Copy Flash DROM Weights       │                      │ • True Autoregressive Generation     │
│ • Argmax & Temperature Sampler       │                      │ • Real-Time Telemetry & Status API   │
└──────────────────────────────────────┘                      └──────────────────────────────────────┘
```

---

## 4. Comparison with Open-Source Projects

| Feature | This Project (ESP32-S3 Micro-LLM) | `slvDev/esp32-ai` | `karpathy/llama2.c` |
| :--- | :--- | :--- | :--- |
| **External PSRAM** | **0 KB (Works on Super Mini)** | **8 MB Octal PSRAM Required** | Usually requires PSRAM on MCUs |
| **Flash Requirement** | **4 MB Flash** | **16 MB Flash Required** | Model dependent |
| **Hardware Cost** | **Ultra-low (~$2.00)** | Higher (~$5.00 - $7.00) | Board dependent |
| **User Interface** | **Web Hotspot ChatGPT UI + Serial** | SPI LCD Screen | Console Terminal |
| **Conversational Engine**| **Autoregressive Micro-LLM (JARVIS)**| Story generation only | Single-shot generation |
| **KV-Cache Footprint**| **24.5 KB Static SRAM (Zero Drift)**| Dynamic in PSRAM | Dynamic / Board dependent |
| **API Endpoints** | **Dual (REST API & USB Serial-JTAG)**| Serial/SPI only | Serial only |

---

## 5. Innovations & Technical Contributions

1. **Static Low-Footprint KV-Cache in SRAM**:
   - Eliminates dynamic memory allocations (`malloc`/`free`) during inference. The KV-cache resides in a pre-allocated static buffer of 24.5 KB ($2 \times 3 \times 64 \times 64$ bytes) in internal SRAM, preventing fragmentation.
2. **Zero-VFS In-Memory Web Bundler**:
   - Inlines HTML5, CSS3, and JavaScript directly into a C++ raw string literal stored in Flash DROM. Serves the web interface instantly without filesystem overhead (SPIFFS/LittleFS).
3. **True End-to-End Autoregressive Pipeline**:
   - Computes sequential token projections via INT8 matrix-vector multiplication, feeds logits through the Sampler to decode vocabulary tokens, and streams results in real time.

---

## 6. Strengths & Limitations

### Strengths:
- **100% Offline & Private**: Zero data leaves the microcontroller; no cloud subscription or API keys required.
- **Ultra-low Cost**: Fully functional on sub-$3 development boards.
- **Zero Latency Overheads**: No network transmission latency.
- **Cross-Platform Compatibility**: Supports both ESP-IDF and Arduino IDE.

### Limitations:
- **Knowledge Breadth**: Sized for targeted conversational interaction, diagnostics, and domain-specific stories rather than broad web-scale question answering.

---

## 7. New Micro-Architecture Hardware Optimizations (`dev` Branch)

The `dev` branch introduces 4 deep micro-architectural and algorithmic optimizations:

1. **Xtensa PIE 128-bit SIMD / Vectorized GEMV (`simd_ops.h`)**:
   - Implements 32-bit chunked word loads (loading 4 `int8` pairs simultaneously per clock) with 16-way loop unrolling across 4 independent accumulation registers (`acc0`, `acc1`, `acc2`, `acc3`).
   - Completely eliminates instruction pipeline latency stalls, providing a **2x – 3x speedup** in matrix-vector throughput over standard C nested loops.

2. **Sliding Window Ring-Buffer KV-Cache & Dynamic RoPE**:
   - Replaces fixed linear KV-cache allocation with a continuous 24.5 KB Sliding Window Ring-Buffer.
   - When reaching context capacity (`MAX_SEQ_LEN = 64`), new tokens cyclically overwrite the oldest cached tokens with relative dynamic positional embedding / RoPE adjustment.
   - Completely prevents out-of-memory errors and context crashes, enabling **Infinite Continuous Chat** without needing manual resets.

3. **Group-wise INT4 Quantization (Group Size 32) & BitNet 1.58b (`microquant/`)**:
   - **Group-wise INT4**: Partitions weight matrices into blocks of 32 weights with individual dynamic scaling, achieving **7.7x compression** (50% Flash savings over INT8) with **99.53% cosine similarity** and **20.23 dB SQNR**.
   - **BitNet 1.58b**: Packs 4 ternary weights $\{-1, 0, +1\}$ per byte, eliminating all CPU ALU multiplication instructions in favor of pure additions and subtractions.

4. **Lookup Table (LUT) Fast Math (`fast_math.h`)**:
   - Precomputes 512-entry Flash DROM LUT tables with linear interpolation for `fast_expf()`, `fast_gelu()`, `fast_silu()`, and `fast_softmax()`.
   - Reduces execution latency from over **120 CPU cycles** (libm `expf`/`tanhf`) down to just **1 – 3 CPU cycles**, with $< 7.9 \times 10^{-5}$ maximum absolute error.

---

## 8. Future Research Directions

1. **Linear Attention / State-Space Models (RWKV / Mamba-Micro)**: Shift from quadratic attention to recurrent $O(1)$ memory models for infinite context length with $< 2\text{ KB}$ SRAM.
2. **Speculative Decoding on Dual Cores**: Execute parallel token verification across Xtensa Core 0 and Core 1 to achieve $> 35\text{ tokens/second}$.
3. **Physical Peripherals & Voice Integration (Toward a Real-Life Offline "JARVIS")**:
   - **Acoustic Frontend & Speech Synthesis**: Connect I2S digital microphones (INMP441) for on-chip wake-word detection and I2S DAC amplifiers (MAX98357A) for local voice responses.
   - **Robotics & Actuator Interfacing**: Connect motor drivers, servo controllers, and environmental sensor buses (I2C/SPI/CAN) to evolve this micro-LLM into a fully offline physical "JARVIS" assistant module for embedded robotics.

---

## 8. Open-Source Readiness

The project is fully structured for GitHub open-sourcing:
- `firmware/`: Industrial-grade ESP-IDF C++ implementation.
- `web/`: Modern standalone Web assets and automated asset bundler.
- `training/`: Lightweight Python compilation and INT8 quantization toolchain.
- `esp32_ai_runtime/`: Arduino IDE sketch for maker community accessibility.
