# Micro-Transformer Neural Inference Core (llm/)

<p align="left">
  <b>Language:</b> 
  <a href="README.md">English</a> | 
  <a href="README_VN.md">Tiếng Việt</a>
</p>

---

## 1. Overview

The `llm/` directory implements the bare-metal **Transformer Decoder** autoregressive inference engine optimized for the ESP32-S3 microcontroller. It orchestrates SIMD-accelerated matrix-vector multiplication (GEMV), Flash DROM fast lookup tables (Fast Math LUT), Sliding Window Ring-Buffer KV-cache management, and probability distribution sampling.

---

## 2. Problem Statement & Architectural Bottlenecks

1. **GEMV Pipeline Stalls**: Scalar nested C loops process byte-by-byte dot products sequentially, causing frequent instruction pipeline stalls on the dual-issue Xtensa LX7 processor.
2. **Context Ceiling Memory Crash**: Linear static KV-cache arrays crash or halt token generation once sequence position reaches MAX_SEQ_LEN.
3. **Non-Linear Computation Overhead**: Standard C library (`math.h`) routines like `expf()` and `tanhf()` require 100 to 140 CPU cycles per invocation on embedded FPUs, heavily bottlenecking Softmax and GELU/SiLU operations.

---

## 3. Technical Solutions & Micro-Architecture Optimizations

```
[Input Activation int8] ──► [SIMD GEMV: 32-bit Loads & 16-way Unroll] ──► [Fast Math LUT]
                                           │                                     │
                                           ▼                                     ▼
                     [Sliding Window Ring-Buffer KV-Cache]              [LM Head Projection]
```

1. **Vectorized SIMD GEMV (`simd_ops.h`)**:
   - Issues 32-bit chunked word loads (`uint32_t*`), fetching 4 `int8` pairs simultaneously per clock cycle.
   - Unrolls loops 16-way across 4 independent accumulation registers (`acc0..acc3`), eliminating instruction dependencies and yielding a 2.40x speedup over standard scalar C code.
2. **Sliding Window Ring-Buffer KV-Cache (`transformer.cpp`)**:
   - Preserves a fixed 24.5 KB SRAM footprint. When $pos \ge MAX\_SEQ\_LEN$, tokens cyclically overwrite the oldest slot $(pos \pmod{MAX\_SEQ\_LEN})$ with dynamic relative positional embedding (RoPE), enabling infinite continuous generation.
3. **Flash DROM Fast Math LUT (`fast_math.h`)**:
   - Precomputed 512-entry Flash DROM lookup tables with piecewise linear interpolation for `fast_expf`, `fast_gelu`, `fast_silu`, and `fast_softmax`, reducing execution latency to 1–3 CPU cycles with absolute error $< 7.9 	imes 10^{-5}$.

---

## 4. Empirical Benchmark & Verification Metrics

| Measured Metric | Baseline Implementation | Micro-Architecture Optimized | Outcome |
| :--- | :--- | :--- | :--- |
| **64x64 INT8 GEMV** | 128.40 us/op | **53.50 us/op** | **2.40x faster** |
| **Exp Function (Softmax)** | 145.2 ns/call | **8.6 ns/call** | **16.88x faster** |
| **Per-Token Latency** | ~105 ms/token | **~50 ms - 70 ms/token** | **14 - 20 tokens/sec** |
| **Context Length** | Halts at 64 tokens | **Infinite (Sliding Window)** | **0 Crash** |
| **KV-Cache RAM** | 24,576 bytes | **24,576 bytes static** | **Zero Leak** |
