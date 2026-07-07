#include "lvk/camera/orbit_camera_controller.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace lvk::camera {
namespace {
constexpr double k_epsilon{1e-12};

[[nodiscard]] bool IsFinitePositive(double value) noexcept { return std::isfinite(value) && value > 0.0; }

[[nodiscard]] double LengthSquared(glm::dvec3 value) noexcept { return glm::dot(value, value); }

[[nodiscard]] glm::dvec3 NormalizeOrFallback(glm::dvec3 value, glm::dvec3 fallback) {
  if (LengthSquared(value) <= k_epsilon) {
    return glm::normalize(fallback);
  }
  return glm::normalize(value);
}

[[nodiscard]] glm::dquat RotationFromTo(glm::dvec3 from, glm::dvec3 to) {
  const glm::dvec3 safe_from{NormalizeOrFallback(from, glm::dvec3{0.0, 0.0, 1.0})};
  const glm::dvec3 safe_to{NormalizeOrFallback(to, glm::dvec3{0.0, 0.0, 1.0})};
  const double dot_value{std::clamp(glm::dot(safe_from, safe_to), -1.0, 1.0)};
  if (dot_value > 1.0 - k_epsilon) {
    return glm::dquat{1.0, 0.0, 0.0, 0.0};
  }
  if (dot_value < -1.0 + k_epsilon) {
    // 反向向量的旋转轴不唯一，选择一个与 from 不平行的轴作为稳定退化处理。
    const glm::dvec3 axis{NormalizeOrFallback(glm::cross(safe_from, glm::dvec3{1.0, 0.0, 0.0}),
                                              glm::cross(safe_from, glm::dvec3{0.0, 1.0, 0.0}))};
    return glm::angleAxis(std::numbers::pi, axis);
  }

  const glm::dvec3 axis{glm::cross(safe_from, safe_to)};
  return glm::normalize(glm::dquat{1.0 + dot_value, axis.x, axis.y, axis.z});
}
}  // namespace

OrbitCameraController::OrbitCameraController(Camera* camera) : CameraController{camera} {}

void OrbitCameraController::Attach(Camera* camera) noexcept {
  CameraController::Attach(camera);
  SyncFromCameraAndTarget();
}

void OrbitCameraController::SetDesc(const OrbitCameraControllerDesc& desc) {
  m_desc = desc;
  ValidateDesc();
  ValidateDistance();
}

const OrbitCameraControllerDesc& OrbitCameraController::GetDesc() const noexcept { return m_desc; }

void OrbitCameraController::Focus(glm::dvec3 target, double distance) {
  m_desc.target = target;
  m_desc.distance = distance;
  ValidateDistance();
}

void OrbitCameraController::SetViewportSize(double width, double height) {
  if (!IsFinitePositive(width) || !IsFinitePositive(height)) {
    throw std::invalid_argument("invalid orbit camera viewport size");
  }
  m_desc.viewport_width = width;
  m_desc.viewport_height = height;
}

void OrbitCameraController::SyncFromCameraAndTarget() {}

void OrbitCameraController::ValidateDesc() const {
  if (LengthSquared(m_desc.world_up) <= k_epsilon || !IsFinitePositive(m_desc.min_distance) ||
      !IsFinitePositive(m_desc.max_distance) || m_desc.max_distance < m_desc.min_distance ||
      !IsFinitePositive(m_desc.distance) || !IsFinitePositive(m_desc.rotate_sensitivity) ||
      !IsFinitePositive(m_desc.pan_sensitivity) || !IsFinitePositive(m_desc.dolly_sensitivity) ||
      m_desc.dolly_sensitivity >= 1.0 || !IsFinitePositive(m_desc.pitch_limit_rad) ||
      m_desc.pitch_limit_rad >= glm::radians(90.0) || !IsFinitePositive(m_desc.viewport_width) ||
      !IsFinitePositive(m_desc.viewport_height)) {
    throw std::invalid_argument("invalid orbit camera controller settings");
  }
}

void OrbitCameraController::ValidateDistance() {
  ValidateDesc();
  m_desc.distance = std::clamp(m_desc.distance, m_desc.min_distance, m_desc.max_distance);
}

void OrbitCameraController::ApplyCommonPanAndDolly(const CameraControllerInput& input) {
  if (nullptr == m_camera) {
    return;
  }

  if (input.pan) {
    // pan 距离随相机距离放大，远处观察时拖拽手感更接近屏幕空间平移。
    const double pan_scale{m_desc.pan_sensitivity * m_desc.distance};
    m_desc.target -= m_camera->GetRight() * input.cursor_delta_x * pan_scale;
    m_desc.target += m_camera->GetUp() * input.cursor_delta_y * pan_scale;
  }

  if (0.0 != input.scroll_delta_y) {
    // 使用指数缩放让滚轮放大/缩小在不同距离下保持相近手感。
    const double factor{std::pow(1.0 - m_desc.dolly_sensitivity, input.scroll_delta_y)};
    m_desc.distance = std::clamp(m_desc.distance * factor, m_desc.min_distance, m_desc.max_distance);
  }
}

glm::dvec3 OrbitCameraController::GetWorldUp() const {
  return NormalizeOrFallback(m_desc.world_up, glm::dvec3{0.0, 1.0, 0.0});
}

YawPitchOrbitCameraController::YawPitchOrbitCameraController(Camera* camera) : OrbitCameraController{camera} {
  SyncFromCameraAndTarget();
}

void YawPitchOrbitCameraController::Update([[maybe_unused]] double delta_seconds, const CameraControllerInput& input) {
  if (nullptr == m_camera) {
    return;
  }

  ValidateDistance();
  if (input.rotate) {
    m_yaw += input.cursor_delta_x * m_desc.rotate_sensitivity;
    m_pitch -= input.cursor_delta_y * m_desc.rotate_sensitivity;
    m_pitch = std::clamp(m_pitch, -m_desc.pitch_limit_rad, m_desc.pitch_limit_rad);
  }

  ApplyCommonPanAndDolly(input);
  ApplyToCamera();
}

void YawPitchOrbitCameraController::Focus(glm::dvec3 target, double distance) {
  OrbitCameraController::Focus(target, distance);
  SyncFromCameraAndTarget();
  ApplyToCamera();
}

void YawPitchOrbitCameraController::SyncFromCameraAndTarget() {
  if (nullptr == m_camera) {
    m_yaw = 0.0;
    m_pitch = 0.0;
    return;
  }

  const glm::dvec3 world_up{GetWorldUp()};
  const glm::dvec3 forward{m_camera->GetForward()};
  const double up_component{std::clamp(glm::dot(forward, world_up), -1.0, 1.0)};
  const glm::dvec3 planar_forward{NormalizeOrFallback(forward - world_up * up_component, forward)};
  const glm::dvec3 fallback_forward{std::abs(world_up.y) < 0.9 ? glm::dvec3{0.0, 1.0, 0.0}
                                                               : glm::dvec3{0.0, 0.0, -1.0}};
  const glm::dvec3 reference_forward{NormalizeOrFallback(
    fallback_forward - world_up * glm::dot(fallback_forward, world_up), glm::dvec3{0.0, 0.0, -1.0})};
  const glm::dvec3 reference_right{
    NormalizeOrFallback(glm::cross(reference_forward, world_up), glm::dvec3{1.0, 0.0, 0.0})};
  const double distance{glm::length(m_camera->GetPosition() - m_desc.target)};
  if (distance > k_epsilon) {
    // 从外部相机同步时以真实 eye-target 距离为准，保持 UI 显示和控制器状态一致。
    m_desc.distance = std::clamp(distance, m_desc.min_distance, m_desc.max_distance);
  }

  m_pitch = std::asin(up_component);
  m_yaw = std::atan2(glm::dot(planar_forward, reference_right), glm::dot(planar_forward, reference_forward));
}

void YawPitchOrbitCameraController::ApplyToCamera() {
  if (nullptr == m_camera) {
    return;
  }

  const glm::dvec3 eye{m_desc.target - GetForward() * m_desc.distance};
  m_camera->LookAt(eye, m_desc.target, m_desc.world_up);
}

glm::dvec3 YawPitchOrbitCameraController::GetForward() const {
  const glm::dvec3 world_up{GetWorldUp()};
  const glm::dvec3 fallback_forward{std::abs(world_up.y) < 0.9 ? glm::dvec3{0.0, 1.0, 0.0}
                                                               : glm::dvec3{0.0, 0.0, -1.0}};
  const glm::dvec3 reference_forward{NormalizeOrFallback(
    fallback_forward - world_up * glm::dot(fallback_forward, world_up), glm::dvec3{0.0, 0.0, -1.0})};
  const glm::dvec3 reference_right{
    NormalizeOrFallback(glm::cross(reference_forward, world_up), glm::dvec3{1.0, 0.0, 0.0})};
  const glm::dvec3 planar_forward{std::cos(m_yaw) * reference_forward + std::sin(m_yaw) * reference_right};
  return glm::normalize(std::cos(m_pitch) * planar_forward + std::sin(m_pitch) * world_up);
}

ArcballOrbitCameraController::ArcballOrbitCameraController(Camera* camera) : OrbitCameraController{camera} {
  SyncFromCameraAndTarget();
}

void ArcballOrbitCameraController::Update([[maybe_unused]] double delta_seconds, const CameraControllerInput& input) {
  if (nullptr == m_camera) {
    return;
  }

  ValidateDistance();
  if (input.rotate) {
    const glm::dvec3 current_arcball_vector{MapToArcball(input.cursor_x, input.cursor_y)};
    if (!m_was_rotating) {
      m_last_arcball_vector = current_arcball_vector;
      m_was_rotating = true;
    }

    const glm::dquat rotation_delta{RotationFromTo(current_arcball_vector, m_last_arcball_vector)};
    const glm::dmat3 view_rotation_matrix{m_camera->GetViewMatrix()};
    const glm::dquat view_rotation{glm::quat_cast(view_rotation_matrix)};
    // arcball delta 先在视图空间产生，再转换回世界空间累计到相机旋转。
    m_rotation = glm::normalize(glm::inverse(view_rotation) * rotation_delta * view_rotation * m_rotation);
    m_last_arcball_vector = current_arcball_vector;
  } else {
    m_was_rotating = false;
  }

  ApplyCommonPanAndDolly(input);
  ApplyToCamera();
}

void ArcballOrbitCameraController::Focus(glm::dvec3 target, double distance) {
  OrbitCameraController::Focus(target, distance);
  SyncFromCameraAndTarget();
  ApplyToCamera();
}

void ArcballOrbitCameraController::SyncFromCameraAndTarget() {
  if (nullptr == m_camera) {
    m_rotation = glm::dquat{1.0, 0.0, 0.0, 0.0};
    return;
  }

  m_rotation = m_camera->GetRotation();
  const double distance{glm::length(m_camera->GetPosition() - m_desc.target)};
  if (distance > k_epsilon) {
    m_desc.distance = std::clamp(distance, m_desc.min_distance, m_desc.max_distance);
  }
}

void ArcballOrbitCameraController::ResetInteractionState() noexcept { m_was_rotating = false; }

void ArcballOrbitCameraController::ApplyToCamera() {
  if (nullptr == m_camera) {
    return;
  }

  const glm::dvec3 forward{glm::normalize(m_rotation * glm::dvec3{0.0, 0.0, -1.0})};
  const glm::dvec3 eye{m_desc.target - forward * m_desc.distance};
  m_camera->LookAt(eye, m_desc.target, m_rotation * glm::dvec3{0.0, 1.0, 0.0});
}

glm::dvec3 ArcballOrbitCameraController::MapToArcball(double mouse_x, double mouse_y) const {
  const double width{std::max(m_desc.viewport_width, 1.0)};
  const double height{std::max(m_desc.viewport_height, 1.0)};
  glm::dvec3 point{
    std::clamp((2.0 * mouse_x - width) / width, -1.0, 1.0),
    std::clamp((height - 2.0 * mouse_y) / height, -1.0, 1.0),
    0.0,
  };

  const double length_squared{point.x * point.x + point.y * point.y};
  if (length_squared <= 1.0) {
    // 鼠标在虚拟球内时映射到球面，球外时归一化到边缘，避免旋转速度突变。
    point.z = std::sqrt(1.0 - length_squared);
  } else {
    point = glm::normalize(point);
  }
  return point;
}
}  // namespace lvk::camera
