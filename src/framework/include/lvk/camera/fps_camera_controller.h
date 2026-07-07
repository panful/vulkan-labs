#pragma once

#include "lvk/camera/camera_controller.h"

namespace lvk::camera {
/// @brief 自由视角相机的朝向控制参数。
struct FreeLookCameraControllerDesc {
  /// @brief 世界空间 up 方向，用于限制 pitch 并构建 yaw 参考平面。
  glm::dvec3 world_up{0.0, 1.0, 0.0};
  /// @brief 鼠标每移动 1 个单位对应的旋转弧度。
  double look_sensitivity{0.005};
  /// @brief pitch 绝对值上限，避免视线与 up 方向完全平行。
  double pitch_limit_rad{glm::radians(89.9)};
};

/// @brief 只负责 yaw/pitch 视角旋转的相机控制器。
/// @details 该类作为 FPS/Fly 控制器的公共基础，也可以单独用于只看不移动的调试相机。
class FreeLookCameraController : public CameraController {
public:
  explicit FreeLookCameraController(Camera* camera = nullptr);

  void Attach(Camera* camera) noexcept override;
  void SetDesc(const FreeLookCameraControllerDesc& desc);
  [[nodiscard]] const FreeLookCameraControllerDesc& GetDesc() const noexcept;
  /// @brief 从当前相机朝向反推出控制器内部 yaw/pitch。
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

/// @brief 第一人称相机移动参数。
struct FpsCameraControllerDesc : FreeLookCameraControllerDesc {
  /// @brief 基础移动速度，单位为世界空间长度/秒。
  double move_speed{3.0};
  /// @brief `input.fast` 为 true 时使用的速度倍率。
  double fast_move_multiplier{4.0};
};

/// @brief 第一人称相机控制器。
/// @details 前后移动会投影到水平面，适合“人在地面上走”的观察方式。
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

/// @brief 飞行相机移动参数。
/// @details 当前与 FPS 参数相同；保留独立类型便于后续扩展 roll 或垂直移动策略。
struct FlyCameraControllerDesc : FpsCameraControllerDesc {};

/// @brief 飞行相机控制器。
/// @details 前后移动沿相机 forward 方向，适合自由穿梭 3D 场景。
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
