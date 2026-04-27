#include "prompt_builder.h"

#include <algorithm>

#include "style_profile.h"

namespace {

std::string assemble(const std::string& base,
                     const std::string& userDesc,
                     const std::vector<std::string>& exemplars,
                     const std::vector<std::string>& avoid) {
    std::string out = base;
    if (!userDesc.empty()) {
        out += "\n\nAbout the user: ";
        out += userDesc;
    }
    if (!exemplars.empty()) {
        out += "\n\nThe user's typical writing style:";
        for (const auto& e : exemplars) {
            out += "\n- ";
            out += e;
        }
    }
    if (!avoid.empty()) {
        out += "\n\nAvoid these phrases: ";
        for (size_t i = 0; i < avoid.size(); ++i) {
            if (i) out += ", ";
            out += avoid[i];
        }
    }
    return out;
}

}  // namespace

std::string buildSystemPrompt(const PromptInputs& in, bool* truncated) {
    if (truncated) *truncated = false;

    std::vector<std::string> exemplars;
    if (!in.exemplarsOverride.empty()) {
        exemplars = in.exemplarsOverride;
    } else if (in.includeExamples && in.profile && !in.profile->exemplars().empty()) {
        exemplars = in.profile->exemplars();
    }

    std::string out = assemble(in.base, in.userDescription, exemplars, in.avoidPhrases);
    if (out.size() <= in.budgetChars) return out;

    // Drop exemplars longest-first until we fit, or run out.
    while (!exemplars.empty() && out.size() > in.budgetChars) {
        auto longest = std::max_element(
            exemplars.begin(), exemplars.end(),
            [](const auto& a, const auto& b) { return a.size() < b.size(); });
        exemplars.erase(longest);
        if (truncated) *truncated = true;
        out = assemble(in.base, in.userDescription, exemplars, in.avoidPhrases);
    }
    if (out.size() <= in.budgetChars) return out;

    // Still over: truncate userDescription tail. The user description is
    // capped at 1000 chars upstream (Session 19), so this should be rare.
    std::string userDesc = in.userDescription;
    while (!userDesc.empty() && out.size() > in.budgetChars) {
        userDesc.pop_back();
        if (truncated) *truncated = true;
        out = assemble(in.base, userDesc, exemplars, in.avoidPhrases);
    }
    return out;
}
