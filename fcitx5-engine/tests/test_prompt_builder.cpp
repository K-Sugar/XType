#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

#include "prompt_builder.h"
#include "style_profile.h"

namespace fs = std::filesystem;

static fs::path uniqueDir(const std::string& tag) {
    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    auto d = fs::temp_directory_path() /
             ("xtype_pb_" + tag + "_" + std::to_string(now) + "_" + std::to_string(getpid()));
    fs::create_directories(d);
    return d;
}

static StyleProfile profileWith(std::vector<std::string> exemplars) {
    auto d = uniqueDir("profile");
    auto p = d / "corpus.txt";
    {
        std::ofstream out(p);
        for (const auto& e : exemplars) out << e << '\n';
    }
    StyleProfile sp;
    sp.loadFromCorpus(p.string(), 1);
    fs::remove_all(d);
    return sp;
}

static const std::string kBase =
    "You are an inline text autocomplete engine. Continue the text you are given with a few "
    "natural words. Output ONLY the continuation. No explanations, no responses, no punctuation "
    "at the start.";

// ── basic shape ──────────────────────────────────────────────────────────────

TEST_CASE("base only — no headers when nothing else provided", "[prompt][shape]") {
    PromptInputs in{};
    in.base = kBase;
    in.budgetChars = 2000;
    auto out = buildSystemPrompt(in);
    REQUIRE(out == kBase);
}

TEST_CASE("nullptr profile is safe", "[prompt][shape]") {
    PromptInputs in{};
    in.base    = kBase;
    in.profile = nullptr;
    in.includeExamples = true;
    in.budgetChars     = 2000;
    auto out = buildSystemPrompt(in);
    REQUIRE(out.find("typical writing style") == std::string::npos);
}

TEST_CASE("user description renders About line", "[prompt][user]") {
    PromptInputs in{};
    in.base            = kBase;
    in.userDescription = "I am a software engineer who writes concise prose.";
    in.budgetChars     = 2000;
    auto out = buildSystemPrompt(in);
    REQUIRE(out.find("About the user: I am a software engineer") != std::string::npos);
}

TEST_CASE("avoid_phrases renders comma-joined", "[prompt][avoid]") {
    PromptInputs in{};
    in.base         = kBase;
    in.avoidPhrases = {"hope this helps", "feel free to"};
    in.budgetChars  = 2000;
    auto out = buildSystemPrompt(in);
    REQUIRE(out.find("Avoid these phrases: hope this helps, feel free to") != std::string::npos);
}

// ── exemplars ────────────────────────────────────────────────────────────────

TEST_CASE("exemplars appear when includeExamples=true and profile has any", "[prompt][exemplars]") {
    auto sp = profileWith({
        "the quick brown fox jumps over many things",
        "she sells seashells by the seashore today",
        "now is the time for all good men to act",
    });
    REQUIRE_FALSE(sp.exemplars().empty());

    PromptInputs in{};
    in.base            = kBase;
    in.profile         = &sp;
    in.includeExamples = true;
    in.budgetChars     = 2000;
    auto out = buildSystemPrompt(in);
    REQUIRE(out.find("typical writing style") != std::string::npos);
    REQUIRE(out.find("- ") != std::string::npos);
}

TEST_CASE("includeExamples=false suppresses style header", "[prompt][exemplars]") {
    auto sp = profileWith({
        "the quick brown fox jumps over many things",
        "she sells seashells by the seashore today",
        "now is the time for all good men to act",
    });
    PromptInputs in{};
    in.base            = kBase;
    in.profile         = &sp;
    in.includeExamples = false;
    in.budgetChars     = 2000;
    auto out = buildSystemPrompt(in);
    REQUIRE(out.find("typical writing style") == std::string::npos);
}

// ── budget cap ───────────────────────────────────────────────────────────────

TEST_CASE("over-budget drops longest exemplar first", "[prompt][budget]") {
    auto sp = profileWith({
        "short one fits",
        std::string(1900, 'x') + " is a very long exemplar that should be dropped",
        "another shortish one stays",
    });
    PromptInputs in{};
    in.base            = kBase;
    in.profile         = &sp;
    in.includeExamples = true;
    in.budgetChars     = 1500;
    bool truncated = false;
    auto out = buildSystemPrompt(in, &truncated);
    REQUIRE(truncated);
    REQUIRE(out.size() <= 1500);
    REQUIRE(out.find("xxxxxxxxxx") == std::string::npos);
}

TEST_CASE("truncation flag false when within budget", "[prompt][budget]") {
    auto sp = profileWith({
        "the quick brown fox jumps over many things",
        "she sells seashells by the seashore today",
    });
    PromptInputs in{};
    in.base            = kBase;
    in.profile         = &sp;
    in.includeExamples = true;
    in.budgetChars     = 2000;
    bool truncated = true;  // start true to verify it's reset
    (void)buildSystemPrompt(in, &truncated);
    REQUIRE_FALSE(truncated);
}

TEST_CASE("user description appears in prompt") {
    PromptInputs in;
    in.base = "Base prompt.";
    in.userDescription = "A software engineer.";
    in.budgetChars = 2000;
    auto p = buildSystemPrompt(in);
    REQUIRE(p.find("A software engineer.") != std::string::npos);
}

TEST_CASE("avoid phrases appear in prompt") {
    PromptInputs in;
    in.base = "Base.";
    in.avoidPhrases = {"synergy", "leverage"};
    in.budgetChars = 2000;
    auto p = buildSystemPrompt(in);
    REQUIRE(p.find("synergy") != std::string::npos);
    REQUIRE(p.find("leverage") != std::string::npos);
}

TEST_CASE("prompt respects budget") {
    PromptInputs in;
    in.base = std::string(1800, 'x');
    in.userDescription = std::string(300, 'y');
    in.budgetChars = 2000;
    bool truncated = false;
    auto p = buildSystemPrompt(in, &truncated);
    REQUIRE(p.size() <= 2000);
    REQUIRE(truncated);
}

TEST_CASE("exemplarsOverride takes precedence over profile") {
    PromptInputs in;
    in.base = "Base.";
    in.exemplarsOverride = {"only this exemplar"};
    in.budgetChars = 2000;
    auto p = buildSystemPrompt(in);
    REQUIRE(p.find("only this exemplar") != std::string::npos);
}

TEST_CASE("drops all exemplars before truncating description", "[prompt][budget]") {
    auto sp = profileWith({
        std::string(400, 'a') + " sentence ends here",
        std::string(400, 'b') + " sentence ends here",
        std::string(400, 'c') + " sentence ends here",
    });
    PromptInputs in{};
    in.base            = kBase;
    in.profile         = &sp;
    in.includeExamples = true;
    in.userDescription = "user description that should survive even when exemplars are dropped";
    in.budgetChars     = kBase.size() + 200;  // tight: forces all exemplar drops
    bool truncated = false;
    auto out = buildSystemPrompt(in, &truncated);
    REQUIRE(truncated);
    REQUIRE(out.find("typical writing style") == std::string::npos);
    REQUIRE(out.find("About the user:") != std::string::npos);
}
