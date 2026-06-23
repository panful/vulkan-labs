#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace lvk::camera {
enum class ProjectionType : uint8_t { Perspective, Orthographic };

struct ProjectionConvention {
  bool flip_y{true};
  bool reverse_z{false};
};

namespace projection_conventions {
inline constexpr ProjectionConvention k_vulkan{};
inline constexpr ProjectionConvention k_vulkan_no_flip_y{false, false};
}  // namespace projection_conventions

struct CameraDesc {
  ProjectionType projection_type{ProjectionType::Perspective};
  glm::dvec3 position{0.0, 0.0, 5.0};
  glm::dquat rotation{1.0, 0.0, 0.0, 0.0};
  double vertical_fov_rad{glm::radians(60.0)};
  double orthographic_height{10.0};
  double aspect_ratio{1.0};
  double near_plane{0.01};
  double far_plane{1000.0};
};

struct GpuCameraMatrices {
  glm::mat4 view{1.0F};
  glm::mat4 projection{1.0F};
  glm::mat4 view_projection{1.0F};
  glm::vec4 camera_position{0.0F};
};
}  // namespace lvk::camera
