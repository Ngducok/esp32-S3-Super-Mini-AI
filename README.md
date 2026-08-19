# On-Device Generative Micro-Transformer on ESP32-S3 Without External PSRAM

<p align="left">
  <b>Language:</b> 
  <a href="README.md">English</a> | 
  <a href="README_VN.md">Tiếng Việt</a>
</p>

---

This is an on-device autoregressive generative language model running entirely locally on an ESP32-S3 microcontroller. It runs on the silicon itself with zero cloud dependencies, generating and streaming tokens at 9.33 to 20.00 tokens per second. It requires no external PSRAM chip, operating within the 384KB internal SRAM boundary of the budget ESP32-S3 Super Mini development board.

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
| **Memory Allocation** | Relies on 8MB Octal PSRAM for KV/weights | **100% Internal SRAM only** (~12 KB KV-Cache) |
| **Output Interface** | SPI LCD Screen wired to GPIOs | **SoftAP WiFi Hotspot + In-Memory Web Chat UI** |
| **Serving Mechanism** | Local SPI frame buffer | Asynchronous HTTP REST API on Port 80 |
| **Cost & Accessibility** | Higher-end module (~$5 - $7) | Ultra-budget module (~$2) |

---

## The numbers

| Metric | Specification |
| :--- | :--- |
| Chip | ESP32-S3 Super Mini (Dual-Core Xtensa LX7 @ 240 MHz) |
| Internal SRAM | 512 KB total (~380 KB usable internal SRAM) |
| External PSRAM | **Disabled / 0 KB Required** (Runs on all board variants) |
| Flash Footprint | 1.44 MB binary (fits within standard 4 MB Flash) |
| Memory Footprint | ~12 KB KV-Cache in SRAM (>220 KB free SRAM remaining) |
| Inference Speed | 9.33 – 20.00 tokens/sec end-to-end |
| Token Latency | ~50 ms – 107 ms per token |
| Connectivity | Standalone SoftAP WiFi (`ESP32-Local-AI`) + USB Serial-JTAG |
| Quantization | INT8 symmetric per-tensor |

> [!NOTE]
> **Why 0 KB PSRAM by Design? (Even if your board has 2MB PSRAM)**:
> While some ESP32-S3 chip variants (like ESP32-S3R2) feature 2MB embedded PSRAM, many entry-level boards (like ESP32-S3-N4) have **0 KB PSRAM**. 
> 
> 1. **Universal Hardware Compatibility**: By restricting memory consumption to internal SRAM, this firmware runs out-of-the-box on **any** ESP32-S3 development board ($2 bare-metal silicon).
> 2. **SRAM Single-Cycle Speed**: Internal SRAM runs at full 240 MHz CPU bus speed (~960 MB/s single-cycle access), whereas PSRAM traverses an external SPI bus (40–80 MHz) with bus contention and latency penalties.
> 3. **Optional Expansion**: Users with 2MB/8MB PSRAM can easily toggle `CONFIG_SPIRAM=y` in `sdkconfig` to scale context length to 512+ tokens.

---

## Why it is hard, and how it fits anyway

Language models are notoriously memory-bound. On edge microcontrollers, available fast memory (SRAM) is measured in hundreds of kilobytes rather than gigabytes. Standard LLM deployments on microcontrollers (such as LLaMA-based ports) typically mandate 8MB or 16MB of external Octal PSRAM.

On a bare-metal ESP32-S3 Super Mini with 0 KB external PSRAM, fitting a neural text generation pipeline requires strict memory tiering and zero dynamic allocation during inference.

### 1. Memory Tiering Hierarchy

```
  SRAM  (Fast, ~384 KB)   KV-Cache, activation buffers, token logits, FreeRTOS stacks
  FLASH (1.44 MB, DROM)   Quantized INT8 weight matrices, vocabulary table, Web UI bundle
```

- **Flash DROM (Zero-Copy Read)**: Weight matrices ($W_q, W_k, W_v, W_o, W_1, W_2, W_{te}, W_{pe}, W_{head}$) and vocabulary strings are mapped as `const int8_t` arrays into Flash Data ROM. The CPU reads matrix rows directly across the SPI Flash cache bus during matrix-vector multiplications without staging full layers into RAM.
- **Internal SRAM (Static Buffers)**: Activations and the autoregressive Key-Value Cache (KV-Cache) reside in static internal memory. For a sequence length of 64 tokens across 3 Transformer layers with hidden dimension $d=64$, the KV-Cache consumes exactly:

$$3 \text{ layers} \times 64 \text{ tokens} \times 64 \text{ dimensions} = 12,288 \text{ bytes} \approx 12 \text{ KB}$$

This leaves over 220 KB of free internal SRAM for WiFi protocol buffers, TCP/IP sockets, and FreeRTOS task stacks.

> [!NOTE]
> No dynamic heap allocation (`malloc` or `free`) occurs inside the token generation loop. This prevents heap fragmentation and guarantees zero memory drift over indefinite runtimes.

---

## Architecture Breakdown

```
[User Input] 
     │
     ▼
[Token Matching / Lookup]
     │
     ▼
[Autoregressive Transformer Core]
  ├── Word + Position Embedding (INT8)
  ├── Multi-Head Self-Attention (L=3, H=4, d_head=16)
  ├── Static SRAM KV-Cache Manager
  ├── GELU Feed-Forward Network (d_ff=128)
  └── LM Head Output Logits Projection
     │
     ▼
[Argmax & Temperature Sampler]
     │
     ├───────────────────────────────┐
     ▼                               ▼
[USB Serial-JTAG Stream]   [SoftAP HTTP Server / Web UI]
```

### Transformer Core Parameters

- Layers ($L$): 3
- Hidden Dimension ($d$): 64
- Attention Heads ($H$): 4 (Head Dimension = 16)
- Feed-Forward Dimension ($d_{ff}$): 128
- Context Sequence Length ($T$): 64
- Quantization: Symmetric INT8

### Zero-VFS In-Memory Web Interface

Standard microcontroller web servers often require a filesystem partition (SPIFFS or LittleFS) on Flash, introducing I/O overhead and complex partition layouts.

In this project, the entire single-page web interface (HTML5, CSS3, and JavaScript) is compiled via `generate_web_header.py` into a raw string literal inside `web_ui.h`. The ESP-IDF HTTP daemon serves requests directly out of Flash memory with sub-millisecond response latency.

---

## Repository Structure

```text
esp32/
├── .gitignore                    # Build & IDE exclusion rules
├── LICENSE                       # MIT License
├── README.md                     # Technical documentation
├── results.md                    # Detailed benchmark report
│
├── firmware/                     # Industrial-grade ESP-IDF C++ project
│   ├── CMakeLists.txt            # Top-level build configuration
│   ├── partitions.csv            # 3.5MB application partition layout
│   ├── sdkconfig                 # ESP32-S3 240MHz & 4MB Flash settings
│   └── main/
│       ├── CMakeLists.txt        # Component registration
│       ├── main.cpp              # Multi-threaded FreeRTOS entry point
│       ├── config/               # Pinout and application configurations
│       ├── diagnostics/          # Hardware probe, memory tracker, telemetry
│       ├── llm/                  # Transformer engine, sampler, INT8 weights
│       └── web/                  # SoftAP WiFi driver & HTTP Web server
│
├── web/                          # Standalone web client application
│   ├── index.html                # Dark Mode chat template
│   ├── style.css                 # Interface stylesheet
│   ├── app.js                    # Web chat client & telemetry poller
│   ├── generate_web_header.py    # Python web asset bundler
│   └── web_ui.h                  # Flash-resident web header
│
└── esp32_ai_runtime/             # Standalone Arduino IDE sketch
    └── esp32_ai_runtime.ino      # Single-file Arduino deployment
```

---

## Getting Started

### Method 1: Using ESP-IDF (Recommended)

#### Requirements
- ESP-IDF v5.1 or later (v6.x supported)
- ESP32-S3 development board connected via USB

#### Build and Flash

1. Navigate to the firmware directory:
   ```bash
   cd firmware
   ```

2. Build the project:
   ```bash
   idf.py build
   ```

3. Flash to the board and open the serial monitor:
   ```bash
   idf.py -p COM5 flash monitor
   ```
   *(Replace `COM5` with your corresponding serial port on Windows or `/dev/ttyUSB0` on Linux/macOS).*

---

### Method 2: Using Arduino IDE

1. Open `esp32_ai_runtime/esp32_ai_runtime.ino` in Arduino IDE.
2. In **Tools > Board**, select **ESP32S3 Dev Module**.
3. Configure the following board settings:
   - **Flash Size**: 4MB
   - **Partition Scheme**: Huge APP (3MB No OTA / 1MB SPIFFS)
   - **PSRAM**: Disabled (or OPI PSRAM if your board has it)
4. Click **Upload**.

---

## Interacting with the Model

### 1. Web Interface (Smartphone / PC)

1. Connect your device to the WiFi access point broadcasted by the ESP32:
   - **SSID**: `ESP32-Local-AI`
   - **Password**: `12345678`
2. Open any web browser and navigate to:
   ```text
   http://192.168.4.1
   ```
3. Type custom prompts into the input box or tap any of the preconfigured quick-action chips.

### 2. USB Serial Terminal

Open a serial terminal at `115200` baud. Type your prompt directly into the console and press Enter:

```text
====================================================================
>>> [PROMPT] : tell me a joke
<<< [STREAM] : tell me a joke : A programmer goes to the grocery store. Wife says: 'Buy a carton of milk, and if they have eggs, buy ten.' He comes back with 10 cartons of milk!
--- [METRICS]: Tokens: 48 | Speed: 18.24 tok/s | Latency: 54.82 ms | Free SRAM: 229740 B
====================================================================
```

---

## Verification and Diagnostics

The firmware includes built-in diagnostics that run automatically on startup:

- **Hardware Probe**: Detects silicon revision, CPU frequency, active cores, and exact SRAM/PSRAM availability.
- **Memory Tracker**: Audits the FreeRTOS heap after each generation to guarantee zero memory leakage.
- **Heartbeat Daemon**: Periodically reports system uptime and free SRAM bytes over the serial monitor.

---

## Contributing

Contributions, bug reports, and feature proposals are welcome. If you would like to contribute to this project:

1. **Fork the Repository**: Click the **Fork** button at the top right of this repository.
2. **Create a Feature Branch**:
   ```bash
   git checkout -b feature/YourFeatureName
   ```
3. **Commit Your Changes**:
   ```bash
   git commit -m "feat: implement INT4 quantization or new feature"
   ```
4. **Push to Your Branch**:
   ```bash
   git push origin feature/YourFeatureName
   ```
5. **Open a Pull Request**: Submit a Pull Request to the `main` branch with a description of your changes and test verification results.

For major architectural changes or model modifications, please open an Issue first to discuss what you would like to change. See [CONTRIBUTING.md](CONTRIBUTING.md) for detailed contribution guidelines.

---

## Academic & Research Motivation

This project was developed as an independent undergraduate engineering research initiative. The primary objective is to investigate the physical and architectural boundaries of executing autoregressive transformer models on ultra-constrained edge silicon ($2 bare-metal microcontrollers with 0 KB external PSRAM), bridging theoretical deep learning paradigms with low-level embedded systems optimization.

The long-term research vision is to integrate physical audio frontend peripherals (I2S digital microphones and DAC amplifiers) and robotics actuator buses (I2C/SPI/CAN) to evolve this on-device language model into a 100% offline, physical "JARVIS" assistant module for embedded robotics and embodied AIoT systems.

---

## Author & Contact

- **Author**: Duc Nguyen
- **Role**: 3rd-year Undergraduate Student in Robotics & AI, 3I Institute, UEH University
- **Email**: [dustinoki.dev@gmail.com](mailto:dustinoki.dev@gmail.com)

> [!NOTE]
> As an undergraduate student, my knowledge and practical experience are still growing. I warmly welcome any constructive feedback, technical guidance, architectural suggestions, or code contributions from the community to help improve this project.

---

## Acknowledgements

Special thanks to the open-source community whose work and shared insights made this project possible:

- [slvDev/esp32-ai](https://github.com/slvDev/esp32-ai) for pioneering the concept of on-device LLM memory tiering on the ESP32-S3.
- [Espressif Systems](https://github.com/espressif) for maintaining the robust ESP-IDF framework and FreeRTOS integration.
- The open-source TinyML and Edge AI research community for pushing the boundaries of neural network inference on microcontrollers.

---

## Star History

[![Star History Chart](https://api.star-history.com/svg?repos=Ngducok/esp32-S3-Super-Mini-AI&type=Date)](https://star-history.com/#Ngducok/esp32-S3-Super-Mini-AI&Date)

---

## License

This project is licensed under the [MIT License](LICENSE).
