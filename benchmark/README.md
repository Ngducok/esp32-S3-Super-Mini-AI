# MLPerf Tiny Reproducible Benchmark Suite

<p align="left">
  <b>Documentation:</b> 
  <a href="../docs/en/BENCHMARK.md">English Benchmark Report</a> | 
  <a href="../docs/vn/BENCHMARK.md">Báo Cáo Benchmark Tiếng Việt</a>
</p>

---

## Overview

This directory contains automated, reproducible benchmark scripts evaluating the on-device Micro-Transformer on ESP32-S3 across 15 standard evaluation dimensions following **MLPerf Tiny** guidelines.

## Quick Start

Execute the complete automated benchmark suite:

```bash
python run_benchmark_suite.py
```

## Individual Benchmark Modules

- `benchmark_e2e.py`: End-to-end token generation throughput, latency percentiles (P95/P99), and TTFT.
- `benchmark_operators.py`: Fine-grained per-operator latency and CPU cycle breakdown.
- `benchmark_ablation.py`: Incremental performance progression across micro-architectural optimizations.
- `benchmark_quantization.py`: Numerical fidelity, SQNR, Top-k logit agreement, and perplexity (PPL).
- `benchmark_memory.py`: Internal SRAM budget allocation and 24-hour continuous memory leak audit.
- `benchmark_energy.py`: Electrical power dissipation and active energy per token (mJ/token).
