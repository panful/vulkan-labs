#include "lvk/camera/fps_camera_controller.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace lvk::camera {
namespace {
constexpr double k_epsilon{1e-12};

[[nodiscard]] bool IsFinitePositive(double value) noexcept { return std::isfinite(value) && value > 0.0; }

[[nodiscard]] bool IsFiniteNonNegative(double value) noexcept { return std::isfinite(value) && value >= 0.0; }

[[nodiscard]] double LengthSquared(glm::dvec3 value) noexcept { return glm::dot(value, value); }

[[nodiscard]] glm::dvec3 NormalizeOrFallback(glm::dvec3 value, glm::dvec3 fallback) {
  if (LengthSquared(value) <= k_epsilon) {
    return glm::normalize(fallback);
  }
  return glm::normalize(value);
}

[[nodiscard]] glm::dvec3 GetReferenceForward(glm::dvec3 world_up) {
  // 当 world_up 接近 Y 轴时，用 -Z 作为参考方向；否则用 Y 轴投影到水平面。
  const glm::dvec3 fallback_forward{std::abs(world_up.y) < 0.9 ? glm::dvec3{0.0, 1.0, 0.0}
                                                               : glm::dvec3{0.0, 0.0, -1.0}};
  return NormalizeOrFallback(fallback_forward - world_up * glm::dot(fallback_forward, world_up),
                             glm::dvec3{0.0, 0.0, -1.0});
}

[[nodiscard]] glm::dvec3 GetReferenceRight(glm::dvec3 reference_forward, glm::dvec3 world_up) {
  return NormalizeOrFallback(glm::cross(reference_forward, world_up), glm::dvec3{1.0, 0.0, 0.0});
}
}  // namespace

FreeLookCameraController::FreeLookCameraController(Camera* camera) : CameraController{camera} { SyncFromCamera(); }

void FreeLookCameraController::Attach(Camera* camera) noexcept {
  CameraController::Attach(camera);
  SyncFromCamera();
}

void FreeLookCameraController::SetDesc(const FreeLookCameraControllerDesc& desc) {
  m_desc = desc;
  ValidateDesc();
  SyncFromCamera();
}

const FreeLookCameraControllerDesc& FreeLookCameraController::GetDesc() const noexcept { return m_desc; }

void FreeLookCameraController::SyncFromCamera() {
  if (nullptr == m_camera) {
    m_yaw = 0.0;
    m_pitch = 0.0;
    return;
  }

  const glm::dvec3 world_up{NormalizeOrFallback(m_desc.world_up, glm::dvec3{0.0, 1.0, 0.0})};
  const glm::dvec3 forward{m_camera->GetForward()};
  const double up_component{std::clamp(glm::dot(forward, world_up), -1.0, 1.0)};
  const glm::dvec3 planar_forward{NormalizeOrFallback(forward - world_up * up_component, m_camera->GetForward())};
  const glm::dvec3 reference_forward{GetReferenceForward(world_up)};
  const glm::dvec3 reference_right{GetReferenceRight(reference_forward, world_up)};

  // 从当前 forward 反推 yaw/pitch，保证切换控制器或重置相机后不会产生方向跳变。
  m_pitch = std::asin(up_component);
  m_yaw = std::atan2(glm::dot(planar_forward, reference_right), glm::dot(planar_forward, reference_forward));
}

void FreeLookCameraController::Update([[maybe_unused]] double delta_seconds, const CameraControllerInput& input) {
  if (input.look) {
    ApplyLookDelta(input.cursor_delta_x, input.cursor_delta_y);
  }
}

void FreeLookCameraController::ApplyLookDelta(double delta_x, double delta_y) {
  if (nullptr == m_camera) {
    return;
  }

  ValidateDesc();
  m_yaw += delta_x * m_desc.look_sensitivity;
  m_pitch -= delta_y * m_desc.look_sensitivity;
  // 限制 pitch，避免 forward 与 world_up 平行时 right 向量退化。
  m_pitch = std::clamp(m_pitch, -m_desc.pitch_limit_rad, m_desc.pitch_limit_rad);
  ApplyRotationToCamera();
}

void FreeLookCameraController::ApplyRotationToCamera() {
  if (nullptr == m_camera) {
    return;
  }

  const glm::dvec3 position{m_camera->GetPosition()};
  m_camera->LookAt(position, position + GetForward(), m_desc.world_up);
}

glm::dvec3 FreeLookCameraController::GetForward() const {
  const glm::dvec3 world_up{NormalizeOrFallback(m_desc.world_up, glm::dvec3{0.0, 1.0, 0.0})};
  const glm::dvec3 reference_forward{GetReferenceForward(world_up)};
  const glm::dvec3 reference_right{GetReferenceRight(reference_forward, world_up)};
  const glm::dvec3 planar_forward{std::cos(m_yaw) * reference_forward + std::sin(m_yaw) * reference_right};
  return glm::normalize(std::cos(m_pitch) * planar_forward + std::sin(m_pitch) * world_up);
}

glm::dvec3 FreeLookCameraController::GetRight() const {
  return NormalizeOrFallback(glm::cross(GetForward(), m_desc.world_up), glm::dvec3{1.0, 0.0, 0.0});
}

glm::dvec3 FreeLookCameraController::GetPlanarForward() const {
  const glm::dvec3 world_up{NormalizeOrFallback(m_desc.world_up, glm::dvec3{0.0, 1.0, 0.0})};
  return NormalizeOrFallback(GetForward() - world_up * glm::dot(GetForward(), world_up), GetForward());
}

void FreeLookCameraController::ValidateDesc() const {
  if (LengthSquared(m_desc.world_up) <= k_epsilon || !IsFinitePositive(m_desc.look_sensitivity) ||
      !IsFinitePositive(m_desc.pitch_limit_rad) || m_desc.pitch_limit_rad >= glm::radians(90.0)) {
    throw std::invalid_argument("invalid free-look camera controller settings");
  }
}

FpsCameraController::FpsCameraController(Camera* camera) : FreeLookCameraController{camera} { SetDesc(m_fps_desc); }

void FpsCameraController::SetDesc(const FpsCameraControllerDesc& desc) {
  m_fps_desc = desc;
  m_desc.world_up = desc.world_up;
  m_desc.look_sensitivity = desc.look_sensitivity;
  m_desc.pitch_limit_rad = desc.pitch_limit_rad;
  ValidateFpsDesc();
  SyncFromCamera();
}

const FpsCameraControllerDesc& FpsCameraController::GetDesc() const noexcept { return m_fps_desc; }

void FpsCameraController::Update(double delta_seconds, const CameraControllerInput& input) {
  FreeLookCameraController::Update(delta_seconds, input);
  if (nullptr == m_camera) {
    return;
  }

  ValidateFpsDesc();
  const glm::dvec3 movement{GetMovementDirection(input)};
  if (LengthSquared(movement) <= k_epsilon) {
    return;
  }

  const double speed{m_fps_desc.move_speed * (input.fast ? m_fps_desc.fast_move_multiplier : 1.0)};
  m_camera->SetPosition(m_camera->GetPosition() + glm::normalize(movement) * speed * delta_seconds);
}

glm::dvec3 FpsCameraController::GetMovementDirection(const CameraControllerInput& input) const {
  glm::dvec3 movement{0.0};
  if (input.move_forward) {
    // FPS 相机前后移动使用水平面方向，避免抬头时 W 键把相机带离地面。
    movement += GetPlanarForward();
  }
  if (input.move_backward) {
    movement -= GetPlanarForward();
  }
  if (input.move_right) {
    movement += GetRight();
  }
  if (input.move_left) {
    movement -= GetRight();
  }
  if (input.move_up) {
    movement += NormalizeOrFallback(m_fps_desc.world_up, glm::dvec3{0.0, 1.0, 0.0});
  }
  if (input.move_down) {
    movement -= NormalizeOrFallback(m_fps_desc.world_up, glm::dvec3{0.0, 1.0, 0.0});
  }
  return movement;
}

void FpsCameraController::ValidateFpsDesc() const {
  ValidateDesc();
  if (!IsFiniteNonNegative(m_fps_desc.move_speed) || !IsFinitePositive(m_fps_desc.fast_move_multiplier)) {
    throw std::invalid_argument("invalid FPS camera controller settings");
  }
}

FlyCameraController::FlyCameraController(Camera* camera) : FreeLookCameraController{camera} { SetDesc(m_fly_desc); }

void FlyCameraController::SetDesc(const FlyCameraControllerDesc& desc) {
  m_fly_desc = desc;
  m_desc.world_up = desc.world_up;
  m_desc.look_sensitivity = desc.look_sensitivity;
  m_desc.pitch_limit_rad = desc.pitch_limit_rad;
  ValidateFlyDesc();
  SyncFromCamera();
}

const FlyCameraControllerDesc& FlyCameraController::GetDesc() const noexcept { return m_fly_desc; }

void FlyCameraController::Update(double delta_seconds, const CameraControllerInput& input) {
  FreeLookCameraController::Update(delta_seconds, input);
  if (nullptr == m_camera) {
    return;
  }

  ValidateFlyDesc();
  const glm::dvec3 movement{GetMovementDirection(input)};
  if (LengthSquared(movement) <= k_epsilon) {
    return;
  }

  const double speed{m_fly_desc.move_speed * (input.fast ? m_fly_desc.fast_move_multiplier : 1.0)};
  m_camera->SetPosition(m_camera->GetPosition() + glm::normalize(movement) * speed * delta_seconds);
}

glm::dvec3 FlyCameraController::GetMovementDirection(const CameraControllerInput& input) const {
  glm::dvec3 movement{0.0};
  if (input.move_forward) {
    // Fly 相机沿真实 forward 飞行，适合自由浏览 3D 场景。
    movement += GetForward();
  }
  if (input.move_backward) {
    movement -= GetForward();
  }
  if (input.move_right) {
    movement += GetRight();
  }
  if (input.move_left) {
    movement -= GetRight();
  }
  if (input.move_up) {
    movement += m_camera ? m_camera->GetUp() : NormalizeOrFallback(m_fly_desc.world_up, glm::dvec3{0.0, 1.0, 0.0});
  }
  if (input.move_down) {
    movement -= m_camera ? m_camera->GetUp() : NormalizeOrFallback(m_fly_desc.world_up, glm::dvec3{0.0, 1.0, 0.0});
  }
  return movement;
}

void FlyCameraController::ValidateFlyDesc() const {
  ValidateDesc();
  if (!IsFiniteNonNegative(m_fly_desc.move_speed) || !IsFinitePositive(m_fly_desc.fast_move_multiplier)) {
    throw std::invalid_argument("invalid fly camera controller settings");
  }
}
}  // namespace lvk::camera
