// claw4/firmware/tests/host/smoke_test.cpp
// Native host smoke test for the WB-STREAM-002 CP0 C++17 toolchain gate.
//
// This program must COMPILE, LINK and RUN on the Windows host with a C++17
// compiler and exit 0. It proves the local toolchain is usable for real
// executable unit tests (unlike -fsyntax-only cross checks) and that paths
// with spaces/unicode do not break the build.

#include <cstdint>
#include <iostream>
#include <string>

namespace {

// Small constexpr check to exercise C++17 features (no third-party code).
constexpr int64_t square(int64_t x) { return x * x; }
static_assert(square(12) == 144, "constexpr smoke check");

}  // namespace

int main() {
    const std::string greeting = "claw4 host smoke";
    const int64_t checksum = square(12) + static_cast<int64_t>(greeting.size());
    std::cout << "[smoke] " << greeting << " checksum=" << checksum
              << " cpp=" << __cplusplus << "\n";
    // Expected checksum: square(12)=144 + len("claw4 host smoke")=16 -> 160.
    return (checksum == 160 && __cplusplus >= 201703L) ? 0 : 1;
}
