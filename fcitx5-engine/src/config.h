#pragma once

#include <string>
#include <vector>

struct InferenceConfig {
    std::string              model             = "qwen2.5:1.5b";
    std::string              ollama_host       = "http://localhost:11434";
    int                      debounce_ms       = 180;
    int                      min_context_chars = 10;
    int                      context_window    = 500;
    int                      num_predict       = 30;
    float                    temperature       = 0.3f;
    float                    top_p             = 0.9f;
    std::vector<std::string> stop_tokens       = {".", "!", "?", "\n"};
};

struct BehaviourConfig {
    bool                     tab_accepts_word      = true;
    bool                     passthrough_terminals = true;
    std::vector<std::string> blocklist_apps        = {"konsole", "alacritty"};
};

struct XTypeConfig {
    InferenceConfig inference;
    BehaviourConfig behaviour;
};
