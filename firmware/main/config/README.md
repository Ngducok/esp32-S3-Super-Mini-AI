# System Configuration & Hardware Pinouts

<p align="left">
  <b>Detailed Documentation:</b> 
  <a href="../../../../docs/en/CONFIG.md">English Guide</a> | 
  <a href="../../../../docs/vn/CONFIG.md">Hướng Dẫn Tiếng Việt</a>
</p>

---

## Overview

Central configuration headers defining hardware pin assignments, task priorities, stack sizes, and model generation limits.

## Core Headers

- `hardware_config.h`: GPIO pin assignments, UART baud rates (115200), and LED indicators.
- `app_config.h`: Max generation tokens (`MAX_GENERATION_TOKENS = 48`), default temperature (`0.0f`), task priorities, and stack sizes.
