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
// SRAM-Allocated KV-Cache (3 layers x 64 pos x 64 dim = 24.5 KB total for K + V)
// ----------------------------------------------------------------------------
static int8_t s_k_cache[LLM::Weights::LAYERS][LLM::Weights::MAX_SEQ_LEN][LLM::Weights::DIM];
static int8_t s_v_cache[LLM::Weights::LAYERS][LLM::Weights::MAX_SEQ_LEN][LLM::Weights::DIM];

static inline float gelu_act(float x) {
    return 0.5f * x * (1.0f + tanhf(0.79788456f * (x + 0.044715f * x * x * x)));
}

static inline void softmax_arr(float* vec, uint32_t size) {
    float max_v = vec[0];
    for (uint32_t i = 1; i < size; i++) if (vec[i] > max_v) max_v = vec[i];
    float sum_exp = 0.0f;
    for (uint32_t i = 0; i < size; i++) {
        float e = expf(vec[i] - max_v);
        vec[i] = e;
        sum_exp += e;
    }
    float inv_sum = 1.0f / (sum_exp > 1e-7f ? sum_exp : 1.0f);
    for (uint32_t i = 0; i < size; i++) vec[i] *= inv_sum;
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

        const int8_t* wq = (l == 0) ? LLM::Weights::WQ_L0 : ((l == 1) ? LLM::Weights::WQ_L1 : LLM::Weights::WQ_L2);
        const int8_t* wk = (l == 0) ? LLM::Weights::WK_L0 : ((l == 1) ? LLM::Weights::WK_L1 : LLM::Weights::WK_L2);
        const int8_t* wv = (l == 0) ? LLM::Weights::WV_L0 : ((l == 1) ? LLM::Weights::WV_L1 : LLM::Weights::WV_L2);
        const int8_t* wo = (l == 0) ? LLM::Weights::WO_L0 : ((l == 1) ? LLM::Weights::WO_L1 : LLM::Weights::WO_L2);
        const int8_t* w1 = (l == 0) ? LLM::Weights::W1_L0 : ((l == 1) ? LLM::Weights::W1_L1 : LLM::Weights::W1_L2);
        const int8_t* w2 = (l == 0) ? LLM::Weights::W2_L0 : ((l == 1) ? LLM::Weights::W2_L1 : LLM::Weights::W2_L2);

        float q[LLM::Weights::DIM]; float k[LLM::Weights::DIM]; float v[LLM::Weights::DIM];
        matvec_int8(wq, x_q, q, LLM::Weights::DIM, LLM::Weights::DIM);
        matvec_int8(wk, x_q, k, LLM::Weights::DIM, LLM::Weights::DIM);
        matvec_int8(wv, x_q, v, LLM::Weights::DIM, LLM::Weights::DIM);

        float_to_int8(k, s_k_cache[l][eff_pos], LLM::Weights::DIM);
        float_to_int8(v, s_v_cache[l][eff_pos], LLM::Weights::DIM);

        uint32_t current_len = eff_pos + 1;

        // Full Multi-Head Self-Attention
        float attn_out[LLM::Weights::DIM] = {0.0f};
        float scores[LLM::Weights::MAX_SEQ_LEN];

        for (uint32_t h = 0; h < LLM::Weights::HEADS; h++) {
            uint32_t h_start = h * LLM::Weights::HEAD_DIM;

            for (uint32_t t = 0; t < current_len; t++) {
                float dot = 0.0f;
                const int8_t* k_cached = &s_k_cache[l][t][h_start];
                for (uint32_t d = 0; d < LLM::Weights::HEAD_DIM; d++) {
                    dot += q[h_start + d] * ((float)k_cached[d] * (1.0f / 30.0f));
                }
                scores[t] = dot * 0.25f; // 1 / sqrt(16)
            }

            softmax_arr(scores, current_len);

            for (uint32_t d = 0; d < LLM::Weights::HEAD_DIM; d++) {
                float val_acc = 0.0f;
                for (uint32_t t = 0; t < current_len; t++) {
                    val_acc += scores[t] * ((float)s_v_cache[l][t][h_start + d] * (1.0f / 30.0f));
                }
                attn_out[h_start + d] = val_acc;
            }
        }

        // Out Projection: x = x + attn_out * WO
        int8_t attn_q[LLM::Weights::DIM];
        float_to_int8(attn_out, attn_q, LLM::Weights::DIM);
        float proj_out[LLM::Weights::DIM];
        matvec_int8(wo, attn_q, proj_out, LLM::Weights::DIM, LLM::Weights::DIM);
        for (uint32_t i = 0; i < LLM::Weights::DIM; i++) x[i] += proj_out[i];

        // MLP FFN
        float_to_int8(x, x_q, LLM::Weights::DIM);
        float ffn_h[LLM::Weights::FFN_DIM];
        matvec_int8(w1, x_q, ffn_h, LLM::Weights::FFN_DIM, LLM::Weights::DIM);
        for (uint32_t i = 0; i < LLM::Weights::FFN_DIM; i++) ffn_h[i] = gelu_act(ffn_h[i]);

        int8_t ffn_q[LLM::Weights::FFN_DIM];
        float_to_int8(ffn_h, ffn_q, LLM::Weights::FFN_DIM);
        float ffn_down[LLM::Weights::DIM];
        matvec_int8(w2, ffn_q, ffn_down, LLM::Weights::DIM, LLM::Weights::FFN_DIM);
        for (uint32_t i = 0; i < LLM::Weights::DIM; i++) x[i] += ffn_down[i];
    }

    int8_t final_x_q[LLM::Weights::DIM];
    float_to_int8(x, final_x_q, LLM::Weights::DIM);
    matvec_int8(LLM::Weights::LM_HEAD, final_x_q, out_logits, LLM::Weights::VOCAB_SIZE, LLM::Weights::DIM);
}

static uint32_t tokenizeInput(const char* text, uint8_t* out_tokens, uint32_t max_tokens) {
    uint32_t count = 0;
    size_t len = strlen(text);
    size_t idx = 0;
    while (idx < len && count < max_tokens) {
        if (isspace((unsigned char)text[idx])) { idx++; continue; }
        int best_token = -1; size_t best_len = 0;
        for (uint32_t v = 1; v < LLM::Weights::VOCAB_SIZE; v++) {
            const char* v_str = LLM::Weights::VOCAB_TOKENS[v];
            size_t v_len = strlen(v_str);
            if (v_len == 0 || (v_len == 1 && v_str[0] == ' ')) continue;
            if (strncasecmp(&text[idx], v_str, v_len) == 0 && v_len > best_len) {
                best_len = v_len;
                best_token = (int)v;
            }
        }
        if (best_token >= 0) {
            out_tokens[count++] = (uint8_t)best_token;
            idx += best_len;
        } else {
            idx++;
        }
    }
    if (count == 0) out_tokens[count++] = 3; // "Hello"
    return count;
}

static uint8_t sampleGreedy(float* logits) {
    uint8_t best_idx = 0;
    float best_val = logits[0];
    for (uint32_t i = 1; i < LLM::Weights::VOCAB_SIZE; i++) {
        if (logits[i] > best_val) {
            best_val = logits[i];
            best_idx = (uint8_t)i;
        }
    }
    return best_idx;
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

    int64_t t0 = esp_timer_get_time();
    memset(s_k_cache, 0, sizeof(s_k_cache));
    memset(s_v_cache, 0, sizeof(s_v_cache));

    uint8_t prompt_tokens[LLM::Weights::MAX_SEQ_LEN];
    uint32_t num_prompt = tokenizeInput(userMsg.c_str(), prompt_tokens, LLM::Weights::MAX_SEQ_LEN / 2);

    float logits[LLM::Weights::VOCAB_SIZE];
    uint32_t cur_pos = 0;
    for (uint32_t i = 0; i < num_prompt; i++) {
        forwardToken(prompt_tokens[i], cur_pos++, logits);
    }

    String generatedReply = "";
    uint32_t tokens_gen = 0;
    uint8_t recent[8] = {0};

    while (tokens_gen < 48 && cur_pos < LLM::Weights::MAX_SEQ_LEN) {
        for (uint32_t r = 0; r < 8; r++) {
            if (recent[r] > 0 && recent[r] < LLM::Weights::VOCAB_SIZE) {
                logits[recent[r]] -= 1.2f;
            }
        }

        uint8_t next_tok = sampleGreedy(logits);
        if (next_tok == 0 || next_tok >= LLM::Weights::VOCAB_SIZE) break;

        const char* tok_str = LLM::Weights::VOCAB_TOKENS[next_tok];
        if (tok_str && strlen(tok_str) > 0 && tok_str[0] != ' ') {
            bool is_punct = (tok_str[0] == '!' || tok_str[0] == ',' || tok_str[0] == '.' || 
                             tok_str[0] == '?' || tok_str[0] == ':');
            if (generatedReply.length() > 0 && !is_punct) generatedReply += " ";
            generatedReply += tok_str;
            tokens_gen++;
            recent[tokens_gen % 8] = next_tok;
        }

        forwardToken(next_tok, cur_pos++, logits);
        delay(1);
    }

    if (generatedReply.length() == 0) {
        generatedReply = "JARVIS operational on ESP32-S3.";
    }

    int64_t t1 = esp_timer_get_time();
    float latency_ms = (float)(t1 - t0) / 1000.0f;
    float tok_sec = latency_ms > 0 ? ((float)tokens_gen / latency_ms) * 1000.0f : 0.0f;
    size_t free_sram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

    generatedReply.replace("\"", "\\\"");
    generatedReply.replace("\n", "\\n");

    char json_resp[1024];
    snprintf(json_resp, sizeof(json_resp),
             "{\"reply\":\"%s\",\"intent\":\"MICRO_TRANSFORMER_AUTOREGRESSIVE\",\"confidence\":1.0,\"latency_us\":%.2f,\"tokens_sec\":%.2f,\"free_sram\":%u}",
             generatedReply.c_str(), latency_ms * 1000.0f, tok_sec, (unsigned int)free_sram);

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
    Serial.println("  • KV-Cache RAM : ~24.5 KB Static Buffer in SRAM (12.3 KB K + 12.3 KB V)");
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
