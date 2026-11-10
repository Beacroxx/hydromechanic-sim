#include "window.hpp"

#include <functional>
#include <tuple>
#include <unordered_map>

struct hash {
  std::size_t operator()(const std::tuple<int, int, int> &key) const noexcept {
    const auto h1 = std::hash<int> {}(std::get<0>(key));
    const auto h2 = std::hash<int> {}(std::get<1>(key));
    const auto h3 = std::hash<int> {}(std::get<2>(key));
    return h1 ^ (h2 << 1) ^ (h3 << 2);
  }
};

static std::unordered_map<std::tuple<int, int, int>, void (*)(GLFWwindow *), hash> handlers = {
    {{GLFW_KEY_ESCAPE, GLFW_PRESS, 0}, [](GLFWwindow *w) { glfwSetWindowShouldClose(w, GLFW_TRUE); }},
};

void Window::keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods) {
  LOG_LOC(Log::Level::DEBUG, "Key '{}', scancode '{}', action '{}', modifier '{}'.", key, scancode, action, mods);

  if (auto it = handlers.find({key, action, mods}); it != handlers.end()) {
    it->second(window);
  }
}

Window::Window(int w, int h, std::string_view title) {
  LOG_LOC(Log::Level::DEBUG, "Initializing GLFW.");

  if (!glfwInit()) {
    const char *err;
    glfwGetError(&err);
    throw ERROR_LOC("GLFW failed to init: {}", err);
  }

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  LOG_LOC(Log::Level::DEBUG, "Spawning Window.");
  window = glfwCreateWindow(w, h, title.data(), nullptr, nullptr);
  glfwSetKeyCallback(window, keyCallback);
}

Window::~Window() {
  if (window) {
    LOG_LOC(Log::Level::DEBUG, "Closing window.");
    glfwDestroyWindow(window);
  }

  LOG_LOC(Log::Level::DEBUG, "Stopping GLFW.");
  glfwTerminate();
}