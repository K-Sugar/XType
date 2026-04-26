#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

enum class TriggerMode { Pause, Manual };
enum class AcceptKey   { Tab, Enter, Right };

struct InferenceConfig {
    std::string              model             = "qwen2.5:1.5b";
    std::string              ollama_host       = "http://localhost:11434";
    int                      debounce_ms       = 220;
    int                      min_context_chars = 10;
    int                      context_window    = 150;
    int                      num_predict       = 30;
    float                    temperature       = 0.3f;
    float                    top_p             = 0.9f;
    std::vector<std::string> stop_tokens       = {".", "!", "?", "\n"};
    std::optional<int>       threads;            // unset = let Ollama decide
};

struct BehaviourConfig {
    bool                     engine_enabled       = true;
    TriggerMode              trigger_mode         = TriggerMode::Pause;
    AcceptKey                accept_full_key      = AcceptKey::Tab;
    bool                     partial_accept       = true;   // renamed from tab_accepts_word
    bool                     esc_dismisses        = true;
    bool                     passthrough_terminals= true;
    std::vector<std::string> blocklist_apps       = {
        "konsole", "alacritty",
        "keepassxc", "1password", "bitwarden", "gnome-keyring", "seahorse"
    };
    std::vector<std::string> blocked_phrases      = {};
};

struct LearningConfig {
    bool        enabled                    = false;
    std::string corpus_path                = "~/.local/share/xtype/corpus.txt";
    int         flush_interval_sec         = 60;
    int         max_corpus_mb              = 50;
    int         min_sentence_chars         = 12;
    bool        include_examples_in_prompt = true;
};

struct UserPromptConfig {
    std::string              description;
    std::vector<std::string> avoid_phrases;
    std::string              tone;
};

struct AppOverride {
    std::optional<bool>        enabled;
    std::optional<std::string> model;
    std::optional<int>         debounce_ms;
    std::optional<int>         num_predict;
    std::optional<std::string> mode;            // Default/Code-aware/Email tone/Casual/Off
    std::optional<std::string> prompt_addendum;
};

struct XTypeConfig {
    InferenceConfig                         inference;
    BehaviourConfig                         behaviour;
    LearningConfig                          learning;
    UserPromptConfig                        user_prompt;
    std::map<std::string, AppOverride>      apps;
};
