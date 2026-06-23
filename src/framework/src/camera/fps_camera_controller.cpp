#include "lvk/camera/fps_camera_controller.h"

#include <algorithm>
#include <cmath>
#include <glm/gtc/quaternion.hpp>
#include <numbers>
#include <stdexcept>

namespace lvk::camera {
namespace {
constexpr double k_pitch_limit{89.9 * std::numbers::pi / 180.0};
constexpr double k_epsilon{1e-12};

[[nodiscard]] glm::dquat RotationFromYawPitch(double yaw, double pitch) {
  const glm::dquat yaw_rotation{glm::angleAxis(yaw, glm::dvec3{0.0, 1.0, 0.0})};
  const glm::dquat pitch_rotation{glm::angleAxis(pitch, glm::dvec3{1.0, 0.0, 0.0})};
  return glm::normalize(yaw_rotation * pitch_rotation);
}

[[nodiscard]] double LengthSquared(glm::dvec3 value) noexcept { return glm::dot(value, value); }
}  // namespace

FpsCameraController::FpsCameraController(Camera* camera) : m_camera(camera) { SyncFromCamera(); }

void FpsCameraController::SetCamera(Camera* camera) noexcept {
  m_camera = camera;
  SyncFromCamera();
}

void FpsCameraController::SetDesc(const FpsCameraControllerDesc& desc) {
  m_desc = desc;
  ValidateDesc();
}

const FpsCameraControllerDesc& FpsCameraController::GetDesc() const noexcept { return m_desc; }

void FpsCameraController::SyncFromCamera() {
  if (nullptr == m_camera) {
    m_yaw = 0.0;
    m_pitch = 0.0;
    return;
  }

  const glm::dvec3 forward{m_camera->GetForward()};
  m_pitch = std::asin(std::clamp(forward.y, -1.0, 1.0));
  m_yaw = std::atan2(-forward.x, -forward.z);
}

void FpsCameraController::Update(double delta_seconds, const InputState& input_state) {
  if (nullptr == m_camera) {
    return;
  }

  ValidateDesc();

  if (input_state.right_mouse_down) {
    m_yaw -= input_state.mouse_delta_x * m_desc.look_sensitivity;
    m_pitch -= input_state.mouse_delta_y * m_desc.look_sensitivity;
    m_pitch = std::clamp(m_pitch, -k_pitch_limit, k_pitch_limit);
    ApplyRotationToCamera();
  }

  glm::dvec3 local_movement{0.0, 0.0, 0.0};
  if (input_state.key_w || input_state.key_up) {
    local_movement.z -= 1.0;
  }
  if (input_state.key_s || input_state.key_down) {
    local_movement.z += 1.0;
  }
  if (input_state.key_a || input_state.key_left) {
    local_movement.x -= 1.0;
  }
  if (input_state.key_d || input_state.key_right) {
    local_movement.x += 1.0;
  }
  if (input_state.key_e || input_state.space_down) {
    local_movement.y += 1.0;
  }
  if (input_state.key_q) {
    local_movement.y -= 1.0;
  }

  if (LengthSquared(local_movement) <= k_epsilon) {
    return;
  }

  const double speed{m_desc.move_speed * (input_state.shift_down ? m_desc.fast_move_multiplier : 1.0)};
  const double distance{speed * delta_seconds};
  const glm::dvec3 direction{glm::normalize(m_camera->GetRight() * local_movement.x +
                                            m_camera->GetUp() * local_movement.y +
                                            m_camera->GetForward() * -local_movement.z)};
  m_camera->SetPosition(m_camera->GetPosition() + direction * distance);
}

void FpsCameraController::ApplyRotationToCamera() {
  if (nullptr == m_camera) {
    return;
  }

  m_camera->SetRotation(RotationFromYawPitch(m_yaw, m_pitch));
}

void FpsCameraController::ValidateDesc() const {
  if (!std::isfinite(m_desc.move_speed) || m_desc.move_speed < 0.0 || !std::isfinite(m_desc.fast_move_multiplier) ||
      m_desc.fast_move_multiplier < 1.0 || !std::isfinite(m_desc.look_sensitivity) || m_desc.look_sensitivity <= 0.0) {
    throw std::invalid_argument("invalid FPS camera controller settings");
  }
}
}  // namespace lvk::camera
