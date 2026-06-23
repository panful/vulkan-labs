#pragma once

#include "lvk/camera/camera.h"
#include "lvk/input_state.h"

namespace lvk::camera {
struct OrbitCameraControllerDesc {
  glm::dvec3 target{0.0, 0.0, 0.0};
  double distance{5.0};
  double min_distance{0.01};
  double max_distance{1.0e9};
  double rotate_sensitivity{0.005};
  double pan_sensitivity{0.0015};
  double dolly_sensitivity{0.12};
};

class OrbitCameraController final {
public:
  explicit OrbitCameraController(Camera* camera = nullptr);

  void SetCamera(Camera* camera) noexcept;
  void SetDesc(const OrbitCameraControllerDesc& desc);
  [[nodiscard]] const OrbitCameraControllerDesc& GetDesc() const noexcept;
  void Focus(glm::dvec3 target, double distance);
  void SyncFromCameraAndTarget();
  void Update(double delta_seconds, const InputState& input_state);

private:
  void ApplyToCamera();
  void ValidateDistance();

private:
  Camera* m_camera{nullptr};
  OrbitCameraControllerDesc m_desc{};
  double m_yaw{};
  double m_pitch{};
};
}  // namespace lvk::camera
