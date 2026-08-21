# System Configuration Subsystem

<p align="left">
  <b>Language:</b> 
  <a href="README.md">English</a> | 
  <a href="README_VN.md">Tiếng Việt</a>
</p>

---

## Overview

The `config/` directory defines compile-time hardware pin mappings, FreeRTOS task priorities, stack size boundaries, and operational runtime thresholds.

---

## Architectural Problem & Solution

### Problem
Hardcoding hardware pin definitions and FreeRTOS task configurations across multiple source files creates maintenance hazards, porting difficulties, and potential stack overflow crashes.

### Solution
1. **Centralized Strongly-Typed Namespaces**:
   Groups all hardware definitions under `Config::Hardware` and application parameters under `Config::App` using compile-time `constexpr` constants.
2. **Explicit Task Stack & Priority Tuning**:
   Allocates appropriate stack sizes based on subsystem requirements:
   - `CHAT_TASK_STACK_SIZE = 8192`: Accommodates the Transformer generation loop, local token buffers, and string conversions.
   - `TELEMETRY_TASK_STACK_SIZE = 4096`: Provides sufficient memory for periodic JSON formatting and serial I/O.

---

## Configuration Parameter Reference

| Parameter | Namespace | Value | Description |
| :--- | :--- | :--- | :--- |
| `PIN_STATUS_LED` | `Config::Hardware` | `GPIO_NUM_8` | Onboard status LED pin on ESP32-S3 Super Mini |
| `STATUS_LED_ACTIVE_LOW` | `Config::Hardware` | `false` | LED polarity setting |
| `MAX_GENERATION_TOKENS` | `Config::App` | `48` | Maximum new tokens generated per prompt |
| `DEFAULT_TEMPERATURE` | `Config::App` | `0.0f` | Greedy Argmax token selection for deterministic output |
| `DEFAULT_TOP_P` | `Config::App` | `0.9f` | Top-P nucleus sampling threshold |
| `CHAT_TASK_PRIORITY` | `Config::App` | `5` | FreeRTOS task priority for user interaction |
| `CHAT_TASK_STACK_SIZE` | `Config::App` | `8192 bytes` | Dedicated stack size for inference task |
| `MIN_SAFE_HEAP_BYTES` | `Config::App` | `32768 bytes` | Critical heap watermark alert threshold |

---

## Source Files

- `hardware_config.h`: GPIO pinout assignments and physical peripheral configuration.
- `app_config.h`: Task priorities, memory allocation sizes, and sampling defaults.
