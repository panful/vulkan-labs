#pragma once

namespace lvk {
struct InputState {
  double mouse_x{};
  double mouse_y{};
  double mouse_delta_x{};
  double mouse_delta_y{};
  double scroll_delta_x{};
  double scroll_delta_y{};

  bool left_mouse_down{false};
  bool middle_mouse_down{false};
  bool right_mouse_down{false};

  bool alt_down{false};
  bool ctrl_down{false};
  bool shift_down{false};
  bool space_down{false};

  bool key_w{false};
  bool key_a{false};
  bool key_s{false};
  bool key_d{false};
  bool key_q{false};
  bool key_e{false};

  bool key_up{false};
  bool key_down{false};
  bool key_left{false};
  bool key_right{false};
};
}  // namespace lvk
