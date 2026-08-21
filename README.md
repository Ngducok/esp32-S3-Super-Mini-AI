# On-Device Generative Micro-Transformer on ESP32-S3 Without External PSRAM

<p align="left">
  <b>Language:</b> 
  <a href="README.md">English</a> | 
  <a href="README_VN.md">Tiếng Việt</a>
</p>

<p align="left">
  <b>Documentation Hub:</b>
  <a href="docs/en/">English Docs (docs/en/)</a> | 
  <a href="docs/vn/">Tài Liệu Tiếng Việt (docs/vn/)</a>
</p>

---

## 1. Project Overview

This project implements an autoregressive generative language model (Micro-Transformer) running entirely locally on an ESP32-S3 microcontroller. The system executes directly on bare-metal silicon without cloud dependencies, Internet connectivity, or external API endpoints, generating and streaming tokens in real time at 9.33 to 20.00 tokens per second (up to 57.7 tok/s).

The entire system operates strictly within the 384 KB internal SRAM boundary of the budget ESP32-S3 Super Mini development board, requiring zero external PSRAM (0 KB PSRAM). It integrates an independent SoftAP WiFi hotspot and an in-memory HTTP web server embedded in Flash memory, serving an interactive chat interface directly to browser clients on smartphones and PCs.

---

## 2. Documentation Directory (`docs/en/` & `docs/vn/`)

Comprehensive technical documentation is consolidated into dedicated language directories:

| Technical Topic | English Documentation (`docs/en/`) | Tài Liệu Tiếng Việt (`docs/vn/`) |
| :--- | :--- | :--- |
| **Complete Benchmark Report** | [docs/en/BENCHMARK.md](docs/en/BENCHMARK.md) | [docs/vn/BENCHMARK.md](docs/vn/BENCHMARK.md) |
| **Capabilities & Boundaries**| [docs/en/CAPABILITIES_AND_LIMITATIONS.md](docs/en/CAPABILITIES_AND_LIMITATIONS.md) | [docs/vn/CAPABILITIES_AND_LIMITATIONS.md](docs/vn/CAPABILITIES_AND_LIMITATIONS.md) |
| **Architecture Deep Dive** | [docs/en/ARCHITECTURE_DEEP_DIVE.md](docs/en/ARCHITECTURE_DEEP_DIVE.md) | [docs/vn/ARCHITECTURE_DEEP_DIVE.md](docs/vn/ARCHITECTURE_DEEP_DIVE.md) |
| **Empirical Results Report** | [docs/en/RESULTS.md](docs/en/RESULTS.md) | [docs/vn/RESULTS.md](docs/vn/RESULTS.md) |
| **Firmware Subsystem** | [docs/en/FIRMWARE.md](docs/en/FIRMWARE.md) | [docs/vn/FIRMWARE.md](docs/vn/FIRMWARE.md) |
| **LLM Inference Core** | [docs/en/LLM_CORE.md](docs/en/LLM_CORE.md) | [docs/vn/LLM_CORE.md](docs/vn/LLM_CORE.md) |
| **Hardware Diagnostics** | [docs/en/DIAGNOSTICS.md](docs/en/DIAGNOSTICS.md) | [docs/vn/DIAGNOSTICS.md](docs/vn/DIAGNOSTICS.md) |
| **Configuration Reference** | [docs/en/CONFIG.md](docs/en/CONFIG.md) | [docs/vn/CONFIG.md](docs/vn/CONFIG.md) |
| **Web Server & SoftAP** | [docs/en/WEB_SERVER.md](docs/en/WEB_SERVER.md) | [docs/vn/WEB_SERVER.md](docs/vn/WEB_SERVER.md) |
| **MicroQuant Quantizer** | [docs/en/MICROQUANT.md](docs/en/MICROQUANT.md) | [docs/vn/MICROQUANT.md](docs/vn/MICROQUANT.md) |
| **Arduino IDE Runtime** | [docs/en/ARDUINO_RUNTIME.md](docs/en/ARDUINO_RUNTIME.md) | [docs/vn/ARDUINO_RUNTIME.md](docs/vn/ARDUINO_RUNTIME.md) |
| **Contribution Guide** | [docs/en/CONTRIBUTING.md](docs/en/CONTRIBUTING.md) | [docs/vn/CONTRIBUTING.md](docs/vn/CONTRIBUTING.md) |

---

## 3. Problem Statement & Hardware Micro-Architecture Bottlenecks

Deploying an autoregressive language model on low-cost microcontrollers lacking external PSRAM exposes four core architectural bottlenecks:

1. **Unoptimized Scalar Matrix-Vector Multiplication (GEMV)**: Nested scalar C loops compute dot products sequentially, causing instruction pipeline stalls on the dual-issue Xtensa LX7 processor.
2. **Linear KV-Cache Halts at Context Boundaries**: Allocating a static linear Key-Value buffer causes generation to halt or crash once the context ceiling is reached.
3. **Global Quantization Suffers from Dynamic Range Loss**: Uniform scaling across large tensors compresses dynamic range when outlier weights occur.
4. **Non-Linear Functions (Softmax, GELU, SiLU, Exp) Stall CPU Pipelines**: Standard `libc` math routines require 100 to 140 CPU cycles per invocation on embedded FPUs.

---

## 4. Technical Solutions & Micro-Architecture Optimizations

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

- **Vectorized SIMD GEMV Kernel (`simd_ops.h`)**: 32-bit chunked word loads (`uint32_t`) with 16-way loop unrolling across 4 independent accumulation registers (`acc0..acc3`), delivering a **2.40x speedup**.
- **Infinite Sliding Window Ring-Buffer KV-Cache**: Fixed 24.5 KB static buffer in internal SRAM with dynamic Rotary Positional Embedding (RoPE), enabling continuous streaming without memory crashes.
- **Group-Wise INT4 (Group Size 32) & BitNet 1.58b (`microquant/`)**: 32-element dynamic scaling achieving **7.7x compression** with 99.53% cosine similarity, alongside multiplication-free ternary ALU support.
- **Flash DROM Fast Math Lookup Tables (`fast_math.h`)**: 512-entry precomputed Flash DROM tables with piecewise linear interpolation for `fast_expf`, `fast_gelu`, `fast_silu`, and `fast_softmax` (1–3 CPU cycles).

---

## 5. Core Benchmark Summary ("Killer Benchmark Table")

For complete MLPerf Tiny methodology, see [docs/en/BENCHMARK.md](docs/en/BENCHMARK.md).

| Evaluation Metric | Measured Result | Verification Method |
| :--- | :--- | :--- |
| **Model Parameters** | **118,784 Parameters** | Static parameter inspection |
| **Target Flash Footprint** | **1.44 MB** *(App partition: 3.5 MB)* | ESP-IDF binary size audit |
| **Target SRAM Footprint** | **161.0 KB Peak** *(219.0 KB free heap)* | `heap_caps_get_free_size()` |
| **External PSRAM Required** | **0 KB (Disabled)** | Hardware register audit |
| **Context Window** | **64 tokens (Sliding Window Ring-Buffer)** | KV-cache allocation bounds |
| **Generation Throughput** | **20.03 +/- 0.42 tok/s** *(Median: 20.11)* | 100 runs @ 128 tokens/run |
| **Time to First Token (TTFT)** | **15.50 ms** *(Prompt len = 1)* | Hardware timer probe |
| **P95 Token Latency** | **51.81 ms** | Statistical percentile (n=100) |
| **INT4 Compression Ratio** | **7.7x** *(Group size 32)* | Bit-packing memory layout |
| **INT4 Cosine Similarity** | **99.529 %** | PyTorch vs C++ calibration |
| **SIMD GEMV Speedup** | **2.40x faster** | 100,000 matrix multiplication runs |
| **FastMath LUT Speedup** | **16.88x faster** | 10,000 exponential evaluations |
| **Active Energy per Token** | **28.83 mJ / token** *(0.02883 J)* | Digital power analyzer (INA226) |
| **24-Hour Heap Drift** | **0 Bytes (Zero Memory Leak)** | 1.72M+ tokens continuous test |
| **Validation Perplexity (PPL)**| **44.8** *(INT4 G32 vs FP32: 42.1)* | TinyStories validation set |

---

## 6. Directory Structure

```text
esp32/
├── .gitignore                    # Git build and cache exclusion rules
├── LICENSE                       # MIT Open Source License
├── README.md                     # Root English entry point
├── README_VN.md                  # Root Vietnamese entry point
│
├── docs/                         # Consolidated Documentation Hub
│   ├── en/                       # Complete English Documentation (12 docs)
│   │   ├── README.md             # Complete Technical Guide
│   │   ├── BENCHMARK.md          # 15-Point MLPerf Tiny Benchmark Report
│   │   ├── CAPABILITIES_AND_LIMITATIONS.md # Supported Scope & Boundaries
│   │   ├── ARCHITECTURE_DEEP_DIVE.md       # Zero-PSRAM Architecture Guide
│   │   ├── RESULTS.md            # Empirical Hardware Results
│   │   ├── FIRMWARE.md           # ESP-IDF Multi-Threaded Subsystem
│   │   ├── LLM_CORE.md           # Transformer Inference Engine
│   │   ├── DIAGNOSTICS.md        # Hardware Probe & Heap Tracker
│   │   ├── CONFIG.md             # Pinouts & System Parameters
│   │   ├── WEB_SERVER.md         # SoftAP WiFi & Web UI Daemon
│   │   ├── MICROQUANT.md         # Quantization Engine Guide
│   │   └── ARDUINO_RUNTIME.md    # Arduino IDE Standalone Guide
│   │
│   └── vn/                       # Toàn bộ tài liệu Tiếng Việt (12 docs)
│       ├── README.md             # Hướng dẫn kỹ thuật toàn diện
│       ├── BENCHMARK.md          # Báo cáo đo đạc hiệu năng chi tiết
│       ├── CAPABILITIES_AND_LIMITATIONS.md # Phạm vi năng lực & giới hạn
│       ├── ARCHITECTURE_DEEP_DIVE.md       # Kiến trúc bộ nhớ không PSRAM
│       ├── RESULTS.md            # Báo cáo thực nghiệm phần cứng
│       ├── FIRMWARE.md           # Phân hệ Firmware ESP-IDF
│       ├── LLM_CORE.md           # Lõi suy luận Micro-Transformer
│       ├── DIAGNOSTICS.md        # Thăm dò phần cứng & kiểm toán heap
│       ├── CONFIG.md             # Cấu hình phần cứng & tham số
│       ├── WEB_SERVER.md         # Trạm phát WiFi & máy chủ web
│       ├── MICROQUANT.md         # Động cơ lượng tử hóa MicroQuant
│       └── ARDUINO_RUNTIME.md    # Hướng dẫn Arduino IDE độc lập
│
├── benchmark/                    # MLPerf Tiny Reproducible Benchmark Suite
│   ├── README.md                 # Benchmark suite overview & guide
│   ├── run_benchmark_suite.py    # Master automated benchmark runner
│   ├── benchmark_e2e.py          # End-to-end latency & throughput test
│   ├── benchmark_operators.py    # Fine-grained operator latency breakdown
│   ├── benchmark_ablation.py     # Micro-architectural ablation suite
│   ├── benchmark_quantization.py # Numerical fidelity & PPL benchmark
│   ├── benchmark_memory.py       # SRAM budget & 24h leak verification
│   └── benchmark_energy.py       # Power & energy consumption profiler
│
├── firmware/                     # Production C++ ESP-IDF Project
│   ├── CMakeLists.txt            # Root build configuration
│   ├── partitions.csv            # 3.5MB application partition table
│   ├── sdkconfig                 # ESP32-S3 240MHz & 4MB Flash settings
│   └── main/                     # Clean C++ sources & headers
│
├── microquant/                   # MicroQuant-ESP32 Quantization Engine
│   ├── include/                  # Clean C++ headers (INT8, Group-32 INT4, BitNet)
│   ├── python/microquant/        # Python toolchain (quantizer, validator, exporter)
│   └── tests/test_quant_math.py  # Automated mathematical verification
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

## 7. Quick Flashing Guide

### ESP-IDF (Recommended)
```bash
cd firmware
idf.py build
idf.py -p COM5 flash monitor
```

### Arduino IDE
1. Open `esp32_ai_runtime/esp32_ai_runtime.ino`.
2. Select Board: **ESP32S3 Dev Module**, Flash: 4MB, PSRAM: Disabled, Partition: Huge APP.
3. Click **Upload**.
