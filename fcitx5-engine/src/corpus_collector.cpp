#include "corpus_collector.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <ctime>
#include <fstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

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

void CorpusCollector::pruneOldEntries(const std::filesystem::path& corpus_path, int days) {
    if (days <= 0) return;

    namespace fs = std::filesystem;
    auto stem = corpus_path.stem().string();
    fs::path tp = corpus_path.parent_path() / (stem + "_timestamps.txt");

    std::error_code ec;
    if (!fs::exists(tp, ec)) return;

    std::ifstream cf(corpus_path);
    std::ifstream tf(tp);
    if (!cf || !tf) return;

    std::vector<std::string> lines;
    std::vector<std::time_t> times;
    std::string line;
    while (std::getline(cf, line)) lines.push_back(line);
    while (std::getline(tf, line)) {
        try { times.push_back(static_cast<std::time_t>(std::stoll(line))); }
        catch (...) { times.push_back(0); }
    }

    std::time_t cutoff = std::time(nullptr) - static_cast<std::time_t>(days) * 86400;
    std::vector<std::string> kept_lines;
    std::vector<std::time_t> kept_times;
    for (size_t i = 0; i < lines.size(); ++i) {
        std::time_t ts = i < times.size() ? times[i] : 0;
        if (ts == 0 || ts >= cutoff) {
            kept_lines.push_back(lines[i]);
            kept_times.push_back(ts);
        }
    }

    // No pruning needed — skip the atomic writes.
    if (kept_lines.size() == lines.size()) return;

    auto write_atomic = [](const fs::path& dst, const auto& vec, auto to_str) {
        fs::path tmp = dst.string() + ".tmp";
        std::ofstream f(tmp);
        if (!f) return;
        for (const auto& v : vec) f << to_str(v) << '\n';
        f.close();
        std::error_code ec2;
        fs::rename(tmp, dst, ec2);
    };
    write_atomic(corpus_path, kept_lines, [](const std::string& s) { return s; });
    write_atomic(tp, kept_times, [](std::time_t t) { return std::to_string(t); });
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

    namespace fs = std::filesystem;
    auto tsPath = _path.parent_path() / (_path.stem().string() + "_timestamps.txt");
    std::ofstream tsOut(tsPath, std::ios::app);
    std::time_t now = std::time(nullptr);

    for (auto& line : drained) {
        out.write(line.data(), static_cast<std::streamsize>(line.size()));
        out.put('\n');
        if (tsOut) tsOut << now << '\n';
    }
    out.flush();
    if (tsOut) tsOut.flush();

    if (_log) {
        std::error_code ec;
        auto sz = fs::file_size(_path, ec);
        std::string msg = "flush: " + std::to_string(drained.size()) +
                          " entries written, file=" +
                          (ec ? std::string("?") : std::to_string(sz)) + " bytes";
        _log(std::move(msg));
    }

    pruneOldEntries(_path, _cfg.forget_after_days);
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
    if (!ec && _log) {
        _log("rotate: " + _path.filename().string() + " -> " +
             rotated.filename().string() + " (was " + std::to_string(sz) + " bytes)");
    }
}
