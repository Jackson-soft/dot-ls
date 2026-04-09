#pragma once

#include "utils/log.hpp"

#include <filesystem>

namespace infra::config {
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
  const auto logPath =
      HomeDirectory() / std::filesystem::path{".cache/dot-ls/logs"};
  if (!std::filesystem::exists(logPath)) {
    if (!std::filesystem::create_directories(logPath)) {
      return false;
    }
  }

  const auto logFile = logPath / std::filesystem::path{"rotating.log"};
  uranus::utils::LogHelper::Instance().Initialize(logFile);

  return true;
}
} // namespace infra::config