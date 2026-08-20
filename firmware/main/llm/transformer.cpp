#include "transformer.h"
#include "model_llm_weights.h"
#include <string.h>
#include <math.h>

namespace LLM {

// SRAM-Allocated KV-Cache (3 layers x 64 tokens x 64 dim x 2 = 24.5 KB total in SRAM)
static int8_t s_k_cache[Weights::LAYERS][Weights::MAX_SEQ_LEN][Weights::DIM];
static int8_t s_v_cache[Weights::LAYERS][Weights::MAX_SEQ_LEN][Weights::DIM];
static uint32_t s_cache_len = 0;

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

// Matrix-Vector Multiplication: out = mat_int8 * in_int8 (scale factor 1/900)
static void matvec_int8(const int8_t* mat, const int8_t* vec_in, float* vec_out, uint32_t rows, uint32_t cols) {
    for (uint32_t r = 0; r < rows; r++) {
        int32_t acc = 0;
        const int8_t* row = &mat[r * cols];
        for (uint32_t c = 0; c < cols; c++) {
            acc += (int32_t)row[c] * (int32_t)vec_in[c];
        }
        vec_out[r] = (float)acc * (1.0f / 900.0f); // 30 * 30 = 900
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
    uint32_t eff_pos = pos % Weights::MAX_SEQ_LEN;
    if (eff_pos == 0 && pos == 0) {
        s_cache_len = 0;
    }

    // 1. Embeddings: x = WTE[token] + WPE[pos]
    float x[Weights::DIM];
    const int8_t* wte_row = &Weights::WTE[token_id * Weights::DIM];
    const int8_t* wpe_row = &Weights::WPE[eff_pos * Weights::DIM];
    for (uint32_t i = 0; i < Weights::DIM; i++) {
        x[i] = ((float)wte_row[i] + (float)wpe_row[i]) * (1.0f / 30.0f);
    }

    // 2. Transformer Decoder Layers
    for (uint32_t l = 0; l < Weights::LAYERS; l++) {
        int8_t x_q[Weights::DIM];
        float_to_int8(x, x_q, Weights::DIM);

        const int8_t* wq;
        const int8_t* wk;
        const int8_t* wv;
        const int8_t* wo;
        const int8_t* w1;
        const int8_t* w2;

        if (l == 0) {
            wq = Weights::WQ_L0; wk = Weights::WK_L0; wv = Weights::WV_L0; wo = Weights::WO_L0;
            w1 = Weights::W1_L0; w2 = Weights::W2_L0;
        } else if (l == 1) {
            wq = Weights::WQ_L1; wk = Weights::WK_L1; wv = Weights::WV_L1; wo = Weights::WO_L1;
            w1 = Weights::W1_L1; w2 = Weights::W2_L1;
        } else {
            wq = Weights::WQ_L2; wk = Weights::WK_L2; wv = Weights::WV_L2; wo = Weights::WO_L2;
            w1 = Weights::W1_L2; w2 = Weights::W2_L2;
        }

        // Q, K, V Projections
        float q[Weights::DIM];
        float k[Weights::DIM];
        float v[Weights::DIM];
        matvec_int8(wq, x_q, q, Weights::DIM, Weights::DIM);
        matvec_int8(wk, x_q, k, Weights::DIM, Weights::DIM);
        matvec_int8(wv, x_q, v, Weights::DIM, Weights::DIM);

        // Store into Layer KV-Cache
        float_to_int8(k, s_k_cache[l][eff_pos], Weights::DIM);
        float_to_int8(v, s_v_cache[l][eff_pos], Weights::DIM);

        uint32_t current_len = eff_pos + 1;
        if (l == 0) s_cache_len = current_len;

        // Multi-Head Attention computation
        float attn_out[Weights::DIM] = {0.0f};
        float scores[Weights::MAX_SEQ_LEN];

        for (uint32_t h = 0; h < Weights::HEADS; h++) {
            uint32_t h_start = h * Weights::HEAD_DIM;

            // Score Q * K
            for (uint32_t t = 0; t < current_len; t++) {
                float dot = 0.0f;
                const int8_t* k_cached = &s_k_cache[l][t][h_start];
                for (uint32_t d = 0; d < Weights::HEAD_DIM; d++) {
                    dot += q[h_start + d] * ((float)k_cached[d] * (1.0f / 30.0f));
                }
                scores[t] = dot * 0.25f; // 1 / sqrt(16)
            }

            softmax_arr(scores, current_len);

            // Attn_out = sum(weights * V)
            for (uint32_t d = 0; d < Weights::HEAD_DIM; d++) {
                float val_acc = 0.0f;
                for (uint32_t t = 0; t < current_len; t++) {
                    val_acc += scores[t] * ((float)s_v_cache[l][t][h_start + d] * (1.0f / 30.0f));
                }
                attn_out[h_start + d] = val_acc;
            }
        }

        // Out Projection: x = x + attn_out * WO
        int8_t attn_q[Weights::DIM];
        float_to_int8(attn_out, attn_q, Weights::DIM);
        float proj_out[Weights::DIM];
        matvec_int8(wo, attn_q, proj_out, Weights::DIM, Weights::DIM);
        for (uint32_t i = 0; i < Weights::DIM; i++) x[i] += proj_out[i];

        // FFN MLP: Up -> GELU -> Down
        float_to_int8(x, x_q, Weights::DIM);
        float ffn_h[Weights::FFN_DIM];
        matvec_int8(w1, x_q, ffn_h, Weights::FFN_DIM, Weights::DIM);
        for (uint32_t i = 0; i < Weights::FFN_DIM; i++) {
            ffn_h[i] = gelu_act(ffn_h[i]);
        }

        int8_t ffn_q[Weights::FFN_DIM];
        float_to_int8(ffn_h, ffn_q, Weights::FFN_DIM);
        float ffn_down[Weights::DIM];
        matvec_int8(w2, ffn_q, ffn_down, Weights::DIM, Weights::FFN_DIM);

        for (uint32_t i = 0; i < Weights::DIM; i++) x[i] += ffn_down[i];
    }

    // 3. Final LM Head: out_logits = x * LM_HEAD
    int8_t final_x_q[Weights::DIM];
    float_to_int8(x, final_x_q, Weights::DIM);
    matvec_int8(Weights::LM_HEAD, final_x_q, out_logits, Weights::VOCAB_SIZE, Weights::DIM);
}

} // namespace LLM
