#pragma once

#include <filesystem>
#include <string>

namespace path_utils {

// Expands a leading ~ in `in` to $HOME. If $HOME is unset/empty, returns
// `in` unchanged (callers detect failure by checking that the path still
// starts with '~'). Empty input returns an empty path.
std::filesystem::path expandTilde(const std::string& in);

}  // namespace path_utils
