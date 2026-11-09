#pragma once

#include <format>
#include <source_location>
#include <string_view>

namespace Log {
constexpr std::string_view SYM_RESET = "\x1b[0m";
constexpr std::string_view SYM_RED = "\x1b[31m";
constexpr std::string_view SYM_YELLOW = "\x1b[33m";
constexpr std::string_view SYM_CYAN = "\x1b[36m";
constexpr std::string_view SYM_GRAY = "\x1b[90m";

enum class Level { DEBUG, INFO, WARN, ERROR };

struct Winsize {
  int cols, rows;
  bool changed = false;
} extern ws;

inline constexpr std::string_view levelName(Level l) noexcept {
  switch (l) {
    using enum Level;

  case DEBUG:
    return "Debug: ";
  case INFO:
    return "Info: ";
  case WARN:
    return "Warn: ";
  case ERROR:
    return "Error: ";
  }

  __builtin_unreachable();
}

inline constexpr std::string_view levelColor(Level l) noexcept {

  switch (l) {
    using enum Level;

  case DEBUG:
    return SYM_GRAY;
  case INFO:
    return SYM_CYAN;
  case WARN:
    return SYM_YELLOW;
  case ERROR:
    return SYM_RED;
  }

  __builtin_unreachable();
}

template <typename... Args> struct logFmt {
  std::format_string<Args...> fmt;
  std::source_location loc;

  consteval logFmt(std::format_string<Args...> f, std::source_location l = std::source_location::current()) noexcept
      : fmt(f), loc(l) {};
};

void updateWS();
void log(Level level, std::string str);
template <typename... Args>
void log(std::source_location loc, Level level, std::format_string<Args...> fmt, Args &&...args);
template <typename... Args>
std::runtime_error makeError(std::source_location loc, std::format_string<Args...> fmt, Args &&...args);
template <typename... Args>
std::string format_plain(std::source_location loc, std::format_string<Args...> fmt, Args &&...args);
} // namespace Log

template <> struct std::formatter<std::source_location> {
  constexpr auto parse(std::format_parse_context &ctx) { return ctx.begin(); }
  auto format(const std::source_location &loc, std::format_context &ctx) const {
    std::string_view func {loc.function_name()};
    if (auto paren = func.find('('); paren != std::string_view::npos) {
      auto prefix = func.substr(0, paren);
      if (auto sep = prefix.find_last_of(" >"); sep != std::string_view::npos)
        func = func.substr(sep + 1);
    }
    std::string_view file {loc.file_name()};
    if (auto sep = file.find_last_of("/\\"); sep != std::string_view::npos)
      file = file.substr(sep + 1);
    return std::format_to(ctx.out(), "{} at {}:{}", func, file, loc.line());
  }
};

template <typename... Args>
std::string Log::format_plain(std::source_location loc, std::format_string<Args...> fmt, Args &&...args) {
  return std::format("{} [{}]", std::format(fmt, std::forward<Args>(args)...), loc);
}

template <typename... Args>
inline void Log::log(std::source_location loc, Level level, std::format_string<Args...> fmt, Args &&...args) {
  log(level, format_plain(loc, fmt, std::forward<Args>(args)...));
}

template <typename... Args>
inline std::runtime_error Log::makeError(std::source_location loc, std::format_string<Args...> fmt, Args &&...args) {
  return std::runtime_error(format_plain(loc, fmt, std::forward<Args>(args)...));
}

#define LOG_LOC(level, fmt, ...) ::Log::log(std::source_location::current(), level, fmt __VA_OPT__(, __VA_ARGS__))
#define ERROR_LOC(fmt, ...)      ::Log::makeError(std::source_location::current(), fmt __VA_OPT__(, __VA_ARGS__))