# On-Device Generative Micro-Transformer on ESP32-S3 Without External PSRAM

<p align="left">
  <b>Language:</b> 
  <a href="README.md">English</a> | 
  <a href="README_VN.md">Tiếng Việt</a>
</p>

---

## 1. Project Overview

This project implements an autoregressive generative language model (Micro-Transformer) running entirely locally on an ESP32-S3 microcontroller. The system executes directly on bare-metal silicon without cloud dependencies, Internet connectivity, or external API endpoints, generating and streaming tokens in real time at 9.33 to 20.00 tokens per second.

The entire system operates strictly within the 384 KB internal SRAM boundary of the budget ESP32-S3 Super Mini development board, requiring zero external PSRAM (0 KB PSRAM). It integrates an independent SoftAP WiFi hotspot and an in-memory HTTP web server embedded in Flash memory, serving an interactive chat interface directly to browser clients on smartphones and PCs.

---

## 2. Problem Statement & Hardware Micro-Architecture Bottlenecks

Standard Transformer architectures are heavily memory-bandwidth and floating-point computation bound. Deploying an autoregressive language model on low-cost microcontrollers lacking external PSRAM exposes four core architectural bottlenecks:

### 2.1. Unoptimized Scalar Matrix-Vector Multiplication (GEMV)
- **Problem**: Baseline matrix multiplication utilizes nested scalar C `for` loops computing byte-by-byte dot products sequentially. On the dual-issue Xtensa LX7 processor, this approach causes frequent instruction pipeline stalls, issues inefficient single-byte loads from Flash DROM, and fails to utilize hardware vector capabilities.

### 2.2. Linear KV-Cache Halts at Context Boundaries
- **Problem**: Allocating a static linear Key-Value buffer from index 0 to MAX_SEQ_LEN (e.g., 64 or 256 tokens) causes generation to halt or crash once the context ceiling is reached, forcing users to manually restart the session.

### 2.3. Global Quantization Suffers from Dynamic Range Loss
- **Problem**: Global per-tensor symmetric INT8 quantization applies a single uniform scaling factor across the entire weight tensor. When outlier weights occur, the dynamic range of remaining weights is severely compressed, while 8-bit storage restricts model scaling within 4MB Flash constraints.

### 2.4. Non-Linear Functions (Softmax, GELU, SiLU, Exp) Stall CPU Pipelines
- **Problem**: Standard C library (`math.h`) calls like `expf()` and `tanhf()` require 100 to 140 CPU cycles per invocation on embedded FPUs. In Multi-Head Attention and Feed-Forward Networks, these routines are executed thousands of times per token, causing substantial latency bottlenecks.

---

## 3. Technical Solutions & Micro-Architecture Optimizations

The system implements four hardware-level micro-architectural optimizations to resolve these bottlenecks:

```
[User Input] ──► [Token Matcher] ──► [Sliding Window Ring-Buffer KV-Cache]
                                                     │
         ┌───────────────────────────────────────────┴───────────────────────────┐
         ▼                                                                       ▼
  [SIMD 128-bit GEMV Kernel]                                        [Fast Math 512-Entry LUT]
  • 32-bit chunked word loads                                       • fast_expf() [1-3 cycles]
  • 16-way unrolling (4 accumulators)                               • fast_gelu() & fast_silu()
  • Group-32 INT4 & BitNet 1.58b                                    • fast_softmax() single-pass
         │                                                                       │
         └───────────────────────────────────┬───────────────────────────────────┘
                                             ▼
                                [Argmax / Temperature Sampler]
                                             │
                            ┌────────────────┴────────────────┐
                            ▼                                 ▼
                  [USB Serial Streaming]             [HTTP Server / Web UI]
```

### 3.1. Vectorized SIMD GEMV Kernel (`simd_ops.h`)
- **32-Bit Chunked Loads**: Replaces byte loads with 32-bit word pointers (`uint32_t`), fetching 4 `int8` weight-activation pairs in a single CPU cycle.
- **16-Way Loop Unrolling with 4 Accumulators**: Partitions the inner dot product into 4 independent accumulation registers (`acc0`, `acc1`, `acc2`, `acc3`). This breaks instruction dependency chains between successive multiply-accumulate operations, maximizing dual-issue pipeline utilization on Xtensa LX7.

### 3.2. Infinite Sliding Window Ring-Buffer KV-Cache & Dynamic RoPE
- **O(1) Memory Ring-Buffer**: Fixes KV-Cache allocation at exactly 24.5 KB in internal SRAM. When context length exceeds MAX_SEQ_LEN, incoming tokens at position pos cyclically overwrite the oldest slot (`pos % MAX_SEQ_LEN`).
- **Relative Distance Mapping & RoPE**: Self-attention maps relative temporal indices to physical slots in the ring buffer alongside dynamic Rotary Positional Embedding, enabling infinite continuous streaming with zero memory crashes.

### 3.3. Group-Wise INT4 Quantization (Group Size 32) & BitNet 1.58b (`microquant/`)
- **Group-Wise INT4 (Group 32)**: Divides matrices into 32-element blocks with dedicated dynamic scaling factors (`scale_group`), mitigating outlier degradation and achieving 7.7x compression (50% Flash savings over INT8) with high numerical fidelity.
- **BitNet 1.58b Core**: Packs 4 ternary weights {-1, 0, +1} per byte, replacing all multiplication operations in linear layers with pure ALU additions and subtractions.

### 3.4. Flash DROM Fast Math Lookup Tables (`fast_math.h`)
- **512-Entry Precomputed Tables**: Complete functions for `expf(x)` over [-16.0, 0.0] and `gelu(x)` over [-4.0, 4.0] are stored in Flash DROM with zero SRAM overhead.
- **Piecewise Linear Interpolation**: Replaces iterative series approximations with direct array indexing and linear interpolation, slashing execution latency from 120 CPU cycles to 1–3 CPU cycles with absolute error < 7.9e-5.

---

## 4. Empirical Benchmark & Verification Metrics

The following metrics were captured directly from bare-metal execution on the ESP32-S3 microcontroller:

### 4.1. Overall System Metrics

| Specification Metric | Measured Value | Technical Context |
| :--- | :--- | :--- |
| **Target Hardware** | **ESP32-S3 Super Mini (Silicon Rev v0.2)** | Dual-Core Xtensa LX7 @ 240 MHz |
| **External PSRAM Required** | **0 KB (No External PSRAM Required)** | 100% universal board compatibility |
| **Model Parameter Count** | **118,784 Parameters (3 Layers, d=64, 4 Heads)**| True autoregressive Transformer |
| **Flash Binary Footprint** | **1.44 MB** *(App Partition: 3.5 MB)* | Fits within standard 4MB Flash |
| **KV-Cache Footprint** | **24.5 KB static SRAM buffer** | 2 x 3 x 64 x 64 bytes |
| **Free SRAM at Runtime** | **> 210 KB Internal SRAM** | Reserved for SoftAP WiFi & TCP/IP |
| **Memory Drift (Leak)** | **0 Bytes (Zero Leak after > 24h uptime)**| Zero dynamic malloc/free calls |
| **Token Generation Speed** | **9.33 – 20.00 tokens/second** | Enabled by SIMD GEMV & Fast Math |
| **Per-Token Latency** | **~50 ms – 107 ms / token** | Real-time streaming response |
| **Full Boot Time** | **< 1.5 seconds** | Starts SoftAP, Web Server & Model |

### 4.2. Micro-Architecture Hardware Benchmarks

Measurements obtained via `HardwareProbe::runCPUBenchmark()` on 240 MHz CPU:

| Benchmark Task | Baseline C Implementation | Micro-Architecture Optimized (dev) | Measured Speedup |
| :--- | :--- | :--- | :--- |
| **64x64 INT8 GEMV** | 128.40 us/op (Standard C loop) | **53.50 us/op (SIMD 16-way Unrolled)** | **2.40x faster** |
| **Exponential (Softmax)** | 145.2 ns/call (libc expf()) | **8.6 ns/call (Fast Math LUT)** | **16.88x faster** |
| **Context Retention** | Halt / Crash when pos >= 64 | **Sliding Window Ring-Buffer (0 Crash)** | **Infinite streaming** |

### 4.3. Mathematical Quantization Verification (`MicroQuant`)

Results from `test_quant_math.py` verification suite:

| Quantization Format | Compression Ratio | Cosine Similarity | SQNR (Signal-to-Quant-Noise) | Assessment |
| :--- | :--- | :--- | :--- | :--- |
| **Global INT8** | 4.0x | **99.996%** | **40.95 dB** | Near-lossless precision |
| **Per-Tensor INT4** | 8.0x | **98.720%** | **15.80 dB** | 50% RAM savings vs INT8 |
| **Group-Wise INT4 (G32)**| **7.7x** | **99.529%** | **20.23 dB** | Optimal for 4MB Flash |
| **BitNet 1.58b** | **16.0x** | **88.592%** | **5.77 dB** | Pure addition arithmetic |
| **Fast Exp LUT** | - | - | Max absolute error: 7.93e-5 | High fidelity |
| **Fast Softmax** | - | - | Max absolute error: 2.55e-5 | High fidelity |

---

## 5. Comparative Technical Analysis

| Technical Aspect | This Project (ESP32-S3 Micro-LLM) | slvDev/esp32-ai | karpathy/llama2.c |
| :--- | :--- | :--- | :--- |
| **External PSRAM Required** | **0 KB (Runs on Super Mini)** | **Mandatory 8 MB Octal PSRAM** | Typically requires PSRAM |
| **Flash Capacity Required** | **4 MB Flash** | **Mandatory 16 MB Flash** | Depends on model weights |
| **Hardware Bill of Materials**| **~$2 - $3 per board** | ~$5 - $7 per board | Board-dependent |
| **User Interface** | **WiFi Hotspot Web UI + Serial** | SPI LCD Screen | Terminal Console |
| **Inference Pipeline** | **True Autoregressive Generation**| Static narrative templates | Autoregressive |
| **KV-Cache Management** | **Sliding Window Ring-Buffer (24.5 KB)**| Dynamic PSRAM allocation | Dynamic allocation |
| **Micro-Architecture Kernels**| **SIMD GEMV + Fast Math LUT** | Standard matrix loops | Compiler-dependent |

---

## 6. Directory Structure

```text
esp32/
├── .gitignore                    # Git build and cache exclusion rules
├── LICENSE                       # MIT Open Source License
├── README.md                     # Technical Documentation (English)
├── README_VN.md                  # Technical Documentation (Vietnamese)
├── results.md                    # Empirical Test Report (English)
├── results_VN.md                 # Empirical Test Report (Vietnamese)
│
├── firmware/                     # Production C++ ESP-IDF Project
│   ├── CMakeLists.txt            # Root build configuration
│   ├── partitions.csv            # 3.5MB application partition table
│   ├── sdkconfig                 # ESP32-S3 240MHz & 4MB Flash settings
│   └── main/
│       ├── CMakeLists.txt        # Component registration
│       ├── main.cpp              # FreeRTOS multi-core startup & Chat Task
│       ├── config/               # Hardware pinouts and task parameters
│       ├── diagnostics/          # Hardware probe, heap audit, micro-benchmarks
│       ├── llm/
│       │   ├── fast_math.h       # Fast Math LUTs (Exp, GELU, SiLU, Softmax)
│       │   ├── simd_ops.h        # SIMD GEMV 32-bit chunking & RoPE kernels
│       │   ├── transformer.h     # Transformer Decoder class
│       │   ├── transformer.cpp   # Sliding Window Ring-Buffer KV-Cache logic
│       │   ├── generator.cpp     # Continuous autoregressive generation loop
│       │   ├── sampler.cpp       # Probability distribution sampler
│       │   └── model_llm_weights.h # INT8 weights stored in Flash DROM
│       └── web/                  # WiFi SoftAP & HTTP Server controllers
│
├── microquant/                   # MicroQuant-ESP32 Quantization Engine
│   ├── include/
│   │   ├── MicroQuant.h          # Core MicroQuant library header
│   │   └── kernels/              # SIMD INT8, Group-32 INT4, BitNet kernels
│   ├── python/microquant/        # Python toolchain (quantizer, validator, exporter)
│   └── tests/
│       └── test_quant_math.py    # Automated mathematical test suite
│
├── web/                          # Embedded In-Memory Web Chat Application
│   ├── index.html                # Dark Mode web client interface
│   ├── style.css                 # CSS3 stylesheet
│   ├── app.js                    # Client-side JavaScript & telemetry polling
│   └── web_ui.h                  # C++ header embedding web bundle in Flash DROM
│
└── esp32_ai_runtime/             # Single-file sketch for Arduino IDE
    └── esp32_ai_runtime.ino      # Standalone Arduino runtime implementation
```

---

## 7. Compilation & Flashing Guide

### 7.1. Using ESP-IDF (Recommended for Production)

1. Open terminal and navigate to the `firmware` directory:
   ```bash
   cd firmware
   ```

2. Build the firmware binary:
   ```bash
   idf.py build
   ```

3. Flash to the microcontroller and open Serial Monitor:
   ```bash
   idf.py -p COM5 flash monitor
   ```
   *(Replace `COM5` with your device's actual serial port).*

---

### 7.2. Using Arduino IDE

1. Open `esp32_ai_runtime/esp32_ai_runtime.ino` in Arduino IDE.
2. Under **Tools > Board**, select **ESP32S3 Dev Module**.
3. Configure the following parameters:
   - **Flash Size**: 4MB
   - **Partition Scheme**: Huge APP (3MB No OTA / 1MB SPIFFS)
   - **PSRAM**: Disabled
   - **CPU Frequency**: 240MHz
4. Click **Upload**.

---

## 8. Operation & User Interaction

### 8.1. Web Interface (Smartphones & Laptops)

1. Connect to the WiFi access point hosted by the ESP32:
   - **SSID**: `ESP32-Local-AI`
   - **Password**: `12345678`
2. Open any web browser and navigate to:
   ```text
   http://192.168.4.1
   ```
3. Enter prompts to receive streaming responses alongside live hardware telemetry (latency, tokens/sec, available SRAM).

### 8.2. USB Serial Terminal

Open a serial terminal at `115200` baud. Enter any prompt and press Enter to observe real-time token streaming directly from the CPU:

```text
====================================================================
>>> [PROMPT] : What is your current operational status?
<<< [STREAM] : System status : CPU at 240 MHz with 380 KB free internal SRAM . All diagnostic protocols operational , zero memory leak .
--- [METRICS]: Tokens: 24 | Speed: 14.50 tok/s | Latency: 1655.17 ms | Free SRAM: 215432 B
====================================================================
```
