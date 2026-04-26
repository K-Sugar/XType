#include "path_utils.h"

#include <cstdlib>

namespace path_utils {

std::filesystem::path expandTilde(const std::string& in) {
    if (in.empty()) return {};
    if (in.front() != '~') return std::filesystem::path(in);
    const char* home = std::getenv("HOME");
    if (!home || !*home) return std::filesystem::path(in);
    return std::filesystem::path(std::string(home) + in.substr(1));
}

}  // namespace path_utils
