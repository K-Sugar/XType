#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iterator>
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

TEST_CASE("pruneOldEntries removes entries older than N days", "[corpus][prune]") {
    auto dir = fs::temp_directory_path() /
               ("xtype_test_prune_" +
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(dir);
    auto cp = dir / "corpus.txt";
    auto tp = dir / "corpus_timestamps.txt";

    std::time_t now   = std::time(nullptr);
    std::time_t old   = now - 10 * 86400;  // 10 days ago

    {
        std::ofstream cf(cp);
        std::ofstream tf(tp);
        cf << "old sentence one here\n";   tf << old  << '\n';
        cf << "old sentence two here\n";   tf << old  << '\n';
        cf << "recent sentence three\n";   tf << now  << '\n';
        cf << "recent sentence four\n";    tf << now  << '\n';
    }

    CorpusCollector::pruneOldEntries(cp, 7);

    std::ifstream cf2(cp);
    std::string contents((std::istreambuf_iterator<char>(cf2)),
                          std::istreambuf_iterator<char>());
    REQUIRE(contents.find("old sentence") == std::string::npos);
    REQUIRE(contents.find("recent sentence three") != std::string::npos);
    REQUIRE(contents.find("recent sentence four")  != std::string::npos);

    fs::remove_all(dir);
}

TEST_CASE("pruneOldEntries is no-op when days == 0", "[corpus][prune]") {
    auto dir = fs::temp_directory_path() /
               ("xtype_test_prune0_" +
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(dir);
    auto cp = dir / "corpus.txt";
    auto tp = dir / "corpus_timestamps.txt";

    std::time_t old = std::time(nullptr) - 100 * 86400;
    {
        std::ofstream cf(cp);
        std::ofstream tf(tp);
        cf << "very old sentence here\n"; tf << old << '\n';
    }

    CorpusCollector::pruneOldEntries(cp, 0);

    std::ifstream cf2(cp);
    std::string contents((std::istreambuf_iterator<char>(cf2)),
                          std::istreambuf_iterator<char>());
    REQUIRE(contents.find("very old sentence here") != std::string::npos);

    fs::remove_all(dir);
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

// ── L0: privacy filter tests (§1.4) ─────────────────────────────────────────
// All sensitive strings must be blocked (not appear in corpus.txt).
// All clean prose must be accepted.

// Helper: config with short min_sentence_chars for short test strings.
static LearningConfig makeShortCfg(const fs::path& p) {
    LearningConfig c = makeCfg(p);
    c.min_sentence_chars = 5;
    return c;
}

TEST_CASE("privacy filter rejects email addresses", "[corpus][privacy]") {
    auto p = makeTempCorpusPath("priv_email");
    {
        CorpusCollector c(makeShortCfg(p));
        c.record("My email is user@example.com please reply");
        c.record("The project deadline is next Thursday");
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    }
    auto contents = readAll(p);
    REQUIRE(contents.find("user@example.com") == std::string::npos);
    REQUIRE(contents.find("The project deadline is next Thursday") != std::string::npos);
    fs::remove_all(p.parent_path());
}

TEST_CASE("privacy filter rejects https URLs", "[corpus][privacy]") {
    auto p = makeTempCorpusPath("priv_https");
    {
        CorpusCollector c(makeShortCfg(p));
        c.record("The URL is https://github.com/foo");
        c.record("Please review the attached document");
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    }
    auto contents = readAll(p);
    REQUIRE(contents.find("https://") == std::string::npos);
    REQUIRE(contents.find("Please review the attached document") != std::string::npos);
    fs::remove_all(p.parent_path());
}

TEST_CASE("privacy filter rejects www URLs", "[corpus][privacy]") {
    auto p = makeTempCorpusPath("priv_www");
    {
        CorpusCollector c(makeShortCfg(p));
        c.record("Visit www.google.com for more info");
        c.record("Meeting notes from the quarterly review");
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    }
    auto contents = readAll(p);
    REQUIRE(contents.find("www.google.com") == std::string::npos);
    REQUIRE(contents.find("Meeting notes from the quarterly review") != std::string::npos);
    fs::remove_all(p.parent_path());
}

TEST_CASE("privacy filter rejects phone numbers", "[corpus][privacy]") {
    auto p = makeTempCorpusPath("priv_phone");
    {
        CorpusCollector c(makeShortCfg(p));
        c.record("Call me at 555-867-5309 anytime");
        c.record("I think the new feature looks great");
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    }
    auto contents = readAll(p);
    REQUIRE(contents.find("555-867-5309") == std::string::npos);
    REQUIRE(contents.find("I think the new feature looks great") != std::string::npos);
    fs::remove_all(p.parent_path());
}

TEST_CASE("privacy filter rejects 4-digit PIN", "[corpus][privacy]") {
    auto p = makeTempCorpusPath("priv_pin");
    {
        CorpusCollector c(makeShortCfg(p));
        c.record("My PIN is 1234 keep it safe");
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    }
    auto contents = readAll(p);
    REQUIRE(contents.find("My PIN is 1234") == std::string::npos);
    fs::remove_all(p.parent_path());
}

TEST_CASE("privacy filter rejects password keyword", "[corpus][privacy]") {
    auto p = makeTempCorpusPath("priv_password");
    {
        CorpusCollector c(makeShortCfg(p));
        c.record("Enter your password to continue");
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    }
    auto contents = readAll(p);
    REQUIRE(contents.find("password") == std::string::npos);
    fs::remove_all(p.parent_path());
}

TEST_CASE("privacy filter rejects passphrase keyword", "[corpus][privacy]") {
    auto p = makeTempCorpusPath("priv_passphrase");
    {
        CorpusCollector c(makeShortCfg(p));
        c.record("passphrase: correct horse battery");
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    }
    auto contents = readAll(p);
    REQUIRE(contents.find("passphrase") == std::string::npos);
    fs::remove_all(p.parent_path());
}

TEST_CASE("privacy filter rejects wallet keyword", "[corpus][privacy]") {
    auto p = makeTempCorpusPath("priv_wallet");
    {
        CorpusCollector c(makeShortCfg(p));
        c.record("wallet address 0x1a2b3c");
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    }
    auto contents = readAll(p);
    REQUIRE(contents.find("wallet") == std::string::npos);
    fs::remove_all(p.parent_path());
}

TEST_CASE("privacy filter rejects SSN pattern", "[corpus][privacy]") {
    auto p = makeTempCorpusPath("priv_ssn");
    {
        CorpusCollector c(makeShortCfg(p));
        c.record("my ssn is 123-45-6789");
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    }
    auto contents = readAll(p);
    REQUIRE(contents.find("ssn") == std::string::npos);
    fs::remove_all(p.parent_path());
}

TEST_CASE("privacy filter rejects CVV keyword", "[corpus][privacy]") {
    auto p = makeTempCorpusPath("priv_cvv");
    {
        CorpusCollector c(makeShortCfg(p));
        c.record("cvv 123 expiry 01/26");
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    }
    auto contents = readAll(p);
    REQUIRE(contents.find("cvv") == std::string::npos);
    fs::remove_all(p.parent_path());
}

TEST_CASE("privacy filter rejects bearer token prefix", "[corpus][privacy]") {
    auto p = makeTempCorpusPath("priv_bearer");
    {
        CorpusCollector c(makeShortCfg(p));
        c.record("bearer eyJhbGc...");
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    }
    auto contents = readAll(p);
    REQUIRE(contents.find("bearer") == std::string::npos);
    fs::remove_all(p.parent_path());
}

// ── L0: code-shape extended patterns (§2.3) ──────────────────────────────────

TEST_CASE("code-shape rejects Python range loop", "[corpus][codeshape]") {
    auto p = makeTempCorpusPath("code_python");
    {
        CorpusCollector c(makeShortCfg(p));
        c.record("for i in range(10):");
        c.record("The installation went smoothly");
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    }
    auto contents = readAll(p);
    REQUIRE(contents.find("for i in range") == std::string::npos);
    REQUIRE(contents.find("The installation went smoothly") != std::string::npos);
    fs::remove_all(p.parent_path());
}

TEST_CASE("code-shape rejects SQL SELECT", "[corpus][codeshape]") {
    auto p = makeTempCorpusPath("code_sql");
    {
        CorpusCollector c(makeShortCfg(p));
        c.record("SELECT name FROM users WHERE id = 1");
        c.record("Please select the correct option from the list");
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    }
    auto contents = readAll(p);
    REQUIRE(contents.find("SELECT name FROM") == std::string::npos);
    REQUIRE(contents.find("Please select the correct option from the list") != std::string::npos);
    fs::remove_all(p.parent_path());
}

TEST_CASE("code-shape rejects YAML config block", "[corpus][codeshape]") {
    auto p = makeTempCorpusPath("code_yaml");
    {
        CorpusCollector c(makeShortCfg(p));
        // Two physical lines with embedded newline
        c.record("config:\n  host: localhost");
        c.record("We need to update our configuration soon");
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    }
    auto contents = readAll(p);
    REQUIRE(contents.find("host: localhost") == std::string::npos);
    REQUIRE(contents.find("We need to update our configuration soon") != std::string::npos);
    fs::remove_all(p.parent_path());
}

TEST_CASE("code-shape rejects shell boolean operator", "[corpus][codeshape]") {
    auto p = makeTempCorpusPath("code_shell");
    {
        CorpusCollector c(makeShortCfg(p));
        c.record("git commit -m \"fix\" && git push");
        c.record("The result was better than expected");
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    }
    auto contents = readAll(p);
    REQUIRE(contents.find("&&") == std::string::npos);
    REQUIRE(contents.find("The result was better than expected") != std::string::npos);
    fs::remove_all(p.parent_path());
}

TEST_CASE("code-shape rejects Markdown heading", "[corpus][codeshape]") {
    auto p = makeTempCorpusPath("code_markdown");
    {
        CorpusCollector c(makeShortCfg(p));
        c.record("## Installation");
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    }
    auto contents = readAll(p);
    REQUIRE(contents.find("## Installation") == std::string::npos);
    fs::remove_all(p.parent_path());
}

TEST_CASE("code-shape rejects Python import statement", "[corpus][codeshape]") {
    auto p = makeTempCorpusPath("code_import");
    {
        CorpusCollector c(makeShortCfg(p));
        c.record("import os; os.path.join(a, b)");
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    }
    auto contents = readAll(p);
    REQUIRE(contents.find("import os") == std::string::npos);
    fs::remove_all(p.parent_path());
}

TEST_CASE("code-shape rejects YAML key-value line", "[corpus][codeshape]") {
    auto p = makeTempCorpusPath("code_yamlkv");
    {
        // min_sentence_chars = 5 so "foo: bar" (8 chars) passes length check
        CorpusCollector c(makeShortCfg(p));
        c.record("foo: bar");
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    }
    auto contents = readAll(p);
    REQUIRE(contents.find("foo: bar") == std::string::npos);
    fs::remove_all(p.parent_path());
}

TEST_CASE("code-shape rejects pipe operator", "[corpus][codeshape]") {
    auto p = makeTempCorpusPath("code_pipe");
    {
        CorpusCollector c(makeShortCfg(p));
        c.record("result = func(x) | other(y)");
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    }
    auto contents = readAll(p);
    REQUIRE(contents.find("func(x) | other(y)") == std::string::npos);
    fs::remove_all(p.parent_path());
}
