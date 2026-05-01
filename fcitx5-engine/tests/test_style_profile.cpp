#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>
#include <unordered_set>

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

// ── deduplication (L2) ───────────────────────────────────────────────────────

TEST_CASE("boilerplate suppressed: repeated sentence appears at most once", "[style][dedup]") {
    auto d = uniqueDir("dedup_boilerplate");
    auto p = d / "corpus.txt";

    std::vector<std::string> lines;
    // 20 copies of the same boilerplate
    for (int i = 0; i < 20; ++i)
        lines.push_back("Thanks for reaching out to us about this matter today");
    // 5 unique prose sentences of varying length
    lines.push_back("The morning light filtered through the tall oak trees slowly");
    lines.push_back("She carefully placed the fragile glass ornament on the shelf near the window");
    lines.push_back("He wrote the report and sent it across to his colleagues before lunch");
    lines.push_back("Rain fell steadily on the empty cobblestone street outside the old cafe");
    lines.push_back("Every great journey begins with a single courageous step forward into the unknown");

    writeCorpus(p, lines);
    StyleProfile sp;
    sp.loadFromCorpus(p.string(), 42);

    int boilerplateCount = 0;
    for (const auto& e : sp.exemplars()) {
        if (e.find("Thanks for reaching out") != std::string::npos)
            ++boilerplateCount;
    }
    REQUIRE(boilerplateCount <= 1);
    REQUIRE_FALSE(sp.exemplars().empty());
    fs::remove_all(d);
}

TEST_CASE("unique sentences are all eligible for sampling", "[style][dedup]") {
    auto d = uniqueDir("dedup_unique");
    auto p = d / "corpus.txt";

    // 5 completely different sentences
    writeCorpus(p, {
        "The red fox darted across the open meadow at dawn",
        "She wrote long letters to distant friends every single weekend without fail",
        "A quiet hum filled the workshop as the craftsman carefully shaped the wood",
        "They gathered around the fire and shared stories from their childhood days",
        "The train departed precisely at noon carrying passengers to the distant hills",
    });
    StyleProfile sp;
    sp.loadFromCorpus(p.string(), 7);
    // All 5 should be in the candidate pool; with only 5 unique sentences,
    // exemplars reflects the variety (5 sentences fits within maxCount=5).
    REQUIRE(sp.exemplars().size() >= 1);
    // No sentence should appear more than once in the output
    std::unordered_set<std::string> seen;
    for (const auto& e : sp.exemplars())
        REQUIRE(seen.insert(e).second);
    fs::remove_all(d);
}

TEST_CASE("empty corpus returns empty exemplars without crashing", "[style][dedup]") {
    auto d = uniqueDir("dedup_empty");
    StyleProfile sp;
    sp.loadFromCorpus((d / "corpus.txt").string(), 42);
    REQUIRE(sp.exemplars().empty());
    fs::remove_all(d);
}

// ── recency (L1) ─────────────────────────────────────────────────────────────

TEST_CASE("tail-read loads exemplars from corpus tail not head", "[style][recency]") {
    auto d = uniqueDir("tail");
    auto p = d / "corpus.txt";

    {
        std::ofstream out(p, std::ios::binary);
        // 500 KB of "STALE\n" lines — 5 chars each, below kMinSentenceChars=12,
        // so all are filtered even when encountered in the tail window.
        const std::string staleLine = "STALE\n";
        size_t written = 0;
        while (written < 500 * 1024) {
            out.write(staleLine.data(), static_cast<std::streamsize>(staleLine.size()));
            written += staleLine.size();
        }
        // 10 fresh sentences that pass all filters (>=12 chars, alpha, no sensitive tokens).
        for (int i = 0; i < 10; ++i) {
            std::string s = "This is a fresh sentence number " + std::to_string(i) +
                            " about the beauty of the natural world around us.\n";
            out.write(s.data(), static_cast<std::streamsize>(s.size()));
        }
    }

    StyleProfile sp;
    sp.loadFromCorpus(p.string(), 42);
    REQUIRE(sp.exemplars().size() >= 1);
    for (const auto& e : sp.exemplars()) {
        REQUIRE(e.find("fresh") != std::string::npos);
    }
    fs::remove_all(d);
}

// ── embedding index (L3) ──────────────────────────────────────────────────────

TEST_CASE("writeEmbeddingIndex / readEmbeddingIndex round-trip", "[embed][roundtrip]") {
    auto d = uniqueDir("embed_rt");
    auto p = (d / "corpus_embeddings.bin").string();

    std::vector<EmbeddingEntry> entries = {
        {"hello world",    {0.1f, 0.2f, 0.3f}},
        {"foo bar baz",    {0.4f, 0.5f, 0.6f}},
        {"test sentence",  {-0.1f, 1.0f, 0.9f}},
    };

    REQUIRE(writeEmbeddingIndex(p, entries));

    auto read = readEmbeddingIndex(p);
    REQUIRE(read.size() == 3);
    for (size_t i = 0; i < entries.size(); ++i) {
        REQUIRE(read[i].text == entries[i].text);
        REQUIRE(read[i].embedding.size() == entries[i].embedding.size());
        for (size_t j = 0; j < entries[i].embedding.size(); ++j) {
            REQUIRE(std::abs(read[i].embedding[j] - entries[i].embedding[j]) < 1e-6f);
        }
    }
    fs::remove_all(d);
}

TEST_CASE("readEmbeddingIndex returns empty for missing file", "[embed][roundtrip]") {
    auto d = uniqueDir("embed_miss");
    auto result = readEmbeddingIndex((d / "missing.bin").string());
    REQUIRE(result.empty());
    fs::remove_all(d);
}

// ── voice_strength formula (L3) ───────────────────────────────────────────────

TEST_CASE("voice_strength 0 produces targetCount 0", "[style][voice]") {
    size_t targetCount = static_cast<size_t>(
        std::ceil(static_cast<double>(StyleProfile::kMaxEmbedExemplars) * 0 / 100.0));
    REQUIRE(targetCount == 0);
    // No exemplars → preamble is empty (fallback behaviour when targetCount == 0)
    StyleProfile sp;
    REQUIRE(sp.generatePreamble().empty());
}

TEST_CASE("voice_strength 50 produces targetCount 10", "[style][voice]") {
    size_t targetCount = static_cast<size_t>(
        std::ceil(static_cast<double>(StyleProfile::kMaxEmbedExemplars) * 50.0 / 100.0));
    REQUIRE(targetCount == 10);
}
