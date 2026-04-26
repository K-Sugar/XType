#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>

#include "corpus_collector.h"

namespace fs = std::filesystem;

static fs::path makeTempCorpusPath(const std::string& tag) {
    auto dir = fs::temp_directory_path() /
               ("xtype_test_" + tag + "_" +
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(dir);
    return dir / "corpus.txt";
}

static LearningConfig makeCfg(const fs::path& p) {
    LearningConfig c;
    c.enabled            = true;
    c.corpus_path        = p.string();
    c.flush_interval_sec = 1;
    c.max_corpus_mb      = 50;
    c.min_sentence_chars = 12;
    return c;
}

static std::string readAll(const fs::path& p) {
    std::ifstream in(p);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// ── isHardBlocked ────────────────────────────────────────────────────────────

TEST_CASE("isHardBlocked matches password managers case-insensitively", "[corpus][block]") {
    REQUIRE(CorpusCollector::isHardBlocked("keepassxc"));
    REQUIRE(CorpusCollector::isHardBlocked("KeePassXC"));
    REQUIRE(CorpusCollector::isHardBlocked("KEEPASSXC"));
    REQUIRE(CorpusCollector::isHardBlocked("1password"));
    REQUIRE(CorpusCollector::isHardBlocked("1Password"));
    REQUIRE(CorpusCollector::isHardBlocked("bitwarden"));
    REQUIRE(CorpusCollector::isHardBlocked("Bitwarden"));
    REQUIRE(CorpusCollector::isHardBlocked("gnome-keyring"));
    REQUIRE(CorpusCollector::isHardBlocked("seahorse"));
}

TEST_CASE("isHardBlocked matches as substring within program name", "[corpus][block]") {
    REQUIRE(CorpusCollector::isHardBlocked("keepassxc-cli"));
    REQUIRE(CorpusCollector::isHardBlocked("/usr/bin/seahorse"));
}

TEST_CASE("isHardBlocked rejects non-sensitive apps", "[corpus][block]") {
    REQUIRE_FALSE(CorpusCollector::isHardBlocked("kate"));
    REQUIRE_FALSE(CorpusCollector::isHardBlocked("firefox"));
    REQUIRE_FALSE(CorpusCollector::isHardBlocked(""));
    REQUIRE_FALSE(CorpusCollector::isHardBlocked("textfield"));
}

// ── ctor / dtor lifecycle ────────────────────────────────────────────────────

TEST_CASE("constructor + destructor without records does not deadlock", "[corpus][lifecycle]") {
    auto p = makeTempCorpusPath("noop");
    {
        CorpusCollector c(makeCfg(p));
        REQUIRE_FALSE(c.disabled());
    }
    fs::remove_all(p.parent_path());
}

TEST_CASE("HOME-less path with literal ~ disables collector", "[corpus][lifecycle]") {
    LearningConfig cfg;
    cfg.enabled     = true;
    cfg.corpus_path = "";  // empty path → disabled
    CorpusCollector c(cfg);
    REQUIRE(c.disabled());
    c.record("this should be a noop on a disabled collector");
}

// ── filtering ────────────────────────────────────────────────────────────────

TEST_CASE("record drops too-short sentences", "[corpus][filter]") {
    auto p = makeTempCorpusPath("short");
    {
        CorpusCollector c(makeCfg(p));
        c.record("too short");                         // 9 chars
        c.record("this is long enough to keep");       // 27 chars
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    }
    auto contents = readAll(p);
    REQUIRE(contents.find("too short") == std::string::npos);
    REQUIRE(contents.find("this is long enough to keep") != std::string::npos);
    fs::remove_all(p.parent_path());
}

TEST_CASE("record drops code-shape sentences", "[corpus][filter]") {
    auto p = makeTempCorpusPath("code");
    {
        CorpusCollector c(makeCfg(p));
        c.record("a={};b={};c=1");                          // code shape
        c.record("she walked across the room slowly");      // prose
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    }
    auto contents = readAll(p);
    REQUIRE(contents.find("a={}") == std::string::npos);
    REQUIRE(contents.find("she walked across the room slowly") != std::string::npos);
    fs::remove_all(p.parent_path());
}

TEST_CASE("record drops sentences with no alpha chars", "[corpus][filter]") {
    auto p = makeTempCorpusPath("noalpha");
    {
        CorpusCollector c(makeCfg(p));
        c.record("123 456 789 000 111");               // digits + spaces only
        c.record("...... ...... ......");              // punctuation only
        c.record("hello world this is a sentence");    // prose
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    }
    auto contents = readAll(p);
    REQUIRE(contents.find("123") == std::string::npos);
    REQUIRE(contents.find("......") == std::string::npos);
    REQUIRE(contents.find("hello world this is a sentence") != std::string::npos);
    fs::remove_all(p.parent_path());
}

// ── write + read round-trip ──────────────────────────────────────────────────

TEST_CASE("multiple records appear in the file", "[corpus][write]") {
    auto p = makeTempCorpusPath("write");
    {
        CorpusCollector c(makeCfg(p));
        c.record("the quick brown fox jumps over");
        c.record("she sells seashells by the shore");
        c.record("now is the time for all good men");
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    }
    auto contents = readAll(p);
    REQUIRE(contents.find("the quick brown fox jumps over") != std::string::npos);
    REQUIRE(contents.find("she sells seashells by the shore") != std::string::npos);
    REQUIRE(contents.find("now is the time for all good men") != std::string::npos);

    // Each entry must end with a newline.
    int newlines = 0;
    for (char ch : contents) if (ch == '\n') ++newlines;
    REQUIRE(newlines >= 3);

    fs::remove_all(p.parent_path());
}

TEST_CASE("destructor flushes pending entries", "[corpus][shutdown]") {
    auto p = makeTempCorpusPath("flush");
    {
        // flush_interval is long; rely on the destructor's final flush.
        LearningConfig cfg = makeCfg(p);
        cfg.flush_interval_sec = 60;
        CorpusCollector c(cfg);
        c.record("destruction must flush this entry to disk");
    }
    auto contents = readAll(p);
    REQUIRE(contents.find("destruction must flush this entry to disk") != std::string::npos);
    fs::remove_all(p.parent_path());
}

TEST_CASE("parent directory is created if missing", "[corpus][fs]") {
    auto base = fs::temp_directory_path() /
                ("xtype_test_mkdir_" +
                 std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    auto deep = base / "a" / "b" / "c" / "corpus.txt";
    REQUIRE_FALSE(fs::exists(deep.parent_path()));
    {
        CorpusCollector c(makeCfg(deep));
        c.record("ensure that nested parent dirs are created on construction");
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    }
    REQUIRE(fs::exists(deep));
    fs::remove_all(base);
}
