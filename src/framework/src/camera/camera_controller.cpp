#include "lvk/camera/camera_controller.h"

namespace lvk::camera {
CameraController::CameraController(Camera* camera) : m_camera{camera} {}

void CameraController::Attach(Camera* camera) noexcept { m_camera = camera; }

void CameraController::Detach() noexcept { Attach(nullptr); }

void CameraController::SetCamera(Camera* camera) noexcept { Attach(camera); }

Camera* CameraController::GetCamera() noexcept { return m_camera; }

const Camera* CameraController::GetCamera() const noexcept { return m_camera; }

void CameraController::ResetInteractionState() noexcept {}
}  // namespace lvk::camera
