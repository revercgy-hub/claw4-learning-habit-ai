#include "../../integration/v6/network/saved_network_policy.h"
#include <cassert>
struct Saved { std::string ssid, password; };
int main() {
    using claw4::DirectCandidates;
    std::vector<Saved> saved{{"hidden", "secret"}, {"other", ""},
                             {"third", "password"}, {"fourth", "password"}};
    assert(DirectCandidates(saved, true).empty());
    assert((DirectCandidates(saved, false) == std::vector<std::size_t>{0, 1, 2}));
    assert(DirectCandidates(std::vector<Saved>{}, false).empty());
    saved = {{"", "x"}, {std::string(33, 'x'), "x"}, {"valid", std::string(65, 'x')},
             {"valid", "x"}, {"valid", "y"}, {std::string(32, 'x'), std::string(64, 'x')}};
    assert((DirectCandidates(saved, false) == std::vector<std::size_t>{3, 5}));
    saved = {{std::string("a\0b", 3), "x"}, {"a", std::string("a\0b", 3)}, {"open", ""}};
    assert((DirectCandidates(saved, false) == std::vector<std::size_t>{2}));
}
