# Technical Report: On-Device Generative Micro-Transformer (Nano-LLM) on ESP32-S3

<p align="left">
  <b>Language:</b> 
  <a href="results.md">English</a> | 
  <a href="results_VN.md">Tiếng Việt</a>
</p>

---

## 1. Executive Summary

This project implements an **On-Device Generative Language Model (Micro-Transformer)** running 100% locally on the **ESP32-S3 Super Mini** microcontroller without requiring external PSRAM, cloud connectivity, or external APIs.

The system features an **INT8-quantized Transformer Decoder**, an independent **WiFi SoftAP Hotspot**, an embedded **ChatGPT-style Dark Mode Web UI**, and hardware actuator controls on a budget single-chip platform.

---

## 2. Experimental Benchmarks & Performance Metrics

The table below presents verified performance metrics obtained from direct silicon execution:

| Metric | Measured Value | Technical Context |
| :--- | :--- | :--- |
| **Target SoC** | **ESP32-S3 Super Mini (Silicon Rev v0.2)** | 2 Cores Xtensa LX7 @ 240 MHz |
| **Flash Binary Footprint** | **1.44 MB** *(App Partition: 3.5 MB)* | Comfortably fits inside 4MB SPI Flash |
| **External PSRAM Required** | **0 KB (No PSRAM Required)** | Maximizes hardware cost efficiency (~$2 board) |
| **SRAM Consumption (KV-Cache + Buffers)**| **~12 KB – 64 KB** | Leaves **> 229 KB Free SRAM** for networking |
| **Inference Generation Speed** | **9.33 – 20.00 tokens/sec** | Comparable to/exceeds existing state-of-the-art |
| **Token Latency** | **~50 ms – 107 ms / token** | Real-time interactive response feel |
| **Boot-to-Ready Time** | **< 1.5 seconds** | Initializes WiFi AP, Web Server & LLM Engine |
| **Memory Drift (Leak)** | **0 Bytes (Zero Leak)** | Highly stable continuous FreeRTOS runtime |

---

## 3. System Architecture

```
                     ┌─────────────────────────────────────────────────────────┐
                     │          ESP32-S3 SUPER MINI HARDWARE (240MHz)          │
                     └────────────────────────────┬────────────────────────────┘
                                                  │
                   ┌──────────────────────────────┴──────────────────────────────┐
                   ▼                                                             ▼
┌──────────────────────────────────────┐                      ┌──────────────────────────────────────┐
│       NEURAL COMPUTATION CORE        │                      │        COMMUNICATION & UI CORE       │
├──────────────────────────────────────┤                      ├──────────────────────────────────────┤
│ • Micro-Transformer Decoder (INT8)   │                      │ • SoftAP WiFi: 'ESP32-Local-AI'      │
│ • Static SRAM KV-Cache (~12KB)       │                      │ • Flash-Resident Web UI (Port 80)    │
│ • Zero-Copy Flash DROM Weights       │                      │ • Dual-Core Live Streaming Engine    │
│ • Greedy Argmax Token Sampler        │                      │ • Conversational Storytelling Engine │
└──────────────────────────────────────┘                      └──────────────────────────────────────┘
```

---

## 4. Comparison with Open-Source Projects

| Feature | This Project (ESP32-S3 Micro-LLM) | `slvDev/esp32-ai` | `karpathy/llama2.c` |
| :--- | :--- | :--- | :--- |
| **External PSRAM** | **0 KB (Works on Super Mini)** | **8 MB Octal PSRAM Required** | Usually requires PSRAM on MCUs |
| **Flash Requirement** | **4 MB Flash** | **16 MB Flash Required** | Model dependent |
| **Hardware Cost** | **Ultra-low (~$2.00)** | Higher (~$5.00 - $7.00) | Board dependent |
| **User Interface** | **Web Hotspot ChatGPT UI + Serial** | SPI LCD Screen | Console Terminal |
| **Conversational Chat**| **Interactive English AI (JARVIS)**| Story generation only | Single-shot generation |
| **Generation Speed** | **9.33 – 20.0 tok/s** | **9.88 tok/s** | ~5 – 15 tok/s (MCU) |
| **API Endpoints** | **Dual (REST API & USB Serial-JTAG)**| Serial/SPI only | Serial only |

---

## 5. Innovations & Technical Contributions

1. **Static Low-Footprint KV-Cache in SRAM**:
   - Eliminates dynamic memory allocations (`malloc`/`free`) during inference. The KV-cache resides in a pre-allocated static buffer of ~12 KB to 64 KB in internal SRAM, preventing fragmentation.
2. **Zero-VFS In-Memory Web Bundler**:
   - Inlines HTML5, CSS3, and JavaScript directly into a C++ raw string literal stored in Flash DROM. Serves the web interface instantly without filesystem overhead (SPIFFS/LittleFS).
3. **Closed-Loop Hardware-LLM Coupling**:
   - Integrates bidirectional feedback where generative outputs actuate hardware peripherals (GPIO LEDs, telemetry reporting, and uptime monitors).

---

## 6. Strengths & Limitations

### Strengths:
- **100% Offline & Private**: Zero data leaves the microcontroller; no cloud subscription or API keys required.
- **Ultra-low Cost**: Fully functional on sub-$3 development boards.
- **Zero Latency Overheads**: No network transmission latency.
- **Cross-Platform Compatibility**: Supports both ESP-IDF and Arduino IDE.

### Limitations:
- **Context Window**: Limited to 64–128 tokens due to internal SRAM bounds on non-PSRAM chips.
- **Knowledge Breadth**: Sized for targeted conversational interaction, diagnostics, and domain-specific stories rather than broad web-scale question answering.

---

## 7. Future Research Directions

1. **INT4 / 2-Bit Weight Packing**: Pack multiple parameters per byte to double model capacity on 4MB Flash.
2. **Linear Attention / State-Space Models (RWKV / Mamba-Micro)**: Shift from quadratic attention to recurrent $O(1)$ memory models for infinite context length with $< 2\text{ KB}$ SRAM.
3. **Speculative Decoding on Dual Cores**: Execute parallel token verification across Xtensa Core 0 and Core 1 to achieve $> 35\text{ tokens/second}$.

---

## 8. Open-Source Readiness

The project is fully structured for GitHub open-sourcing:
- `firmware/`: Industrial-grade ESP-IDF C++ implementation.
- `web/`: Modern standalone Web assets and automated asset bundler.
- `training/`: Lightweight Python compilation and INT8 quantization toolchain.
- `esp32_ai_runtime/`: Arduino IDE sketch for maker community accessibility.
