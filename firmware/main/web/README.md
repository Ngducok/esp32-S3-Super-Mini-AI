# SoftAP WiFi & Web Server Subsystem

## Overview

The `web/` directory manages the standalone wireless access point (SoftAP) and the embedded HTTP daemon. It handles client browser connections, serves the in-memory web chat interface, and routes REST API requests to the generative language model engine.

---

## Architectural Problem & Solution

### Problem
1. Traditional embedded web servers rely on SPIFFS or LittleFS filesystem partitions on SPI Flash, adding partition complexity, read latencies, and RAM buffer overhead.
2. Ingesting incoming HTTP POST payloads and generating token responses synchronously can block network sockets and cause connection timeouts.

### Solution
1. **Zero-VFS In-Memory Flash Asset Serving**:
   The HTML, CSS, and JavaScript bundle is compiled into a single C++ header (`web_ui.h`) stored as a raw string literal in Flash Data ROM (`DROM`). The HTTP daemon serves the interface directly from Flash memory with zero filesystem overhead.
2. **REST API Architecture**:
   - `GET /`: Serves the responsive single-page chat interface.
   - `GET /api/status`: Returns system telemetry (free SRAM, free PSRAM, uptime) in JSON format.
   - `POST /api/chat`: Ingests user prompt strings, executes the Micro-Transformer generation loop, and returns the generated text with latency and token speed metrics.

---

## Client-Server Interaction Flowchart

```
[Client Browser (Phone/PC)]                   [ESP32-S3 HTTP Daemon]
            │                                           │
            ├─────── 1. Connect to WiFi SoftAP ────────►│ (SSID: 'ESP32-Local-AI')
            │                                           │
            ├─────── 2. HTTP GET / ────────────────────►│
            │◄────── 3. Serve Flash CHAT_HTML ──────────┤ (Instant 200 OK)
            │                                           │
            ├─────── 4. HTTP POST /api/chat ───────────►│ (Payload: {"message": "..."})
            │           {"message": "tell me a joke"}   │
            │                                           ├── [Parse JSON Prompt]
            │                                           ├── [Run Transformer Forward]
            │                                           ├── [Stream Tokens to Buffer]
            │                                           └── [Assemble JSON Response]
            │◄────── 5. HTTP 200 OK JSON ───────────────┤
            │           {"reply": "...", "tok_sec": ..} │
            │                                           │
            └─────── 6. Render Bubble in UI ────────────┘
```

---

## Source Files

- `wifi_ap.h` / `wifi_ap.cpp`: Initializes NVS, TCP/IP network interfaces, and launches the SoftAP broadcast (`192.168.4.1`).
- `web_server.h` / `web_server.cpp`: Configures the ESP-IDF HTTP server, registers URI handlers, parses JSON payloads, and interfaces with `LLM::Generator`.
- `web_ui.h`: Generated C++ header containing the inlined HTML5, CSS3, and JavaScript web assets.
