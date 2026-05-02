#include <catch2/catch_test_macros.hpp>
#include <string>

// Mirrors kMaxLineBuf + the overflow check in stream_cb (inference_client.cpp).
// stream_cb is static in the TU, so the logic is tested via a local replica.
static constexpr size_t kMaxLineBuf = 65536;

static size_t stream_cb_overflow(std::string& line_buf, const char* ptr, size_t nmemb) {
    line_buf.append(ptr, nmemb);
    if (line_buf.size() > kMaxLineBuf)
        return 0;
    return nmemb;
}

TEST_CASE("stream_cb aborts when line_buf exceeds 64 KB") {
    std::string line_buf;
    std::string chunk(1024, 'x');  // 1 KB, no newlines

    size_t result = chunk.size();
    while (result == chunk.size()) {
        result = stream_cb_overflow(line_buf, chunk.data(), chunk.size());
    }

    CHECK(result == 0);
    CHECK(line_buf.size() > 65536);
}
