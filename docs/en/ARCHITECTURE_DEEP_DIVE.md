# Deep Dive: Micro-Transformer Architecture & Zero-PSRAM Memory Tiering

<p align="left">
  <b>Language:</b> 
  <a href="ARCHITECTURE_DEEP_DIVE.md">English</a> | 
  <a href="ARCHITECTURE_DEEP_DIVE_VN.md">Tiếng Việt</a>
</p>

---

## 1. Architectural Philosophy

Running generative autoregressive neural language models on microcontrollers without external memory requires a strict memory tiering model.

Standard open-source embedded LLM ports (e.g. LLaMA on microcontrollers) allocate multi-megabyte activation and KV buffers in Octal PSRAM. When operating on a budget development board (such as the ESP32-S3 Super Mini) with **0 KB external PSRAM**, memory allocation must remain completely bounded within internal SRAM (~384 KB).

---

## 2. Memory Tiering & Zero-Copy Flash DROM Mapping

```
+-------------------------------------------------------------------------+
| ESP32-S3 ADDRESS SPACE                                                  |
|                                                                         |
|  0x3F400000 (Flash DROM - Read Only, Zero Copy)                         |
|  +-------------------------------------------------------------------+  |
|  | INT8 Matrices (WQ, WK, WV, WO, W1, W2, WTE, WPE, LM_HEAD) 119 KB  |  |
|  | FastMath 512-Entry Lookup Tables (Exp, GELU, SiLU, Softmax)  6 KB |  |
|  | Vocabulary String Tokenizer Table                            4 KB |  |
|  | Web UI HTML5/CSS3/JS Bundle Header                          18 KB |  |
|  +-------------------------------------------------------------------+  |
|                                                                         |
|  0x3FC00000 (Internal SRAM - Fast Single-Cycle Read/Write)              |
|  +-------------------------------------------------------------------+  |
|  | Static KV-Cache (3 layers x 64 tokens x 64 dim x 2)       24.5 KB |  |
|  | Activation Tensors (x, q, k, v, attn_out, ffn_h)          18.0 KB |  |
|  | FreeRTOS Tasks & Network TCP/IP Driver Buffers            78.0 KB |  |
|  | Free Unfragmented SRAM Heap                              219.0 KB |  |
|  +-------------------------------------------------------------------+  |
+-------------------------------------------------------------------------+
```

### Key Engineering Decisions:
1. **Zero Dynamic Allocation in Inference**:
   All working vectors and KV slots are statically allocated. No `malloc` or `free` calls occur inside the autoregressive token generation loop, preventing heap fragmentation and guaranteeing zero memory drift over multi-day runtimes.
2. **Flash DROM Memory-Mapped Weights**:
   All 118,784 parameters are compiled as `const int8_t` arrays in Flash Data ROM (`.rodata`). The Xtensa CPU reads matrix rows directly over the internal instruction/data cache bus without staging weights into RAM.
3. **Sliding Window Ring-Buffer KV-Cache**:
   Replaces static linear array allocation with a cyclic buffer. Once context reaches $MAX\_SEQ\_LEN = 64$, incoming tokens overwrite the oldest slot $(pos \pmod{MAX\_SEQ\_LEN})$ with dynamic RoPE adjustment, enabling infinite continuous generation.
