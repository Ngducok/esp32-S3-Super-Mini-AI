#pragma once

#include <stdint.h>

namespace LLM {

class Sampler {
public:
    static uint8_t sample(float* logits, uint32_t vocab_size = 256, float temperature = 0.7f, float top_p = 0.9f);
};

} // namespace LLM
