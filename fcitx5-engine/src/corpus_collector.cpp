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

// Returns true if the string starts with prefix after trimming leading whitespace.
bool startsWithTrimmed(const std::string& s, std::string_view prefix) {
    size_t i = 0;
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
    if (s.size() - i < prefix.size()) return false;
    return s.compare(i, prefix.size(), prefix) == 0;
}

// Check a single physical line for code-shape patterns.
// Returns true if the line looks like code.
bool lineHasCodeShape(const std::string& line) {
    // ── C/C++/Java brace / assignment runs ───────────────────────────────────
    {
        int run = 0;
        for (char c : line) {
            if (c == '{' || c == '}' || c == ';' || c == '=') {
                if (++run >= 2) return true;
            } else {
                run = 0;
            }
        }
    }

    // ── Shell / terminal ──────────────────────────────────────────────────────
    if (line.find("$(") != std::string::npos) return true;
    if (line.find('`') != std::string::npos)  return true;
    if (line.find(" | ") != std::string::npos) return true;
    if (line.find("&&") != std::string::npos) return true;
    if (line.find("||") != std::string::npos) return true;
    if (startsWithTrimmed(line, "$ "))  return true;
    if (startsWithTrimmed(line, "# "))  return true;

    // ── Python / indented blocks ──────────────────────────────────────────────
    // Indented with 4 spaces or a tab followed by non-space
    if (line.size() >= 5 && line[0] == ' ' && line[1] == ' ' &&
        line[2] == ' ' && line[3] == ' ' && !std::isspace(static_cast<unsigned char>(line[4])))
        return true;
    if (!line.empty() && line[0] == '\t' && line.size() >= 2 &&
        !std::isspace(static_cast<unsigned char>(line[1])))
        return true;

    // def func(
    {
        auto pos = line.find("def ");
        if (pos != std::string::npos && line.find('(', pos) != std::string::npos)
            return true;
    }

    // from X import / import X
    if (line.find("from ") != std::string::npos && line.find(" import") != std::string::npos)
        return true;
    if (startsWithTrimmed(line, "import ")) return true;

    // Colon-terminated line (function/if/for/class definition)
    {
        std::string t = trim(line);
        if (t.size() >= 2 && t.back() == ':' && t[t.size()-2] != ':')
            return true;
    }

    // ── YAML / config ─────────────────────────────────────────────────────────
    if (line.find("---") != std::string::npos) return true;
    if (line.find("...") != std::string::npos) return true;
    // Key-value: starts with ^[a-z_]{3,}: (after optional whitespace)
    {
        size_t i = 0;
        while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i]))) ++i;
        size_t key_start = i;
        while (i < line.size() && (std::islower(static_cast<unsigned char>(line[i])) ||
                                   line[i] == '_')) ++i;
        size_t key_len = i - key_start;
        if (key_len >= 3 && i + 1 < line.size() && line[i] == ':' && line[i+1] == ' ')
            return true;
    }

    // ── SQL (uppercase keywords — case-sensitive to avoid prose false positives) ─
    // Real SQL code uses uppercase keywords; prose uses lowercase "select", "from", etc.
    {
        constexpr std::array<std::string_view, 9> kSQL = {
            "SELECT ", "FROM ", "WHERE ", "INSERT INTO", "UPDATE ",
            "DELETE FROM", "CREATE TABLE", "DROP TABLE", "ALTER TABLE"
        };
        for (auto kw : kSQL) {
            if (line.find(kw) != std::string::npos) return true;
        }
    }

    // ── Markdown / markup ─────────────────────────────────────────────────────
    if (startsWithTrimmed(line, "##"))  return true;
    if (startsWithTrimmed(line, " - ")) return true;
    if (startsWithTrimmed(line, "* "))  return true;
    if (line.find("**") != std::string::npos) return true;
    if (line.find("__") != std::string::npos) return true;
    // 3+ backticks (code fence)
    {
        int bt = 0;
        for (char c : line) {
            if (c == '`') { if (++bt >= 3) return true; }
            else bt = 0;
        }
    }

    return false;
}

bool hasCodeShape(const std::string& s) {
    // Apply alpha-ratio check to the whole string (≥ 20 chars).
    if (s.size() >= 20) {
        int alpha = 0, nonws = 0;
        for (unsigned char c : s) {
            if (!std::isspace(c)) {
                ++nonws;
                if (std::isalpha(c)) ++alpha;
            }
        }
        if (nonws > 0 && static_cast<double>(alpha) / nonws < 0.55) return true;
    }

    // Check per physical line (handles embedded newlines).
    std::string line;
    for (char c : s) {
        if (c == '\n') {
            if (lineHasCodeShape(line)) return true;
            line.clear();
        } else {
            line.push_back(c);
        }
    }
    if (!line.empty() && lineHasCodeShape(line)) return true;

    return false;
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

// ── privacy filter ───────────────────────────────────────────────────────────

// Returns true (reject) if the text contains any privacy-sensitive content.
// This is the PRIMARY enforcement gate — every sentence is checked here before
// any disk write. StyleProfile runs its own filters as a second line of defense.
bool CorpusCollector::hasPrivateTerm(const std::string& s) {
    // ── Keyword list (case-insensitive substring) ─────────────────────────────
    {
        static const std::array<std::string_view, 18> kKeywords = {
            "password", "secret", "token", "auth", "credentials", "passphrase",
            "private key", "wallet", "seed phrase", "ssn", "dob", "cvv",
            "api key", "access key", "bearer", "private_key", "secret_key",
            "credit card"
        };
        std::string lower = toLower(s);
        for (auto kw : kKeywords) {
            if (lower.find(kw) != std::string::npos) return true;
        }
    }

    // ── URL patterns ─────────────────────────────────────────────────────────
    if (s.find("https://") != std::string::npos) return true;
    if (s.find("http://")  != std::string::npos) return true;
    if (s.find("www.")     != std::string::npos) return true;
    // Bare domain: [a-z0-9-]+\.[a-z]{2,} flanked by non-alpha (e.g. "github.com")
    {
        for (size_t i = 0; i < s.size(); ) {
            // Find a dot that is not at position 0 or end
            size_t dot = s.find('.', i);
            if (dot == std::string::npos || dot == 0 || dot + 1 >= s.size()) break;

            // Check character before dot: must be alnum or '-' (part of a domain)
            char before = s[dot - 1];
            if (!std::isalnum(static_cast<unsigned char>(before)) && before != '-') {
                i = dot + 1;
                continue;
            }

            // Scan TLD: letters only, 2+ chars
            size_t tld_start = dot + 1;
            size_t tld_end   = tld_start;
            while (tld_end < s.size() &&
                   std::isalpha(static_cast<unsigned char>(s[tld_end]))) ++tld_end;
            size_t tld_len = tld_end - tld_start;
            if (tld_len < 2) { i = dot + 1; continue; }

            // The character after the TLD (if any) must be non-alpha (word boundary)
            if (tld_end < s.size() &&
                std::isalpha(static_cast<unsigned char>(s[tld_end]))) {
                i = dot + 1;
                continue;
            }

            // Scan left for domain label chars
            size_t lhs_end = dot;
            while (lhs_end > 0 &&
                   (std::isalnum(static_cast<unsigned char>(s[lhs_end - 1])) ||
                    s[lhs_end - 1] == '-')) --lhs_end;
            size_t label_len = dot - lhs_end;
            if (label_len < 1) { i = dot + 1; continue; }

            // Character before the label (if any) must be non-alpha (word boundary)
            if (lhs_end > 0 &&
                std::isalpha(static_cast<unsigned char>(s[lhs_end - 1]))) {
                i = dot + 1;
                continue;
            }

            return true;  // Found a bare domain pattern
        }
    }

    // ── Email pattern ────────────────────────────────────────────────────────
    // Port from StyleProfile::containsEmailLike()
    {
        auto at = s.find('@');
        if (at != std::string::npos && at > 0 && at + 1 < s.size()) {
            auto isWs = [](char c) { return std::isspace(static_cast<unsigned char>(c)); };
            if (!isWs(s[at - 1]) && !isWs(s[at + 1])) {
                auto dot = s.find('.', at + 1);
                if (dot != std::string::npos && dot + 1 < s.size() &&
                    !isWs(s[dot - 1]) && !isWs(s[dot + 1])) {
                    return true;
                }
            }
        }
    }

    // ── Phone patterns ───────────────────────────────────────────────────────
    // 7+ consecutive digits (covers international)
    {
        int run = 0;
        for (unsigned char c : s) {
            if (std::isdigit(c)) { if (++run >= 7) return true; }
            else                 { run = 0; }
        }
    }
    // Formatted patterns: NNN-NNN-NNNN  NNN NNN NNNN  +N NNN NNN NNNN
    {
        auto isPhone = [&]() -> bool {
            // Simple scan for digit-separator patterns
            for (size_t i = 0; i + 11 < s.size(); ++i) {
                // NNN-NNN-NNNN
                if (std::isdigit(static_cast<unsigned char>(s[i])) &&
                    std::isdigit(static_cast<unsigned char>(s[i+1])) &&
                    std::isdigit(static_cast<unsigned char>(s[i+2])) &&
                    (s[i+3] == '-' || s[i+3] == ' ') &&
                    std::isdigit(static_cast<unsigned char>(s[i+4])) &&
                    std::isdigit(static_cast<unsigned char>(s[i+5])) &&
                    std::isdigit(static_cast<unsigned char>(s[i+6])) &&
                    (s[i+7] == '-' || s[i+7] == ' ') &&
                    std::isdigit(static_cast<unsigned char>(s[i+8])) &&
                    std::isdigit(static_cast<unsigned char>(s[i+9])) &&
                    std::isdigit(static_cast<unsigned char>(s[i+10])) &&
                    std::isdigit(static_cast<unsigned char>(s[i+11])))
                    return true;
            }
            return false;
        };
        if (isPhone()) return true;
    }

    // ── Short digit run (4+ consecutive digits — PINs, account numbers) ──────
    {
        int run = 0;
        for (unsigned char c : s) {
            if (std::isdigit(c)) { if (++run >= 4) return true; }
            else                 { run = 0; }
        }
    }

    return false;
}

// ── internals ────────────────────────────────────────────────────────────────

bool CorpusCollector::acceptable(const std::string& text) const {
    if (text.empty()) return false;
    if (static_cast<int>(text.size()) < _cfg.min_sentence_chars) return false;
    if (!hasAlpha(text)) return false;
    if (hasCodeShape(text)) return false;
    if (hasPrivateTerm(text)) return false;
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
