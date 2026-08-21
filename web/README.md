# Embedded Web Chat Application

<p align="left">
  <b>Detailed Documentation:</b> 
  <a href="../docs/en/WEB_SERVER.md">English Guide</a> | 
  <a href="../docs/vn/WEB_SERVER.md">Hướng Dẫn Tiếng Việt</a>
</p>

---

## Overview

Contains the standalone source assets (HTML5, CSS3, JavaScript) for the ChatGPT-style Dark Mode Web UI embedded inside the ESP32-S3 Flash memory.

## Key Files

- `index.html`: Responsive Dark Mode chat interface with live telemetry display.
- `style.css`: Modern styling optimized for mobile phones and desktop browsers.
- `app.js`: Asynchronous fetch streaming, token rendering, and hardware telemetry polling.
- `web_ui.h`: C++ header exporting the compressed web assets for Flash DROM compilation.
