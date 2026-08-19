#pragma once

#include <stdint.h>
#include "driver/gpio.h"

namespace Config {
namespace Hardware {

    // Status LED pin on ESP32-S3 Super Mini
    constexpr gpio_num_t PIN_STATUS_LED        = GPIO_NUM_8;
    constexpr bool       STATUS_LED_ACTIVE_LOW = false;

} // namespace Hardware
} // namespace Config
