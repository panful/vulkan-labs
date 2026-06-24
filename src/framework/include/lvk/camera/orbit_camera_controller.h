#pragma once

#include "lvk/camera/camera_controller.h"

namespace lvk::camera {
struct OrbitCameraControllerDesc {
  glm::dvec3 target{0.0, 0.0, 0.0};
  glm::dvec3 world_up{0.0, 1.0, 0.0};
  double distance{5.0};
  double min_distance{0.01};
  double max_distance{1.0e9};
  double rotate_sensitivity{0.005};
  double pan_sensitivity{0.0015};
  double dolly_sensitivity{0.12};
  double pitch_limit_rad{glm::radians(89.9)};
  double viewport_width{1.0};
  double viewport_height{1.0};
};

class OrbitCameraController : public CameraController {
public:
  explicit OrbitCameraController(Camera* camera = nullptr);

  void Attach(Camera* camera) noexcept override;
  void SetDesc(const OrbitCameraControllerDesc& desc);
  [[nodiscard]] const OrbitCameraControllerDesc& GetDesc() const noexcept;
  virtual void Focus(glm::dvec3 target, double distance);
  void SetViewportSize(double width, double height);
  virtual void SyncFromCameraAndTarget();

protected:
  void ValidateDesc() const;
  void ValidateDistance();
  void ApplyCommonPanAndDolly(const CameraControllerInput& input);
  [[nodiscard]] glm::dvec3 GetWorldUp() const;

protected:
  OrbitCameraControllerDesc m_desc{};
};

class YawPitchOrbitCameraController final : public OrbitCameraController {
public:
  explicit YawPitchOrbitCameraController(Camera* camera = nullptr);

  void Update(double delta_seconds, const CameraControllerInput& input) override;
  void Focus(glm::dvec3 target, double distance) override;
  void SyncFromCameraAndTarget() override;

private:
  void ApplyToCamera();
  [[nodiscard]] glm::dvec3 GetForward() const;

private:
  double m_yaw{};
  double m_pitch{};
};

class ArcballOrbitCameraController final : public OrbitCameraController {
public:
  explicit ArcballOrbitCameraController(Camera* camera = nullptr);

  void Update(double delta_seconds, const CameraControllerInput& input) override;
  void Focus(glm::dvec3 target, double distance) override;
  void SyncFromCameraAndTarget() override;
  void ResetInteractionState() noexcept override;

private:
  void ApplyToCamera();
  [[nodiscard]] glm::dvec3 MapToArcball(double mouse_x, double mouse_y) const;

private:
  glm::dquat m_rotation{1.0, 0.0, 0.0, 0.0};
  glm::dvec3 m_last_arcball_vector{0.0, 0.0, 1.0};
  bool m_was_rotating{false};
};
}  // namespace lvk::camera
