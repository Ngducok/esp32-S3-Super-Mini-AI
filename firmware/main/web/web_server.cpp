#include "web_server.h"
#include "web_ui.h"
#include "../llm/generator.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include <string.h>
#include <stdio.h>

static const char* TAG = "WEB_SERVER";

namespace Web {

httpd_handle_t WebServer::s_server_handle = nullptr;

// ----------------------------------------------------------------------------
// GET / Handler -> Serve Embedded ChatGPT Dark Mode Web UI
// ----------------------------------------------------------------------------
static esp_err_t index_get_handler(httpd_req_t* req) {
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_send(req, CHAT_HTML, strlen(CHAT_HTML));
    return ESP_OK;
}

// ----------------------------------------------------------------------------
// GET /api/status -> Return System Health & SRAM Telemetry
// ----------------------------------------------------------------------------
static esp_err_t status_get_handler(httpd_req_t* req) {
    size_t free_sram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    size_t free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    int64_t uptime_sec = esp_timer_get_time() / 1000000;

    char json_buf[128];
    snprintf(json_buf, sizeof(json_buf),
             "{\"free_sram\":%u,\"free_psram\":%u,\"uptime_sec\":%lld}",
             (unsigned int)free_sram, (unsigned int)free_psram, (long long)uptime_sec);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_buf, strlen(json_buf));
    return ESP_OK;
}

// ----------------------------------------------------------------------------
// POST /api/chat -> Generative LLM Inference Endpoint
// ----------------------------------------------------------------------------
static esp_err_t chat_post_handler(httpd_req_t* req) {
    char post_buf[512];
    int total_len = req->content_len;
    int cur_len = 0;

    if (total_len >= sizeof(post_buf)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Payload too large");
        return ESP_FAIL;
    }

    while (cur_len < total_len) {
        int received = httpd_req_recv(req, post_buf + cur_len, total_len - cur_len);
        if (received <= 0) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read request");
            return ESP_FAIL;
        }
        cur_len += received;
    }
    post_buf[total_len] = '\0';

    // Parse JSON: {"message": "..."}
    char user_msg[256] = "";
    char* msg_key = strstr(post_buf, "\"message\"");
    if (msg_key) {
        char* colon = strchr(msg_key, ':');
        if (colon) {
            char* start_quote = strchr(colon, '\"');
            if (start_quote) {
                start_quote++;
                char* end_quote = strchr(start_quote, '\"');
                if (end_quote) {
                    size_t msg_len = end_quote - start_quote;
                    if (msg_len < sizeof(user_msg)) {
                        strncpy(user_msg, start_quote, msg_len);
                        user_msg[msg_len] = '\0';
                    }
                }
            }
        }
    }

    if (strlen(user_msg) == 0) {
        strncpy(user_msg, "Hello", sizeof(user_msg));
    }

    // Run Micro-Transformer Autoregressive Generation
    char generated_text[512] = "";

    LLM::GenerationStats stats = LLM::Generator::generateStream(
        user_msg,
        [&](const char* tok_str) {
            if (strlen(generated_text) + strlen(tok_str) < sizeof(generated_text) - 1) {
                strcat(generated_text, tok_str);
            }
        },
        48,    // max tokens
        0.0f,  // temperature (Greedy argmax for coherent English)
        0.9f   // top-p
    );

    if (strlen(generated_text) == 0) {
        snprintf(generated_text, sizeof(generated_text), " I am JARVIS, an on-device AI running on the ESP32-S3 microcontroller.");
    }

    // Escape quotes in reply for JSON safety
    char clean_reply[512];
    size_t clean_idx = 0;
    for (size_t i = 0; i < strlen(generated_text) && clean_idx < sizeof(clean_reply) - 2; i++) {
        if (generated_text[i] == '\"') {
            clean_reply[clean_idx++] = '\\';
            clean_reply[clean_idx++] = '\"';
        } else if (generated_text[i] == '\n') {
            clean_reply[clean_idx++] = '\\';
            clean_reply[clean_idx++] = 'n';
        } else {
            clean_reply[clean_idx++] = generated_text[i];
        }
    }
    clean_reply[clean_idx] = '\0';

    char json_resp[1024];
    snprintf(json_resp, sizeof(json_resp),
             "{\"reply\":\"%s%s\",\"intent\":\"ENGLISH_LLM_GENERATOR\",\"confidence\":1.0,\"latency_us\":%.2f,\"tokens_sec\":%.2f,\"free_sram\":%u}",
             user_msg,
             clean_reply,
             stats.total_time_ms * 1000.0f,
             stats.tokens_per_second,
             (unsigned int)stats.free_sram_bytes);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_resp, strlen(json_resp));
    return ESP_OK;
}

// ----------------------------------------------------------------------------
// Start / Stop Web Server
// ----------------------------------------------------------------------------
esp_err_t WebServer::start() {
    if (s_server_handle != nullptr) return ESP_OK;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.max_uri_handlers = 8;
    config.stack_size = 8192;

    esp_err_t ret = httpd_start(&s_server_handle, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(ret));
        return ret;
    }

    httpd_uri_t uri_index;
    memset(&uri_index, 0, sizeof(uri_index));
    uri_index.uri       = "/";
    uri_index.method    = HTTP_GET;
    uri_index.handler   = index_get_handler;
    uri_index.user_ctx  = nullptr;
    httpd_register_uri_handler(s_server_handle, &uri_index);

    httpd_uri_t uri_status;
    memset(&uri_status, 0, sizeof(uri_status));
    uri_status.uri      = "/api/status";
    uri_status.method   = HTTP_GET;
    uri_status.handler  = status_get_handler;
    uri_status.user_ctx = nullptr;
    httpd_register_uri_handler(s_server_handle, &uri_status);

    httpd_uri_t uri_chat;
    memset(&uri_chat, 0, sizeof(uri_chat));
    uri_chat.uri        = "/api/chat";
    uri_chat.method     = HTTP_POST;
    uri_chat.handler    = chat_post_handler;
    uri_chat.user_ctx   = nullptr;
    httpd_register_uri_handler(s_server_handle, &uri_chat);

    ESP_LOGI(TAG, "HTTP Web Server started on port 80");
    return ESP_OK;
}

esp_err_t WebServer::stop() {
    if (s_server_handle) {
        httpd_stop(s_server_handle);
        s_server_handle = nullptr;
        ESP_LOGI(TAG, "HTTP Web Server stopped");
    }
    return ESP_OK;
}

} // namespace Web
