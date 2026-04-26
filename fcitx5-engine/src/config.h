#pragma once

#include <string>
#include <vector>

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
};

struct BehaviourConfig {
    bool                     tab_accepts_word      = true;
    bool                     passthrough_terminals = true;
    std::vector<std::string> blocklist_apps        = {
        "konsole", "alacritty",
        "keepassxc", "1password", "bitwarden", "gnome-keyring", "seahorse"
    };
};

struct LearningConfig {
    bool        enabled                    = false;  // OPT-IN: default off
    std::string corpus_path                = "~/.local/share/xtype/corpus.txt";
    int         flush_interval_sec         = 60;
    int         max_corpus_mb              = 50;
    int         min_sentence_chars         = 12;
    bool        include_examples_in_prompt = true;   // suppress exemplars without disabling collection
};

struct XTypeConfig {
    InferenceConfig inference;
    BehaviourConfig behaviour;
    LearningConfig  learning;
};
