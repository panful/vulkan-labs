#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace lvk::camera {
/// @brief 相机投影类型。
enum class ProjectionType : uint8_t { Perspective, Orthographic };

/// @brief GPU 投影矩阵约定。
/// @details Vulkan NDC 的 Y 方向和 OpenGL 习惯不同，depth 范围为 `[0, 1]`；这里用显式配置避免样例里藏魔法数。
struct ProjectionConvention {
  /// @brief 是否翻转投影矩阵的 Y 轴。
  bool flip_y{true};
  /// @brief 是否使用 reverse-Z 深度分布。
  bool reverse_z{false};
};

namespace projection_conventions {
/// @brief 默认 Vulkan 投影约定：翻转 Y，深度范围 `[0, 1]`。
inline constexpr ProjectionConvention k_vulkan{};
/// @brief 不翻转 Y 的 Vulkan 深度约定，适合已经在 shader 或 viewport 中处理 Y 方向的样例。
inline constexpr ProjectionConvention k_vulkan_no_flip_y{false, false};
}  // namespace projection_conventions

/// @brief 相机的完整描述对象。
/// @details
/// - 位置和旋转使用 double 精度，最后上传 GPU 时再转为 float。
/// - 默认相机位于 `(0, 0, 5)`，朝向本地 `-Z`。
struct CameraDesc {
  /// @brief 当前投影类型。
  ProjectionType projection_type{ProjectionType::Perspective};
  /// @brief 世界空间相机位置。
  glm::dvec3 position{0.0, 0.0, 5.0};
  /// @brief 世界空间相机朝向，表示从相机本地坐标到世界坐标的旋转。
  glm::dquat rotation{1.0, 0.0, 0.0, 0.0};
  /// @brief 透视投影垂直视场角，单位为弧度。
  double vertical_fov_rad{glm::radians(60.0)};
  /// @brief 正交投影视口高度，单位为世界空间长度。
  double orthographic_height{10.0};
  /// @brief 宽高比 `width / height`。
  double aspect_ratio{1.0};
  /// @brief 近裁剪面距离，必须为正数。
  double near_plane{0.01};
  /// @brief 远裁剪面距离，必须大于近裁剪面。
  double far_plane{1000.0};
};

/// @brief 上传给 shader 的相机矩阵集合。
/// @details 成员对齐按 GLM 默认布局；样例中作为简单 uniform buffer 数据使用。
struct GpuCameraMatrices {
  glm::mat4 view{1.0F};
  glm::mat4 projection{1.0F};
  glm::mat4 view_projection{1.0F};
  glm::vec4 camera_position{0.0F};
};
}  // namespace lvk::camera
