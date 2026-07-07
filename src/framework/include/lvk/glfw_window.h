#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

struct GLFWwindow;

namespace lvk {
/// @brief GLFW 窗口和输入回调的轻量封装。
/// @details
/// - 构造函数会初始化 GLFW，析构函数会销毁窗口并终止 GLFW。
/// - 当前教学框架只面向单窗口；如果要支持多窗口，应先拆出 GLFW runtime 生命周期。
/// - `Add*Callback` 返回的句柄可用于移除订阅，避免重建 UI 层后重复接收事件。
class GlfwWindow final {
public:
  using ResizeCallback = std::function<void(uint32_t width, uint32_t height)>;
  using CursorPositionCallback = std::function<void(double x, double y)>;
  using MouseButtonCallback = std::function<void(int button, int action, int mods)>;
  using ScrollCallback = std::function<void(double x_offset, double y_offset)>;
  using KeyCallback = std::function<void(int key, int scancode, int action, int mods)>;
  using CharCallback = std::function<void(uint32_t codepoint)>;
  using CallbackHandle = uint64_t;

public:
  /// @brief 无效回调句柄，传给 `RemoveCallback` 会被忽略。
  static constexpr CallbackHandle k_invalid_callback_handle{};

public:
  /// @brief 创建无 OpenGL context 的 GLFW 窗口，用于 Vulkan surface。
  /// @throws std::runtime_error GLFW 初始化或窗口创建失败时抛出。
  GlfwWindow(uint32_t width, uint32_t height, const std::string& title);
  ~GlfwWindow() noexcept;

  GlfwWindow(const GlfwWindow&) = delete;
  GlfwWindow& operator=(const GlfwWindow&) = delete;
  GlfwWindow(GlfwWindow&&) noexcept = delete;
  GlfwWindow& operator=(GlfwWindow&&) noexcept = delete;

  /// @brief 轮询并分发 GLFW 事件。
  void PollEvents() const noexcept;
  /// @brief 阻塞等待下一次 GLFW 事件。
  void WaitEvents() const noexcept;
  /// @brief 在窗口最小化导致 framebuffer 为 0 时等待其恢复可见。
  void WaitForVisibleFramebuffer() const noexcept;
  /// @brief 窗口是否收到关闭请求。
  [[nodiscard]] bool ShouldClose() const noexcept;
  /// @brief 原始 GLFWwindow 指针；调用方不拥有该对象。
  [[nodiscard]] GLFWwindow* GetNativeWindow() const noexcept;
  /// @brief 当前 framebuffer 宽度，单位为像素。
  [[nodiscard]] uint32_t GetFramebufferWidth() const noexcept;
  /// @brief 当前 framebuffer 高度，单位为像素。
  [[nodiscard]] uint32_t GetFramebufferHeight() const noexcept;
  /// @brief 设置 framebuffer resize 回调；这里只保留一个回调，因为它代表框架级尺寸事件。
  void SetResizeCallback(ResizeCallback resize_callback);
  /// @brief 追加鼠标位置回调，返回可移除句柄。
  [[nodiscard]] CallbackHandle AddCursorPositionCallback(CursorPositionCallback callback);
  /// @brief 追加鼠标按钮回调，返回可移除句柄。
  [[nodiscard]] CallbackHandle AddMouseButtonCallback(MouseButtonCallback callback);
  /// @brief 追加滚轮回调，返回可移除句柄。
  [[nodiscard]] CallbackHandle AddScrollCallback(ScrollCallback callback);
  /// @brief 追加键盘按键回调，返回可移除句柄。
  [[nodiscard]] CallbackHandle AddKeyCallback(KeyCallback callback);
  /// @brief 追加字符输入回调，返回可移除句柄。
  [[nodiscard]] CallbackHandle AddCharCallback(CharCallback callback);
  /// @brief 移除通过 `Add*Callback` 注册的回调；未知句柄会被忽略。
  void RemoveCallback(CallbackHandle callback_handle) noexcept;

private:
  template <typename Callback>
  struct CallbackEntry {
    CallbackHandle handle{k_invalid_callback_handle};
    Callback callback{};
  };

private:
  [[nodiscard]] CallbackHandle NextCallbackHandle() noexcept;
  template <typename Callback>
  [[nodiscard]] CallbackHandle AddCallback(std::vector<CallbackEntry<Callback>>& callbacks, Callback callback);
  template <typename Callback>
  static void RemoveCallback(std::vector<CallbackEntry<Callback>>& callbacks, CallbackHandle callback_handle) noexcept;
  static void OnFramebufferResize(GLFWwindow* window, int width, int height) noexcept;
  static void OnCursorPosition(GLFWwindow* window, double x, double y) noexcept;
  static void OnMouseButton(GLFWwindow* window, int button, int action, int mods) noexcept;
  static void OnScroll(GLFWwindow* window, double x_offset, double y_offset) noexcept;
  static void OnKey(GLFWwindow* window, int key, int scancode, int action, int mods) noexcept;
  static void OnChar(GLFWwindow* window, unsigned int codepoint) noexcept;

private:
  GLFWwindow* m_window{nullptr};
  ResizeCallback m_resize_callback{};
  CallbackHandle m_next_callback_handle{1};
  std::vector<CallbackEntry<CursorPositionCallback>> m_cursor_position_callbacks{};
  std::vector<CallbackEntry<MouseButtonCallback>> m_mouse_button_callbacks{};
  std::vector<CallbackEntry<ScrollCallback>> m_scroll_callbacks{};
  std::vector<CallbackEntry<KeyCallback>> m_key_callbacks{};
  std::vector<CallbackEntry<CharCallback>> m_char_callbacks{};
};
}  // namespace lvk
