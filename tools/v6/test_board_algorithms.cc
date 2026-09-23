#include "../../integration/v6/board/claw4-learning-v6/board_algorithms.h"
#include <cassert>
#include <climits>
#include <vector>

int main() {
    using claw4::ActiveLowButtonDebouncer;
    using claw4::ButtonEvent;
    ActiveLowButtonDebouncer boot_held;
    boot_held.Begin(true, 0);
    assert(boot_held.Sample(true, 2000) == ButtonEvent::None);
    assert(boot_held.Sample(false, 2100) == ButtonEvent::None);
    assert(boot_held.Sample(false, 2150) == ButtonEvent::None);
    assert(boot_held.Sample(true, 2200) == ButtonEvent::None);
    assert(boot_held.Sample(true, 2250) == ButtonEvent::None);
    assert(boot_held.Sample(false, 2300) == ButtonEvent::None); // rejected bounce
    assert(boot_held.Sample(true, 2320) == ButtonEvent::None);
    assert(boot_held.Sample(true, 2370) == ButtonEvent::None);
    assert(boot_held.Sample(false, 2500) == ButtonEvent::None);
    assert(boot_held.Sample(false, 2550) == ButtonEvent::ShortPress);

    ActiveLowButtonDebouncer long_press;
    long_press.Begin(false, 0);
    assert(long_press.Sample(true, 100) == ButtonEvent::None);
    assert(long_press.Sample(true, 150) == ButtonEvent::None);
    assert(long_press.Sample(true, 1649) == ButtonEvent::None);
    assert(long_press.Sample(true, 1650) == ButtonEvent::LongPress);
    assert(long_press.Sample(true, 3000) == ButtonEvent::None);
    assert(long_press.Sample(false, 3010) == ButtonEvent::None);
    assert(long_press.Sample(false, 3060) == ButtonEvent::None);

    for (int sample = -32768; sample <= 32767; ++sample)
        assert(claw4::DecodePcm16(int32_t(int64_t(sample) * 65536)) == sample);
    assert(claw4::DecodePcm16(INT32_MIN) == -32768);
    assert(claw4::DecodePcm16(INT32_MAX) == 32767);
    assert(claw4::DecodePcm16(-1) == -1);
    assert(claw4::DecodePcm16(65535) == 0);
    assert(claw4::DecodePcm16(-65537) == -2);
    for (unsigned success = 1; success <= 4; ++success) {
        std::vector<unsigned> attempts, waits;
        unsigned resets = 0;
        const int result = claw4::RetryThree([&](unsigned attempt) {
            attempts.push_back(attempt);
            return attempt == success ? 0 : -17;
        }, [&] { ++resets; }, [&](unsigned ms) { waits.push_back(ms); });
        const unsigned count = success < 4 ? success : 3;
        assert(attempts.size() == count);
        assert(resets == count - 1 && waits.size() == count - 1);
        assert(result == (success < 4 ? 0 : -17));
        for (unsigned i = 0; i < count; ++i) assert(attempts[i] == i + 1);
        for (unsigned i = 0; i + 1 < count; ++i) assert(waits[i] == 100 * (i + 1));
    }
}
