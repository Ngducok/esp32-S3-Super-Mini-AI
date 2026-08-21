#include "generator.h"
#include "transformer.h"
#include "sampler.h"
#include "model_llm_weights.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include <string.h>
#include <ctype.h>
#include <strings.h>

namespace LLM {

void Generator::init() {
    Transformer::init();
}

// Greedy Longest-Match Subword Tokenizer against Flash Vocabulary
static uint32_t tokenize(const char* text, uint8_t* out_tokens, uint32_t max_tokens) {
    uint32_t count = 0;
    size_t len = strlen(text);
    size_t idx = 0;

    while (idx < len && count < max_tokens) {
        if (isspace((unsigned char)text[idx])) {
            idx++;
            continue;
        }

        int best_token = -1;
        size_t best_len = 0;

        for (uint32_t v = 1; v < Weights::VOCAB_SIZE; v++) {
            const char* v_str = Weights::VOCAB_TOKENS[v];
            size_t v_len = strlen(v_str);
            if (v_len == 0 || (v_len == 1 && v_str[0] == ' ')) continue;

            if (strncasecmp(&text[idx], v_str, v_len) == 0) {
                if (v_len > best_len) {
                    best_len = v_len;
                    best_token = (int)v;
                }
            }
        }

        if (best_token >= 0) {
            out_tokens[count++] = (uint8_t)best_token;
            idx += best_len;
        } else {
            idx++; // Advance past unmatched characters
        }
    }

    if (count == 0) {
        out_tokens[count++] = 3; // Default seed: "Hello"
    }

    return count;
}

GenerationStats Generator::generateStream(const char* prompt,
                                          std::function<void(const char* tok_str)> on_token,
                                          uint32_t max_new_tokens,
                                          float temperature,
                                          float top_p) {
    GenerationStats stats;
    memset(&stats, 0, sizeof(stats));

    int64_t t_start = esp_timer_get_time();

    // 1. Tokenize Input Prompt
    uint8_t prompt_tokens[Weights::MAX_SEQ_LEN];
    uint32_t num_prompt_tokens = tokenize(prompt, prompt_tokens, Weights::MAX_SEQ_LEN / 2);

    // 2. Prefill Phase: Ingest Prompt Tokens into Transformer KV-Cache
    Transformer::reset();
    float logits[Weights::VOCAB_SIZE];
    uint32_t cur_pos = 0;

    for (uint32_t i = 0; i < num_prompt_tokens; i++) {
        Transformer::forwardToken(prompt_tokens[i], cur_pos++, logits);
    }

    // 3. Autoregressive Decode Loop (Sliding Window Continuous Generation)
    uint32_t tokens_gen = 0;
    uint8_t recent_tokens[8] = {0};
    uint32_t recent_idx = 0;

    while (tokens_gen < max_new_tokens) {
        // Apply lightweight repetition penalty on recently generated tokens
        for (uint32_t r = 0; r < 8; r++) {
            if (recent_tokens[r] > 0 && recent_tokens[r] < Weights::VOCAB_SIZE) {
                logits[recent_tokens[r]] -= 1.2f;
            }
        }

        // Sample next token ID from neural network logits
        uint8_t next_token = Sampler::sample(logits, Weights::VOCAB_SIZE, temperature, top_p);

        // Stop generation if end-of-sequence / pad token is sampled
        if (next_token == 0 || next_token >= Weights::VOCAB_SIZE) {
            break;
        }

        const char* tok_str = Weights::VOCAB_TOKENS[next_token];
        if (tok_str && strlen(tok_str) > 0 && tok_str[0] != ' ') {
            bool is_punct = (tok_str[0] == '!' || tok_str[0] == ',' || tok_str[0] == '.' || 
                             tok_str[0] == '?' || tok_str[0] == ':');
            if (on_token) {
                if (tokens_gen > 0 && !is_punct) {
                    on_token(" ");
                }
                on_token(tok_str);
            }
            tokens_gen++;

            recent_tokens[recent_idx % 8] = next_token;
            recent_idx++;
        }

        // Forward next token into Transformer for autoregressive continuation
        Transformer::forwardToken(next_token, cur_pos++, logits);

        // Yield CPU to FreeRTOS watchdog
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    int64_t t_end = esp_timer_get_time();
    stats.prompt_tokens = num_prompt_tokens;
    stats.generated_tokens = tokens_gen;
    stats.total_time_ms = (float)(t_end - t_start) / 1000.0f;
    if (stats.total_time_ms > 0.0f) {
        stats.tokens_per_second = ((float)stats.generated_tokens / stats.total_time_ms) * 1000.0f;
    }
    stats.free_sram_bytes = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

    return stats;
}

} // namespace LLM

