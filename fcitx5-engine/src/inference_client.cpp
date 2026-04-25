#include "inference_client.h"

#include <curl/curl.h>
#include <cstdio>
#include <string_view>

static constexpr char SYSTEM_PROMPT[] =
    "Output ONLY the completion text, no explanation. "
    "5-15 words max. Stop at sentence boundaries.";

// ── JSON helpers ──────────────────────────────────────────────────────────────

static std::string json_escape(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;
        }
    }
    return out;
}

// Extract a JSON string value for `key` from a flat JSON object.
// Returns empty string if not found.
static std::string json_str(std::string_view json, std::string_view key) {
    std::string needle;
    needle.reserve(key.size() + 4);
    needle += '"'; needle += key; needle += "\":\"";
    auto pos = json.find(needle);
    if (pos == std::string_view::npos) return {};
    pos += needle.size();
    std::string result;
    while (pos < json.size()) {
        char c = json[pos++];
        if (c == '"') break;
        if (c == '\\' && pos < json.size()) {
            char e = json[pos++];
            switch (e) {
                case '"':  result += '"';  break;
                case '\\': result += '\\'; break;
                case 'n':  result += '\n'; break;
                case 't':  result += '\t'; break;
                case 'r':  result += '\r'; break;
                default:   result += e;
            }
        } else {
            result += c;
        }
    }
    return result;
}

static bool json_bool(std::string_view json, std::string_view key) {
    std::string needle;
    needle += '"'; needle += key; needle += "\":true";
    return json.find(needle) != std::string_view::npos;
}

// ── Payload builder ───────────────────────────────────────────────────────────

static std::string build_payload(const InferenceConfig& cfg, const std::string& context) {
    std::string stop_arr = "[";
    for (size_t i = 0; i < cfg.stop_tokens.size(); ++i) {
        if (i) stop_arr += ',';
        stop_arr += '"';
        stop_arr += json_escape(cfg.stop_tokens[i]);
        stop_arr += '"';
    }
    stop_arr += ']';

    char temp_buf[16], top_p_buf[16];
    std::snprintf(temp_buf,  sizeof(temp_buf),  "%.4g", static_cast<double>(cfg.temperature));
    std::snprintf(top_p_buf, sizeof(top_p_buf), "%.4g", static_cast<double>(cfg.top_p));

    return std::string(R"({"model":")") + json_escape(cfg.model)
         + R"(","prompt":")"  + json_escape(context)
         + R"(","system":")"  + json_escape(SYSTEM_PROMPT)
         + R"(","stream":true,"options":{"num_predict":)"
         + std::to_string(cfg.num_predict)
         + R"(,"temperature":)" + temp_buf
         + R"(,"top_p":)"      + top_p_buf
         + R"(,"stop":)"       + stop_arr + "}}";
}

// ── CURL write callbacks ──────────────────────────────────────────────────────

struct WriteState {
    std::string                    line_buf;
    uint64_t                       my_gen;
    std::atomic<uint64_t>&         cur_gen;
    const InferenceClient::TokenCb& on_token;

    WriteState(uint64_t g, std::atomic<uint64_t>& cg, const InferenceClient::TokenCb& cb)
        : my_gen(g), cur_gen(cg), on_token(cb) {}
};

static size_t stream_cb(char* ptr, size_t /*size*/, size_t nmemb, void* ud) {
    auto* s = static_cast<WriteState*>(ud);
    // Returning 0 aborts curl with CURLE_WRITE_ERROR — our cancel signal.
    if (s->my_gen != s->cur_gen.load(std::memory_order_relaxed)) return 0;

    s->line_buf.append(ptr, nmemb);

    size_t start = 0, nl;
    while ((nl = s->line_buf.find('\n', start)) != std::string::npos) {
        std::string_view line(s->line_buf.data() + start, nl - start);
        start = nl + 1;
        if (line.empty()) continue;
        auto token = json_str(line, "response");
        if (!token.empty()) s->on_token(std::move(token));
    }
    s->line_buf.erase(0, start);
    return nmemb;
}

static size_t collect_cb(char* ptr, size_t /*size*/, size_t nmemb, void* ud) {
    static_cast<std::string*>(ud)->append(ptr, nmemb);
    return nmemb;
}

// ── InferenceClient ───────────────────────────────────────────────────────────

InferenceClient::InferenceClient(InferenceConfig cfg) : _cfg(std::move(cfg)) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    _thread = std::thread(&InferenceClient::run, this);
}

InferenceClient::~InferenceClient() {
    ++_gen;  // abort any in-flight request
    {
        std::lock_guard<std::mutex> lk(_mutex);
        _shutdown.store(true);
        _pending.reset();
    }
    _cv.notify_one();
    if (_thread.joinable()) _thread.join();
    curl_global_cleanup();
}

void InferenceClient::request(std::string context, TokenCb on_token,
                              DoneCb on_done, ErrCb on_error) {
    uint64_t gen = ++_gen;
    {
        std::lock_guard<std::mutex> lk(_mutex);
        _pending = Req{std::move(context), gen,
                       std::move(on_token), std::move(on_done), std::move(on_error)};
    }
    _cv.notify_one();
}

void InferenceClient::cancel() {
    ++_gen;
    {
        std::lock_guard<std::mutex> lk(_mutex);
        _pending.reset();
    }
}

bool InferenceClient::health_check() {
    CURL* curl = curl_easy_init();
    if (!curl) return false;

    std::string body;
    std::string url = _cfg.ollama_host + "/api/tags";
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, collect_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 2L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) return false;
    // Search for our model name in the response body.
    return body.find('"' + _cfg.model + '"') != std::string::npos;
}

void InferenceClient::run() {
    while (true) {
        std::optional<Req> req;
        {
            std::unique_lock<std::mutex> lk(_mutex);
            _cv.wait(lk, [this] { return _pending.has_value() || _shutdown.load(); });
            if (_shutdown.load() && !_pending.has_value()) break;
            req = std::move(_pending);
            _pending.reset();
        }
        if (req) execute(*req);
    }
}

void InferenceClient::execute(Req& req) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        req.on_error("curl_easy_init failed");
        return;
    }

    std::string payload = build_payload(_cfg, req.context);
    std::string url     = _cfg.ollama_host + "/api/generate";

    WriteState ws(req.gen, _gen, req.on_token);

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL,           url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST,           1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS,     payload.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE,  static_cast<long>(payload.size()));
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER,     headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,  stream_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,      &ws);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 2L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,        30L);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    // CURLE_WRITE_ERROR means stream_cb returned 0 — i.e. we were cancelled.
    bool cancelled = (req.gen != _gen.load());
    if (!cancelled) {
        if (res == CURLE_OK)
            req.on_done();
        else
            req.on_error(curl_easy_strerror(res));
    }
}
