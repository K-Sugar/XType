#include <catch2/catch_test_macros.hpp>
#include "phrase_blocklist.h"

TEST_CASE("empty list never matches") {
    PhraseBlocklist bl;
    bl.load({});
    CHECK(!bl.matches("anything"));
}

TEST_CASE("exact literal match") {
    PhraseBlocklist bl;
    bl.load({"hereinafter", "pursuant to"});
    CHECK(bl.matches("The term hereinafter refers"));
    CHECK(bl.matches("Pursuant to section 4"));     // case-insensitive
    CHECK(!bl.matches("unrelated text"));
}

TEST_CASE("substring match") {
    PhraseBlocklist bl;
    bl.load({"foo"});
    CHECK(bl.matches("foobar"));
    CHECK(bl.matches("barfoo"));
    CHECK(!bl.matches("bar"));
}

TEST_CASE("empty phrase in list is skipped") {
    PhraseBlocklist bl;
    bl.load({"", "ok"});
    CHECK(!bl.matches("anything"));   // empty phrase must not always-match
    CHECK(bl.matches("this is ok"));
}

TEST_CASE("reload clears old phrases") {
    PhraseBlocklist bl;
    bl.load({"alpha"});
    CHECK(bl.matches("alpha"));
    bl.load({"beta"});
    CHECK(!bl.matches("alpha"));
    CHECK(bl.matches("beta"));
}
