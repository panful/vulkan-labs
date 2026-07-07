#pragma once

#include "lvk/camera/camera_types.h"

namespace lvk::camera {
/// @brief 可在 CPU 侧生成 view/projection 矩阵的相机。
/// @details
/// - 本地坐标约定为右手系：`-Z` 是 forward，`+X` 是 right，`+Y` 是 up。
/// - `GetProjectionMatrix` 通过 `ProjectionConvention` 显式适配 Vulkan/OpenGL 风格差异。
/// - setter 会校验投影参数，避免把 NaN、负裁剪面或非法 FOV 传播到 shader。
class Camera final {
public:
  Camera();
  /// @brief 使用描述对象创建相机；构造时会校验投影参数并规范化旋转。
  explicit Camera(const CameraDesc& desc);

  /// @brief 切换到透视投影。
  void SetPerspective(double vertical_fov_rad, double aspect_ratio, double near_plane, double far_plane);
  /// @brief 切换到正交投影。
  void SetOrthographic(double orthographic_height, double aspect_ratio, double near_plane, double far_plane);
  /// @brief 只更新宽高比，保持当前投影类型和裁剪面。
  void SetAspectRatio(double aspect_ratio);
  /// @brief 让相机从 eye 看向 target。
  /// @throws std::invalid_argument 视线方向或 up 向量非法时抛出。
  void LookAt(glm::dvec3 eye, glm::dvec3 target, glm::dvec3 world_up = glm::dvec3{0.0, 1.0, 0.0});
  /// @brief 同时设置位置和旋转；旋转会被规范化。
  void SetPose(glm::dvec3 position, glm::dquat rotation) noexcept;
  void SetPosition(glm::dvec3 position) noexcept;
  void SetRotation(glm::dquat rotation) noexcept;

  [[nodiscard]] ProjectionType GetProjectionType() const noexcept;
  [[nodiscard]] double GetAspectRatio() const noexcept;
  [[nodiscard]] double GetOrthographicHeight() const noexcept;
  /// @brief 设置正交投影高度；只影响正交投影参数。
  void SetOrthographicHeight(double orthographic_height);
  [[nodiscard]] const glm::dvec3& GetPosition() const noexcept;
  [[nodiscard]] const glm::dquat& GetRotation() const noexcept;
  /// @brief 获取世界空间 forward 方向。
  [[nodiscard]] glm::dvec3 GetForward() const noexcept;
  /// @brief 获取世界空间 right 方向。
  [[nodiscard]] glm::dvec3 GetRight() const noexcept;
  /// @brief 获取世界空间 up 方向。
  [[nodiscard]] glm::dvec3 GetUp() const noexcept;
  /// @brief 获取从世界空间到相机空间的 view 矩阵。
  [[nodiscard]] glm::dmat4 GetViewMatrix() const;
  /// @brief 获取符合指定 GPU 约定的 projection 矩阵。
  [[nodiscard]] glm::dmat4 GetProjectionMatrix(ProjectionConvention convention) const;
  /// @brief 获取常用 GPU uniform 矩阵集合。
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
