#pragma once

#include "lvk/camera/camera.h"

namespace lvk::camera {
struct CameraControllerInput {
  double cursor_x{};
  double cursor_y{};
  double cursor_delta_x{};
  double cursor_delta_y{};
  double scroll_delta_y{};

  bool rotate{};
  bool pan{};
  bool look{};

  bool move_forward{};
  bool move_backward{};
  bool move_left{};
  bool move_right{};
  bool move_up{};
  bool move_down{};
  bool fast{};
  bool slow{};
};

class CameraController {
public:
  CameraController() = default;
  explicit CameraController(Camera* camera);
  virtual ~CameraController() = default;

  CameraController(const CameraController&) = delete;
  CameraController& operator=(const CameraController&) = delete;
  CameraController(CameraController&&) noexcept = default;
  CameraController& operator=(CameraController&&) noexcept = default;

  virtual void Attach(Camera* camera) noexcept;
  virtual void Detach() noexcept;
  void SetCamera(Camera* camera) noexcept;
  [[nodiscard]] Camera* GetCamera() noexcept;
  [[nodiscard]] const Camera* GetCamera() const noexcept;

  virtual void ResetInteractionState() noexcept;
  virtual void Update(double delta_seconds, const CameraControllerInput& input) = 0;

protected:
  Camera* m_camera{nullptr};
};
}  // namespace lvk::camera
