#pragma once

#include <stdint.h>
#include <stddef.h>

namespace LLM {

class Transformer {
public:
    static void init();
    static void reset();
    static void forwardToken(uint8_t token_id, uint32_t pos, float* out_logits);
    static uint32_t getContextLength();

private:
    static uint32_t s_cur_pos;
};

} // namespace LLM
