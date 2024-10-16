#pragma once

#include "utils/log.hpp"

#include <cstdlib>
#include <filesystem>

namespace basic::boot {
inline auto HomeDirectory() -> std::filesystem::path {
    const char *homeDir{nullptr};
#ifdef _WIN32
    homeDir = std::getenv("USERPROFILE");
#else
    homeDir = std::getenv("HOME");
#endif

    if (homeDir != nullptr) {
        return {homeDir};
    }
    return {};
}

inline auto Boot() -> bool {
    auto                        home    = HomeDirectory();
    std::filesystem::path const logPath = home / std::filesystem::path{".cache/dot-ls/logs"};
    if (!std::filesystem::exists(logPath)) {
        std::filesystem::create_directories(logPath);
    }

    auto logFile = logPath / std::filesystem::path{"rotating.log"};
    uranus::utils::LogHelper::Instance().Initalize(logFile.c_str());
    return true;
}
}  // namespace basic::boot