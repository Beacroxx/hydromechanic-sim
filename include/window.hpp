#pragma once

#define GLFW_INCLUDE_VULKAN

#include "GLFW/glfw3.h"
#include "logger.hpp"

class Window {
public:
  Window(int w, int h, std::string_view title);
  ~Window();

  static void closeCallback(GLFWwindow *window);
  static void keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods);

  operator GLFWwindow *() const { return window; }

  GLFWwindow *window = nullptr;
};