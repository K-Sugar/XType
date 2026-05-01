#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <deque>
#include <fstream>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "style_profile.h"

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

// ── sentenceAligned ───────────────────────────────────────────────────────────

static std::string_view sentenceAligned(std::string_view s) {
    for (size_t i = 0; i + 1 < s.size(); ++i) {
        char c = s[i];
        if ((c == '.' || c == '!' || c == '?') && s[i + 1] == ' ') {
            size_t start = i + 2;
            while (start < s.size() && s[start] == ' ') ++start;
            if (start < s.size()) return s.substr(start);
        }
    }
    return s;
}

TEST_CASE("no boundary returns full string", "[sentenceAligned]") {
    std::string_view s = "hello world";
    REQUIRE(sentenceAligned(s) == s);
}

TEST_CASE("single sentence boundary with period", "[sentenceAligned]") {
    std::string_view s = "First sentence. Second sentence starts here";
    REQUIRE(sentenceAligned(s) == "Second sentence starts here");
}

TEST_CASE("exclamation boundary", "[sentenceAligned]") {
    std::string_view s = "Wow! That was great";
    REQUIRE(sentenceAligned(s) == "That was great");
}

TEST_CASE("question boundary", "[sentenceAligned]") {
    std::string_view s = "Is this working? Yes it is";
    REQUIRE(sentenceAligned(s) == "Yes it is");
}

TEST_CASE("multiple boundaries — returns text after first", "[sentenceAligned]") {
    std::string_view s = "One. Two. Three";
    REQUIRE(sentenceAligned(s) == "Two. Three");
}

TEST_CASE("period not followed by space — not a boundary", "[sentenceAligned]") {
    std::string_view s = "e.g.something here";
    REQUIRE(sentenceAligned(s) == s);
}

TEST_CASE("boundary at very end — no text after — returns full string", "[sentenceAligned]") {
    std::string_view s = "Done. ";
    REQUIRE(sentenceAligned(s) == s);
}

TEST_CASE("empty string", "[sentenceAligned]") {
    REQUIRE(sentenceAligned("") == "");
}

// ── scoreAndRankExemplars (L4) ────────────────────────────────────────────────
// Mirror of the static function in xtype.cpp — same logic, tested in isolation.

static std::vector<std::string> scoreAndRankExemplars(
    const std::vector<EmbeddingEntry>& index,
    const std::vector<float>& queryVec,
    const std::deque<std::vector<float>>& acceptRing,
    size_t maxCount)
{
    if (index.empty() || queryVec.empty() || maxCount == 0) return {};
    std::vector<std::pair<float, size_t>> scores;
    scores.reserve(index.size());
    for (size_t i = 0; i < index.size(); ++i) {
        const auto& entry = index[i];
        float dot = 0.f;
        size_t dim = std::min(queryVec.size(), entry.embedding.size());
        for (size_t d = 0; d < dim; ++d) dot += queryVec[d] * entry.embedding[d];
        float acceptBoost = 0.f;
        for (const auto& av : acceptRing) {
            float ab = 0.f;
            size_t adim = std::min(av.size(), entry.embedding.size());
            for (size_t d = 0; d < adim; ++d) ab += av[d] * entry.embedding[d];
            acceptBoost = std::max(acceptBoost, ab);
        }
        scores.emplace_back(0.7f * dot + 0.3f * acceptBoost, i);
    }
    size_t n = std::min(maxCount, scores.size());
    std::partial_sort(scores.begin(), scores.begin() + n, scores.end(),
                      [](const auto& a, const auto& b){ return a.first > b.first; });
    std::vector<std::string> results;
    results.reserve(n);
    for (size_t i = 0; i < n; ++i)
        results.push_back(index[scores[i].second].text);
    return results;
}

TEST_CASE("scoreAndRankExemplars: returns most similar entry", "[embed][L4]") {
    // Three entries with orthogonal unit vectors in dim-0, dim-1, dim-2.
    std::vector<EmbeddingEntry> index = {
        {"sentence A", {1.f, 0.f, 0.f}},
        {"sentence B", {0.f, 1.f, 0.f}},
        {"sentence C", {0.f, 0.f, 1.f}},
    };
    std::vector<float> query = {1.f, 0.f, 0.f};  // identical to A
    std::deque<std::vector<float>> ring;
    auto results = scoreAndRankExemplars(index, query, ring, 1);
    REQUIRE(results.size() == 1);
    REQUIRE(results[0] == "sentence A");
}

TEST_CASE("scoreAndRankExemplars: empty index returns empty", "[embed][L4]") {
    std::vector<EmbeddingEntry> index;
    std::vector<float> query = {1.f, 0.f, 0.f};
    std::deque<std::vector<float>> ring;
    REQUIRE(scoreAndRankExemplars(index, query, ring, 5).empty());
}

TEST_CASE("scoreAndRankExemplars: maxCount=0 returns empty", "[embed][L4]") {
    std::vector<EmbeddingEntry> index = {{"sentence A", {1.f, 0.f}}};
    std::vector<float> query = {1.f, 0.f};
    std::deque<std::vector<float>> ring;
    REQUIRE(scoreAndRankExemplars(index, query, ring, 0).empty());
}

TEST_CASE("scoreAndRankExemplars: accept ring changes ranking", "[embed][L4]") {
    // Without ring: query [0.4, 0.3] → A (dot=0.4) beats B (dot=0.3).
    // With ring containing B's direction [0,1]: B's blended score = 0.7*0.3+0.3*1 = 0.51
    // which beats A's score = 0.7*0.4+0.3*0 = 0.28, so B now wins.
    std::vector<EmbeddingEntry> index = {
        {"sentence A", {1.f, 0.f}},
        {"sentence B", {0.f, 1.f}},
    };
    std::vector<float> query = {0.4f, 0.3f};
    std::deque<std::vector<float>> ring;

    auto without_ring = scoreAndRankExemplars(index, query, ring, 1);
    REQUIRE(without_ring[0] == "sentence A");

    ring.push_back({0.f, 1.f});  // B's direction in ring
    auto with_ring = scoreAndRankExemplars(index, query, ring, 1);
    REQUIRE(with_ring[0] == "sentence B");
}

TEST_CASE("accept ring fills and rolls at kAcceptRingSize", "[embed][L4]") {
    static constexpr size_t kRingSize = 20;
    std::deque<std::vector<float>> ring;
    for (size_t i = 0; i < 25; ++i) {
        if (ring.size() >= kRingSize) ring.pop_front();
        ring.push_back({static_cast<float>(i)});
    }
    REQUIRE(ring.size() == kRingSize);
    REQUIRE(ring.front()[0] == 5.f);
    REQUIRE(ring.back()[0] == 24.f);
}

// ── accept_signals.json round-trip (mirrors XTypeEngine private methods) ──────

static bool writeAcceptSignals(const std::deque<std::vector<float>>& ring,
                                const std::string& path) {
    if (path.empty() || ring.empty()) return false;
    std::string tmp = path + ".tmp";
    FILE* f = std::fopen(tmp.c_str(), "w");
    if (!f) return false;
    std::fprintf(f, "{\n  \"version\": 1,\n  \"saved_at\": 0,\n"
                    "  \"accept_embeddings\": [\n");
    bool firstVec = true;
    for (const auto& vec : ring) {
        if (!firstVec) std::fputs(",\n", f);
        firstVec = false;
        std::fputs("    [", f);
        bool firstVal = true;
        for (float v : vec) {
            if (!firstVal) std::fputc(',', f);
            firstVal = false;
            std::fprintf(f, "%.7g", static_cast<double>(v));
        }
        std::fputs("]", f);
    }
    std::fputs("\n  ]\n}\n", f);
    std::fclose(f);
    return std::rename(tmp.c_str(), path.c_str()) == 0;
}

static std::deque<std::vector<float>> loadAcceptSignals(const std::string& path,
                                                         size_t maxRing) {
    std::ifstream in(path);
    if (!in) return {};
    std::string content((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());

    auto pos = content.find("\"accept_embeddings\"");
    if (pos == std::string::npos) return {};
    pos = content.find('[', pos);
    if (pos == std::string::npos) return {};

    std::deque<std::vector<float>> loaded;
    size_t i = pos + 1;
    size_t expectedDim = 0;

    while (i < content.size()) {
        while (i < content.size() && (std::isspace(static_cast<unsigned char>(content[i])) ||
                                      content[i] == ',')) ++i;
        if (i >= content.size() || content[i] == ']') break;
        if (content[i] != '[') break;
        ++i;

        std::vector<float> vec;
        while (i < content.size()) {
            while (i < content.size() && (std::isspace(static_cast<unsigned char>(content[i])) ||
                                          content[i] == ',')) ++i;
            if (i >= content.size() || content[i] == ']') break;
            const char* start = content.data() + i;
            char* end;
            float v = std::strtof(start, &end);
            if (end == start) break;
            vec.push_back(v);
            i = static_cast<size_t>(end - content.data());
        }
        while (i < content.size() && content[i] != ']') ++i;
        if (i < content.size()) ++i;

        if (vec.empty()) continue;

        if (expectedDim == 0) {
            expectedDim = vec.size();
        } else if (vec.size() != expectedDim) {
            return {};  // dimension mismatch — discard
        }

        loaded.push_back(std::move(vec));
    }

    while (loaded.size() > maxRing) loaded.pop_front();
    return loaded;
}

TEST_CASE("accept_signals: round-trip preserves all vectors", "[embed][accept_signals]") {
    auto path = std::string(std::getenv("TMPDIR") ? std::getenv("TMPDIR") : "/tmp") +
                "/xtype_test_accept_signals_rtrip.json";
    std::deque<std::vector<float>> ring;
    ring.push_back({0.1f, 0.2f, 0.3f});
    ring.push_back({0.4f, 0.5f, 0.6f});
    ring.push_back({-0.7f, 0.0f, 1.0f});

    REQUIRE(writeAcceptSignals(ring, path));

    auto loaded = loadAcceptSignals(path, 20);
    REQUIRE(loaded.size() == 3);
    for (size_t i = 0; i < 3; ++i) {
        REQUIRE(loaded[i].size() == 3);
        for (size_t d = 0; d < 3; ++d)
            CHECK(std::fabs(loaded[i][d] - ring[i][d]) < 1e-5f);
    }
    std::remove(path.c_str());
}

TEST_CASE("accept_signals: missing file returns empty ring", "[embed][accept_signals]") {
    auto loaded = loadAcceptSignals("/tmp/xtype_no_such_accept_signals.json", 20);
    REQUIRE(loaded.empty());
}

TEST_CASE("accept_signals: mismatched dims discards entire file", "[embed][accept_signals]") {
    auto path = std::string(std::getenv("TMPDIR") ? std::getenv("TMPDIR") : "/tmp") +
                "/xtype_test_accept_signals_dim.json";
    // Write raw JSON with conflicting dimensions manually.
    {
        std::ofstream f(path);
        f << "{\n  \"version\": 1,\n  \"saved_at\": 0,\n"
             "  \"accept_embeddings\": [\n"
             "    [0.1,0.2,0.3],\n"   // dim 3
             "    [0.4,0.5]\n"         // dim 2 — mismatch
             "  ]\n}\n";
    }
    auto loaded = loadAcceptSignals(path, 20);
    REQUIRE(loaded.empty());
    std::remove(path.c_str());
}

TEST_CASE("accept_signals: ring on disk > maxRing is capped on load", "[embed][accept_signals]") {
    auto path = std::string(std::getenv("TMPDIR") ? std::getenv("TMPDIR") : "/tmp") +
                "/xtype_test_accept_signals_cap.json";
    // Write 5 vectors to disk with maxRing=20, but load with maxRing=3.
    std::deque<std::vector<float>> ring;
    for (int i = 0; i < 5; ++i) ring.push_back({static_cast<float>(i), 0.f});
    REQUIRE(writeAcceptSignals(ring, path));

    auto loaded = loadAcceptSignals(path, 3);
    REQUIRE(loaded.size() == 3);
    // pop_front trims oldest; last 3 are indices 2,3,4
    CHECK(loaded.front()[0] == 2.f);
    CHECK(loaded.back()[0]  == 4.f);
    std::remove(path.c_str());
}
