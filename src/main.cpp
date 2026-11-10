#include "logger.hpp"
#include "vulkan.hpp"
#include "window.hpp"

#include <csignal>

extern "C" void handleSigwinch(int) { Log::ws.changed = true; }

int main() {
  try {
    signal(SIGWINCH, handleSigwinch);
    Log::updateWS();
    LOG_LOC(Log::Level::INFO, "hd-sim");

    Window window(1080, 720, "Vulkan");
    Vulkan vulkan(window);

    while (true) {
      glfwPollEvents();
      if (glfwWindowShouldClose(window))
        break;

      vulkan.drawFrame();
    }

  } catch (const std::exception &e) {
    Log::log(Log::Level::ERROR, e.what());
    return 1;
  }

  LOG_LOC(Log::Level::DEBUG, "Exiting.");
  return 0;
}