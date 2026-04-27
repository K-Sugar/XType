#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

// Mirrors isBlocked() logic: separators are '.' and '_' only (not '-').
// This prevents "kate" matching "kate-beta" while allowing "org.kde.kate" to match.
static bool isSep(char c) { return c == '.' || c == '_'; }

static bool blockedBy(const std::string& prog, const std::vector<std::string>& list) {
    std::string p = prog;
    std::transform(p.begin(), p.end(), p.begin(), ::tolower);
    for (const auto& entry : list) {
        if (entry.empty()) continue;
        auto pos = p.find(entry);
        while (pos != std::string::npos) {
            auto after = pos + entry.size();
            bool s = (pos == 0)          || isSep(p[pos - 1]);
            bool e = (after == p.size()) || isSep(p[after]);
            if (s && e) return true;
            pos = p.find(entry, pos + 1);
        }
    }
    return false;
}

TEST_CASE("isBlocked word-boundary: konsole blocks konsole and org.kde.konsole") {
    std::vector<std::string> list = {"konsole"};
    CHECK(blockedBy("konsole",         list));
    CHECK(blockedBy("org.kde.konsole", list));
    CHECK(!blockedBy("konsoleboard",   list));
    CHECK(!blockedBy("mykonsoleapp",   list));
}

TEST_CASE("isBlocked word-boundary: keepassxc blocks keepassxc only") {
    std::vector<std::string> list = {"keepassxc"};
    CHECK(blockedBy("keepassxc",          list));
    CHECK(!blockedBy("keepassxc-browser", list));
}

TEST_CASE("isBlocked word-boundary: empty entry is skipped") {
    std::vector<std::string> list = {""};
    CHECK(!blockedBy("anyapp", list));
}

TEST_CASE("isBlocked word-boundary: case-insensitive match") {
    std::vector<std::string> list = {"keepassxc"};
    CHECK(blockedBy("KeePassXC", list));
}

TEST_CASE("isBlocked word-boundary: dash separator") {
    std::vector<std::string> list = {"kate"};
    CHECK(blockedBy("kate",       list));
    CHECK(blockedBy("org.kde.kate", list));
    CHECK(!blockedBy("kate-beta", list));
}
