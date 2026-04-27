#pragma once
#include "config.h"
#include <string>

namespace config_loader {

// Reads ~/.config/xtype/config.toml (or `path` if non-empty override).
// Returns XTypeConfig{} defaults for any missing key or on parse error.
// Thread-safe after one-time static init of path_utils.
XTypeConfig load(const std::string& path = "");

// Returns the canonical config path: ~/.config/xtype/config.toml
// Expands ~ using $HOME; returns empty string on failure.
std::string default_path();

}  // namespace config_loader
