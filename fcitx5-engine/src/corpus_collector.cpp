#include "corpus_collector.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <fstream>
#include <system_error>
#include <utility>

#include "path_utils.h"

namespace {

constexpr std::array<std::string_view, 5> kAlwaysBlocked = {
    "keepassxc", "1password", "bitwarden", "gnome-keyring", "seahorse"
};

std::string toLower(std::string_view s) {
    std::string out(s);
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return out;
}

bool hasCodeShape(const std::string& s) {
    int run = 0;
    for (char c : s) {
        if (c == '{' || c == '}' || c == ';' || c == '=') {
            if (++run >= 2) return true;
        } else {
            run = 0;
        }
    }
    return false;
}

bool hasAlpha(const std::string& s) {
    for (unsigned char c : s)
        if (std::isalpha(c)) return true;
    return false;
}

std::string trim(std::string s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.erase(0, 1);
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))  s.pop_back();
    return s;
}

}  // namespace

// ── ctor / dtor ──────────────────────────────────────────────────────────────

CorpusCollector::CorpusCollector(LearningConfig cfg)
    : _cfg(std::move(cfg))
{
    if (_cfg.corpus_path.empty()) { _disabled = true; return; }
    _path = path_utils::expandTilde(_cfg.corpus_path);
    // path_utils returns the input as-is if HOME is unresolvable; detect that.
    if (!_path.empty() && _path.string().front() == '~') {
        _disabled = true;
        return;
    }

    std::error_code ec;
    if (!_path.parent_path().empty())
        std::filesystem::create_directories(_path.parent_path(), ec);
    // ec is ignored: directory may already exist; fatal errors surface on first flush

    _thread = std::thread([this] { run(); });
}

CorpusCollector::~CorpusCollector() {
    if (_disabled) return;
    {
        std::lock_guard<std::mutex> lk(_mu);
        _shutdown = true;
    }
    _cv.notify_all();
    if (_thread.joinable()) _thread.join();
}

// ── public api ───────────────────────────────────────────────────────────────

void CorpusCollector::record(std::string text) {
    if (_disabled) return;
    text = trim(std::move(text));
    if (!acceptable(text)) return;

    {
        std::lock_guard<std::mutex> lk(_mu);
        if (_queue.size() >= kQueueCap) _queue.pop_front();
        _queue.push_back(std::move(text));
        if (_queue.size() >= kEagerFlushAt) _cv.notify_all();
    }
}

bool CorpusCollector::isHardBlocked(std::string_view program) {
    std::string lower = toLower(program);
    for (auto needle : kAlwaysBlocked) {
        if (lower.find(needle) != std::string::npos) return true;
    }
    return false;
}

// ── internals ────────────────────────────────────────────────────────────────

bool CorpusCollector::acceptable(const std::string& text) const {
    if (text.empty()) return false;
    if (static_cast<int>(text.size()) < _cfg.min_sentence_chars) return false;
    if (!hasAlpha(text)) return false;
    if (hasCodeShape(text)) return false;
    return true;
}

void CorpusCollector::run() {
    using namespace std::chrono;
    while (true) {
        std::deque<std::string> drained;
        bool stopping = false;
        {
            std::unique_lock<std::mutex> lk(_mu);
            _cv.wait_for(lk, seconds(_cfg.flush_interval_sec), [this] {
                return _shutdown.load() || _queue.size() >= kEagerFlushAt;
            });
            stopping = _shutdown.load();
            drained.swap(_queue);
        }
        if (!drained.empty()) flushLocked(drained);
        rotateIfNeeded();
        if (stopping) break;
    }
}

void CorpusCollector::flushLocked(std::deque<std::string>& drained) {
    std::ofstream out(_path, std::ios::app | std::ios::binary);
    if (!out) return;
    for (auto& line : drained) {
        out.write(line.data(), static_cast<std::streamsize>(line.size()));
        out.put('\n');
    }
}

void CorpusCollector::rotateIfNeeded() {
    std::error_code ec;
    auto sz = std::filesystem::file_size(_path, ec);
    if (ec) return;
    const auto cap = static_cast<std::uintmax_t>(_cfg.max_corpus_mb) * 1024ull * 1024ull;
    if (sz <= cap) return;

    auto rotated = _path;
    rotated += ".1";
    std::filesystem::rename(_path, rotated, ec);
    // Next flush re-creates _path via ofstream(append).
}
