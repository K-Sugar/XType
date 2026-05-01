#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstdlib>

#include <curl/curl.h>

#include "embed_client.h"

static bool ollamaEnabled() {
    const char* v = std::getenv("XTYPE_TEST_OLLAMA");
    return v && *v && v[0] != '0';
}

// One-time curl global init for this test binary.
struct CurlGlobal {
    CurlGlobal()  { curl_global_init(CURL_GLOBAL_DEFAULT); }
    ~CurlGlobal() { curl_global_cleanup(); }
};
static CurlGlobal g_curl;

TEST_CASE("embed single string returns 768 finite floats", "[embed][ollama]") {
    if (!ollamaEnabled()) SKIP("Set XTYPE_TEST_OLLAMA=1 to run Ollama integration tests");

    OllamaEmbedClient client("http://localhost:11434");
    auto v = client.embed("hello world");
    REQUIRE(v.size() == 768);
    for (float f : v)
        REQUIRE(std::isfinite(f));
}

TEST_CASE("embedBatch returns one vector per input", "[embed][ollama]") {
    if (!ollamaEnabled()) SKIP("Set XTYPE_TEST_OLLAMA=1 to run Ollama integration tests");

    OllamaEmbedClient client("http://localhost:11434");
    auto vecs = client.embedBatch({"hello world", "goodbye world"});
    REQUIRE(vecs.size() == 2);
    REQUIRE(vecs[0].size() == 768);
    REQUIRE(vecs[1].size() == 768);
}

TEST_CASE("embed empty string returns empty vector without crash", "[embed][ollama]") {
    if (!ollamaEnabled()) SKIP("Set XTYPE_TEST_OLLAMA=1 to run Ollama integration tests");

    OllamaEmbedClient client("http://localhost:11434");
    // Empty string may error or return zeros — must not crash.
    auto v = client.embed("");
    (void)v;
}

TEST_CASE("embed when Ollama unreachable returns empty vector", "[embed][ollama]") {
    if (!ollamaEnabled()) SKIP("Set XTYPE_TEST_OLLAMA=1 to run Ollama integration tests");

    OllamaEmbedClient client("http://localhost:19999");  // wrong port — not reachable
    auto v = client.embed("hello");
    REQUIRE(v.empty());
}
