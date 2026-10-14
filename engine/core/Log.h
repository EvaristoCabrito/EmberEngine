#pragma once

#include <format>
#include <string_view>
#include <utility>

namespace ember {

enum class LogLevel { Info, Warn, Error };

void logWrite(LogLevel level, std::string_view message);
[[noreturn]] void fatal(std::string_view message);

template <class... Args>
void logInfo(std::format_string<Args...> fmt, Args&&... args) {
  logWrite(LogLevel::Info, std::format(fmt, std::forward<Args>(args)...));
}

template <class... Args>
void logWarn(std::format_string<Args...> fmt, Args&&... args) {
  logWrite(LogLevel::Warn, std::format(fmt, std::forward<Args>(args)...));
}

template <class... Args>
void logError(std::format_string<Args...> fmt, Args&&... args) {
  logWrite(LogLevel::Error, std::format(fmt, std::forward<Args>(args)...));
}

}  // namespace ember

#define EMBER_CHECK(cond, msg)                          \
  do {                                                  \
    if (!(cond)) ::ember::fatal(std::format("{} ({}:{})", msg, __FILE__, __LINE__)); \
  } while (0)
