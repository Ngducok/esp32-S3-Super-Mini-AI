# Embedded Web Server & SoftAP Controller

<p align="left">
  <b>Detailed Documentation:</b> 
  <a href="../../../../docs/en/WEB_SERVER.md">English Guide</a> | 
  <a href="../../../../docs/vn/WEB_SERVER.md">Hướng Dẫn Tiếng Việt</a>
</p>

---

## Overview

Implements the independent SoftAP WiFi hotspot (`ESP32-Local-AI`) and Flash-resident HTTP server serving the ChatGPT Dark Mode Web UI directly from Flash DROM.

## Core Modules

- `wifi_ap.cpp` & `wifi_ap.h`: SoftAP initialization (IP: `192.168.4.1`, SSID: `ESP32-Local-AI`).
- `web_server.cpp` & `web_server.h`: HTTP server handling `/`, `/api/chat`, and `/api/status` endpoints.
- `web_ui.h`: Minified HTML5, CSS3, and JavaScript bundle embedded as a Flash DROM byte array.
