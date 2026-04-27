#include "config_loader.h"
#include "path_utils.h"
#include "toml/toml.hpp"

#include <cstdio>
#include <filesystem>
#include <string>

std::string config_loader::default_path() {
    auto p = path_utils::expandTilde("~/.config/xtype/config.toml");
    return p.string();
}

XTypeConfig config_loader::load(const std::string& path) {
    XTypeConfig cfg;
    const std::string p = path.empty() ? default_path() : path;
    if (p.empty()) return cfg;

    std::error_code ec;
    if (!std::filesystem::exists(p, ec)) return cfg;

    toml::table tbl;
    try {
        tbl = toml::parse_file(p);
    } catch (const toml::parse_error& e) {
        std::fprintf(stderr, "[xtype] config parse error in %s: %s\n",
                     p.c_str(), e.what());
        return cfg;
    }

    // [inference]
    if (auto* t = tbl["inference"].as_table()) {
        if (auto v = (*t)["model"].value<std::string>())         cfg.inference.model              = *v;
        if (auto v = (*t)["ollama_host"].value<std::string>())   cfg.inference.ollama_host        = *v;
        if (auto v = (*t)["debounce_ms"].value<int64_t>())       cfg.inference.debounce_ms        = (int)*v;
        if (auto v = (*t)["min_context_chars"].value<int64_t>()) cfg.inference.min_context_chars  = (int)*v;
        if (auto v = (*t)["context_window"].value<int64_t>())    cfg.inference.context_window     = (int)*v;
        if (auto v = (*t)["num_predict"].value<int64_t>())       cfg.inference.num_predict        = (int)*v;
        if (auto v = (*t)["temperature"].value<double>())        cfg.inference.temperature        = (float)*v;
        if (auto v = (*t)["top_p"].value<double>())              cfg.inference.top_p              = (float)*v;
        if (auto v = (*t)["threads"].value<int64_t>())           cfg.inference.threads            = (int)*v;
        if (auto* arr = (*t)["stop_tokens"].as_array()) {
            cfg.inference.stop_tokens.clear();
            for (const auto& el : *arr)
                if (auto s = el.value<std::string>(); s && !s->empty())
                    cfg.inference.stop_tokens.push_back(*s);
        }
    }

    // [behaviour]
    if (auto* t = tbl["behaviour"].as_table()) {
        if (auto v = (*t)["engine_enabled"].value<bool>())    cfg.behaviour.engine_enabled         = *v;
        if (auto v = (*t)["partial_accept"].value<bool>())
            cfg.behaviour.partial_accept = *v;
        else if (auto v2 = (*t)["tab_accepts_word"].value<bool>())
            cfg.behaviour.partial_accept = *v2;
        if (auto v = (*t)["esc_dismisses"].value<bool>())     cfg.behaviour.esc_dismisses          = *v;
        if (auto v = (*t)["passthrough_terminals"].value<bool>())
            cfg.behaviour.passthrough_terminals = *v;
        if (auto v = (*t)["trigger_mode"].value<std::string>()) {
            if (*v == "manual") cfg.behaviour.trigger_mode = TriggerMode::Manual;
            else                cfg.behaviour.trigger_mode = TriggerMode::Pause;
        }
        if (auto v = (*t)["accept_full_key"].value<std::string>()) {
            if (*v == "enter")      cfg.behaviour.accept_full_key = AcceptKey::Enter;
            else if (*v == "right") cfg.behaviour.accept_full_key = AcceptKey::Right;
            else                    cfg.behaviour.accept_full_key = AcceptKey::Tab;
        }
        if (auto* arr = (*t)["blocklist_apps"].as_array()) {
            cfg.behaviour.blocklist_apps.clear();
            for (const auto& el : *arr)
                if (auto s = el.value<std::string>())
                    cfg.behaviour.blocklist_apps.push_back(*s);
        }
        if (auto* arr = (*t)["blocked_phrases"].as_array()) {
            cfg.behaviour.blocked_phrases.clear();
            for (const auto& el : *arr)
                if (auto s = el.value<std::string>())
                    cfg.behaviour.blocked_phrases.push_back(*s);
        }
    }

    // [learning]
    if (auto* t = tbl["learning"].as_table()) {
        if (auto v = (*t)["enabled"].value<bool>())                cfg.learning.enabled                    = *v;
        if (auto v = (*t)["corpus_path"].value<std::string>())     cfg.learning.corpus_path                = *v;
        if (auto v = (*t)["flush_interval_sec"].value<int64_t>())  cfg.learning.flush_interval_sec         = (int)*v;
        if (auto v = (*t)["max_corpus_mb"].value<int64_t>())       cfg.learning.max_corpus_mb              = (int)*v;
        if (auto v = (*t)["min_sentence_chars"].value<int64_t>())  cfg.learning.min_sentence_chars         = (int)*v;
        if (auto v = (*t)["include_examples_in_prompt"].value<bool>())
            cfg.learning.include_examples_in_prompt = *v;
        if (auto v = (*t)["voice_strength"].value<int64_t>())      cfg.learning.voice_strength             = (int)*v;
        if (auto v = (*t)["forget_after_days"].value<int64_t>())   cfg.learning.forget_after_days          = (int)*v;
    }

    // [user_prompt]
    if (auto* t = tbl["user_prompt"].as_table()) {
        if (auto v = (*t)["description"].value<std::string>()) cfg.user_prompt.description = *v;
        if (auto v = (*t)["tone"].value<std::string>())        cfg.user_prompt.tone        = *v;
        if (auto* arr = (*t)["avoid_phrases"].as_array()) {
            cfg.user_prompt.avoid_phrases.clear();
            for (const auto& el : *arr)
                if (auto s = el.value<std::string>())
                    cfg.user_prompt.avoid_phrases.push_back(*s);
        }
    }

    // [apps] — per-app overrides
    if (auto* apps = tbl["apps"].as_table()) {
        for (auto& [key, val] : *apps) {
            if (auto* at = val.as_table()) {
                AppOverride ov;
                if (auto v = (*at)["enabled"].value<bool>())        ov.enabled         = *v;
                if (auto v = (*at)["model"].value<std::string>())   ov.model           = *v;
                if (auto v = (*at)["debounce_ms"].value<int64_t>()) ov.debounce_ms     = (int)*v;
                if (auto v = (*at)["num_predict"].value<int64_t>()) ov.num_predict     = (int)*v;
                if (auto v = (*at)["mode"].value<std::string>())    ov.mode            = *v;
                if (auto v = (*at)["prompt_addendum"].value<std::string>()) ov.prompt_addendum = *v;
                cfg.apps[std::string(key.str())] = ov;
            }
        }
    }

    return cfg;
}
