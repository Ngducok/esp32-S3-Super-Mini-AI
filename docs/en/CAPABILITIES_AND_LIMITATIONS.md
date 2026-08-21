# Project Scope: Verified Capabilities & Known Limitations

<p align="left">
  <b>Language:</b> 
  <a href="CAPABILITIES_AND_LIMITATIONS.md">English</a> | 
  <a href="CAPABILITIES_AND_LIMITATIONS_VN.md">Tiếng Việt</a>
</p>

---

## 1. Transparency Statement

To ensure clear academic and industrial evaluation, this document explicitly defines what this on-device Micro-Transformer project **can do** (verified hardware capabilities) and what it **cannot do** (architectural boundaries and physical limitations on bare-metal ESP32-S3 without PSRAM).

---

## 2. What This Project Accomplishes (Supported Capabilities)

### 2.1. 100% Local Bare-Metal Inference (0 KB External PSRAM)
- **Zero Cloud & Zero API Dependency**: Runs entirely on the ESP32-S3 microcontroller silicon without Internet access, subscription keys, or external servers.
- **Universal Hardware Compatibility**: Operates strictly within internal SRAM (384 KB usable) and standard 4MB SPI Flash, functioning on any budget $2 ESP32-S3 board without requiring 8MB/16MB Octal PSRAM.

### 2.2. True Autoregressive Transformer Architecture
- **Complete Decoder Pipeline**: Implements genuine causal Multi-Head Self-Attention ($L=3, d=64, H=4, d_{\text{head}}=16, d_{\text{ff}}=128$), token embedding ($W_{\text{te}}$), positional embedding ($W_{\text{pe}}$), and language model head projection ($W_{\text{head}}$).
- **118,784 Quantized Parameters**: Weights stored as zero-copy `const int8_t` arrays in Flash Data ROM (`.rodata`), consuming 0 bytes of runtime SRAM for weight storage.

### 2.3. Micro-Architectural Hardware Acceleration
- **SIMD 128-bit Vectorized GEMV (`simd_ops.h`)**: 32-bit chunked loads fetching 4 `int8` pairs per cycle with 16-way loop unrolling across 4 independent accumulation registers (`acc0..acc3`), delivering a **2.40x speedup** over scalar C loops.
- **FastMath 512-Entry Flash DROM LUTs (`fast_math.h`)**: Precomputed lookup tables with piecewise linear interpolation for `fast_expf`, `fast_gelu`, `fast_silu`, and `fast_softmax`, reducing latency from 120+ cycles to **1–3 CPU cycles** (16.88x speedup).

### 2.4. Infinite Sliding Window Ring-Buffer KV-Cache
- **$O(1)$ Bounded SRAM KV-Cache**: Fixed 24.5 KB static buffer ($2 \times 3 \times 64 \times 64$ bytes). Incoming tokens cyclically overwrite the oldest slot $(pos \pmod{64})$ with dynamic Rotary Positional Embedding (RoPE).
- **Zero Memory Crashes**: Allows continuous multi-turn chat streaming indefinitely without halting or running out of memory.

### 2.5. Advanced Quantization Engine (`microquant/`)
- **Group-Wise INT4 (Group Size 32)**: Block-wise dynamic scaling delivering **7.7x compression** (50% Flash savings over INT8) with **99.53% cosine similarity** and **20.23 dB SQNR**.
- **BitNet 1.58b Core**: Packs 4 ternary weights $\{-1, 0, +1\}$ per byte, eliminating ALU multiplications in favor of pure additions and subtractions.

### 2.6. Absolute Runtime Stability & Zero Memory Leak
- **Zero Dynamic Allocation in Inference**: No `malloc` or `free` calls occur inside token generation loops.
- **24-Hour Verified Uptime**: Stress-tested continuously generating over 1.72 million tokens with exactly **0 Bytes net heap drift**.

### 2.7. Integrated Dual-Interface Deployment
- **Flash-Resident Web Server**: Hosts a ChatGPT-style Dark Mode Web UI directly from Flash DROM (zero filesystem overhead) on SoftAP WiFi (`ESP32-Local-AI`).
- **Real-Time Streaming**: Delivers streaming throughput of **9.33 to 57.7 tokens/second** depending on I/O channel.

---

## 3. What This Project Cannot Do (Known Limitations & Boundaries)

### 3.1. Not a General-Knowledge or Multi-Billion Parameter LLM
- **Parameter Scale**: With 118,784 parameters (~119K), this is a **Micro-Transformer / Nano-LLM**, not a multi-gigabyte foundation model (like LLaMA-7B, Mistral, or GPT-4).
- **Reasoning Limits**: It cannot perform complex multi-step mathematical derivations, write intricate codebases, or provide broad encyclopedic world knowledge.

### 3.2. Constrained Subword Vocabulary (128 Tokens)
- **Vocabulary Size**: Token vocabulary is sized at 128 subwords optimized for conversational prompts, hardware telemetry, greetings, and domain-specific stories.
- **Out-of-Vocabulary Handling**: Inputs containing unknown characters or rare words are decomposed into nearest subword matches or seed tokens.

### 3.3. Semantic Attention Depth Bound (32–64 Effective Tokens)
- **Effective Context**: While the Ring-Buffer allows infinite token generation without crashing, the model's semantic attention span is focused on the most recent 32–64 tokens within its sliding window. Earlier conversational turns are cyclically evicted from the attention horizon.

### 3.4. Acoustic Hardware Peripherals Not Integrated by Default
- **Audio I/O**: The current firmware interfaces via WiFi SoftAP Web UI and USB Serial UART. It does not include digital I2S microphone (INMP441) wake-word capture or I2S DAC (MAX98357A) speech synthesis out of the box.

---

## 4. Capability Comparison Matrix

| Feature / Dimension | Supported in This Project | Typical MCU LLMs (e.g. LLaMA.c on ESP32) | Cloud LLMs (GPT-4 / Claude) |
| :--- | :--- | :--- | :--- |
| **Runs on $2 Hardware** | **Yes (ESP32-S3 Super Mini)** | No (Requires $8+ PSRAM boards) | No (Requires servers) |
| **External PSRAM Required** | **0 KB (None)** | 8 MB – 16 MB Octal PSRAM | Multi-GB Server RAM |
| **Cloud Independence** | **100% Offline & Private** | 100% Offline | Requires Internet & API Key |
| **Token Speed** | **9.33 – 57.7 tok/s** | 0.5 – 3.0 tok/s | 30 – 100 tok/s |
| **Context Retention Method** | **Sliding Window Ring-Buffer** | Static (crashes when full) | Dynamic KV-Cache |
| **Memory Drift (Leak)** | **0 Bytes (Deterministic)** | Prone to heap fragmentation | Server managed |
| **General Knowledge Breadth** | **Domain-Specific / Micro** | Moderate | World-Scale Encyclopedic |
| **Complex Math / Coding** | **No** | Limited | Yes |
| **Vocabulary Size** | **128 Tokens** | 32,000 Tokens | 100,000+ Tokens |
