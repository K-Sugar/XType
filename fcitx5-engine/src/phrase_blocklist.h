#pragma once

#include <string>
#include <string_view>
#include <vector>

class PhraseBlocklist {
public:
    void load(const std::vector<std::string>& phrases) {
        _phrases = phrases;
    }

    // Stub: always returns false. Regex/literal matching wired in a future session.
    bool matches(std::string_view) const { return false; }

private:
    std::vector<std::string> _phrases;
};
