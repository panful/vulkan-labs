#pragma once

#include "lvk/camera/camera_controller.h"

namespace lvk::camera {
/// @brief 轨道相机控制参数。
struct OrbitCameraControllerDesc {
  /// @brief 轨道中心点。
  glm::dvec3 target{0.0, 0.0, 0.0};
  /// @brief 世界空间 up 方向。
  glm::dvec3 world_up{0.0, 1.0, 0.0};
  /// @brief 相机到 target 的距离。
  double distance{5.0};
  /// @brief dolly 后允许的最小距离。
  double min_distance{0.01};
  /// @brief dolly 后允许的最大距离。
  double max_distance{1.0e9};
  /// @brief 鼠标旋转灵敏度，单位为弧度/输入单位。
  double rotate_sensitivity{0.005};
  /// @brief 平移灵敏度，会乘以当前 distance。
  double pan_sensitivity{0.0015};
  /// @brief 滚轮缩放灵敏度，范围必须在 `(0, 1)`。
  double dolly_sensitivity{0.12};
  /// @brief yaw/pitch 轨道相机的 pitch 绝对值上限。
  double pitch_limit_rad{glm::radians(89.9)};
  /// @brief arcball 映射使用的视口宽度。
  double viewport_width{1.0};
  /// @brief arcball 映射使用的视口高度。
  double viewport_height{1.0};
};

/// @brief 轨道相机控制器公共基类。
/// @details 管理 target、distance、pan、dolly 等共同逻辑；具体旋转方式由派生类决定。
class OrbitCameraController : public CameraController {
public:
  explicit OrbitCameraController(Camera* camera = nullptr);

  void Attach(Camera* camera) noexcept override;
  void SetDesc(const OrbitCameraControllerDesc& desc);
  [[nodiscard]] const OrbitCameraControllerDesc& GetDesc() const noexcept;
  /// @brief 设置轨道中心和距离。
  virtual void Focus(glm::dvec3 target, double distance);
  /// @brief 设置 arcball 映射使用的视口尺寸。
  void SetViewportSize(double width, double height);
  /// @brief 从当前相机和 target 同步控制器内部状态。
  virtual void SyncFromCameraAndTarget();

protected:
  void ValidateDesc() const;
  void ValidateDistance();
  void ApplyCommonPanAndDolly(const CameraControllerInput& input);
  [[nodiscard]] glm::dvec3 GetWorldUp() const;

protected:
  OrbitCameraControllerDesc m_desc{};
};

/// @brief yaw/pitch 风格轨道相机。
/// @details 操作方式接近常见 DCC/编辑器相机：水平 yaw、垂直 pitch，并限制 pitch 避免翻转。
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

/// @brief arcball 风格轨道相机。
/// @details 将鼠标位置映射到虚拟球面，适合展示物体时获得更自然的旋转手感。
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
