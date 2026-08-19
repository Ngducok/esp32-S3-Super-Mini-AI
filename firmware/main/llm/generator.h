#pragma once

#include <stdint.h>
#include <stddef.h>
#include <functional>

namespace LLM {

struct GenerationStats {
    uint32_t prompt_tokens;
    uint32_t generated_tokens;
    float total_time_ms;
    float tokens_per_second;
    size_t free_sram_bytes;
};

class Generator {
public:
    static void init();
    static GenerationStats generateStream(const char* prompt,
                                          std::function<void(const char* tok_str)> on_token,
                                          uint32_t max_new_tokens = 48,
                                          float temperature = 0.7f,
                                          float top_p = 0.9f);
};

} // namespace LLM
