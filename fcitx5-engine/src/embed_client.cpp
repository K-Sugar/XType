#include "embed_client.h"

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <string_view>

#include <curl/curl.h>

// ── helpers ───────────────────────────────────────────────────────────────────

static std::string embedJsonEscape(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x",
                                  static_cast<unsigned char>(c));
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    return out;
}

// Parse {"embeddings":[[f,f,...],[f,f,...],...],...} using strtof.
// Returns one float vector per inner array. Malformed → empty outer vector.
static std::vector<std::vector<float>> parseEmbeddings(const std::string& json) {
    std::vector<std::vector<float>> result;

    auto pos = json.find("\"embeddings\":");
    if (pos == std::string::npos) return result;
    pos += sizeof("\"embeddings\":") - 1;

    while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) ++pos;
    if (pos >= json.size() || json[pos] != '[') return result;
    ++pos;

    while (pos < json.size()) {
        while (pos < json.size() &&
               (std::isspace(static_cast<unsigned char>(json[pos])) || json[pos] == ','))
            ++pos;
        if (pos >= json.size() || json[pos] == ']') break;
        if (json[pos] != '[') break;
        ++pos;

        std::vector<float> vec;
        while (pos < json.size()) {
            while (pos < json.size() &&
                   (std::isspace(static_cast<unsigned char>(json[pos])) || json[pos] == ','))
                ++pos;
            if (pos >= json.size() || json[pos] == ']') break;

            char* end = nullptr;
            float val = std::strtof(json.c_str() + pos, &end);
            if (!end || end == json.c_str() + pos) break;
            vec.push_back(val);
            pos = static_cast<size_t>(end - json.c_str());
        }
        if (pos < json.size() && json[pos] == ']') ++pos;
        result.push_back(std::move(vec));
    }
    return result;
}

// ── OllamaEmbedClient ─────────────────────────────────────────────────────────

OllamaEmbedClient::OllamaEmbedClient(std::string host) : _host(std::move(host)) {}

size_t OllamaEmbedClient::collectCb(char* ptr, size_t /*size*/, size_t nmemb, void* ud) {
    static_cast<std::string*>(ud)->append(ptr, nmemb);
    return nmemb;
}

std::vector<std::vector<float>> OllamaEmbedClient::doRequest(const std::string& body) {
    CURL* curl = curl_easy_init();
    if (!curl) return {};

    std::string response;
    std::string url = _host + "/api/embed";

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL,            url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST,            1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS,      body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE,   static_cast<long>(body.size()));
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER,      headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,   collectCb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,       &response);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT,  2L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,         30L);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) return {};
    return parseEmbeddings(response);
}

std::vector<float> OllamaEmbedClient::embed(const std::string& text) {
    auto vecs = embedBatch({text});
    if (vecs.empty()) return {};
    return std::move(vecs[0]);
}

std::vector<std::vector<float>> OllamaEmbedClient::embedBatch(
    const std::vector<std::string>& texts)
{
    if (texts.empty()) return {};

    std::string body = "{\"model\":\"nomic-embed-text\",\"input\":[";
    for (size_t i = 0; i < texts.size(); ++i) {
        if (i) body += ',';
        body += '"';
        body += embedJsonEscape(texts[i]);
        body += '"';
    }
    body += "]}";

    auto vecs = doRequest(body);

    // Ensure we return exactly one entry per input (empty for failures).
    vecs.resize(texts.size());
    return vecs;
}
