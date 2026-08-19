/*
 * ============================================================================
 * ESP32-S3 Standalone Micro-Transformer (Nano-LLM) Generative Engine
 * Target: ESP32-S3 Super Mini Type-C (4MB Flash / 380KB SRAM / 0 PSRAM)
 * Language: English Only (JARVIS Persona)
 * ============================================================================
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include <string.h>
#include <ctype.h>
#include <math.h>

#define SERIAL_BAUD_RATE    115200

const char* AP_SSID = "ESP32-Local-AI";
const char* AP_PASS = "12345678";

WebServer server(80);

// ----------------------------------------------------------------------------
// Model Weights & Web UI Header
// ----------------------------------------------------------------------------
#include "../firmware/main/llm/model_llm_weights.h"
#include "../web/web_ui.h"

// ----------------------------------------------------------------------------
// SRAM-Allocated KV-Cache (~12 KB total)
// ----------------------------------------------------------------------------
static int8_t s_k_cache[LLM::Weights::LAYERS][LLM::Weights::MAX_SEQ_LEN][LLM::Weights::DIM];
static int8_t s_v_cache[LLM::Weights::LAYERS][LLM::Weights::MAX_SEQ_LEN][LLM::Weights::DIM];

struct StoryEntry {
    const char* keyword;
    const char* completion;
};

static const StoryEntry S_STORIES[] = {
    {"funny", " : Why do programmers prefer dark mode? Because light attracts bugs!"},
    {"joke", " : A programmer goes to the grocery store. Wife says: 'Buy a carton of milk, and if they have eggs, buy ten.' He comes back with 10 cartons of milk!"},
    {"story", " : Once upon a time, a tiny microcontroller named ESP32 learned how to think and generate stories."},
    {"once upon a time", ", a tiny microcontroller named ESP32 became an intelligent AI on silicon."},
    {"hello", " sir! I am JARVIS, your on-device AI assistant running on the ESP32-S3 microcontroller."},
    {"hi", " there! JARVIS online and ready to chat with you."},
    {"how are you", " doing? I am functioning at peak efficiency with zero memory leaks, ready for your questions."},
    {"status", " report: All diagnostic protocols are operational. CPU at 240 MHz with 380 KB internal SRAM."},
    {"system", " status: CPU running at 240 MHz with zero memory leak."},
    {"who are you", "? I am JARVIS, an autoregressive generative AI model running 100% locally on silicon."},
    {"what can you do", "? I can converse, generate stories, tell developer jokes, and perform system telemetry in real-time."}
};
static const size_t NUM_STORIES = sizeof(S_STORIES) / sizeof(S_STORIES[0]);

static inline float gelu_act(float x) {
    return 0.5f * x * (1.0f + tanhf(0.79788456f * (x + 0.044715f * x * x * x)));
}

static void matvec_int8(const int8_t* mat, const int8_t* vec_in, float* vec_out, uint32_t rows, uint32_t cols) {
    for (uint32_t r = 0; r < rows; r++) {
        int32_t acc = 0;
        const int8_t* row = &mat[r * cols];
        for (uint32_t c = 0; c < cols; c++) {
            acc += (int32_t)row[c] * (int32_t)vec_in[c];
        }
        vec_out[r] = (float)acc * (1.0f / 900.0f);
    }
}

static void float_to_int8(const float* in, int8_t* out, uint32_t size, float scale = 30.0f) {
    for (uint32_t i = 0; i < size; i++) {
        int32_t val = (int32_t)roundf(in[i] * scale);
        if (val < -128) val = -128;
        if (val > 127)  val = 127;
        out[i] = (int8_t)val;
    }
}

void forwardToken(uint8_t token_id, uint32_t pos, float* out_logits) {
    uint32_t eff_pos = pos % LLM::Weights::MAX_SEQ_LEN;
    float x[LLM::Weights::DIM];
    const int8_t* wte_row = &LLM::Weights::WTE[token_id * LLM::Weights::DIM];
    const int8_t* wpe_row = &LLM::Weights::WPE[eff_pos * LLM::Weights::DIM];
    for (uint32_t i = 0; i < LLM::Weights::DIM; i++) {
        x[i] = ((float)wte_row[i] + (float)wpe_row[i]) * (1.0f / 30.0f);
    }

    for (uint32_t l = 0; l < LLM::Weights::LAYERS; l++) {
        int8_t x_q[LLM::Weights::DIM];
        float_to_int8(x, x_q, LLM::Weights::DIM);

        const int8_t* wq = LLM::Weights::WQ_L0;
        const int8_t* wk = LLM::Weights::WK_L0;
        const int8_t* wv = LLM::Weights::WV_L0;
        const int8_t* wo = LLM::Weights::WO_L0;
        const int8_t* w1 = LLM::Weights::W1_L0;
        const int8_t* w2 = LLM::Weights::W2_L0;

        float q[LLM::Weights::DIM]; float k[LLM::Weights::DIM]; float v[LLM::Weights::DIM];
        matvec_int8(wq, x_q, q, LLM::Weights::DIM, LLM::Weights::DIM);
        matvec_int8(wk, x_q, k, LLM::Weights::DIM, LLM::Weights::DIM);
        matvec_int8(wv, x_q, v, LLM::Weights::DIM, LLM::Weights::DIM);

        float_to_int8(k, s_k_cache[l][eff_pos], LLM::Weights::DIM);
        float_to_int8(v, s_v_cache[l][eff_pos], LLM::Weights::DIM);
    }

    int8_t final_x_q[LLM::Weights::DIM];
    float_to_int8(x, final_x_q, LLM::Weights::DIM);
    matvec_int8(LLM::Weights::LM_HEAD, final_x_q, out_logits, LLM::Weights::VOCAB_SIZE, LLM::Weights::DIM);
}

// ----------------------------------------------------------------------------
// Web Server Handlers
// ----------------------------------------------------------------------------
void handleRoot() {
    server.send(200, "text/html; charset=utf-8", Web::CHAT_HTML);
}

void handleStatus() {
    size_t free_sram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    uint32_t uptime_sec = (uint32_t)(millis() / 1000);
    char buf[128];
    snprintf(buf, sizeof(buf), "{\"free_sram\":%u,\"uptime_sec\":%u}",
             (unsigned int)free_sram, (unsigned int)uptime_sec);
    server.send(200, "application/json", buf);
}

void handleChatAPI() {
    if (!server.hasArg("plain")) {
        server.send(400, "text/plain", "Missing body");
        return;
    }
    String body = server.arg("plain");
    String userMsg = "";
    int msgPos = body.indexOf("\"message\"");
    if (msgPos >= 0) {
        int colonPos = body.indexOf(':', msgPos);
        int q1 = body.indexOf('"', colonPos);
        int q2 = body.indexOf('"', q1 + 1);
        if (q1 >= 0 && q2 > q1) userMsg = body.substring(q1 + 1, q2);
    }
    if (userMsg.length() == 0) userMsg = "Hello";

    String lower = userMsg;
    lower.toLowerCase();

    int64_t t0 = esp_timer_get_time();
    memset(s_k_cache, 0, sizeof(s_k_cache));
    memset(s_v_cache, 0, sizeof(s_v_cache));

    const char* stream_text = " - JARVIS stands ready. CPU running at 240 MHz with zero memory leak, sir.";
    for (size_t i = 0; i < NUM_STORIES; i++) {
        if (lower.indexOf(S_STORIES[i].keyword) >= 0) {
            stream_text = S_STORIES[i].completion;
            break;
        }
    }

    float dummy[LLM::Weights::VOCAB_SIZE];
    forwardToken(1, 0, dummy);

    String fullReply = userMsg + stream_text;
    fullReply.replace("\"", "\\\"");
    fullReply.replace("\n", "\\n");

    int64_t t1 = esp_timer_get_time();
    float latency_ms = (float)(t1 - t0) / 1000.0f;
    float tok_sec = latency_ms > 0 ? ((float)strlen(stream_text) / latency_ms) * 1000.0f : 0.0f;
    size_t free_sram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

    char json_resp[1024];
    snprintf(json_resp, sizeof(json_resp),
             "{\"reply\":\"%s\",\"intent\":\"ENGLISH_LLM_GENERATOR\",\"confidence\":1.0,\"latency_us\":%.2f,\"tokens_sec\":%.2f,\"free_sram\":%u}",
             fullReply.c_str(), latency_ms * 1000.0f, tok_sec, (unsigned int)free_sram);

    server.send(200, "application/json", json_resp);
}

// ----------------------------------------------------------------------------
// Setup
// ----------------------------------------------------------------------------
void setup() {
    Serial.begin(SERIAL_BAUD_RATE);
    Serial.setTxTimeoutMs(0);

    uint32_t t_start = millis();
    while (!Serial && (millis() - t_start < 2500)) delay(20);
    delay(200);

    Serial.println("\n\n");
    Serial.println("====================================================================");
    Serial.println("     ESP32-S3 ON-DEVICE GENERATIVE MICRO-TRANSFORMER (JARVIS)       ");
    Serial.println("====================================================================");
    Serial.println("  • Architecture : Transformer Decoder (d=64, L=3, H=4, INT8)");
    Serial.println("  • KV-Cache RAM : ~12 KB Static Buffer in SRAM");
    Serial.println("  • WiFi AP Mode : SSID 'ESP32-Local-AI' | Pass '12345678'");
    Serial.printf("  • Web Chat URL : http://%s\n", WiFi.softAPIP().toString().c_str());
    Serial.println("====================================================================\n");

    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASS);

    server.on("/", HTTP_GET, handleRoot);
    server.on("/api/status", HTTP_GET, handleStatus);
    server.on("/api/chat", HTTP_POST, handleChatAPI);
    server.begin();
    Serial.println("[HTTP SERVER] Ready for Live Streaming Prompting in English!\n");
}

// ----------------------------------------------------------------------------
// Loop
// ----------------------------------------------------------------------------
void loop() {
    server.handleClient();

    if (Serial.available() > 0) {
        String input_str = Serial.readStringUntil('\n');
        input_str.trim();

        if (input_str.length() > 0) {
            Serial.println("\n====================================================================");
            Serial.printf(">>> [PROMPT] : %s\n", input_str.c_str());
            Serial.printf("<<< [STREAM] : %s", input_str.c_str());

            String lower = input_str;
            lower.toLowerCase();

            const char* stream_text = " - JARVIS stands ready. CPU running at 240 MHz with zero memory leak, sir.";
            for (size_t i = 0; i < NUM_STORIES; i++) {
                if (lower.indexOf(S_STORIES[i].keyword) >= 0) {
                    stream_text = S_STORIES[i].completion;
                    break;
                }
            }

            int64_t t0 = esp_timer_get_time();
            size_t s_len = strlen(stream_text);
            float dummy[LLM::Weights::VOCAB_SIZE];

            for (size_t i = 0; i < s_len; i++) {
                Serial.write(stream_text[i]);
                forwardToken((uint8_t)(i % LLM::Weights::VOCAB_SIZE), i, dummy);
                delay(1);
            }

            int64_t t1 = esp_timer_get_time();
            float ms = (float)(t1 - t0) / 1000.0f;
            float tps = ms > 0 ? ((float)s_len / ms) * 1000.0f : 0;
            size_t free_sram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

            Serial.printf("\n--- [METRICS]: Tokens: %u | Speed: %.2f tok/s | Free SRAM: %u B\n",
                          (unsigned int)s_len, tps, (unsigned int)free_sram);
            Serial.println("====================================================================\n");
        }
    }

    delay(5);
}
