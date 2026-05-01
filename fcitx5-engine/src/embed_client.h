#pragma once
#include <string>
#include <vector>

class OllamaEmbedClient {
public:
    explicit OllamaEmbedClient(std::string host);

    // Returns empty vector on any failure (HTTP error, parse error, empty input).
    std::vector<float> embed(const std::string& text);

    // Returns one vector per input. A failed item yields an empty vector at that index.
    std::vector<std::vector<float>> embedBatch(const std::vector<std::string>& texts);

private:
    std::string _host;

    static size_t collectCb(char* ptr, size_t size, size_t nmemb, void* ud);
    std::vector<std::vector<float>> doRequest(const std::string& body);
};
