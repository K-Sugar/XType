#include "style_profile.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <random>
#include <sstream>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <unordered_set>

#include "embed_client.h"
#include "path_utils.h"

namespace {

constexpr size_t      kMaxScannedSentences  = 1000;
constexpr int         kMinSentenceChars     = 12;
constexpr int         kBucketShortMax       = 49;   // [12, 49]
constexpr int         kBucketMediumMax      = 100;  // [50, 100], long is [101, ∞)
constexpr int         kMaxStaleScanBytes    = 5 * 1024 * 1024;
constexpr int         kProfileSchemaVer     = 1;
constexpr std::streamsize kTailScanBytes    = 256 * 1024; // 256 KB
constexpr size_t      kMaxIndexedSentences  = 2000;
constexpr size_t      kEmbedBatchSize       = 100;

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

std::vector<std::string> pickExemplars(
    const std::vector<std::string>& sentences,
    const std::vector<double>&      weights,
    size_t                          maxCount,
    unsigned                        seed = 0)
{
    std::vector<std::string> result;
    if (sentences.empty()) return result;

    struct Bucket { std::vector<size_t> indices; std::vector<double> weights; };
    Bucket shortB, medB, longB;

    for (size_t i = 0; i < sentences.size(); ++i) {
        size_t len = sentences[i].size();
        double w   = weights[i];
        if      (len <= kBucketShortMax)  { shortB.indices.push_back(i); shortB.weights.push_back(w); }
        else if (len <= kBucketMediumMax) { medB.indices.push_back(i);   medB.weights.push_back(w); }
        else                              { longB.indices.push_back(i);  longB.weights.push_back(w); }
    }

    std::mt19937 rng(seed == 0 ? static_cast<unsigned>(std::time(nullptr)) : seed);

    auto pickOne = [&](Bucket& b) -> std::optional<std::string> {
        if (b.indices.empty()) return std::nullopt;
        std::discrete_distribution<size_t> dist(b.weights.begin(), b.weights.end());
        size_t pos    = dist(rng);
        size_t chosen = b.indices[pos];
        b.indices.erase(b.indices.begin() + pos);
        b.weights.erase(b.weights.begin() + pos);
        return sentences[chosen];
    };

    if (auto s = pickOne(shortB)) result.push_back(std::move(*s));
    if (auto s = pickOne(medB))   result.push_back(std::move(*s));
    if (auto s = pickOne(longB))  result.push_back(std::move(*s));

    while (result.size() < maxCount) {
        Bucket* b = &shortB;
        if (medB.indices.size() > b->indices.size()) b = &medB;
        if (longB.indices.size() > b->indices.size()) b = &longB;
        if (b->indices.empty()) break;
        if (auto s = pickOne(*b)) result.push_back(std::move(*s));
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

// ── seed loader ──────────────────────────────────────────────────────────────

std::vector<std::string> StyleProfile::loadSeedExemplars(const std::string& path) {
    std::vector<std::string> result;
    std::ifstream in(path);
    if (!in) return result;
    std::stringstream ss;
    ss << in.rdbuf();
    std::string json = ss.str();

    long long version = 0;
    if (!findIntValue(json, "version", version) || version != 1)
        return result;

    findStringArray(json, "exemplars", result);
    return result;
}

// ── public API ───────────────────────────────────────────────────────────────

void StyleProfile::loadFromCorpus(const std::string& corpus_path, unsigned seed) {
    _exemplars.clear();
    _openers.clear();
    _avgChars   = 0;
    _count      = 0;
    _lastUpdated = 0;

    auto expandedPath = path_utils::expandTilde(corpus_path);

    // Load rotation seed exemplars first — works even when corpus.txt is absent
    // immediately after rotation (corpus.txt renamed to corpus.txt.1).
    // Seeds are real corpus sentences already through acceptable() filters.
    auto seedPath = expandedPath.parent_path() / "corpus_seed.json";
    auto seedExemplars = loadSeedExemplars(seedPath.string());

    // Corpus-only sentences (no seeds): used for sort/trim/dedup.
    // sentenceApp maps sentence text → first app tag seen (first-tab split; empty = untagged).
    std::vector<std::string> corpusSentences;
    std::unordered_map<std::string, std::string> sentenceApp;
    corpusSentences.reserve(256);
    {
        std::ifstream in(expandedPath, std::ios::binary | std::ios::ate);
        if (in) {
            const std::streamsize fileSize = static_cast<std::streamsize>(in.tellg());
            const std::streamsize seekPos =
                std::max(std::streamsize{0}, fileSize - kTailScanBytes);
            in.seekg(seekPos);

            if (seekPos > 0) {
                std::string discard;
                std::getline(in, discard);
            }

            std::string line;
            while (std::getline(in, line) && corpusSentences.size() < kMaxScannedSentences) {
                // Parse optional app-id prefix: "app\tsentence text" (L6 format).
                // Lines without '\t' are untagged (old corpus, backwards compatible).
                std::string app;
                std::string text = line;
                auto tab = line.find('\t');
                if (tab != std::string::npos) {
                    app  = line.substr(0, tab);
                    text = line.substr(tab + 1);
                }
                size_t before = corpusSentences.size();
                appendSentencesFromLine(text, corpusSentences);
                // Record app tag for each sentence added (first occurrence wins on dup).
                for (size_t k = before; k < corpusSentences.size(); ++k)
                    sentenceApp.try_emplace(corpusSentences[k], app);
            }
        }
    }

    // kept = seeds + corpus: used for avgChars/count/openers (mirrors L5 behaviour).
    std::vector<std::string> kept = corpusSentences;
    kept.insert(kept.begin(), seedExemplars.begin(), seedExemplars.end());

    if (kept.empty() && seedExemplars.empty()) return;

    if (!kept.empty()) {
        long long sum = 0;
        for (const auto& s : kept) sum += static_cast<long long>(s.size());
        _avgChars = static_cast<int>(sum / static_cast<long long>(kept.size()));
        _count    = static_cast<int>(kept.size());
    }

    // Sort corpus-only sentences by length, drop shortest and longest 5%.
    std::vector<std::string> sorted = corpusSentences;
    std::sort(sorted.begin(), sorted.end(),
              [](const auto& a, const auto& b) { return a.size() < b.size(); });
    size_t drop = sorted.size() / 20;
    std::vector<std::string> pool;
    if (sorted.size() > 2 * drop) {
        pool.assign(sorted.begin() + drop, sorted.end() - drop);
    } else {
        pool = std::move(sorted);
    }

    // Deduplicate and build inverse-frequency weights (boilerplate suppression).
    std::unordered_map<std::string, int> freq;
    for (const auto& s : pool) freq[s]++;

    std::vector<std::string> unique;
    std::unordered_set<std::string> seen;
    for (const auto& s : pool) {
        if (seen.insert(s).second) unique.push_back(s);
    }

    std::vector<double> wts;
    wts.reserve(unique.size());
    for (const auto& s : unique)
        wts.push_back(1.0 / static_cast<double>(freq.at(s)));

    // App weighting (L6): sentences from the current app get 2× weight.
    if (!_currentApp.empty()) {
        for (size_t i = 0; i < unique.size(); ++i) {
            auto it = sentenceApp.find(unique[i]);
            if (it != sentenceApp.end() && it->second == _currentApp)
                wts[i] *= 2.0;
        }
    }

    // Prepend seed sentences with weight 1.0 — they are unique by construction
    // (already privacy-filtered corpus sentences persisted across rotation).
    if (!seedExemplars.empty()) {
        std::vector<double> seedWeights(seedExemplars.size(), 1.0);
        unique.insert(unique.begin(), seedExemplars.begin(), seedExemplars.end());
        wts.insert(wts.begin(), seedWeights.begin(), seedWeights.end());
    }

    _exemplars   = pickExemplars(unique, wts, 5, seed);
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

// ── Embedding index ───────────────────────────────────────────────────────────

bool writeEmbeddingIndex(const std::string& path,
                         const std::vector<EmbeddingEntry>& entries) {
    if (entries.empty()) return false;

    const uint32_t dim   = static_cast<uint32_t>(entries[0].embedding.size());
    const uint32_t count = static_cast<uint32_t>(entries.size());
    const int64_t  ts    = static_cast<int64_t>(std::time(nullptr));

    std::string tmp = path + ".tmp";
    std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
    if (!out) return false;

    // Header: magic 'XTES' (4 bytes), version, dim, count, created_at
    const char magic[4] = {'X', 'T', 'E', 'S'};
    out.write(magic, 4);
    uint32_t version = 1;
    out.write(reinterpret_cast<const char*>(&version), 4);
    out.write(reinterpret_cast<const char*>(&dim),     4);
    out.write(reinterpret_cast<const char*>(&count),   4);
    out.write(reinterpret_cast<const char*>(&ts),      8);

    for (const auto& e : entries) {
        if (e.embedding.size() != dim) continue;  // skip dimension mismatch
        uint32_t tlen = static_cast<uint32_t>(e.text.size());
        out.write(reinterpret_cast<const char*>(&tlen), 4);
        out.write(e.text.data(), static_cast<std::streamsize>(tlen));
        out.write(reinterpret_cast<const char*>(e.embedding.data()),
                  static_cast<std::streamsize>(dim * sizeof(float)));
    }

    if (!out) {
        std::error_code ec;
        std::filesystem::remove(tmp, ec);
        return false;
    }
    out.close();

    std::error_code ec;
    std::filesystem::rename(tmp, path, ec);
    if (ec) {
        std::filesystem::remove(tmp, ec);
        return false;
    }
    return true;
}

std::vector<EmbeddingEntry> readEmbeddingIndex(const std::string& path) {
    std::vector<EmbeddingEntry> result;
    std::ifstream in(path, std::ios::binary);
    if (!in) return result;

    char magic[4];
    in.read(magic, 4);
    if (!in || magic[0] != 'X' || magic[1] != 'T' || magic[2] != 'E' || magic[3] != 'S')
        return result;

    uint32_t version = 0, dim = 0, count = 0;
    int64_t  ts = 0;
    in.read(reinterpret_cast<char*>(&version), 4);
    in.read(reinterpret_cast<char*>(&dim),     4);
    in.read(reinterpret_cast<char*>(&count),   4);
    in.read(reinterpret_cast<char*>(&ts),      8);
    if (!in || version != 1 || dim == 0 || dim > 16384 || count == 0) return result;

    result.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t tlen = 0;
        in.read(reinterpret_cast<char*>(&tlen), 4);
        if (!in || tlen > 65536) return result;

        std::string text(tlen, '\0');
        in.read(text.data(), static_cast<std::streamsize>(tlen));
        if (!in) return result;

        std::vector<float> emb(dim);
        in.read(reinterpret_cast<char*>(emb.data()),
                static_cast<std::streamsize>(dim * sizeof(float)));
        if (!in) return result;

        result.push_back({std::move(text), std::move(emb)});
    }
    return result;
}

void StyleProfile::buildEmbeddingIndex(const std::string& corpusPath,
                                       const std::string& indexPath,
                                       const std::string& ollamaHost) {
    namespace fs = std::filesystem;
    std::error_code ec;

    auto corpusP = path_utils::expandTilde(corpusPath);
    auto indexP  = fs::path(indexPath);

    // Skip rebuild when index is at least as fresh as the corpus.
    if (fs::exists(indexP, ec) && !ec && fs::exists(corpusP, ec) && !ec) {
        auto idxMtime = fs::last_write_time(indexP, ec);
        if (!ec) {
            auto cpMtime = fs::last_write_time(corpusP, ec);
            if (!ec && idxMtime >= cpMtime) return;
        }
    }

    // Read corpus tail sentences (same window as loadFromCorpus).
    std::vector<std::string> sentences;
    {
        std::ifstream in(corpusP, std::ios::binary | std::ios::ate);
        if (!in) return;

        const auto fileSize = static_cast<std::streamsize>(in.tellg());
        const auto seekPos  = std::max(std::streamsize{0}, fileSize - kTailScanBytes);
        in.seekg(seekPos);
        if (seekPos > 0) {
            std::string discard;
            std::getline(in, discard);
        }

        std::string line;
        while (std::getline(in, line) && sentences.size() < kMaxIndexedSentences) {
            // Strip optional app-id prefix (L6 format) before embedding.
            std::string text = line;
            auto tab = line.find('\t');
            if (tab != std::string::npos)
                text = line.substr(tab + 1);
            appendSentencesFromLine(text, sentences);
        }
    }

    if (sentences.empty()) return;

    // Dedup (same invariant as loadFromCorpus L2 deduplication).
    std::vector<std::string> unique;
    std::unordered_set<std::string> seen;
    for (const auto& s : sentences) {
        if (seen.insert(s).second) unique.push_back(s);
    }

    // Embed in chunks to avoid oversized JSON payloads.
    OllamaEmbedClient client(ollamaHost);
    std::vector<EmbeddingEntry> entries;
    entries.reserve(unique.size());

    for (size_t i = 0; i < unique.size(); i += kEmbedBatchSize) {
        size_t end = std::min(i + kEmbedBatchSize, unique.size());
        std::vector<std::string> batch(unique.begin() + static_cast<std::ptrdiff_t>(i),
                                       unique.begin() + static_cast<std::ptrdiff_t>(end));
        auto vecs = client.embedBatch(batch);
        for (size_t j = 0; j < batch.size(); ++j) {
            if (j < vecs.size() && !vecs[j].empty()) {
                entries.push_back({batch[j], std::move(vecs[j])});
            }
        }
    }

    if (!entries.empty()) {
        writeEmbeddingIndex(indexPath, entries);
    }
}
