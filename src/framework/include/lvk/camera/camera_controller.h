#pragma once

#include "lvk/camera/camera.h"

namespace lvk::camera {
/// @brief 相机控制器每帧消费的抽象输入。
/// @details
/// - 该结构不直接暴露 GLFW 键码，方便样例把鼠标/键盘/ImGui 捕获状态先映射成领域语义。
/// - `fast` 通常由 Shift 映射，用于临时加速 FPS/Fly 相机移动。
struct CameraControllerInput {
  /// @brief 当前光标位置，单位由调用方输入系统决定，当前 GLFW 适配层使用窗口坐标。
  double cursor_x{};
  double cursor_y{};
  /// @brief 当前帧光标移动量。
  double cursor_delta_x{};
  double cursor_delta_y{};
  /// @brief 当前帧滚轮纵向偏移。
  double scroll_delta_y{};

  /// @brief 轨道相机旋转输入。
  bool rotate{};
  /// @brief 轨道相机平移输入。
  bool pan{};
  /// @brief 自由视角相机转头输入。
  bool look{};

  /// @brief 六自由度/第一人称移动输入。
  bool move_forward{};
  bool move_backward{};
  bool move_left{};
  bool move_right{};
  bool move_up{};
  bool move_down{};
  bool fast{};
};

/// @brief 相机控制器基类。
/// @details
/// - 控制器不拥有 `Camera`，只保存可空观察指针。
/// - 派生类可以在没有相机时安全接收输入，但不会修改任何状态到外部相机。
class CameraController {
public:
  CameraController() = default;
  explicit CameraController(Camera* camera);
  virtual ~CameraController() = default;

  CameraController(const CameraController&) = delete;
  CameraController& operator=(const CameraController&) = delete;
  CameraController(CameraController&&) noexcept = default;
  CameraController& operator=(CameraController&&) noexcept = default;

  /// @brief 绑定要控制的相机；传入 nullptr 表示暂时不控制任何相机。
  virtual void Attach(Camera* camera) noexcept;
  /// @brief 解除当前相机绑定。
  virtual void Detach() noexcept;
  void SetCamera(Camera* camera) noexcept;
  [[nodiscard]] Camera* GetCamera() noexcept;
  [[nodiscard]] const Camera* GetCamera() const noexcept;

  /// @brief 重置拖拽、arcball 等跨帧交互状态。
  virtual void ResetInteractionState() noexcept;
  /// @brief 根据一帧输入更新相机。
  virtual void Update(double delta_seconds, const CameraControllerInput& input) = 0;

protected:
  Camera* m_camera{nullptr};
};
}  // namespace lvk::camera
