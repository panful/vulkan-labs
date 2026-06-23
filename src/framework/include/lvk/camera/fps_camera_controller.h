#pragma once

#include "lvk/camera/camera.h"
#include "lvk/input_state.h"

namespace lvk::camera {
struct FpsCameraControllerDesc {
  double move_speed{3.0};
  double fast_move_multiplier{4.0};
  double look_sensitivity{0.005};
};

class FpsCameraController final {
public:
  explicit FpsCameraController(Camera* camera = nullptr);

  void SetCamera(Camera* camera) noexcept;
  void SetDesc(const FpsCameraControllerDesc& desc);
  [[nodiscard]] const FpsCameraControllerDesc& GetDesc() const noexcept;
  void SyncFromCamera();
  void Update(double delta_seconds, const InputState& input_state);

private:
  void ApplyRotationToCamera();
  void ValidateDesc() const;

private:
  Camera* m_camera{nullptr};
  FpsCameraControllerDesc m_desc{};
  double m_yaw{};
  double m_pitch{};
};
}  // namespace lvk::camera
