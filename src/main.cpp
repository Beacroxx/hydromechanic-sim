#include "logger.hpp"

#include <csignal>

extern "C" void handleSigwinch(int) { Log::ws.changed = true; }

int main() {
  signal(SIGWINCH, handleSigwinch);
  Log::updateWS();
  LOG_LOC(Log::Level::INFO, "hd-sim");

  return 0;
}