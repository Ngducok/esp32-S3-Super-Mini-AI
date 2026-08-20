# Micro-Transformer Inference Core

<p align="left">
  <b>Language:</b> 
  <a href="README.md">English</a> | 
  <a href="README_VN.md">Tiếng Việt</a>
</p>

---

## Overview

The `llm/` directory implements the on-device **Micro-Transformer Decoder** engine. It performs quantized fixed-point matrix-vector multiplication, static Key-Value (KV) cache management, Multi-Head Self-Attention, and deterministic token sampling.

---

## Architectural Problem & Solution

### Problem
1. Standard floating-point (FP32) Transformer inference requires significant computational and memory bandwidth.
2. Dynamic memory allocations for KV-cache during token generation cause heap fragmentation and crashes on microcontrollers with only internal SRAM.
3. High-entropy random sampling produces character gibberish on small-scale quantized language models.

### Solution
1. **Symmetric INT8 Quantization**:
   All weight matrices ($W_q, W_k, W_v, W_o, W_1, W_2, W_{te}, W_{pe}, W_{head}$) are quantized to 8-bit signed integers (`int8_t`). Matrix-vector multiplication is computed using integer accumulators scaled by $1/900.0f$:

$$\text{Output}[r] = \left( \sum_{c=0}^{\text{cols}-1} W[r, c] \cdot X_q[c] \right) \times \frac{1}{900.0}$$

2. **Static Pre-Allocated KV-Cache in SRAM**:
   The Key and Value states are stored in statically allocated multi-dimensional arrays in internal SRAM:
   ```cpp
   static int8_t s_k_cache[LAYERS][MAX_SEQ_LEN][DIM];
   static int8_t s_v_cache[LAYERS][MAX_SEQ_LEN][DIM];
   ```
   For $L=3, T=64, d=64$, the combined Key and Value cache consumes exactly $2 \times 3 \times 64 \times 64 = 24,576\text{ bytes}$ (~24.5 KB).

3. **Deterministic Low-Temperature / Greedy Argmax Sampler**:
   When temperature is zero or near zero, the sampler selects the token corresponding to $\arg\max(\text{logits})$, guaranteeing coherent sentence completion.

---

## Autoregressive Generation Flowchart

```
                      [Input Prompt String]
                                │
                                ▼
                       [Prompt Tokenizer]
                                │
                 ┌──────────────┴──────────────┐
                 ▼                             ▼
       [Token Embeddings WTE]        [Position Embeddings WPE]
                 └──────────────┬──────────────┘
                                ▼
                   [Residual Stream: X = WTE + WPE]
                                │
          ┌─────────────────────┴─────────────────────┐
          ▼                                           ▼
[Layer 0, 1, 2: Transformer Block]           [KV-Cache Update]
  ├── Q, K, V Projections (INT8)                └── Store K, V at pos
  ├── Multi-Head Attention (H=4, d_head=16)
  ├── Attention Output Projection (W_o)
  ├── Residual Addition (X = X + Attn_Out)
  ├── GELU MLP Feed-Forward (d_ff=128)
  └── Residual Addition (X = X + MLP_Out)
                                │
                                ▼
                   [LM Head Output Projection]
                                │
                                ▼
                    [Output Logits: 128-dim]
                                │
                                ▼
                  [Greedy / Softmax Sampler]
                                │
                                ▼
                   [Emit Next Token String]
                                │
                                ├───────────► (Repeat for next step)
                                ▼
                 [Break on Newline or Max Seq]
```

---

## Transformer Hyperparameters

| Hyperparameter | Symbol | Value |
| :--- | :--- | :--- |
| Vocabulary Size | $V$ | 128 tokens |
| Hidden Dimension | $d$ | 64 |
| Transformer Layers | $L$ | 3 |
| Attention Heads | $H$ | 4 |
| Head Dimension | $d_{\text{head}}$ | 16 |
| Feed-Forward Dimension | $d_{\text{ff}}$ | 128 |
| Maximum Context Sequence | $T$ | 64 |
| SRAM KV-Cache Size | - | 24,576 bytes (~24.5 KB total for K + V) |
| Flash Weight Footprint | - | 118,784 params (~119 KB INT8 in Flash DROM) |

---

## Source Files

- `transformer.h` / `transformer.cpp`: Core Multi-Head Attention, GELU activation, and static KV-cache manager.
- `sampler.h` / `sampler.cpp`: Low-temperature softmax and greedy argmax sampler.
- `generator.h` / `generator.cpp`: Autoregressive generation loop with FreeRTOS watchdog yields and token streaming callbacks.
- `model_llm_weights.h`: Quantized INT8 weight matrices and ASCII vocabulary lookup tables stored in Flash DROM.
