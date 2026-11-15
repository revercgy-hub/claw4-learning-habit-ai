#pragma once

#include <algorithm>
#include <cstdint>

namespace claw4::v6 {

class LearningTodayRetry {
 public:
  enum class Result { Updated, Unreachable, Rejected };

  constexpr int64_t DelayAfter(Result result) {
    if (result == Result::Unreachable) {
      const auto delay = network_delay_ms_;
      network_delay_ms_ = std::min(delay * 2, kRefreshMs);
      return delay;
    }
    network_delay_ms_ = 2'000;
    return result == Result::Updated ? kRefreshMs : 60'000;
  }

 private:
  static constexpr int64_t kRefreshMs = 30'000;
  int64_t network_delay_ms_ = 2'000;
};

}  // namespace claw4::v6
