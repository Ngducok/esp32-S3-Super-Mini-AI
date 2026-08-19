# Standalone Web Application & Asset Bundler

<p align="left">
  <b>Language:</b> 
  <a href="README.md">English</a> | 
  <a href="README_VN.md">Tiếng Việt</a>
</p>

---

## Overview

The `web/` directory contains the source code for the embedded ChatGPT-style dark mode web interface and the automated asset compiler that packages HTML, CSS, and JavaScript into a Flash-resident C++ header.

---

## Architectural Problem & Solution

### Problem
1. Serving individual web assets (HTML, CSS, JS) over multiple HTTP requests on a microcontroller increases connection handshakes, exhausts limited socket slots, and introduces latency.
2. Relying on filesystem partitions (SPIFFS/LittleFS) consumes Flash space and requires file I/O operations for every page load.

### Solution
1. **Single-Bundle Inlining (`generate_web_header.py`)**:
   A Python build utility reads `index.html`, `style.css`, and `app.js`, merges the stylesheet into `<style>` tags, embeds the script into `<script>` tags, and generates `web_ui.h`.
2. **Flash DROM Storage**:
   The bundled HTML page is declared as a C++ `const char CHAT_HTML[]` using raw string literals:
   ```cpp
   namespace Web {
   static const char CHAT_HTML[] = R"rawliteral(<!DOCTYPE html>...)rawliteral";
   }
   ```
   When a client navigates to `http://192.168.4.1`, the entire page is delivered in a single HTTP response directly from Flash memory.

---

## Asset Compilation Flowchart

```
┌──────────────┐     ┌──────────────┐     ┌──────────────┐
│  index.html  │     │  style.css   │     │    app.js    │
└──────┬───────┘     └──────┬───────┘     └──────┬───────┘
       │                    │                    │
       └──────────────┐     │     ┌──────────────┘
                      ▼     ▼     ▼
         [generate_web_header.py Tool]
                      │
                      ├── Inlines <style>...</style>
                      ├── Inlines <script>...</script>
                      └── Wraps in C++ R"rawliteral(...)rawliteral"
                      │
                      ▼
               [web/web_ui.h]
                      │
                      ▼
    [Included directly in firmware/main/web/web_server.cpp]
```

---

## Client Application Architecture

- **`index.html`**: Clean semantic layout featuring a sticky header, chat message stream, quick prompt chips, and an input bar.
- **`style.css`**: Modern dark mode color palette (`#131314` background, `#1e1f20` cards, `#388bfd` accents) styled for mobile and desktop screens without external CSS framework dependencies.
- **`app.js`**: Asynchronous client-side logic that handles:
  - Submitting user prompts to `POST /api/chat`.
  - Rendering conversational bubbles with token speed and latency badges.
  - Polling `GET /api/status` every 5 seconds to update real-time SRAM and uptime stats.

---

## Rebuilding Web Assets

If you modify `index.html`, `style.css`, or `app.js`, re-run the bundler to update `web_ui.h`:

```bash
python web/generate_web_header.py
```
