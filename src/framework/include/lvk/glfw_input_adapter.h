#pragma once

#include "lvk/input_state.h"

struct GLFWwindow;

namespace lvk {
/// @brief 将 GLFW 即时输入查询和回调事件整理成每帧 `InputState`。
/// @details
/// - 键盘和鼠标按钮使用 GLFW 每帧查询，适合教学样例的持续状态输入。
/// - 滚轮只能通过回调累计，因此由 `AddScroll` 写入、`BeginFrame` 消费后清零。
class GlfwInputAdapter final {
public:
  /// @brief 开始新一帧输入采样。
  void BeginFrame(GLFWwindow* window);
  /// @brief 累计 GLFW scroll callback 提供的滚轮偏移。
  void AddScroll(double x_offset, double y_offset) noexcept;
  /// @brief 获取最近一次 `BeginFrame` 生成的输入快照。
  [[nodiscard]] const InputState& GetInputState() const noexcept;

private:
  InputState m_state{};
  bool m_has_previous_mouse{false};
  double m_previous_mouse_x{};
  double m_previous_mouse_y{};
  double m_accumulated_scroll_x{};
  double m_accumulated_scroll_y{};
};
}  // namespace lvk
