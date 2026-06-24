#pragma once

#include "lvk/camera/camera_controller.h"

namespace lvk::camera {
struct FreeLookCameraControllerDesc {
  glm::dvec3 world_up{0.0, 1.0, 0.0};
  double look_sensitivity{0.005};
  double pitch_limit_rad{glm::radians(89.9)};
};

class FreeLookCameraController : public CameraController {
public:
  explicit FreeLookCameraController(Camera* camera = nullptr);

  void Attach(Camera* camera) noexcept override;
  void SetDesc(const FreeLookCameraControllerDesc& desc);
  [[nodiscard]] const FreeLookCameraControllerDesc& GetDesc() const noexcept;
  void SyncFromCamera();
  void Update(double delta_seconds, const CameraControllerInput& input) override;

protected:
  void ApplyLookDelta(double delta_x, double delta_y);
  void ApplyRotationToCamera();
  [[nodiscard]] glm::dvec3 GetForward() const;
  [[nodiscard]] glm::dvec3 GetRight() const;
  [[nodiscard]] glm::dvec3 GetPlanarForward() const;
  void ValidateDesc() const;

protected:
  FreeLookCameraControllerDesc m_desc{};
  double m_yaw{};
  double m_pitch{};
};

struct FpsCameraControllerDesc : FreeLookCameraControllerDesc {
  double move_speed{3.0};
  double fast_move_multiplier{4.0};
};

class FpsCameraController : public FreeLookCameraController {
public:
  explicit FpsCameraController(Camera* camera = nullptr);

  void SetDesc(const FpsCameraControllerDesc& desc);
  [[nodiscard]] const FpsCameraControllerDesc& GetDesc() const noexcept;
  void Update(double delta_seconds, const CameraControllerInput& input) override;

private:
  [[nodiscard]] glm::dvec3 GetMovementDirection(const CameraControllerInput& input) const;
  void ValidateFpsDesc() const;

private:
  FpsCameraControllerDesc m_fps_desc{};
};

struct FlyCameraControllerDesc : FpsCameraControllerDesc {
  bool enable_roll{false};
  double roll_speed_rad{1.5};
};

class FlyCameraController final : public FreeLookCameraController {
public:
  explicit FlyCameraController(Camera* camera = nullptr);

  void SetDesc(const FlyCameraControllerDesc& desc);
  [[nodiscard]] const FlyCameraControllerDesc& GetDesc() const noexcept;
  void Update(double delta_seconds, const CameraControllerInput& input) override;

private:
  [[nodiscard]] glm::dvec3 GetMovementDirection(const CameraControllerInput& input) const;
  void ValidateFlyDesc() const;

private:
  FlyCameraControllerDesc m_fly_desc{};
};
}  // namespace lvk::camera
