#include <catch2/catch_test_macros.hpp>
#include "context_buffer.h"

static ContextBuffer filled(const char* chars) {
    ContextBuffer cb;
    for (const char* p = chars; *p; ++p) cb.appendChar(*p);
    return cb;
}

// ── contextText ───────────────────────────────────────────────────────────────

TEST_CASE("contextText empty", "[context_text]") {
    REQUIRE(ContextBuffer().contextText() == "");
}

TEST_CASE("contextText after typing", "[context_text]") {
    REQUIRE(filled("hello").contextText() == "hello");
}

TEST_CASE("contextText after acceptNextWord", "[context_text]") {
    auto cb = filled("type ");
    cb.setSuggestion("fast");
    cb.acceptNextWord();
    REQUIRE(cb.contextText() == "type fast");
}

TEST_CASE("contextText after acceptAll", "[context_text]") {
    auto cb = filled("type ");
    cb.setSuggestion("fast now");
    cb.acceptAll();
    REQUIRE(cb.contextText() == "type fast now");
}

// ── appendChar ────────────────────────────────────────────────────────────────

TEST_CASE("appendChar grows buffer", "[append_char]") {
    ContextBuffer cb;
    cb.appendChar('a');
    cb.appendChar('b');
    REQUIRE(cb.contextText() == "ab");
}

TEST_CASE("appendChar dismisses active suggestion", "[append_char]") {
    ContextBuffer cb;
    cb.setSuggestion("hello world");
    cb.appendChar('x');
    REQUIRE_FALSE(cb.hasSuggestion());
    REQUIRE(cb.contextText() == "x");
}

// ── backspace ─────────────────────────────────────────────────────────────────

TEST_CASE("backspace removes last char", "[backspace]") {
    auto cb = filled("abc");
    cb.backspace();
    REQUIRE(cb.contextText() == "ab");
}

TEST_CASE("backspace on empty buffer no crash", "[backspace]") {
    ContextBuffer cb;
    cb.backspace();
    REQUIRE(cb.contextText() == "");
}

TEST_CASE("backspace with suggestion dismisses only", "[backspace]") {
    auto cb = filled("hi");
    cb.setSuggestion("there");
    cb.backspace();
    REQUIRE_FALSE(cb.hasSuggestion());
    REQUIRE(cb.contextText() == "hi");
}

TEST_CASE("backspace without suggestion removes char", "[backspace]") {
    auto cb = filled("hi");
    cb.backspace();
    REQUIRE(cb.contextText() == "h");
}

// ── setSuggestion / dismiss / hasSuggestion ───────────────────────────────────

TEST_CASE("setSuggestion stores text", "[suggestion]") {
    ContextBuffer cb;
    cb.setSuggestion("hello world");
    REQUIRE(cb.suggestion() != nullptr);
    REQUIRE(*cb.suggestion() == "hello world");
    REQUIRE(cb.hasSuggestion());
}

TEST_CASE("setSuggestion empty string clears", "[suggestion]") {
    ContextBuffer cb;
    cb.setSuggestion("something");
    cb.setSuggestion("");
    REQUIRE_FALSE(cb.hasSuggestion());
    REQUIRE(cb.suggestion() == nullptr);
}

TEST_CASE("setSuggestion replaces previous", "[suggestion]") {
    ContextBuffer cb;
    cb.setSuggestion("first");
    cb.setSuggestion("second");
    REQUIRE(*cb.suggestion() == "second");
}

TEST_CASE("dismiss clears suggestion", "[suggestion]") {
    ContextBuffer cb;
    cb.setSuggestion("hello");
    cb.dismiss();
    REQUIRE_FALSE(cb.hasSuggestion());
}

TEST_CASE("dismiss on no suggestion no crash", "[suggestion]") {
    ContextBuffer cb;
    cb.dismiss();
    REQUIRE_FALSE(cb.hasSuggestion());
}

TEST_CASE("dismiss does not modify buffer", "[suggestion]") {
    auto cb = filled("ab");
    cb.setSuggestion("cd");
    cb.dismiss();
    REQUIRE(cb.contextText() == "ab");
}

// ── acceptNextWord ────────────────────────────────────────────────────────────

TEST_CASE("acceptNextWord single word no space", "[accept_next_word]") {
    ContextBuffer cb;
    cb.setSuggestion("hello");
    REQUIRE(cb.acceptNextWord() == "hello");
    REQUIRE_FALSE(cb.hasSuggestion());
}

TEST_CASE("acceptNextWord returns word with trailing space", "[accept_next_word]") {
    ContextBuffer cb;
    cb.setSuggestion("quick brown fox");
    REQUIRE(cb.acceptNextWord() == "quick ");
}

TEST_CASE("acceptNextWord advances through suggestion", "[accept_next_word]") {
    ContextBuffer cb;
    cb.setSuggestion("one two three");
    REQUIRE(cb.acceptNextWord() == "one ");
    REQUIRE(cb.acceptNextWord() == "two ");
    REQUIRE(cb.acceptNextWord() == "three");
    REQUIRE_FALSE(cb.hasSuggestion());
}

TEST_CASE("acceptNextWord appends to buffer", "[accept_next_word]") {
    auto cb = filled("start ");
    cb.setSuggestion("here now");
    cb.acceptNextWord();
    REQUIRE(cb.contextText() == "start here ");
}

TEST_CASE("acceptNextWord exhausts suggestion on last word", "[accept_next_word]") {
    ContextBuffer cb;
    cb.setSuggestion("last");
    cb.acceptNextWord();
    REQUIRE(cb.suggestion() == nullptr);
    REQUIRE_FALSE(cb.hasSuggestion());
}

TEST_CASE("acceptNextWord no suggestion returns empty", "[accept_next_word]") {
    REQUIRE(ContextBuffer().acceptNextWord() == "");
}

TEST_CASE("acceptNextWord full sequence appended to buffer", "[accept_next_word]") {
    ContextBuffer cb;
    cb.setSuggestion("the lazy dog");
    cb.acceptNextWord();
    cb.acceptNextWord();
    cb.acceptNextWord();
    REQUIRE(cb.contextText() == "the lazy dog");
}

// ── acceptAll ─────────────────────────────────────────────────────────────────

TEST_CASE("acceptAll returns full remaining suggestion", "[accept_all]") {
    ContextBuffer cb;
    cb.setSuggestion("quick brown fox");
    REQUIRE(cb.acceptAll() == "quick brown fox");
}

TEST_CASE("acceptAll clears suggestion", "[accept_all]") {
    ContextBuffer cb;
    cb.setSuggestion("something");
    cb.acceptAll();
    REQUIRE_FALSE(cb.hasSuggestion());
}

TEST_CASE("acceptAll appends to buffer", "[accept_all]") {
    auto cb = filled("a ");
    cb.setSuggestion("big idea");
    cb.acceptAll();
    REQUIRE(cb.contextText() == "a big idea");
}

TEST_CASE("acceptAll no suggestion returns empty", "[accept_all]") {
    REQUIRE(ContextBuffer().acceptAll() == "");
}

TEST_CASE("acceptAll after partial accept", "[accept_all]") {
    ContextBuffer cb;
    cb.setSuggestion("one two three");
    cb.acceptNextWord();  // consumes "one "
    REQUIRE(cb.acceptAll() == "two three");
    REQUIRE(cb.contextText() == "one two three");
    REQUIRE_FALSE(cb.hasSuggestion());
}

// ── Buffer overflow (maxlen=500) ──────────────────────────────────────────────

TEST_CASE("buffer respects maxlen", "[overflow]") {
    ContextBuffer cb;
    for (int i = 0; i < 600; ++i) cb.appendChar('x');
    REQUIRE(cb.contextText().size() == 500u);
}

TEST_CASE("buffer drops oldest chars", "[overflow]") {
    ContextBuffer cb;
    for (int i = 0; i < 500; ++i) cb.appendChar('a');
    cb.appendChar('z');
    auto t = cb.contextText();
    REQUIRE(t.size() == 500u);
    REQUIRE(t.back() == 'z');
    REQUIRE(t.front() == 'a');
}

TEST_CASE("acceptAll respects maxlen", "[overflow]") {
    ContextBuffer cb;
    for (int i = 0; i < 490; ++i) cb.appendChar('a');
    cb.setSuggestion(std::string(20, 'x'));
    cb.acceptAll();
    REQUIRE(cb.contextText().size() == 500u);
}

// ── reset ─────────────────────────────────────────────────────────────────────

TEST_CASE("reset clears buffer", "[reset]") {
    auto cb = filled("abc");
    cb.reset();
    REQUIRE(cb.contextText() == "");
}

TEST_CASE("reset clears suggestion", "[reset]") {
    ContextBuffer cb;
    cb.setSuggestion("hello world");
    cb.reset();
    REQUIRE_FALSE(cb.hasSuggestion());
    REQUIRE(cb.suggestion() == nullptr);
}

TEST_CASE("reset clears both", "[reset]") {
    auto cb = filled("type ");
    cb.setSuggestion("this");
    cb.reset();
    REQUIRE(cb.contextText() == "");
    REQUIRE_FALSE(cb.hasSuggestion());
}

TEST_CASE("reset on empty buffer no crash", "[reset]") {
    ContextBuffer cb;
    cb.reset();
    REQUIRE(cb.contextText() == "");
    REQUIRE_FALSE(cb.hasSuggestion());
}

TEST_CASE("reset allows fresh use", "[reset]") {
    auto cb = filled("hello");
    cb.setSuggestion("world");
    cb.reset();
    cb.appendChar('x');
    REQUIRE(cb.contextText() == "x");
    REQUIRE_FALSE(cb.hasSuggestion());
}
