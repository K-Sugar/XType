#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <vector>

class PhraseBlocklist {
public:
    void load(const std::vector<std::string>& phrases) {
        _phrases = phrases;
        // Lowercase all entries once for case-insensitive matching.
        for (auto& p : _phrases) {
            std::transform(p.begin(), p.end(), p.begin(),
                           [](unsigned char c){ return std::tolower(c); });
        }
    }

    // Returns true if any blocked phrase appears (case-insensitively) as a
    // substring of `text`. Empty phrase list always returns false.
    bool matches(std::string_view text) const {
        if (_phrases.empty() || text.empty()) return false;
        std::string lower;
        lower.reserve(text.size());
        for (char c : text)
            lower.push_back(static_cast<char>(
                std::tolower(static_cast<unsigned char>(c))));

        for (const auto& phrase : _phrases)
            if (!phrase.empty() && lower.find(phrase) != std::string::npos)
                return true;
        return false;
    }

private:
    std::vector<std::string> _phrases;  // stored lowercased
};
