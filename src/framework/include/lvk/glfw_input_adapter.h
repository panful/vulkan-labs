#pragma once

#include "lvk/input_state.h"

struct GLFWwindow;

namespace lvk {
class GlfwInputAdapter final {
public:
  void BeginFrame(GLFWwindow* window);
  void AddScroll(double x_offset, double y_offset) noexcept;
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
