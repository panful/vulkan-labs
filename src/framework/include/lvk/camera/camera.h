#pragma once

#include "lvk/camera/camera_types.h"

namespace lvk::camera {
class Camera final {
public:
  Camera();
  explicit Camera(const CameraDesc& desc);

  void SetPerspective(double vertical_fov_rad, double aspect_ratio, double near_plane, double far_plane);
  void SetOrthographic(double orthographic_height, double aspect_ratio, double near_plane, double far_plane);
  void SetAspectRatio(double aspect_ratio);
  void LookAt(glm::dvec3 eye, glm::dvec3 target, glm::dvec3 world_up = glm::dvec3{0.0, 1.0, 0.0});
  void SetPose(glm::dvec3 position, glm::dquat rotation) noexcept;
  void SetPosition(glm::dvec3 position) noexcept;
  void SetRotation(glm::dquat rotation) noexcept;

  [[nodiscard]] ProjectionType GetProjectionType() const noexcept;
  [[nodiscard]] double GetAspectRatio() const noexcept;
  [[nodiscard]] double GetOrthographicHeight() const noexcept;
  void SetOrthographicHeight(double orthographic_height);
  [[nodiscard]] const glm::dvec3& GetPosition() const noexcept;
  [[nodiscard]] const glm::dquat& GetRotation() const noexcept;
  [[nodiscard]] glm::dvec3 GetForward() const noexcept;
  [[nodiscard]] glm::dvec3 GetRight() const noexcept;
  [[nodiscard]] glm::dvec3 GetUp() const noexcept;
  [[nodiscard]] glm::dmat4 GetViewMatrix() const;
  [[nodiscard]] glm::dmat4 GetProjectionMatrix(ProjectionConvention convention) const;
  [[nodiscard]] GpuCameraMatrices GetGpuMatrices(ProjectionConvention convention) const;

private:
  static void ValidateProjection(const CameraDesc& desc);
  static void ValidatePerspective(double vertical_fov_rad, double aspect_ratio, double near_plane, double far_plane);
  static void ValidateOrthographic(double orthographic_height, double aspect_ratio, double near_plane,
                                   double far_plane);
  static void ValidateClipPlanes(double near_plane, double far_plane);

private:
  CameraDesc m_desc{};
};
}  // namespace lvk::camera
