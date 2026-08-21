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

This is an on-device autoregressive generative language model (Micro-Transformer) running entirely locally on an ESP32-S3 microcontroller. It executes directly on bare-metal silicon with zero cloud dependencies, generating and streaming tokens at 9.33 to 20.00 tokens per second (bursting up to 57.7 tok/s). It requires zero external PSRAM, operating strictly within the 384KB internal SRAM boundary of the budget ESP32-S3 Super Mini development board.

The system pairs the Transformer decoder engine with an independent SoftAP WiFi hotspot and an in-memory HTTP web server, serving a responsive dark-mode chat interface directly from microcontroller Flash memory.

---

## Inspiration and Relationship to slvDev/esp32-ai

This project was inspired by the memory tiering philosophy demonstrated in [slvDev/esp32-ai](https://github.com/slvDev/esp32-ai), which showed how large embedding tables (such as Google's Per-Layer Embeddings from [Gemma 3n](https://ai.google.dev/gemma/docs/gemma-3n)) can live in slow Flash memory while fast compute executes on microcontroller silicon.

While `slvDev/esp32-ai` targets larger ESP32-S3 modules equipped with 8MB Octal PSRAM and 16MB Flash to drive an external SPI LCD display, this project explores an alternative architectural challenge: **How far can we push on-device neural text generation on a minimal $2 development board with 0 KB external PSRAM and standard 4MB Flash?**

### Architectural Comparison

| Architectural Aspect | `slvDev/esp32-ai` | This Project (ESP32-S3 Micro-LLM) |
| :--- | :--- | :--- |
| **Inspiration Source** | Google Gemma 3n (Per-Layer Embeddings) | `slvDev/esp32-ai` & LLaMA Decoder Architecture |
| **Target Hardware** | ESP32-S3 N16R8 (16MB Flash / 8MB PSRAM) | ESP32-S3 Super Mini (4MB Flash / **0 KB PSRAM**) |
| **Memory Allocation** | Relies on 8MB Octal PSRAM for KV/weights | **100% Internal SRAM only** (~24.5 KB KV-Cache) |
| **Output Interface** | SPI LCD Screen wired to GPIOs | **SoftAP WiFi Hotspot + In-Memory Web Chat UI** |
| **Serving Mechanism** | Local SPI frame buffer | Asynchronous HTTP REST API on Port 80 |
| **Context Retention** | Static buffer in PSRAM | **Sliding Window Ring-Buffer (Zero Crash)** |
| **Micro-Kernels** | Standard matrix loops | **128-bit SIMD GEMV + FastMath 512-LUT** |
| **Cost & Accessibility** | Higher-end module (~$5 - $7) | Ultra-budget module (~$2) |

---

## The Numbers (Verified Hardware Benchmarks)

For full MLPerf Tiny methodology, see [docs/en/BENCHMARK.md](docs/en/BENCHMARK.md).

| Metric | Specification / Measured Result | Verification Method |
| :--- | :--- | :--- |
| **SoC Target** | **ESP32-S3 Super Mini (Silicon Rev v0.2)** | Dual-Core Xtensa LX7 @ 240 MHz |
| **External PSRAM Required**| **Disabled / 0 KB Required** | Hardware register audit |
| **Internal SRAM Footprint**| **161.0 KB Peak** *(219.0 KB free heap)* | `heap_caps_get_free_size()` |
| **Model Parameters** | **118,784 Parameters** (~119K INT8, 3 Layers, $d=64$, 4 Heads) | Static parameter audit |
| **Flash Binary Footprint** | **1.44 MB** *(App partition: 3.5 MB)* | ESP-IDF binary size audit |
| **KV-Cache Footprint** | **24.5 KB static buffer** in SRAM ($2 \times 3 \times 64 \times 64\text{ B}$) | Sliding Window Ring-Buffer |
| **Generation Throughput** | **20.03 +/- 0.42 tok/s** *(Median: 20.11, Burst: 57.7)* | 100 runs @ 128 tokens/run |
| **Time to First Token (TTFT)**| **15.50 ms** *(Prompt len = 1)* | Hardware timer probe (`esp_timer`) |
| **P95 Token Latency** | **51.81 ms** | Statistical percentile ($n=100$) |
| **SIMD GEMV Speedup** | **2.40x faster** | 100,000 matrix multiplication runs |
| **FastMath LUT Speedup** | **16.88x faster** | 10,000 exponential evaluations |
| **Active Energy per Token**| **28.83 mJ / token** *(0.02883 J)* | Digital power analyzer (INA226) |
| **24-Hour Memory Drift** | **0 Bytes (Zero Memory Leak)** | 1.72M+ tokens continuous test |
| **Validation Perplexity** | **44.8** *(INT4 G32 vs FP32: 42.1)* | TinyStories validation set |

> [!NOTE]
> **Why 0 KB PSRAM by Design? (Even if your board has 2MB PSRAM)**:
> While some ESP32-S3 chip variants (like ESP32-S3R2) feature 2MB embedded PSRAM, many entry-level boards (like ESP32-S3-N4) have **0 KB PSRAM**. 
> 
> 1. **Universal Hardware Compatibility**: By restricting memory consumption to internal SRAM, this firmware runs out-of-the-box on **any** ESP32-S3 development board ($2 bare-metal silicon).
> 2. **SRAM Single-Cycle Speed**: Internal SRAM runs at full 240 MHz CPU bus speed (~960 MB/s single-cycle access), whereas PSRAM traverses an external SPI bus (40–80 MHz) with bus contention and latency penalties.
> 3. **Optional Expansion**: Users with 2MB/8MB PSRAM can easily toggle `CONFIG_SPIRAM=y` in `sdkconfig` to scale context length to 512+ tokens.

---

## How It Works (Under the Hood)

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

### 1. Vectorized SIMD GEMV Kernel (`simd_ops.h`)
- Replaces byte loads with 32-bit word pointers (`uint32_t`), fetching 4 `int8` weight-activation pairs in a single cycle.
- Partitions the inner dot product into 4 independent accumulation registers (`acc0..acc3`), eliminating pipeline stalls and delivering a **2.40x speedup**.

### 2. Infinite Sliding Window Ring-Buffer KV-Cache
- Allocates a fixed 24.5 KB static buffer in internal SRAM. When context exceeds 64 tokens, incoming tokens cyclically overwrite the oldest slot $(pos \pmod{64})$ with dynamic RoPE adjustment, enabling continuous generation with zero memory crashes.

### 3. Flash DROM Fast Math Lookup Tables (`fast_math.h`)
- 512-entry precomputed tables in Flash DROM with piecewise linear interpolation for `fast_expf`, `fast_gelu`, `fast_silu`, and `fast_softmax`, reducing execution from 120+ cycles to **1–3 CPU cycles** (16.88x speedup).

### 4. Advanced Quantization Engine (`microquant/`)
- Group-Wise INT4 (Group size 32) achieves **7.7x compression** (50% Flash savings over INT8) with 99.53% cosine similarity, alongside BitNet 1.58b multiplication-free ternary ALU support.

---

## Documentation Hub (`docs/en/` & `docs/vn/`)

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

## Directory Structure

```text
esp32/
├── .gitignore                    # Git build and cache exclusion rules
├── LICENSE                       # MIT Open Source License
├── README.md                     # Root English entry point
├── README_VN.md                  # Root Vietnamese entry point
│
├── docs/                         # Consolidated Documentation Hub
│   ├── en/                       # Complete English Documentation (12 docs)
│   └── vn/                       # Toàn bộ tài liệu Tiếng Việt (12 docs)
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

## Quick Start & Flashing Guide

### Using ESP-IDF (Recommended for Production)
```bash
cd firmware
idf.py build
idf.py -p COM5 flash monitor
```

### Using Arduino IDE
1. Open `esp32_ai_runtime/esp32_ai_runtime.ino`.
2. Under **Tools > Board**, select **ESP32S3 Dev Module**.
3. Configure settings:
   - **Flash Size**: 4MB
   - **Partition Scheme**: Huge APP (3MB No OTA / 1MB SPIFFS)
   - **PSRAM**: Disabled
   - **CPU Frequency**: 240MHz
4. Click **Upload**.

---

## Interacting with the Model

### 1. Web Interface (Smartphones & Laptops)
- Connect to WiFi: **SSID**: `ESP32-Local-AI` | **Password**: `12345678`
- Open browser at `http://192.168.4.1` to chat in real time.

### 2. USB Serial Terminal
- Open Serial Monitor at **`115200`** baud, type any prompt and press Enter.
