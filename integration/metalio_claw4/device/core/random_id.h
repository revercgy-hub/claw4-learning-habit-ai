// claw4/integration/metalio_claw4/device/core/random_id.h
// Fixed-width entropy id formatting for ESP-IDF nano-newlib builds.
#pragma once

#include <cstdint>
#include <string>

namespace claw4 {
namespace metalio {

// Avoid printf-family 64-bit format specifiers here. The Claw4 firmware uses
// nano-newlib, where unsupported "%llx" formatting produced the same literal
// text for every random value and therefore duplicate outbox event ids.
inline std::string formatEntropyId(const char* prefix, uint32_t high,
                                   uint32_t low) {
  static constexpr char kHex[] = "0123456789abcdef";
  std::string out = prefix ? prefix : "";
  out.reserve(out.size() + 16);
  const auto append32 = [&](uint32_t value) {
    for (int shift = 28; shift >= 0; shift -= 4) {
      out.push_back(kHex[(value >> shift) & 0x0fU]);
    }
  };
  append32(high);
  append32(low);
  return out;
}

}  // namespace metalio
}  // namespace claw4
