#include "sampler.h"
#include "fast_math.h"
#include "esp_random.h"
#include <math.h>
#include <string.h>

namespace LLM {

uint8_t Sampler::sample(float* logits, uint32_t vocab_size, float temperature, float top_p) {
    if (vocab_size > 128) vocab_size = 128;

    // Argmax (Greedy) for coherent, deterministic generation
    uint8_t best_idx = 0;
    float best_val = logits[0];
    for (uint32_t i = 1; i < vocab_size; i++) {
        if (logits[i] > best_val) {
            best_val = logits[i];
            best_idx = (uint8_t)i;
        }
    }

    if (temperature < 0.2f) {
        return best_idx;
    }

    // 1. High-Speed Softmax with Temperature using FastMath LUT
    float inv_temp = 1.0f / temperature;
    float max_logit = best_val;
    float exp_sum = 0.0f;
    float probs[128];

    for (uint32_t i = 0; i < vocab_size; i++) {
        float scaled = (logits[i] - max_logit) * inv_temp;
        probs[i] = FastMath::fast_expf(scaled);
        exp_sum += probs[i];
    }

    float inv_sum = 1.0f / (exp_sum > 1e-7f ? exp_sum : 1.0f);
    for (uint32_t i = 0; i < vocab_size; i++) {
        probs[i] *= inv_sum;
    }

    // 2. CDF Sampling
    float r = (float)(esp_random() % 10000) / 10000.0f;
    float cdf = 0.0f;
    for (uint32_t i = 0; i < vocab_size; i++) {
        cdf += probs[i];
        if (cdf >= r) {
            return (uint8_t)i;
        }
    }

    return best_idx;
}

} // namespace LLM
