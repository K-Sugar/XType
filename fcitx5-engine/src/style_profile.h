#pragma once

// Pure, side-effect-free user-style summariser.
//
// Reads the local writing corpus (one sentence per line, ASCII), filters out
// privacy-sensitive content (emails, long digit runs, password/secret/token/
// auth substrings), samples 3–5 length-varied exemplars, and serialises the
// result to JSON. The class is self-contained: no threads, no I/O outside the
// explicit load/serialize entry points. Session 17 wires it into the engine
// prompt builder on a background thread.
//
// Bigram convention: "first two whitespace-delimited words of a sentence,
// lowercased, joined by a single space". Sentences with one word contribute
// nothing to the openers list.
//
// Encoding: corpus is currently printable-ASCII (the collector only writes
// FcitxKey_space..asciitilde). All length filters are byte-based; revisit if
// the collector widens to UTF-8.

#include <ctime>
#include <string>
#include <vector>

class StyleProfile {
public:
    StyleProfile() = default;

    // Stream-reads corpus_path, parses sentences, applies privacy filter,
    // computes exemplars / avg_sentence_chars / common_openers.
    // seed=0 means "use std::time(nullptr)". Non-zero seed → deterministic.
    void loadFromCorpus(const std::string& corpus_path, unsigned seed = 0);

    // Render the user-style preamble for the system prompt.
    // Empty profile (no exemplars) → empty string (no header).
    std::string generatePreamble() const;

    // JSON round-trip at fixed schema (version=1).
    bool                serialize(const std::string& path) const;       // false on I/O error
    static StyleProfile deserialize(const std::string& path);            // empty on missing/invalid

    // Stale if profile is missing, the corpus is older than min_age_seconds
    // ahead of the profile, or the corpus has grown by more than
    // new_lines_threshold lines since the profile was built.
    static bool isStale(const std::string& corpus_path,
                        const std::string& profile_path,
                        int new_lines_threshold = 200,
                        int min_age_seconds     = 30 * 60);

    // Accessors (also used by tests).
    const std::vector<std::string>& exemplars()      const { return _exemplars; }
    int                             avgSentenceLen() const { return _avgChars; }
    const std::vector<std::string>& commonOpeners()  const { return _openers; }
    std::time_t                     lastUpdated()    const { return _lastUpdated; }
    int                             sentenceCount()  const { return _count; }

private:
    static std::vector<std::string> loadSeedExemplars(const std::string& path);

    std::vector<std::string> _exemplars;
    int                      _avgChars{0};
    std::vector<std::string> _openers;
    std::time_t              _lastUpdated{0};
    int                      _count{0};
};
