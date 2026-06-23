#include "lvk/glfw_window.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace lvk {
namespace {
uint32_t ToFramebufferSize(int value) noexcept { return static_cast<uint32_t>(std::max(value, 0)); }
}  // namespace

GlfwWindow::GlfwWindow(uint32_t width, uint32_t height, const std::string& title) {
  if (GLFW_TRUE != glfwInit()) {
    throw std::runtime_error("failed to initialize GLFW");
  }

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  m_window = glfwCreateWindow(static_cast<int>(width), static_cast<int>(height), title.c_str(), nullptr, nullptr);
  if (nullptr == m_window) {
    glfwTerminate();
    throw std::runtime_error("failed to create GLFW window");
  }

  glfwSetWindowUserPointer(m_window, this);
  glfwSetFramebufferSizeCallback(m_window, OnFramebufferResize);
  glfwSetCursorPosCallback(m_window, OnCursorPosition);
  glfwSetMouseButtonCallback(m_window, OnMouseButton);
  glfwSetScrollCallback(m_window, OnScroll);
  glfwSetKeyCallback(m_window, OnKey);
  glfwSetCharCallback(m_window, OnChar);
}

GlfwWindow::~GlfwWindow() noexcept {
  if (nullptr != m_window) {
    glfwDestroyWindow(m_window);
    m_window = nullptr;
  }

  glfwTerminate();
}

void GlfwWindow::PollEvents() const noexcept { glfwPollEvents(); }

void GlfwWindow::WaitEvents() const noexcept { glfwWaitEvents(); }

void GlfwWindow::WaitForVisibleFramebuffer() const noexcept {
  int width{};
  int height{};
  glfwGetFramebufferSize(m_window, &width, &height);
  while (0 == width || 0 == height) {
    glfwGetFramebufferSize(m_window, &width, &height);
    glfwWaitEvents();
  }
}

bool GlfwWindow::ShouldClose() const noexcept { return GLFW_TRUE == glfwWindowShouldClose(m_window); }

GLFWwindow* GlfwWindow::GetNativeWindow() const noexcept { return m_window; }

uint32_t GlfwWindow::GetFramebufferWidth() const noexcept {
  int width{};
  int height{};
  glfwGetFramebufferSize(m_window, &width, &height);
  return ToFramebufferSize(width);
}

uint32_t GlfwWindow::GetFramebufferHeight() const noexcept {
  int width{};
  int height{};
  glfwGetFramebufferSize(m_window, &width, &height);
  return ToFramebufferSize(height);
}

void GlfwWindow::SetResizeCallback(ResizeCallback resize_callback) { m_resize_callback = std::move(resize_callback); }

void GlfwWindow::AddCursorPositionCallback(CursorPositionCallback callback) {
  m_cursor_position_callbacks.emplace_back(std::move(callback));
}

void GlfwWindow::AddMouseButtonCallback(MouseButtonCallback callback) {
  m_mouse_button_callbacks.emplace_back(std::move(callback));
}

void GlfwWindow::AddScrollCallback(ScrollCallback callback) { m_scroll_callbacks.emplace_back(std::move(callback)); }

void GlfwWindow::AddKeyCallback(KeyCallback callback) { m_key_callbacks.emplace_back(std::move(callback)); }

void GlfwWindow::AddCharCallback(CharCallback callback) { m_char_callbacks.emplace_back(std::move(callback)); }

void GlfwWindow::OnFramebufferResize(GLFWwindow* window, int width, int height) noexcept {
  auto* app_window = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(window));
  if (nullptr == app_window || !app_window->m_resize_callback) {
    return;
  }

  app_window->m_resize_callback(ToFramebufferSize(width), ToFramebufferSize(height));
}

void GlfwWindow::OnCursorPosition(GLFWwindow* window, double x, double y) noexcept {
  auto* app_window = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(window));
  if (nullptr == app_window) {
    return;
  }

  for (const CursorPositionCallback& callback : app_window->m_cursor_position_callbacks) {
    callback(x, y);
  }
}

void GlfwWindow::OnMouseButton(GLFWwindow* window, int button, int action, int mods) noexcept {
  auto* app_window = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(window));
  if (nullptr == app_window) {
    return;
  }

  for (const MouseButtonCallback& callback : app_window->m_mouse_button_callbacks) {
    callback(button, action, mods);
  }
}

void GlfwWindow::OnScroll(GLFWwindow* window, double x_offset, double y_offset) noexcept {
  auto* app_window = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(window));
  if (nullptr == app_window) {
    return;
  }

  for (const ScrollCallback& callback : app_window->m_scroll_callbacks) {
    callback(x_offset, y_offset);
  }
}

void GlfwWindow::OnKey(GLFWwindow* window, int key, int scancode, int action, int mods) noexcept {
  auto* app_window = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(window));
  if (nullptr == app_window) {
    return;
  }

  for (const KeyCallback& callback : app_window->m_key_callbacks) {
    callback(key, scancode, action, mods);
  }
}

void GlfwWindow::OnChar(GLFWwindow* window, unsigned int codepoint) noexcept {
  auto* app_window = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(window));
  if (nullptr == app_window) {
    return;
  }

  for (const CharCallback& callback : app_window->m_char_callbacks) {
    callback(static_cast<uint32_t>(codepoint));
  }
}
}  // namespace lvk
