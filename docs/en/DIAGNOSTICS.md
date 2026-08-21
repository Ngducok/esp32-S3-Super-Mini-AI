# System Diagnostics, Hardware Probing & Memory Tracking (diagnostics/)

<p align="left">
  <b>Language:</b> 
  <a href="README.md">English</a> | 
  <a href="README_VN.md">Tiếng Việt</a>
</p>

---

## 1. Overview

The `diagnostics/` directory provides bare-metal hardware introspection, real-time memory leak auditing, CPU performance benchmarking, and structured telemetry logging.

---

## 2. Problem Statement & Technical Solution

### Problem
1. Operating neural models on microcontrollers without virtual memory managers can silently exhaust internal SRAM, triggering fatal exceptions (`LoadProhibited`, `Guru Meditation`).
2. Micro-architectural throughput (SIMD GEMV speedup and LUT latency) must be directly verified on silicon.

### Solution
1. **Zero-Leak Memory Auditor (`MemoryTracker`)**: Captures internal SRAM watermarks before and after inference passes:
   $$\text{Drift} = \text{Free SRAM}_{\text{post}} - \text{Free SRAM}_{\text{baseline}}$$
   Guarantees zero net heap drift across continuous multi-hour runs.
2. **Hardware Benchmark Suite (`HardwareProbe`)**: Evaluates 1,000 iterations of 64x64 INT8 GEMV (Scalar vs SIMD) and 10,000 calls of `expf()` (libc vs Fast Math LUT).

---

## 3. Measured Silicon Benchmark Metrics

| Benchmark Task | Baseline C Implementation | Micro-Architecture Optimized | Result |
| :--- | :--- | :--- | :--- |
| **64x64 INT8 GEMV** | 128.40 us/op | **53.50 us/op** | **2.40x faster** |
| **Exponential Function** | 145.2 ns/call | **8.6 ns/call** | **16.88x faster** |
| **Heap Drift** | - | **0 Bytes (Zero Leak)** | **Absolute Stability** |
| **Probe Initialization**| - | **< 10 ms** | **Instant Ready** |
