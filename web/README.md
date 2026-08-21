# In-Memory Flash-Resident Web UI (web/)

<p align="left">
  <b>Language:</b> 
  <a href="README.md">English</a> | 
  <a href="README_VN.md">Tiếng Việt</a>
</p>

---

## 1. Overview

The `web/` directory contains the embedded web client (HTML5, CSS3, JavaScript) stored directly in Flash DROM as a C++ header bundle (`web_ui.h`).

---

## 2. Problem Statement & Technical Solution

### Problem
Hosting web assets via external file systems (SPIFFS/LittleFS) requires dedicated Flash partitions and consumes runtime SRAM for file descriptors.

### Solution
Web assets are compiled directly into a constant C++ Flash DROM header:
1. `generate_web_header.py` packages `index.html`, `style.css`, and `app.js` into `web_ui.h`.
2. The HTTP daemon serves the UI bundle instantly (< 5 ms latency) with zero filesystem RAM overhead.
