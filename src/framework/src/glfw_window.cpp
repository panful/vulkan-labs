#include "lvk/glfw_window.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace lvk {
namespace {
uint32_t ToFramebufferSize(int value) noexcept {
  // GLFW 在窗口最小化等情况下可能返回 0；负数没有有效语义，统一夹到 0。
  return static_cast<uint32_t>(std::max(value, 0));
}
}  // namespace

GlfwWindow::GlfwWindow(uint32_t width, uint32_t height, const std::string& title) {
  if (GLFW_TRUE != glfwInit()) {
    throw std::runtime_error("failed to initialize GLFW");
  }

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  // Vulkan 会通过 surface 与窗口关联，不需要 GLFW 创建 OpenGL context。
  m_window = glfwCreateWindow(static_cast<int>(width), static_cast<int>(height), title.c_str(), nullptr, nullptr);
  if (nullptr == m_window) {
    glfwTerminate();
    throw std::runtime_error("failed to create GLFW window");
  }

  // GLFW 回调只能拿到 GLFWwindow*，通过 user pointer 找回当前 C++ 对象。
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
    // 最小化窗口时 framebuffer 为 0，等待事件可以避免忙等占满 CPU。
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

GlfwWindow::CallbackHandle GlfwWindow::AddCursorPositionCallback(CursorPositionCallback callback) {
  return AddCallback(m_cursor_position_callbacks, std::move(callback));
}

GlfwWindow::CallbackHandle GlfwWindow::AddMouseButtonCallback(MouseButtonCallback callback) {
  return AddCallback(m_mouse_button_callbacks, std::move(callback));
}

GlfwWindow::CallbackHandle GlfwWindow::AddScrollCallback(ScrollCallback callback) {
  return AddCallback(m_scroll_callbacks, std::move(callback));
}

GlfwWindow::CallbackHandle GlfwWindow::AddKeyCallback(KeyCallback callback) {
  return AddCallback(m_key_callbacks, std::move(callback));
}

GlfwWindow::CallbackHandle GlfwWindow::AddCharCallback(CharCallback callback) {
  return AddCallback(m_char_callbacks, std::move(callback));
}

void GlfwWindow::RemoveCallback(CallbackHandle callback_handle) noexcept {
  // 句柄在所有回调列表中全局唯一；逐个列表尝试移除可以让调用方只保存一个 opaque handle。
  RemoveCallback(m_cursor_position_callbacks, callback_handle);
  RemoveCallback(m_mouse_button_callbacks, callback_handle);
  RemoveCallback(m_scroll_callbacks, callback_handle);
  RemoveCallback(m_key_callbacks, callback_handle);
  RemoveCallback(m_char_callbacks, callback_handle);
}

GlfwWindow::CallbackHandle GlfwWindow::NextCallbackHandle() noexcept { return m_next_callback_handle++; }

template <typename Callback>
GlfwWindow::CallbackHandle GlfwWindow::AddCallback(std::vector<CallbackEntry<Callback>>& callbacks, Callback callback) {
  const CallbackHandle callback_handle{NextCallbackHandle()};
  callbacks.emplace_back(CallbackEntry<Callback>{callback_handle, std::move(callback)});
  return callback_handle;
}

template <typename Callback>
void GlfwWindow::RemoveCallback(std::vector<CallbackEntry<Callback>>& callbacks,
                                CallbackHandle callback_handle) noexcept {
  if (k_invalid_callback_handle == callback_handle) {
    return;
  }

  std::erase_if(callbacks,
                [callback_handle](const CallbackEntry<Callback>& entry) { return entry.handle == callback_handle; });
}

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

  // 使用范围 for 直接分发；教学框架约定回调内不修改同一个回调列表。
  for (const CallbackEntry<CursorPositionCallback>& callback_entry : app_window->m_cursor_position_callbacks) {
    callback_entry.callback(x, y);
  }
}

void GlfwWindow::OnMouseButton(GLFWwindow* window, int button, int action, int mods) noexcept {
  auto* app_window = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(window));
  if (nullptr == app_window) {
    return;
  }

  for (const CallbackEntry<MouseButtonCallback>& callback_entry : app_window->m_mouse_button_callbacks) {
    callback_entry.callback(button, action, mods);
  }
}

void GlfwWindow::OnScroll(GLFWwindow* window, double x_offset, double y_offset) noexcept {
  auto* app_window = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(window));
  if (nullptr == app_window) {
    return;
  }

  for (const CallbackEntry<ScrollCallback>& callback_entry : app_window->m_scroll_callbacks) {
    callback_entry.callback(x_offset, y_offset);
  }
}

void GlfwWindow::OnKey(GLFWwindow* window, int key, int scancode, int action, int mods) noexcept {
  auto* app_window = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(window));
  if (nullptr == app_window) {
    return;
  }

  for (const CallbackEntry<KeyCallback>& callback_entry : app_window->m_key_callbacks) {
    callback_entry.callback(key, scancode, action, mods);
  }
}

void GlfwWindow::OnChar(GLFWwindow* window, unsigned int codepoint) noexcept {
  auto* app_window = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(window));
  if (nullptr == app_window) {
    return;
  }

  for (const CallbackEntry<CharCallback>& callback_entry : app_window->m_char_callbacks) {
    callback_entry.callback(static_cast<uint32_t>(codepoint));
  }
}
}  // namespace lvk
