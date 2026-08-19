#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

namespace Web {

class WebServer {
public:
    static esp_err_t start();
    static void stop();

private:
    static httpd_handle_t s_server_handle;
};

} // namespace Web
