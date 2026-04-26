#pragma once

// Pure function that composes the system prompt sent to Ollama.
// Layered structure:
//   <base>
//   About the user: <userDescription>            (if non-empty)
//   The user's typical writing style:            (if includeExamples && profile has exemplars)
//   - <exemplar 1>
//   - <exemplar 2>
//   ...
//   Avoid these phrases: a, b, c                 (if avoidPhrases non-empty)
//
// Budget cap: if the assembled output exceeds budgetChars, exemplars are
// dropped longest-first. If still over budget after dropping all exemplars,
// userDescription is truncated. Output is ASCII; bytes == chars.

#include <cstddef>
#include <string>
#include <vector>

class StyleProfile;

struct PromptInputs {
    std::string              base;                  // base instruction (canonical text)
    const StyleProfile*      profile{nullptr};      // nullptr if absent
    bool                     includeExamples{true};
    std::string              userDescription;       // empty in Session 17
    std::vector<std::string> avoidPhrases;          // empty in Session 17
    std::size_t              budgetChars{2000};
};

// `truncated` (optional out-param) is set to true when any drop happened.
std::string buildSystemPrompt(const PromptInputs& in, bool* truncated = nullptr);
