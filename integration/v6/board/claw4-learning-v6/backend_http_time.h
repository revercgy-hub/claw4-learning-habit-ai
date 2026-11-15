#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

namespace claw4::v6 {

struct BackendTimeSample {
  int64_t epoch_ms;
  int64_t uncertainty_ms;
};

// Parse the preferred HTTP Date format. Include network travel and whole-second
// rounding in the uncertainty instead of assuming when the server stamped it.
inline std::optional<BackendTimeSample> ParseBackendTime(std::string_view date, int64_t request_ms) {
  using namespace std::literals;
  constexpr std::array weekdays{"Sun"sv, "Mon"sv, "Tue"sv, "Wed"sv, "Thu"sv, "Fri"sv, "Sat"sv};
  constexpr std::array months{"Jan"sv, "Feb"sv, "Mar"sv, "Apr"sv, "May"sv, "Jun"sv,
                              "Jul"sv, "Aug"sv, "Sep"sv, "Oct"sv, "Nov"sv, "Dec"sv};
  constexpr std::array month_days{31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (request_ms < 0 || request_ms > 4'000 || date.size() != 29 || date.substr(3, 2) != ", " ||
      date[7] != ' ' || date[11] != ' ' || date[16] != ' ' || date[19] != ':' || date[22] != ':' ||
      date.substr(25) != " GMT") return std::nullopt;

  const auto number = [date](size_t pos, size_t length) {
    int value = 0;
    for (const auto digit : date.substr(pos, length)) {
      if (digit < '0' || digit > '9') return -1;
      value = value * 10 + (digit - '0');
    }
    return value;
  };
  const auto leap = [](int year) { return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0); };
  const auto year = number(12, 4);
  const auto day = number(5, 2);
  const auto hour = number(17, 2);
  const auto minute = number(20, 2);
  const auto second = number(23, 2);
  if (year < 2025 || year >= 2100 || day < 1 || hour < 0 || hour > 23 ||
      minute < 0 || minute > 59 || second < 0 || second > 59) return std::nullopt;

  size_t month = 0;
  while (month < months.size() && months[month] != date.substr(8, 3)) ++month;
  if (month == months.size() || day > month_days[month] + (month == 1 && leap(year))) return std::nullopt;

  int64_t days = day - 1;
  for (int y = 1970; y < year; ++y) days += 365 + leap(y);
  for (size_t m = 0; m < month; ++m) days += month_days[m] + (m == 1 && leap(year));
  if (weekdays[(days + 4) % 7] != date.substr(0, 3)) return std::nullopt;
  return BackendTimeSample{((days * 24 + hour) * 3600 + minute * 60 + second) * 1000,
                           request_ms + 1'000};
}

}  // namespace claw4::v6
