#pragma once

#include "esp_err.h"

namespace Web {

class WifiAP {
public:
    static esp_err_t init(const char* ssid = "ESP32-Local-AI", const char* password = "12345678");
};

} // namespace Web
