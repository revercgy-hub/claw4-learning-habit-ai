#pragma once
#include <cstdint>

namespace claw4 {
// Normalize a signed full-width I2S slot to PCM16. Divide with floor semantics
// so negative values behave like an arithmetic shift on every host compiler.
constexpr int16_t DecodePcm16(int32_t slot) {
    const int64_t value = slot;
    return static_cast<int16_t>(value >= 0 ? value / 65536 : -((-value + 65535) / 65536));
}

// Recovery policy is shared with host fault-injection tests. Hardware operations
// and the final diagnostic halt remain the board adapter's responsibility.
template<typename Operation, typename Recover, typename Wait>
auto RetryThree(Operation operation, Recover recover, Wait wait) {
    auto status = operation(1U);
    for (unsigned attempt = 2; status != 0 && attempt <= 3; ++attempt) {
        recover();
        wait(100U * (attempt - 1));
        status = operation(attempt);
    }
    return status;
}
}
