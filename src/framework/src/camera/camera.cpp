#include "lvk/camera/camera.h"

#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <numbers>
#include <stdexcept>

namespace lvk::camera {
namespace {
constexpr double k_epsilon{1e-12};

[[nodiscard]] bool IsFinitePositive(double value) noexcept { return std::isfinite(value) && value > 0.0; }

[[nodiscard]] double LengthSquared(glm::dvec3 value) noexcept { return glm::dot(value, value); }
}  // namespace

Camera::Camera() { ValidateProjection(m_desc); }

Camera::Camera(const CameraDesc& desc) : m_desc(desc) {
  ValidateProjection(m_desc);
  m_desc.rotation = glm::normalize(m_desc.rotation);
}

void Camera::SetPerspective(double vertical_fov_rad, double aspect_ratio, double near_plane, double far_plane) {
  ValidatePerspective(vertical_fov_rad, aspect_ratio, near_plane, far_plane);
  m_desc.projection_type = ProjectionType::Perspective;
  m_desc.vertical_fov_rad = vertical_fov_rad;
  m_desc.aspect_ratio = aspect_ratio;
  m_desc.near_plane = near_plane;
  m_desc.far_plane = far_plane;
}

void Camera::SetOrthographic(double orthographic_height, double aspect_ratio, double near_plane, double far_plane) {
  ValidateOrthographic(orthographic_height, aspect_ratio, near_plane, far_plane);
  m_desc.projection_type = ProjectionType::Orthographic;
  m_desc.orthographic_height = orthographic_height;
  m_desc.aspect_ratio = aspect_ratio;
  m_desc.near_plane = near_plane;
  m_desc.far_plane = far_plane;
}

void Camera::SetAspectRatio(double aspect_ratio) {
  if (ProjectionType::Perspective == m_desc.projection_type) {
    ValidatePerspective(m_desc.vertical_fov_rad, aspect_ratio, m_desc.near_plane, m_desc.far_plane);
  } else {
    ValidateOrthographic(m_desc.orthographic_height, aspect_ratio, m_desc.near_plane, m_desc.far_plane);
  }
  m_desc.aspect_ratio = aspect_ratio;
}

void Camera::LookAt(glm::dvec3 eye, glm::dvec3 target, glm::dvec3 world_up) {
  const glm::dvec3 forward_vector{target - eye};
  if (LengthSquared(forward_vector) <= k_epsilon || LengthSquared(world_up) <= k_epsilon) {
    throw std::invalid_argument("invalid camera look-at vectors");
  }

  const glm::dvec3 forward{glm::normalize(forward_vector)};
  glm::dvec3 right{glm::cross(forward, glm::normalize(world_up))};
  if (LengthSquared(right) <= k_epsilon) {
    throw std::invalid_argument("camera up vector is parallel to view direction");
  }

  right = glm::normalize(right);
  const glm::dvec3 up{glm::normalize(glm::cross(right, forward))};

  glm::dmat3 basis{1.0};
  basis[0] = right;
  basis[1] = up;
  basis[2] = -forward;

  m_desc.position = eye;
  m_desc.rotation = glm::normalize(glm::quat_cast(basis));
}

void Camera::SetPose(glm::dvec3 position, glm::dquat rotation) noexcept {
  m_desc.position = position;
  m_desc.rotation = glm::normalize(rotation);
}

void Camera::SetPosition(glm::dvec3 position) noexcept { m_desc.position = position; }

void Camera::SetRotation(glm::dquat rotation) noexcept { m_desc.rotation = glm::normalize(rotation); }

ProjectionType Camera::GetProjectionType() const noexcept { return m_desc.projection_type; }

double Camera::GetAspectRatio() const noexcept { return m_desc.aspect_ratio; }

double Camera::GetOrthographicHeight() const noexcept { return m_desc.orthographic_height; }

void Camera::SetOrthographicHeight(double orthographic_height) {
  if (!IsFinitePositive(orthographic_height)) {
    throw std::invalid_argument("orthographic height must be finite and positive");
  }
  m_desc.orthographic_height = orthographic_height;
}

const glm::dvec3& Camera::GetPosition() const noexcept { return m_desc.position; }

glm::dvec3 Camera::GetForward() const noexcept { return glm::normalize(m_desc.rotation * glm::dvec3{0.0, 0.0, -1.0}); }

glm::dvec3 Camera::GetRight() const noexcept { return glm::normalize(m_desc.rotation * glm::dvec3{1.0, 0.0, 0.0}); }

glm::dvec3 Camera::GetUp() const noexcept { return glm::normalize(m_desc.rotation * glm::dvec3{0.0, 1.0, 0.0}); }

glm::dmat4 Camera::GetViewMatrix() const {
  const glm::dmat4 rotate_inverse{glm::mat4_cast(glm::conjugate(glm::normalize(m_desc.rotation)))};
  const glm::dmat4 translate_inverse{glm::translate(glm::dmat4{1.0}, -m_desc.position)};
  return rotate_inverse * translate_inverse;
}

glm::dmat4 Camera::GetProjectionMatrix(ProjectionConvention convention) const {
  const double near_plane{m_desc.near_plane};
  const double far_plane{m_desc.far_plane};
  const double near_depth{convention.reverse_z ? 1.0 : 0.0};
  const double far_depth{convention.reverse_z ? 0.0 : 1.0};

  if (ProjectionType::Orthographic == m_desc.projection_type) {
    ValidateOrthographic(m_desc.orthographic_height, m_desc.aspect_ratio, near_plane, far_plane);

    const double width{m_desc.orthographic_height * m_desc.aspect_ratio};
    const double x_scale{2.0 / width};
    const double y_scale{2.0 / m_desc.orthographic_height};
    const double z_scale{(near_depth - far_depth) / (far_plane - near_plane)};
    const double z_offset{near_depth + z_scale * near_plane};

    glm::dmat4 result{1.0};
    result[0][0] = x_scale;
    result[1][1] = convention.flip_y ? -y_scale : y_scale;
    result[2][2] = z_scale;
    result[3][2] = z_offset;
    return result;
  }

  ValidatePerspective(m_desc.vertical_fov_rad, m_desc.aspect_ratio, near_plane, far_plane);

  const double y_scale{1.0 / std::tan(m_desc.vertical_fov_rad * 0.5)};
  const double x_scale{y_scale / m_desc.aspect_ratio};
  const double a{(far_plane * far_depth - near_plane * near_depth) / (near_plane - far_plane)};
  const double b{near_plane * (a + near_depth)};

  glm::dmat4 result{0.0};
  result[0][0] = x_scale;
  result[1][1] = convention.flip_y ? -y_scale : y_scale;
  result[2][2] = a;
  result[2][3] = -1.0;
  result[3][2] = b;
  return result;
}

GpuCameraMatrices Camera::GetGpuMatrices(ProjectionConvention convention) const {
  GpuCameraMatrices matrices{};
  const glm::dmat4 view{GetViewMatrix()};
  const glm::dmat4 projection{GetProjectionMatrix(convention)};
  matrices.view = glm::mat4{view};
  matrices.projection = glm::mat4{projection};
  matrices.view_projection = glm::mat4{projection * view};
  matrices.camera_position = glm::vec4{glm::vec3{m_desc.position}, 1.0F};
  return matrices;
}

void Camera::ValidateProjection(const CameraDesc& desc) {
  if (ProjectionType::Perspective == desc.projection_type) {
    ValidatePerspective(desc.vertical_fov_rad, desc.aspect_ratio, desc.near_plane, desc.far_plane);
  } else {
    ValidateOrthographic(desc.orthographic_height, desc.aspect_ratio, desc.near_plane, desc.far_plane);
  }
}

void Camera::ValidatePerspective(double vertical_fov_rad, double aspect_ratio, double near_plane, double far_plane) {
  ValidateClipPlanes(near_plane, far_plane);
  if (!IsFinitePositive(aspect_ratio)) {
    throw std::invalid_argument("camera aspect ratio must be finite and positive");
  }
  if (!std::isfinite(vertical_fov_rad) || vertical_fov_rad <= 0.0 || vertical_fov_rad >= std::numbers::pi) {
    throw std::invalid_argument("camera vertical FOV must be finite and in range (0, pi)");
  }
}

void Camera::ValidateOrthographic(double orthographic_height, double aspect_ratio, double near_plane,
                                  double far_plane) {
  ValidateClipPlanes(near_plane, far_plane);
  if (!IsFinitePositive(aspect_ratio)) {
    throw std::invalid_argument("camera aspect ratio must be finite and positive");
  }
  if (!IsFinitePositive(orthographic_height)) {
    throw std::invalid_argument("orthographic height must be finite and positive");
  }
}

void Camera::ValidateClipPlanes(double near_plane, double far_plane) {
  if (!IsFinitePositive(near_plane) || !std::isfinite(far_plane) || far_plane <= near_plane) {
    throw std::invalid_argument("camera clip planes are invalid");
  }
}
}  // namespace lvk::camera
