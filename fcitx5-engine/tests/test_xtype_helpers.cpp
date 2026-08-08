#include <algorithm>
#include <cctype>
#include <optional>
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

// ── Suggestion gate ───────────────────────────────────────────────────────────
// Mirrors the pass-through checks at the top of keyEvent(), in the same order:
// master switch, then app blocklist, then per-app override. The engine itself
// cannot be unit-tested without a live Fcitx5 instance, so the ordering is
// asserted here — the master switch must win over every later check.

struct AppOverrideStub { std::optional<bool> enabled; };

static bool suggestsFor(bool engineEnabled,
                        const std::string& prog,
                        const std::vector<std::string>& blocklist,
                        const std::optional<AppOverrideStub>& ov) {
    if (!engineEnabled) return false;
    if (blockedBy(prog, blocklist)) return false;
    if (ov && ov->enabled.has_value() && !*ov->enabled) return false;
    return true;
}

TEST_CASE("gate: engine_enabled=false suppresses suggestions everywhere") {
    CHECK(!suggestsFor(false, "kate",  {}, std::nullopt));
    CHECK(!suggestsFor(false, "firefox", {}, std::nullopt));
}

TEST_CASE("gate: engine_enabled=false is not overridable per-app") {
    // [apps.kate] enabled = true must not resurrect a globally disabled engine.
    CHECK(!suggestsFor(false, "kate", {}, AppOverrideStub{true}));
}

TEST_CASE("gate: engine_enabled=true preserves blocklist and per-app opt-out") {
    CHECK(suggestsFor(true, "kate", {}, std::nullopt));
    CHECK(!suggestsFor(true, "konsole", {"konsole"}, std::nullopt));
    CHECK(!suggestsFor(true, "kate", {}, AppOverrideStub{false}));
    CHECK(suggestsFor(true, "kate", {}, AppOverrideStub{true}));
}
