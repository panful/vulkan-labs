#pragma once

namespace lvk {
/// @brief 每帧采样后的输入快照。
/// @details
/// - 鼠标位置来自 GLFW 当前帧查询。
/// - delta 字段只表示上一帧到当前帧的变化量。
/// - 这是教学框架的轻量输入层，不处理按键边沿、文本编辑状态或复杂输入映射。
struct InputState {
  /// @brief 鼠标当前位置，单位为 GLFW 窗口坐标。
  double mouse_x{};
  double mouse_y{};
  /// @brief 鼠标相对上一帧的移动量。
  double mouse_delta_x{};
  double mouse_delta_y{};
  /// @brief 当前帧累计的滚轮偏移；读取后由输入适配器清零。
  double scroll_delta_x{};
  double scroll_delta_y{};

  /// @brief 鼠标按钮当前是否按下。
  bool left_mouse_down{false};
  bool middle_mouse_down{false};
  bool right_mouse_down{false};

  /// @brief 常用修饰键当前是否按下。
  bool alt_down{false};
  bool ctrl_down{false};
  bool shift_down{false};
  bool space_down{false};

  /// @brief 教学样例常用的移动按键状态。
  bool key_w{false};
  bool key_a{false};
  bool key_s{false};
  bool key_d{false};
  bool key_q{false};
  bool key_e{false};

  /// @brief 方向键状态，通常与 WASD 共同映射到相机移动。
  bool key_up{false};
  bool key_down{false};
  bool key_left{false};
  bool key_right{false};
};
}  // namespace lvk
