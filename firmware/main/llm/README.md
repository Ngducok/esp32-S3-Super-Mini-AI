# Neural Computation Core (LLM Engine)

<p align="left">
  <b>Detailed Documentation:</b> 
  <a href="../../../../docs/en/LLM_CORE.md">English Guide</a> | 
  <a href="../../../../docs/vn/LLM_CORE.md">Hướng Dẫn Tiếng Việt</a>
</p>

---

## Overview

The `llm/` component contains the micro-architectural inference engine executing the 118,784-parameter Micro-Transformer on the ESP32-S3 Xtensa LX7 dual-core processor.

## Core Modules

- `simd_ops.h`: 32-bit chunked word loads with 16-way loop unrolling across 4 accumulation registers (`acc0..acc3`), delivering 2.40x GEMV acceleration.
- `fast_math.h`: 512-entry Flash DROM precomputed lookup tables with piecewise linear interpolation for `fast_expf`, `fast_gelu`, `fast_silu`, and `fast_softmax` (1–3 CPU cycles).
- `transformer.cpp` & `transformer.h`: Autoregressive Decoder with $O(1)$ 24.5 KB Sliding Window Ring-Buffer KV-cache and Dynamic RoPE.
- `generator.cpp` & `generator.h`: Tokenizer and streaming generation loop with 24-token frequency repetition penalty.
- `sampler.cpp` & `sampler.h`: Temperature and Top-P probability samplers.
- `model_llm_weights.h`: INT8 model parameters compiled into Flash Data ROM (`.rodata`).
