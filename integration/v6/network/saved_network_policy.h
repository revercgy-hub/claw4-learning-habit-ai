#pragma once
#include <cstddef>
#include <string>
#include <vector>

namespace claw4 {
// Used only after a successful all-channel scan returned no saved SSID.
// Never invent a BSSID or relax an explicit pinning preference.
template<class Saved>
std::vector<std::size_t> DirectCandidates(const std::vector<Saved>& saved,
                                          bool pinned) {
    std::vector<std::size_t> result;
    if (pinned) return result;
    for (std::size_t i = 0; i < saved.size() && result.size() < 3; ++i) {
        const auto& item = saved[i];
        if (item.ssid.empty() || item.ssid.size() > 32 || item.password.size() > 64 ||
            item.ssid.find('\0') != std::string::npos ||
            item.password.find('\0') != std::string::npos) continue;
        bool duplicate = false;
        for (auto prior : result) if (saved[prior].ssid == item.ssid) duplicate = true;
        if (!duplicate) result.push_back(i);
    }
    return result;
}
}
