#pragma once

#include <string>
#include <vector>

struct InferenceConfig {
    std::string model          = "qwen2.5:1.5b";
    std::string ollama_host    = "http://localhost:11434";
    int         debounce_ms    = 180;
    int         min_context    = 10;
    int         context_window = 500;
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
