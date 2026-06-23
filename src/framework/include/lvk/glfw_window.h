#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

struct GLFWwindow;

namespace lvk {
class GlfwWindow final {
public:
  using ResizeCallback = std::function<void(uint32_t width, uint32_t height)>;
  using CursorPositionCallback = std::function<void(double x, double y)>;
  using MouseButtonCallback = std::function<void(int button, int action, int mods)>;
  using ScrollCallback = std::function<void(double x_offset, double y_offset)>;
  using KeyCallback = std::function<void(int key, int scancode, int action, int mods)>;
  using CharCallback = std::function<void(uint32_t codepoint)>;

public:
  GlfwWindow(uint32_t width, uint32_t height, const std::string& title);
  ~GlfwWindow() noexcept;

  GlfwWindow(const GlfwWindow&) = delete;
  GlfwWindow& operator=(const GlfwWindow&) = delete;
  GlfwWindow(GlfwWindow&&) noexcept = delete;
  GlfwWindow& operator=(GlfwWindow&&) noexcept = delete;

  void PollEvents() const noexcept;
  void WaitEvents() const noexcept;
  void WaitForVisibleFramebuffer() const noexcept;
  [[nodiscard]] bool ShouldClose() const noexcept;
  [[nodiscard]] GLFWwindow* GetNativeWindow() const noexcept;
  [[nodiscard]] uint32_t GetFramebufferWidth() const noexcept;
  [[nodiscard]] uint32_t GetFramebufferHeight() const noexcept;
  void SetResizeCallback(ResizeCallback resize_callback);
  void AddCursorPositionCallback(CursorPositionCallback callback);
  void AddMouseButtonCallback(MouseButtonCallback callback);
  void AddScrollCallback(ScrollCallback callback);
  void AddKeyCallback(KeyCallback callback);
  void AddCharCallback(CharCallback callback);

private:
  static void OnFramebufferResize(GLFWwindow* window, int width, int height) noexcept;
  static void OnCursorPosition(GLFWwindow* window, double x, double y) noexcept;
  static void OnMouseButton(GLFWwindow* window, int button, int action, int mods) noexcept;
  static void OnScroll(GLFWwindow* window, double x_offset, double y_offset) noexcept;
  static void OnKey(GLFWwindow* window, int key, int scancode, int action, int mods) noexcept;
  static void OnChar(GLFWwindow* window, unsigned int codepoint) noexcept;

private:
  GLFWwindow* m_window{nullptr};
  ResizeCallback m_resize_callback{};
  std::vector<CursorPositionCallback> m_cursor_position_callbacks{};
  std::vector<MouseButtonCallback> m_mouse_button_callbacks{};
  std::vector<ScrollCallback> m_scroll_callbacks{};
  std::vector<KeyCallback> m_key_callbacks{};
  std::vector<CharCallback> m_char_callbacks{};
};
}  // namespace lvk
