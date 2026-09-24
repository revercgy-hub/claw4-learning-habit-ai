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
    assert(!claw4::IsNearFullScalePcm16(32759));
    assert(claw4::IsNearFullScalePcm16(32760));
    assert(claw4::IsNearFullScalePcm16(32767));
    assert(!claw4::IsNearFullScalePcm16(-32759));
    assert(claw4::IsNearFullScalePcm16(-32760));
    assert(claw4::IsNearFullScalePcm16(INT16_MIN));

    claw4::PlaybackReferenceDelay reference(3);
    const auto first = reference.Begin();
    assert(first != 0 && reference.active());
    assert(reference.Push(first, 10));
    assert(reference.Push(first, 11));
    assert(reference.Next() == 0);
    assert(reference.Next() == 0);
    assert(reference.Push(first, 12));
    assert(reference.Next() == 0);
    assert(reference.Push(first, 13));
    assert(reference.Next() == 10);
    assert(reference.Next() == 11);
    assert(reference.Next() == 12);
    assert(reference.Next() == 13);
    assert(reference.pending_size() == 0);
    auto stats = reference.TakeStats();
    assert(stats.underflow == 3 && stats.overflow == 0);
    assert(reference.Push(first, 77));
    assert(reference.Next() == 0);
    assert(reference.Push(first, 78));
    reference.End(); // Early stop discards both pending and delayed audio.
    assert(!reference.active() && reference.pending_size() == 0);
    assert(reference.Next() == 0);
    assert(!reference.Push(first, 88)); // A blocked old TX write returned late.
    const auto second = reference.Begin();
    assert(second != first);
    assert(!reference.Push(first, 99));
    assert(reference.Push(second, 20));
    assert(reference.Next() == 0);
    assert(reference.Next() == 0);
    assert(reference.Next() == 0);
    assert(reference.Next() == 20);
    stats = reference.TakeStats();
    assert(stats.discarded == 2 && stats.stale_writes == 2);
    reference.End();

    claw4::PlaybackReferenceDelay immediate_reference(0);
    assert(immediate_reference.Next() == 0);
    const auto immediate_token = immediate_reference.Begin();
    assert(immediate_reference.Push(immediate_token, -1234));
    assert(immediate_reference.Next() == -1234);
    for (std::size_t i = 0; i < claw4::PlaybackReferenceDelay::kPendingCapacity; ++i)
        assert(immediate_reference.Push(immediate_token, static_cast<int16_t>(i)));
    assert(!immediate_reference.Push(immediate_token, 99)); // overflow invalidates alignment
    assert(immediate_reference.Next() == 0);
    assert(!immediate_reference.active() && immediate_reference.pending_size() == 0);
    stats = immediate_reference.TakeStats();
    assert(stats.overflow == 1 && stats.discarded == claw4::PlaybackReferenceDelay::kPendingCapacity);
    const auto recovery_token = immediate_reference.Begin();
    assert(recovery_token != immediate_token);
    assert(!immediate_reference.Push(immediate_token, 123));
    assert(immediate_reference.Push(recovery_token, 456));
    assert(immediate_reference.Next() == 456);
    assert(immediate_reference.Next() == 0);
    stats = immediate_reference.TakeStats();
    assert(stats.stale_writes == 1 && stats.underflow == 1);
    immediate_reference.End();

    claw4::PlaybackReferenceDelay oversized_reference(
        claw4::PlaybackReferenceDelay::kMaxDelaySamples + 1);
    const auto oversized_token = oversized_reference.Begin();
    assert(oversized_reference.Push(oversized_token, 7));
    for (std::size_t i = 0; i < claw4::PlaybackReferenceDelay::kMaxDelaySamples; ++i)
        assert(oversized_reference.Next() == 0);
    assert(oversized_reference.Next() == 7);
    oversized_reference.End();

    // Consecutive playback cycles must never emit the prior cycle's delayed tail.
    for (int cycle = 0; cycle < 20; ++cycle) {
        const auto token = reference.Begin();
        assert(reference.Push(token, static_cast<int16_t>(cycle + 1)));
        assert(reference.Next() == 0);
        reference.End();
        assert(reference.Next() == 0);
    }

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
