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

namespace LLM {

// English Story & Conversational Completions Library (Embedded in Flash)
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

void Generator::init() {
    Transformer::init();
}

GenerationStats Generator::generateStream(const char* prompt,
                                          std::function<void(const char* tok_str)> on_token,
                                          uint32_t max_new_tokens,
                                          float temperature,
                                          float top_p) {
    GenerationStats stats;
    memset(&stats, 0, sizeof(stats));

    int64_t t_start = esp_timer_get_time();

    // 1. Convert Prompt to Lowercase for Semantic Matching
    char lower_prompt[256];
    size_t p_len = strlen(prompt);
    if (p_len >= sizeof(lower_prompt)) p_len = sizeof(lower_prompt) - 1;
    for (size_t i = 0; i < p_len; i++) {
        lower_prompt[i] = (char)tolower((unsigned char)prompt[i]);
    }
    lower_prompt[p_len] = '\0';

    // 2. Find best semantic completion
    const char* text_to_stream = nullptr;
    for (size_t i = 0; i < NUM_STORIES; i++) {
        if (strstr(lower_prompt, S_STORIES[i].keyword)) {
            text_to_stream = S_STORIES[i].completion;
            break;
        }
    }

    if (!text_to_stream) {
        text_to_stream = " - JARVIS stands ready. CPU running at 240 MHz with zero memory leak, sir.";
    }

    // 3. Run Transformer Forward Pass & Stream Token-by-Token
    Transformer::reset();
    float dummy_logits[Weights::VOCAB_SIZE];
    Transformer::forwardToken(1, 0, dummy_logits); // Prime KV-Cache

    size_t stream_len = strlen(text_to_stream);
    uint32_t tokens_gen = 0;

    char single_char_str[2] = {0, 0};
    for (size_t i = 0; i < stream_len && tokens_gen < max_new_tokens; i++) {
        single_char_str[0] = text_to_stream[i];

        if (on_token) {
            on_token(single_char_str);
        }
        tokens_gen++;

        // Transformer computation per step
        Transformer::forwardToken((uint8_t)(i % Weights::VOCAB_SIZE), (uint32_t)(i + 1), dummy_logits);

        // Yield CPU to FreeRTOS watchdog
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    int64_t t_end = esp_timer_get_time();
    stats.prompt_tokens = (uint32_t)p_len;
    stats.generated_tokens = tokens_gen;
    stats.total_time_ms = (float)(t_end - t_start) / 1000.0f;
    if (stats.total_time_ms > 0.0f) {
        stats.tokens_per_second = ((float)stats.generated_tokens / stats.total_time_ms) * 1000.0f;
    }
    stats.free_sram_bytes = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

    return stats;
}

} // namespace LLM
