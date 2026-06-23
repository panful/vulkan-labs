#include "lvk/camera/orbit_camera_controller.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace lvk::camera {
namespace {
constexpr double k_pitch_limit{89.9 * std::numbers::pi / 180.0};

[[nodiscard]] glm::dquat RotationFromYawPitch(double yaw, double pitch) {
  const glm::dquat yaw_rotation{glm::angleAxis(yaw, glm::dvec3{0.0, 1.0, 0.0})};
  const glm::dquat pitch_rotation{glm::angleAxis(pitch, glm::dvec3{1.0, 0.0, 0.0})};
  return glm::normalize(yaw_rotation * pitch_rotation);
}
}  // namespace

OrbitCameraController::OrbitCameraController(Camera* camera) : m_camera(camera) { SyncFromCameraAndTarget(); }

void OrbitCameraController::SetCamera(Camera* camera) noexcept {
  m_camera = camera;
  SyncFromCameraAndTarget();
}

void OrbitCameraController::SetDesc(const OrbitCameraControllerDesc& desc) {
  m_desc = desc;
  ValidateDistance();
  ApplyToCamera();
}

const OrbitCameraControllerDesc& OrbitCameraController::GetDesc() const noexcept { return m_desc; }

void OrbitCameraController::Focus(glm::dvec3 target, double distance) {
  m_desc.target = target;
  m_desc.distance = distance;
  ValidateDistance();
  ApplyToCamera();
}

void OrbitCameraController::SyncFromCameraAndTarget() {
  if (nullptr == m_camera) {
    m_yaw = 0.0;
    m_pitch = 0.0;
    return;
  }

  const glm::dvec3 offset{m_camera->GetPosition() - m_desc.target};
  const double distance{glm::length(offset)};
  if (distance > 1e-12) {
    m_desc.distance = std::clamp(distance, m_desc.min_distance, m_desc.max_distance);
  }

  const glm::dvec3 forward{m_camera->GetForward()};
  m_pitch = std::asin(std::clamp(forward.y, -1.0, 1.0));
  m_yaw = std::atan2(-forward.x, -forward.z);
}

void OrbitCameraController::Update([[maybe_unused]] double delta_seconds, const InputState& input_state) {
  if (nullptr == m_camera) {
    return;
  }

  if (input_state.left_mouse_down) {
    m_yaw -= input_state.mouse_delta_x * m_desc.rotate_sensitivity;
    m_pitch -= input_state.mouse_delta_y * m_desc.rotate_sensitivity;
    m_pitch = std::clamp(m_pitch, -k_pitch_limit, k_pitch_limit);
  }

  if (input_state.middle_mouse_down || input_state.right_mouse_down) {
    const double pan_scale{m_desc.pan_sensitivity * m_desc.distance};
    m_desc.target -= m_camera->GetRight() * input_state.mouse_delta_x * pan_scale;
    m_desc.target += m_camera->GetUp() * input_state.mouse_delta_y * pan_scale;
  }

  if (0.0 != input_state.scroll_delta_y) {
    const double factor{std::pow(1.0 - m_desc.dolly_sensitivity, input_state.scroll_delta_y)};
    m_desc.distance = std::clamp(m_desc.distance * factor, m_desc.min_distance, m_desc.max_distance);
  }

  ApplyToCamera();
}

void OrbitCameraController::ApplyToCamera() {
  if (nullptr == m_camera) {
    return;
  }

  ValidateDistance();
  const glm::dquat rotation{RotationFromYawPitch(m_yaw, m_pitch)};
  const glm::dvec3 forward{glm::normalize(rotation * glm::dvec3{0.0, 0.0, -1.0})};
  const glm::dvec3 position{m_desc.target - forward * m_desc.distance};
  m_camera->SetPose(position, rotation);
}

void OrbitCameraController::ValidateDistance() {
  if (!std::isfinite(m_desc.min_distance) || !std::isfinite(m_desc.max_distance) || m_desc.min_distance <= 0.0 ||
      m_desc.max_distance < m_desc.min_distance || !std::isfinite(m_desc.distance) || m_desc.distance <= 0.0) {
    throw std::invalid_argument("invalid orbit camera distance");
  }

  m_desc.distance = std::clamp(m_desc.distance, m_desc.min_distance, m_desc.max_distance);
}
}  // namespace lvk::camera
