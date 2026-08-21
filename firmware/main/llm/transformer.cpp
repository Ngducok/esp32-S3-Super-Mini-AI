#include "transformer.h"
#include "model_llm_weights.h"
#include "fast_math.h"
#include "simd_ops.h"
#include <string.h>
#include <math.h>

namespace LLM {

// SRAM-Allocated Ring-Buffer KV-Cache (3 layers x 64 tokens x 64 dim x 2 = 24.5 KB total in SRAM)
static int8_t s_k_cache[Weights::LAYERS][Weights::MAX_SEQ_LEN][Weights::DIM];
static int8_t s_v_cache[Weights::LAYERS][Weights::MAX_SEQ_LEN][Weights::DIM];
static uint32_t s_cache_len = 0;

static inline void float_to_int8(const float* in, int8_t* out, uint32_t size, float scale = 30.0f) {
    uint32_t i = 0;
    for (; i + 4 <= size; i += 4) {
        int32_t v0 = (int32_t)roundf(in[i + 0] * scale);
        int32_t v1 = (int32_t)roundf(in[i + 1] * scale);
        int32_t v2 = (int32_t)roundf(in[i + 2] * scale);
        int32_t v3 = (int32_t)roundf(in[i + 3] * scale);
        if (v0 < -128) { v0 = -128; } else if (v0 > 127) { v0 = 127; }
        if (v1 < -128) { v1 = -128; } else if (v1 > 127) { v1 = 127; }
        if (v2 < -128) { v2 = -128; } else if (v2 > 127) { v2 = 127; }
        if (v3 < -128) { v3 = -128; } else if (v3 > 127) { v3 = 127; }
        out[i + 0] = (int8_t)v0;
        out[i + 1] = (int8_t)v1;
        out[i + 2] = (int8_t)v2;
        out[i + 3] = (int8_t)v3;
    }
    for (; i < size; i++) {
        int32_t val = (int32_t)roundf(in[i] * scale);
        if (val < -128) { val = -128; } else if (val > 127) { val = 127; }
        out[i] = (int8_t)val;
    }
}

void Transformer::init() {
    reset();
}

void Transformer::reset() {
    memset(s_k_cache, 0, sizeof(s_k_cache));
    memset(s_v_cache, 0, sizeof(s_v_cache));
    s_cache_len = 0;
}

uint32_t Transformer::getContextLength() {
    return s_cache_len;
}

void Transformer::forwardToken(uint8_t token_id, uint32_t pos, float* out_logits) {
    // 1. Sliding Window Ring-Buffer slot assignment
    uint32_t ring_slot = pos % Weights::MAX_SEQ_LEN;
    uint32_t window_len = (pos < Weights::MAX_SEQ_LEN) ? (pos + 1) : Weights::MAX_SEQ_LEN;
    s_cache_len = window_len;

    // 2. Relative Positional Embedding lookup
    // When sliding window is active, map query to newest window slot (MAX_SEQ_LEN - 1)
    uint32_t wpe_pos = (pos < Weights::MAX_SEQ_LEN) ? pos : (Weights::MAX_SEQ_LEN - 1);
    float x[Weights::DIM];
    const int8_t* wte_row = &Weights::WTE[token_id * Weights::DIM];
    const int8_t* wpe_row = &Weights::WPE[wpe_pos * Weights::DIM];
    for (uint32_t i = 0; i < Weights::DIM; i++) {
        x[i] = ((float)wte_row[i] + (float)wpe_row[i]) * (1.0f / 30.0f);
    }

    // 3. Transformer Decoder Layers
    for (uint32_t l = 0; l < Weights::LAYERS; l++) {
        int8_t x_q[Weights::DIM];
        float_to_int8(x, x_q, Weights::DIM);

        const int8_t* wq = (l == 0) ? Weights::WQ_L0 : ((l == 1) ? Weights::WQ_L1 : Weights::WQ_L2);
        const int8_t* wk = (l == 0) ? Weights::WK_L0 : ((l == 1) ? Weights::WK_L1 : Weights::WK_L2);
        const int8_t* wv = (l == 0) ? Weights::WV_L0 : ((l == 1) ? Weights::WV_L1 : Weights::WV_L2);
        const int8_t* wo = (l == 0) ? Weights::WO_L0 : ((l == 1) ? Weights::WO_L1 : Weights::WO_L2);
        const int8_t* w1 = (l == 0) ? Weights::W1_L0 : ((l == 1) ? Weights::W1_L1 : Weights::W1_L2);
        const int8_t* w2 = (l == 0) ? Weights::W2_L0 : ((l == 1) ? Weights::W2_L1 : Weights::W2_L2);

        // Q, K, V Projections using SIMD GEMV
        float q[Weights::DIM];
        float k[Weights::DIM];
        float v[Weights::DIM];
        SIMD::matvec_int8(wq, x_q, q, Weights::DIM, Weights::DIM);
        SIMD::matvec_int8(wk, x_q, k, Weights::DIM, Weights::DIM);
        SIMD::matvec_int8(wv, x_q, v, Weights::DIM, Weights::DIM);

        // Store into Layer Ring-Buffer KV-Cache
        float_to_int8(k, s_k_cache[l][ring_slot], Weights::DIM);
        float_to_int8(v, s_v_cache[l][ring_slot], Weights::DIM);

        // Multi-Head Attention computation across Sliding Window
        float attn_out[Weights::DIM] = {0.0f};
        float scores[Weights::MAX_SEQ_LEN];

        for (uint32_t h = 0; h < Weights::HEADS; h++) {
            uint32_t h_start = h * Weights::HEAD_DIM;

            // Score Q * K across active sliding window
            for (uint32_t t = 0; t < window_len; t++) {
                // Map temporal index t to physical ring buffer slot
                uint32_t slot = (pos < Weights::MAX_SEQ_LEN) ? t : ((pos - window_len + 1 + t) % Weights::MAX_SEQ_LEN);
                
                float dot = 0.0f;
                const int8_t* k_cached = &s_k_cache[l][slot][h_start];
                for (uint32_t d = 0; d < Weights::HEAD_DIM; d++) {
                    dot += q[h_start + d] * ((float)k_cached[d] * (1.0f / 30.0f));
                }
                scores[t] = dot * 0.25f; // 1 / sqrt(16)
            }

            // High-Speed Fast Softmax (LUT-based)
            FastMath::fast_softmax(scores, window_len);

            // Attn_out = sum(weights * V)
            for (uint32_t d = 0; d < Weights::HEAD_DIM; d++) {
                float val_acc = 0.0f;
                for (uint32_t t = 0; t < window_len; t++) {
                    uint32_t slot = (pos < Weights::MAX_SEQ_LEN) ? t : ((pos - window_len + 1 + t) % Weights::MAX_SEQ_LEN);
                    val_acc += scores[t] * ((float)s_v_cache[l][slot][h_start + d] * (1.0f / 30.0f));
                }
                attn_out[h_start + d] = val_acc;
            }
        }

        // Out Projection: x = x + attn_out * WO
        int8_t attn_q[Weights::DIM];
        float_to_int8(attn_out, attn_q, Weights::DIM);
        float proj_out[Weights::DIM];
        SIMD::matvec_int8(wo, attn_q, proj_out, Weights::DIM, Weights::DIM);
        for (uint32_t i = 0; i < Weights::DIM; i++) x[i] += proj_out[i];

        // FFN MLP: Up -> Fast GELU (LUT) -> Down
        float_to_int8(x, x_q, Weights::DIM);
        float ffn_h[Weights::FFN_DIM];
        SIMD::matvec_int8(w1, x_q, ffn_h, Weights::FFN_DIM, Weights::DIM);
        for (uint32_t i = 0; i < Weights::FFN_DIM; i++) {
            ffn_h[i] = FastMath::fast_gelu(ffn_h[i]);
        }

        int8_t ffn_q[Weights::FFN_DIM];
        float_to_int8(ffn_h, ffn_q, Weights::FFN_DIM);
        float ffn_down[Weights::DIM];
        SIMD::matvec_int8(w2, ffn_q, ffn_down, Weights::DIM, Weights::FFN_DIM);

        for (uint32_t i = 0; i < Weights::DIM; i++) x[i] += ffn_down[i];
    }

    // 4. Final LM Head: out_logits = x * LM_HEAD
    int8_t final_x_q[Weights::DIM];
    float_to_int8(x, final_x_q, Weights::DIM);
    SIMD::matvec_int8(Weights::LM_HEAD, final_x_q, out_logits, Weights::VOCAB_SIZE, Weights::DIM);
}

} // namespace LLM
