#include "lvk/camera/camera_controller.h"

namespace lvk::camera {
CameraController::CameraController(Camera* camera) : m_camera{camera} {}

void CameraController::Attach(Camera* camera) noexcept { m_camera = camera; }

void CameraController::Detach() noexcept { Attach(nullptr); }

void CameraController::SetCamera(Camera* camera) noexcept { Attach(camera); }

Camera* CameraController::GetCamera() noexcept { return m_camera; }

const Camera* CameraController::GetCamera() const noexcept { return m_camera; }

void CameraController::ResetInteractionState() noexcept {
  // 基类没有跨帧交互状态；需要清理拖拽/arcball 状态的控制器在派生类中覆盖。
}
}  // namespace lvk::camera
