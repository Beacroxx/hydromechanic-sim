#include "logger.hpp"

#include <fstream>
#include <iostream>
#include <mutex>
#include <ranges>
#include <sstream>
#include <sys/ioctl.h>
#include <unistd.h>

Log::Winsize Log::ws {};

void Log::updateWS() {
  winsize ws {};
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
    Log::ws.rows = ws.ws_row;
    Log::ws.cols = ws.ws_col;
  }
}

static std::size_t visibleLength(const std::string &str) {
  std::size_t length = 0;

  for (std::size_t i = 0; i < str.size();) {
    if (str[i] == '\033') {
      ++i;

      if (i < str.size() && str[i] == '[') {
        ++i;

        while (i < str.size() && !(str[i] >= '@' && str[i] <= '~'))
          ++i;

        if (i < str.size())
          ++i;
      }
    } else {
      ++length;
      ++i;
    }
  }

  return length;
}

void Log::log(Level level, std::string str) {
  using enum Level;

  if (level == DEBUG && false /* Add CMDLine*/)
    return;

  str = std::string(levelName(level)) + str;
  auto colon = str.find(':');
  std::string::size_type open = std::string::npos;
  if (!str.empty() && str.back() == ']') {
    int depth = 0;
    auto isOutermostOpen = [&depth](char c) {
      if (c == ']')
        ++depth;
      else if (c == '[' && --depth == 0)
        return true;
      return false;
    };
    auto it = std::ranges::find_if(str | std::views::reverse, isOutermostOpen);
    if (it != str.rend()) {
      auto idx = static_cast<std::string::size_type>(it.base() - str.begin()) - 1;
      open = idx > 0 ? idx - 1 : idx;
    }
  }
  if (colon != std::string::npos && open != std::string::npos) {
    const std::string prefix = str.substr(0, colon + 1);
    const std::string middle = str.substr(colon + 1, open - (colon + 1));
    const std::string bracket = str.substr(open);

    const std::size_t terminalWidth = Log::ws.cols > 0 ? Log::ws.cols : 80;
    const std::size_t usedWidth = visibleLength(prefix) + visibleLength(middle) + visibleLength(bracket);
    const std::size_t padding = usedWidth < terminalWidth ? terminalWidth - usedWidth : 0;

    std::cerr << levelColor(level) << prefix << SYM_RESET << middle << std::string(padding, ' ') << SYM_GRAY << bracket
              << SYM_RESET << '\n';
  } else if (colon != std::string::npos) {
    std::cerr << levelColor(level) << str.substr(0, colon + 1) << SYM_RESET << str.substr(colon + 1) << '\n';
  } else {
    std::cerr << str << '\n';
  }
}