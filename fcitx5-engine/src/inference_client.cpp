#include "inference_client.h"

#include <curl/curl.h>
#include <algorithm>
#include <charconv>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <string_view>

static FILE *ic_logfile() {
    static FILE *f = std::fopen("/home/saint/Desktop/XType/fcitx5-engine/thread.log", "w");
    return f;
}
static void iclog(const char *fmt, ...) {
    FILE *f = ic_logfile();
    if (!f) return;
    va_list ap; va_start(ap, fmt); std::vfprintf(f, fmt, ap); va_end(ap);
    std::fputc('\n', f); std::fflush(f);
}

static constexpr char kBaseSystemPrompt[] =
    "You are an inline text autocomplete engine. Continue the text you are given with a "
    "few natural words. Output ONLY the continuation. No explanations, no responses, "
    "no punctuation at the start.";

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

static std::string build_payload(const InferenceConfig& cfg,
                                 const std::string& context,
                                 const std::string& system_prompt) {
    std::string stop_arr = "[";
    for (size_t i = 0; i < cfg.stop_tokens.size(); ++i) {
        if (i) stop_arr += ',';
        stop_arr += '"';
        stop_arr += json_escape(cfg.stop_tokens[i]);
        stop_arr += '"';
    }
    stop_arr += ']';

    // std::to_chars is locale-independent — snprintf respects LC_NUMERIC and would
    // emit "0,3" instead of "0.3" under European locales, breaking Ollama's JSON parser.
    char temp_buf[32] = {}, top_p_buf[32] = {};
    {
        auto r = std::to_chars(temp_buf, temp_buf + sizeof(temp_buf) - 1,
                               static_cast<double>(cfg.temperature),
                               std::chars_format::general, 6);
        *r.ptr = '\0';
    }
    {
        auto r = std::to_chars(top_p_buf, top_p_buf + sizeof(top_p_buf) - 1,
                               static_cast<double>(cfg.top_p),
                               std::chars_format::general, 6);
        *r.ptr = '\0';
    }

    // Assistant-prefill: context is placed in an incomplete assistant turn.
    // The model continues its own text, bypassing the chat-response pattern that
    // fires when conversational text appears in the user role.
    return std::string(R"({"model":")") + json_escape(cfg.model)
         + R"(","messages":[)"
         + R"({"role":"system","content":")"    + json_escape(system_prompt) + R"("},)"
         + R"({"role":"assistant","content":")" + json_escape(context) + R"("})"
         + R"(],"stream":true,"options":{"num_predict":)"
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
    if (s->my_gen != s->cur_gen.load(std::memory_order_relaxed))
        return 0;

    s->line_buf.append(ptr, nmemb);

    size_t start = 0, nl;
    while ((nl = s->line_buf.find('\n', start)) != std::string::npos) {
        std::string_view line(s->line_buf.data() + start, nl - start);
        start = nl + 1;
        if (line.empty()) continue;
        // /api/chat emits {"message":{"role":"assistant","content":"token"},...}
        // json_str flat-searches for "content":" regardless of nesting depth.
        auto token = json_str(line, "content");
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

InferenceClient::InferenceClient(InferenceConfig cfg)
    : _cfg(std::move(cfg)),
      _system_prompt(kBaseSystemPrompt)
{
    curl_global_init(CURL_GLOBAL_DEFAULT);
    _thread = std::thread(&InferenceClient::run, this);
}

void InferenceClient::set_system_prompt(std::string s) {
    std::lock_guard<std::mutex> lk(_mutex);
    if (s == _system_prompt) {
        iclog("[prompt] set: no-op (unchanged, %zu chars)", _system_prompt.size());
        return;
    }
    size_t was = _system_prompt.size();
    _system_prompt = std::move(s);
    iclog("[prompt] set %zu chars (was %zu)", _system_prompt.size(), was);
    if (const char* v = std::getenv("XTYPE_DEBUG_VERBOSE"); v && *v && v[0] != '0') {
        std::string head = _system_prompt.substr(0, std::min<size_t>(200, _system_prompt.size()));
        iclog("[prompt] verbose head='%s'", head.c_str());
    }
}

std::string_view InferenceClient::base_system_prompt() {
    return std::string_view(kBaseSystemPrompt);
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
    iclog("inference thread started");
    while (true) {
        std::optional<Req> req;
        {
            std::unique_lock<std::mutex> lk(_mutex);
            _cv.wait(lk, [this] { return _pending.has_value() || _shutdown.load(); });
            if (_shutdown.load() && !_pending.has_value()) break;
            req = std::move(_pending);
            _pending.reset();
        }
        if (req) {
            iclog("run: dispatching gen=%llu", (unsigned long long)req->gen);
            execute(*req);
        }
    }
    iclog("inference thread exiting");
}

void InferenceClient::execute(Req& req) {
    iclog("execute: gen=%llu ctx='%.40s'", (unsigned long long)req.gen, req.context.c_str());
    CURL* curl = curl_easy_init();
    if (!curl) {
        iclog("execute: curl_easy_init failed");
        req.on_error("curl_easy_init failed");
        return;
    }

    std::string prompt_snapshot;
    {
        std::lock_guard<std::mutex> lk(_mutex);
        prompt_snapshot = _system_prompt;  // frozen for this request
    }

    std::string payload = build_payload(_cfg, req.context, prompt_snapshot);
    std::string url     = _cfg.ollama_host + "/api/chat";

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

    // Flush any data not terminated by \n (last chunk from Ollama may omit it).
    if (!ws.line_buf.empty()) {
        iclog("execute: flushing line_buf='%.200s'", ws.line_buf.c_str());
        auto token = json_str(ws.line_buf, "content");
        if (!token.empty()) req.on_token(std::move(token));
        ws.line_buf.clear();
    }

    // CURLE_WRITE_ERROR means stream_cb returned 0 — i.e. we were cancelled.
    bool cancelled = (req.gen != _gen.load());
    if (!cancelled) {
        if (res == CURLE_OK)
            req.on_done();
        else
            req.on_error(curl_easy_strerror(res));
    }
}
