#include "lvk/glfw_input_adapter.h"

#include <GLFW/glfw3.h>

namespace lvk {
namespace {
[[nodiscard]] bool IsKeyDown(GLFWwindow* window, int key) {
  return GLFW_PRESS == glfwGetKey(window, key) || GLFW_REPEAT == glfwGetKey(window, key);
}

[[nodiscard]] bool IsMouseDown(GLFWwindow* window, int button) {
  return GLFW_PRESS == glfwGetMouseButton(window, button);
}
}  // namespace

void GlfwInputAdapter::BeginFrame(GLFWwindow* window) {
  InputState next{};

  glfwGetCursorPos(window, &next.mouse_x, &next.mouse_y);
  if (m_has_previous_mouse) {
    next.mouse_delta_x = next.mouse_x - m_previous_mouse_x;
    next.mouse_delta_y = next.mouse_y - m_previous_mouse_y;
  } else {
    // 第一帧没有上一帧坐标，delta 保持 0，避免相机初始化时突然跳动。
    m_has_previous_mouse = true;
  }

  m_previous_mouse_x = next.mouse_x;
  m_previous_mouse_y = next.mouse_y;

  next.scroll_delta_x = m_accumulated_scroll_x;
  next.scroll_delta_y = m_accumulated_scroll_y;
  // 滚轮来自 GLFW 回调，按帧消费后清零，避免一次滚动影响多帧。
  m_accumulated_scroll_x = 0.0;
  m_accumulated_scroll_y = 0.0;

  next.left_mouse_down = IsMouseDown(window, GLFW_MOUSE_BUTTON_LEFT);
  next.middle_mouse_down = IsMouseDown(window, GLFW_MOUSE_BUTTON_MIDDLE);
  next.right_mouse_down = IsMouseDown(window, GLFW_MOUSE_BUTTON_RIGHT);

  next.alt_down = IsKeyDown(window, GLFW_KEY_LEFT_ALT) || IsKeyDown(window, GLFW_KEY_RIGHT_ALT);
  next.ctrl_down = IsKeyDown(window, GLFW_KEY_LEFT_CONTROL) || IsKeyDown(window, GLFW_KEY_RIGHT_CONTROL);
  next.shift_down = IsKeyDown(window, GLFW_KEY_LEFT_SHIFT) || IsKeyDown(window, GLFW_KEY_RIGHT_SHIFT);
  next.space_down = IsKeyDown(window, GLFW_KEY_SPACE);

  next.key_w = IsKeyDown(window, GLFW_KEY_W);
  next.key_a = IsKeyDown(window, GLFW_KEY_A);
  next.key_s = IsKeyDown(window, GLFW_KEY_S);
  next.key_d = IsKeyDown(window, GLFW_KEY_D);
  next.key_q = IsKeyDown(window, GLFW_KEY_Q);
  next.key_e = IsKeyDown(window, GLFW_KEY_E);

  next.key_up = IsKeyDown(window, GLFW_KEY_UP);
  next.key_down = IsKeyDown(window, GLFW_KEY_DOWN);
  next.key_left = IsKeyDown(window, GLFW_KEY_LEFT);
  next.key_right = IsKeyDown(window, GLFW_KEY_RIGHT);

  m_state = next;
}

void GlfwInputAdapter::AddScroll(double x_offset, double y_offset) noexcept {
  m_accumulated_scroll_x += x_offset;
  m_accumulated_scroll_y += y_offset;
}

const InputState& GlfwInputAdapter::GetInputState() const noexcept { return m_state; }
}  // namespace lvk
