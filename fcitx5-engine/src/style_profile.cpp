#include "style_profile.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <random>
#include <sstream>
#include <string_view>
#include <system_error>

#include "path_utils.h"

namespace {

constexpr size_t kMaxScannedSentences = 1000;
constexpr int    kMinSentenceChars    = 12;
constexpr int    kBucketShortMax      = 49;   // [12, 49]
constexpr int    kBucketMediumMax     = 100;  // [50, 100], long is [101, ∞)
constexpr int    kMaxStaleScanBytes   = 5 * 1024 * 1024;
constexpr int    kProfileSchemaVer    = 1;

// ── small utils ──────────────────────────────────────────────────────────────

std::string toLower(std::string_view s) {
    std::string out(s);
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return out;
}

std::string trim(std::string s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.erase(0, 1);
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))  s.pop_back();
    return s;
}

bool hasAlpha(const std::string& s) {
    for (unsigned char c : s)
        if (std::isalpha(c)) return true;
    return false;
}

// ── privacy filters ──────────────────────────────────────────────────────────

bool containsEmailLike(const std::string& s) {
    auto at = s.find('@');
    if (at == std::string::npos || at == 0 || at + 1 >= s.size()) return false;
    auto isWs = [](char c) { return std::isspace(static_cast<unsigned char>(c)); };
    if (isWs(s[at - 1]) || isWs(s[at + 1])) return false;
    auto dot = s.find('.', at + 1);
    if (dot == std::string::npos || dot + 1 >= s.size()) return false;
    if (isWs(s[dot - 1]) || isWs(s[dot + 1])) return false;
    return true;
}

bool hasLongDigitRun(const std::string& s) {
    int run = 0;
    for (unsigned char c : s) {
        if (std::isdigit(c)) { if (++run >= 5) return true; }
        else                 { run = 0; }
    }
    return false;
}

bool containsSensitive(const std::string& s) {
    static const std::array<std::string_view, 4> kNeedles = {
        "password", "secret", "token", "auth"
    };
    std::string lower = toLower(s);
    for (auto needle : kNeedles)
        if (lower.find(needle) != std::string::npos) return true;
    return false;
}

// Per-sentence: only length + alpha. Privacy checks run line-level (below).
bool acceptableSentence(const std::string& s) {
    if (static_cast<int>(s.size()) < kMinSentenceChars) return false;
    if (!hasAlpha(s))           return false;
    return true;
}

// Whole-line privacy check. Runs before sentence splitting so emails and
// other patterns that contain internal '.' aren't broken apart.
bool lineHasPrivacyLeak(const std::string& line) {
    if (containsEmailLike(line)) return true;
    if (hasLongDigitRun(line))   return true;
    if (containsSensitive(line)) return true;
    return false;
}

// ── sentence splitting ───────────────────────────────────────────────────────

void appendSentencesFromLine(const std::string& line, std::vector<std::string>& out) {
    if (lineHasPrivacyLeak(line)) return;
    std::string cur;
    cur.reserve(line.size());
    for (char c : line) {
        if (c == '.' || c == '!' || c == '?' || c == '\n') {
            auto t = trim(std::move(cur));
            if (acceptableSentence(t)) out.push_back(std::move(t));
            cur.clear();
            if (out.size() >= kMaxScannedSentences) return;
        } else {
            cur.push_back(c);
        }
    }
    auto t = trim(std::move(cur));
    if (acceptableSentence(t)) out.push_back(std::move(t));
}

// ── exemplar sampler ─────────────────────────────────────────────────────────

std::vector<std::string> pickExemplars(std::vector<std::string> pool, unsigned seed) {
    std::vector<std::string> result;
    if (pool.empty()) return result;

    std::vector<std::string> shortB, mediumB, longB;
    for (auto& s : pool) {
        int n = static_cast<int>(s.size());
        if      (n <= kBucketShortMax)  shortB.push_back(std::move(s));
        else if (n <= kBucketMediumMax) mediumB.push_back(std::move(s));
        else                            longB.push_back(std::move(s));
    }

    std::mt19937 rng(seed == 0 ? static_cast<unsigned>(std::time(nullptr)) : seed);
    auto pickOne = [&](std::vector<std::string>& bucket) {
        if (bucket.empty()) return;
        std::uniform_int_distribution<size_t> dist(0, bucket.size() - 1);
        size_t idx = dist(rng);
        result.push_back(std::move(bucket[idx]));
        bucket.erase(bucket.begin() + idx);
    };

    pickOne(shortB);
    pickOne(mediumB);
    pickOne(longB);

    // Fill up to 5 from the largest remaining bucket each round.
    while (result.size() < 5) {
        std::vector<std::string>* b = &shortB;
        if (mediumB.size() > b->size()) b = &mediumB;
        if (longB.size()   > b->size()) b = &longB;
        if (b->empty()) break;
        pickOne(*b);
    }

    return result;
}

// ── common openers ───────────────────────────────────────────────────────────

std::vector<std::string> computeCommonOpeners(const std::vector<std::string>& sentences) {
    std::map<std::string, int> counts;
    for (const auto& s : sentences) {
        std::istringstream iss(s);
        std::string w1, w2;
        if (!(iss >> w1)) continue;
        if (!(iss >> w2)) continue;
        counts[toLower(w1) + " " + toLower(w2)] += 1;
    }
    std::vector<std::pair<std::string, int>> sorted(counts.begin(), counts.end());
    std::sort(sorted.begin(), sorted.end(),
              [](const auto& a, const auto& b) {
                  if (a.second != b.second) return a.second > b.second;
                  return a.first < b.first;
              });
    std::vector<std::string> out;
    for (const auto& [k, _] : sorted) {
        if (out.size() >= 10) break;
        out.push_back(k);
    }
    return out;
}

// ── JSON helpers ─────────────────────────────────────────────────────────────

std::string jsonEscape(std::string_view s) {
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
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    return out;
}

std::string jsonUnescape(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (c == '\\' && i + 1 < s.size()) {
            char e = s[++i];
            switch (e) {
                case '"':  out += '"';  break;
                case '\\': out += '\\'; break;
                case 'n':  out += '\n'; break;
                case 'r':  out += '\r'; break;
                case 't':  out += '\t'; break;
                case 'u':
                    // Skip 4 hex digits (corpus is ASCII; we never emit non-ASCII escapes
                    // for content, so any \uXXXX must be a control char we wrote).
                    if (i + 4 < s.size()) {
                        unsigned v = 0;
                        for (int k = 0; k < 4; ++k) {
                            char h = s[++i];
                            v <<= 4;
                            if      (h >= '0' && h <= '9') v |= (h - '0');
                            else if (h >= 'a' && h <= 'f') v |= (h - 'a' + 10);
                            else if (h >= 'A' && h <= 'F') v |= (h - 'A' + 10);
                        }
                        if (v < 0x80) out += static_cast<char>(v);
                    }
                    break;
                default:   out += e;
            }
        } else {
            out += c;
        }
    }
    return out;
}

// Find a string value for a given top-level key. Returns false on miss.
bool findStringValue(std::string_view json, std::string_view key, std::string& out) {
    std::string needle;
    needle.reserve(key.size() + 4);
    needle += '"'; needle += key; needle += "\":\"";
    auto pos = json.find(needle);
    if (pos == std::string_view::npos) return false;
    pos += needle.size();
    std::string acc;
    while (pos < json.size()) {
        char c = json[pos++];
        if (c == '\\' && pos < json.size()) { acc += c; acc += json[pos++]; continue; }
        if (c == '"') { out = jsonUnescape(acc); return true; }
        acc += c;
    }
    return false;
}

// Find an integer value for a given top-level key.
bool findIntValue(std::string_view json, std::string_view key, long long& out) {
    std::string needle;
    needle.reserve(key.size() + 3);
    needle += '"'; needle += key; needle += "\":";
    auto pos = json.find(needle);
    if (pos == std::string_view::npos) return false;
    pos += needle.size();
    while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) ++pos;
    size_t start = pos;
    if (pos < json.size() && (json[pos] == '-' || json[pos] == '+')) ++pos;
    while (pos < json.size() && std::isdigit(static_cast<unsigned char>(json[pos]))) ++pos;
    if (pos == start) return false;
    try {
        out = std::stoll(std::string(json.substr(start, pos - start)));
        return true;
    } catch (...) { return false; }
}

// Find an array of strings for a given top-level key.
bool findStringArray(std::string_view json, std::string_view key, std::vector<std::string>& out) {
    std::string needle;
    needle.reserve(key.size() + 3);
    needle += '"'; needle += key; needle += "\":";
    auto pos = json.find(needle);
    if (pos == std::string_view::npos) return false;
    pos += needle.size();
    while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) ++pos;
    if (pos >= json.size() || json[pos] != '[') return false;
    ++pos;
    while (pos < json.size()) {
        while (pos < json.size() &&
               (std::isspace(static_cast<unsigned char>(json[pos])) || json[pos] == ',')) ++pos;
        if (pos >= json.size() || json[pos] == ']') break;
        if (json[pos] != '"') return false;
        ++pos;
        std::string acc;
        while (pos < json.size()) {
            char c = json[pos++];
            if (c == '\\' && pos < json.size()) { acc += c; acc += json[pos++]; continue; }
            if (c == '"') break;
            acc += c;
        }
        out.push_back(jsonUnescape(acc));
    }
    return true;
}

}  // namespace

// ── public API ───────────────────────────────────────────────────────────────

std::vector<std::string> StyleProfile::loadSeedExemplars(const std::string& path) {
    std::vector<std::string> result;
    std::ifstream in(path);
    if (!in) return result;
    std::stringstream ss;
    ss << in.rdbuf();
    std::string json = ss.str();
    long long version = 0;
    if (!findIntValue(json, "version", version) || version != 1) return result;
    findStringArray(json, "exemplars", result);
    return result;
}

void StyleProfile::loadFromCorpus(const std::string& corpus_path, unsigned seed) {
    _exemplars.clear();
    _openers.clear();
    _avgChars   = 0;
    _count      = 0;
    _lastUpdated = 0;

    auto expandedPath = path_utils::expandTilde(corpus_path);

    // Load rotation seed exemplars first — must run even if corpus.txt is absent
    // (e.g., immediately after rotation renames corpus.txt to corpus.txt.1).
    auto seedPath = expandedPath.parent_path() / "corpus_seed.json";
    auto seedExemplars = loadSeedExemplars(seedPath.string());

    std::ifstream in(expandedPath);

    std::vector<std::string> kept;
    if (in) {
        kept.reserve(256);
        std::string line;
        while (std::getline(in, line) && kept.size() < kMaxScannedSentences) {
            appendSentencesFromLine(line, kept);
        }
    }

    if (kept.empty() && seedExemplars.empty()) return;

    if (!kept.empty()) {
        long long sum = 0;
        for (const auto& s : kept) sum += static_cast<long long>(s.size());
        _avgChars = static_cast<int>(sum / static_cast<long long>(kept.size()));
        _count    = static_cast<int>(kept.size());
    }

    // Sort by length, drop longest 5% and shortest 5%.
    std::vector<std::string> sorted = kept;
    std::sort(sorted.begin(), sorted.end(),
              [](const auto& a, const auto& b) { return a.size() < b.size(); });
    size_t drop = sorted.size() / 20;
    std::vector<std::string> pool;
    if (sorted.size() > 2 * drop) {
        pool.assign(sorted.begin() + drop, sorted.end() - drop);
    } else {
        pool = std::move(sorted);
    }

    // Prepend seed sentences to the candidate pool so they are eligible for sampling.
    // They are real corpus sentences (already privacy-filtered) from before rotation.
    pool.insert(pool.begin(), seedExemplars.begin(), seedExemplars.end());

    _exemplars   = pickExemplars(std::move(pool), seed);
    _openers     = computeCommonOpeners(kept);
    _lastUpdated = std::time(nullptr);
}

std::string StyleProfile::generatePreamble() const {
    if (_exemplars.empty()) return {};
    std::string out = "The user's typical writing style:\n";
    for (const auto& e : _exemplars) {
        out += "- ";
        out += e;
        out += '\n';
    }
    if (!out.empty() && out.back() == '\n') out.pop_back();
    return out;
}

bool StyleProfile::serialize(const std::string& path) const {
    auto p = path_utils::expandTilde(path);
    std::error_code ec;
    if (!p.parent_path().empty())
        std::filesystem::create_directories(p.parent_path(), ec);

    std::ofstream out(p, std::ios::trunc);
    if (!out) return false;

    out << "{\n";
    out << "  \"version\": " << kProfileSchemaVer << ",\n";
    out << "  \"last_updated\": " << static_cast<long long>(_lastUpdated) << ",\n";
    out << "  \"avg_sentence_chars\": " << _avgChars << ",\n";
    out << "  \"sentence_count\": " << _count << ",\n";

    out << "  \"exemplars\": [";
    for (size_t i = 0; i < _exemplars.size(); ++i) {
        if (i) out << ", ";
        out << "\"" << jsonEscape(_exemplars[i]) << "\"";
    }
    out << "],\n";

    out << "  \"common_openers\": [";
    for (size_t i = 0; i < _openers.size(); ++i) {
        if (i) out << ", ";
        out << "\"" << jsonEscape(_openers[i]) << "\"";
    }
    out << "]\n";
    out << "}\n";
    return static_cast<bool>(out);
}

StyleProfile StyleProfile::deserialize(const std::string& path) {
    StyleProfile sp;
    std::ifstream in(path_utils::expandTilde(path));
    if (!in) return sp;

    std::stringstream ss;
    ss << in.rdbuf();
    std::string json = ss.str();

    long long version = 0;
    if (!findIntValue(json, "version", version) || version != kProfileSchemaVer)
        return StyleProfile{};

    long long lu = 0, avg = 0, cnt = 0;
    findIntValue(json, "last_updated",       lu);
    findIntValue(json, "avg_sentence_chars", avg);
    findIntValue(json, "sentence_count",     cnt);
    sp._lastUpdated = static_cast<std::time_t>(lu);
    sp._avgChars    = static_cast<int>(avg);
    sp._count       = static_cast<int>(cnt);

    findStringArray(json, "exemplars",      sp._exemplars);
    findStringArray(json, "common_openers", sp._openers);
    return sp;
}

bool StyleProfile::isStale(const std::string& corpus_path,
                           const std::string& profile_path,
                           int new_lines_threshold,
                           int min_age_seconds) {
    auto cp = path_utils::expandTilde(corpus_path);
    auto pp = path_utils::expandTilde(profile_path);
    std::error_code ec;
    if (!std::filesystem::exists(pp, ec)) return true;

    StyleProfile sp = deserialize(pp.string());

    if (std::filesystem::exists(cp, ec)) {
        auto cmt = std::filesystem::last_write_time(cp, ec);
        if (!ec) {
            // Compare via system_clock conversion: avoid file_clock vs. time_t mismatch.
            auto sysNow = std::chrono::system_clock::now();
            auto fileNow = std::filesystem::file_time_type::clock::now();
            auto cmt_sys = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                cmt - fileNow + sysNow);
            std::time_t cmt_t = std::chrono::system_clock::to_time_t(cmt_sys);
            if (cmt_t > sp.lastUpdated() + min_age_seconds) return true;
        }

        // Fast '\n' count over the first kMaxStaleScanBytes of the corpus.
        std::ifstream cin(cp, std::ios::binary);
        if (cin) {
            std::vector<char> buf(64 * 1024);
            int newlines = 0;
            int read_total = 0;
            while (cin && read_total < kMaxStaleScanBytes) {
                cin.read(buf.data(), static_cast<std::streamsize>(buf.size()));
                auto got = cin.gcount();
                if (got <= 0) break;
                read_total += static_cast<int>(got);
                newlines += static_cast<int>(
                    std::count(buf.data(), buf.data() + got, '\n'));
            }
            if (newlines > sp.sentenceCount() + new_lines_threshold) return true;
        } else {
            return true;  // can't read corpus → defensive refresh
        }
    }
    return false;
}
