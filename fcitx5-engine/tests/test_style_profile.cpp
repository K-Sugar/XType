#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

#include "style_profile.h"

namespace fs = std::filesystem;

static fs::path uniqueDir(const std::string& tag) {
    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    auto dir = fs::temp_directory_path() /
               ("xtype_sp_" + tag + "_" + std::to_string(now) + "_" + std::to_string(getpid()));
    fs::create_directories(dir);
    return dir;
}

static void writeCorpus(const fs::path& path, const std::vector<std::string>& lines) {
    std::ofstream out(path);
    for (const auto& l : lines) out << l << '\n';
}

// ── parsing ──────────────────────────────────────────────────────────────────

TEST_CASE("loadFromCorpus parses a simple corpus", "[style][parse]") {
    auto d = uniqueDir("parse");
    auto p = d / "corpus.txt";
    writeCorpus(p, {
        "the quick brown fox jumps over the lazy dog",
        "she sells seashells by the seashore today",
        "now is the time for all good men to act",
    });
    StyleProfile sp;
    sp.loadFromCorpus(p.string(), 42);
    REQUIRE(sp.sentenceCount() == 3);
    REQUIRE_FALSE(sp.exemplars().empty());
    fs::remove_all(d);
}

TEST_CASE("splits multiple sentences within a single line", "[style][parse]") {
    auto d = uniqueDir("split");
    auto p = d / "corpus.txt";
    writeCorpus(p, {
        "hello world how are you. fine thanks for asking. lets get going now.",
    });
    StyleProfile sp;
    sp.loadFromCorpus(p.string(), 42);
    REQUIRE(sp.sentenceCount() == 3);
    fs::remove_all(d);
}

TEST_CASE("rejects too-short sentences", "[style][filter]") {
    auto d = uniqueDir("short");
    auto p = d / "corpus.txt";
    writeCorpus(p, {
        "hi.",
        "this sentence is long enough to be kept here",
    });
    StyleProfile sp;
    sp.loadFromCorpus(p.string(), 42);
    REQUIRE(sp.sentenceCount() == 1);
    fs::remove_all(d);
}

// ── privacy filter ───────────────────────────────────────────────────────────

TEST_CASE("rejects email-like patterns", "[style][filter]") {
    auto d = uniqueDir("email");
    auto p = d / "corpus.txt";
    writeCorpus(p, {
        "please contact foo@bar.com today",
        "she walked across the room slowly today",
    });
    StyleProfile sp;
    sp.loadFromCorpus(p.string(), 42);
    REQUIRE(sp.sentenceCount() == 1);
    fs::remove_all(d);
}

TEST_CASE("rejects long digit runs", "[style][filter]") {
    auto d = uniqueDir("digits");
    auto p = d / "corpus.txt";
    writeCorpus(p, {
        "call me at 5551234567 anytime soon",
        "he had two or three small dogs at home",
    });
    StyleProfile sp;
    sp.loadFromCorpus(p.string(), 42);
    REQUIRE(sp.sentenceCount() == 1);
    fs::remove_all(d);
}

TEST_CASE("rejects sensitive tokens (password/secret/token/auth)", "[style][filter]") {
    auto d = uniqueDir("sens");
    auto p = d / "corpus.txt";
    writeCorpus(p, {
        "my password is something very secure",
        "the secret to this recipe is fresh basil",
        "an api Token must be refreshed weekly",
        "she had to AUTH before continuing onward",
        "the cat sat on the warm windowsill happily",
    });
    StyleProfile sp;
    sp.loadFromCorpus(p.string(), 42);
    REQUIRE(sp.sentenceCount() == 1);
    fs::remove_all(d);
}

// ── sampling ─────────────────────────────────────────────────────────────────

TEST_CASE("length-varied sampling produces 3-5 exemplars across buckets", "[style][sample]") {
    auto d = uniqueDir("varied");
    auto p = d / "corpus.txt";
    std::vector<std::string> lines;
    // Short bucket [12,49]
    for (int i = 0; i < 20; ++i)
        lines.push_back("short sentence number " + std::to_string(i) + " here");  // ~32 chars
    // Medium bucket [50,100]
    for (int i = 0; i < 20; ++i)
        lines.push_back("a medium length sentence with several common words and number " +
                        std::to_string(i));
    // Long bucket [101,inf)
    for (int i = 0; i < 20; ++i)
        lines.push_back(
            "this is a much longer sentence that contains a variety of words and "
            "several phrases joined together to exceed one hundred characters total " +
            std::to_string(i));
    writeCorpus(p, lines);

    StyleProfile sp;
    sp.loadFromCorpus(p.string(), 42);
    REQUIRE(sp.exemplars().size() >= 3);
    REQUIRE(sp.exemplars().size() <= 5);

    bool seenShort = false, seenMedium = false, seenLong = false;
    for (const auto& e : sp.exemplars()) {
        int n = static_cast<int>(e.size());
        if      (n <= 49)  seenShort  = true;
        else if (n <= 100) seenMedium = true;
        else               seenLong   = true;
    }
    REQUIRE(seenShort);
    REQUIRE(seenMedium);
    REQUIRE(seenLong);
    fs::remove_all(d);
}

TEST_CASE("deterministic with same seed", "[style][sample]") {
    auto d = uniqueDir("det");
    auto p = d / "corpus.txt";
    std::vector<std::string> lines;
    for (int i = 0; i < 50; ++i)
        lines.push_back("sentence number " + std::to_string(i) +
                        " offers something a little different each time around");
    writeCorpus(p, lines);

    StyleProfile a, b;
    a.loadFromCorpus(p.string(), 1234);
    b.loadFromCorpus(p.string(), 1234);
    REQUIRE(a.exemplars() == b.exemplars());
    fs::remove_all(d);
}

TEST_CASE("tiny corpus still produces at least one exemplar", "[style][sample]") {
    auto d = uniqueDir("tiny");
    auto p = d / "corpus.txt";
    writeCorpus(p, {
        "one decent sentence here, please",
        "another short useful sentence",
    });
    StyleProfile sp;
    sp.loadFromCorpus(p.string(), 7);
    REQUIRE_FALSE(sp.exemplars().empty());
    fs::remove_all(d);
}

// ── JSON round-trip ──────────────────────────────────────────────────────────

TEST_CASE("serialize/deserialize round-trip preserves all fields", "[style][json]") {
    auto d = uniqueDir("rt");
    auto cp = d / "corpus.txt";
    writeCorpus(cp, {
        "the quick brown fox jumps over things",
        "the quick brown rabbit hops about freely",
        "she said hello to everyone in the room",
        "she said goodbye when the day was done",
        "now is the moment to make a real choice",
        "hello world this is a longer demo sentence about typing",
        "this longer sentence contains a quoted phrase \"hello world\" inside",
    });
    StyleProfile a;
    a.loadFromCorpus(cp.string(), 99);

    auto jp = d / "style.json";
    REQUIRE(a.serialize(jp.string()));

    StyleProfile b = StyleProfile::deserialize(jp.string());
    REQUIRE(b.exemplars()      == a.exemplars());
    REQUIRE(b.commonOpeners()  == a.commonOpeners());
    REQUIRE(b.avgSentenceLen() == a.avgSentenceLen());
    REQUIRE(b.lastUpdated()    == a.lastUpdated());
    REQUIRE(b.sentenceCount()  == a.sentenceCount());
    fs::remove_all(d);
}

TEST_CASE("corrupt JSON deserialize is safe", "[style][json]") {
    auto d = uniqueDir("corrupt");
    auto jp = d / "style.json";
    {
        std::ofstream out(jp);
        out << "{ this is not valid json at all";
    }
    StyleProfile sp = StyleProfile::deserialize(jp.string());
    REQUIRE(sp.exemplars().empty());
    REQUIRE(sp.avgSentenceLen() == 0);
    fs::remove_all(d);
}

TEST_CASE("missing JSON file deserialize returns empty profile", "[style][json]") {
    auto d = uniqueDir("nojson");
    auto sp = StyleProfile::deserialize((d / "missing.json").string());
    REQUIRE(sp.exemplars().empty());
    REQUIRE(sp.sentenceCount() == 0);
    fs::remove_all(d);
}

// ── preamble ─────────────────────────────────────────────────────────────────

TEST_CASE("empty corpus produces empty preamble", "[style][preamble]") {
    auto d = uniqueDir("empty");
    StyleProfile sp;
    sp.loadFromCorpus((d / "missing.txt").string(), 42);
    REQUIRE(sp.generatePreamble().empty());
    fs::remove_all(d);
}

TEST_CASE("populated profile preamble contains exemplars", "[style][preamble]") {
    auto d = uniqueDir("pre");
    auto p = d / "corpus.txt";
    writeCorpus(p, {
        "this is one perfectly fine sentence here",
        "and a second one of similar length here",
        "third option appears among the candidates",
    });
    StyleProfile sp;
    sp.loadFromCorpus(p.string(), 42);
    auto pre = sp.generatePreamble();
    REQUIRE(pre.find("The user's typical writing style:") != std::string::npos);
    REQUIRE(pre.find("- ") != std::string::npos);
    fs::remove_all(d);
}

// ── isStale ──────────────────────────────────────────────────────────────────

TEST_CASE("isStale returns true when profile missing", "[style][stale]") {
    auto d = uniqueDir("stale_miss");
    auto cp = d / "corpus.txt";
    writeCorpus(cp, {"a perfectly normal sentence about something"});
    auto jp = d / "style.json";
    REQUIRE(StyleProfile::isStale(cp.string(), jp.string()));
    fs::remove_all(d);
}

TEST_CASE("isStale returns false when fresh", "[style][stale]") {
    auto d = uniqueDir("stale_fresh");
    auto cp = d / "corpus.txt";
    writeCorpus(cp, {"one fresh sentence about something nice"});

    StyleProfile sp;
    sp.loadFromCorpus(cp.string(), 42);
    auto jp = d / "style.json";
    REQUIRE(sp.serialize(jp.string()));

    // Profile was just written — corpus is older. Threshold high so 1 line < count+200.
    REQUIRE_FALSE(StyleProfile::isStale(cp.string(), jp.string(),
                                         /*new_lines_threshold=*/200,
                                         /*min_age_seconds=*/3600));
    fs::remove_all(d);
}

// ── common openers ───────────────────────────────────────────────────────────

TEST_CASE("commonOpeners lowercases and counts", "[style][openers]") {
    auto d = uniqueDir("openers");
    auto p = d / "corpus.txt";
    writeCorpus(p, {
        "The quick brown fox runs about gracefully",
        "the quick brown bear lumbers along slowly",
        "the quick rabbit hops across the meadow",
        "She said goodbye when the night came down",
        "she said hello to everyone in the room",
    });
    StyleProfile sp;
    sp.loadFromCorpus(p.string(), 42);
    REQUIRE_FALSE(sp.commonOpeners().empty());
    REQUIRE(sp.commonOpeners().front() == "the quick");
}
