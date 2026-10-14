#include "core/Log.h"

#include <cstdio>
#include <cstdlib>
#include <mutex>

namespace ember {

namespace {
std::mutex gLogMutex;
}

void logWrite(LogLevel level, std::string_view message) {
  const char* tag = level == LogLevel::Info ? "info" : level == LogLevel::Warn ? "warn" : "ERROR";
  std::lock_guard lock(gLogMutex);
  std::FILE* out = level == LogLevel::Info ? stdout : stderr;
  std::fprintf(out, "[%s] %.*s\n", tag, static_cast<int>(message.size()), message.data());
  std::fflush(out);
}

void fatal(std::string_view message) {
  logWrite(LogLevel::Error, message);
  std::exit(1);
}

}  // namespace ember
