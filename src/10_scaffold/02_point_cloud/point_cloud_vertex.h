#pragma once

#include <vulkan/vulkan.h>

#include <array>
#include <cstdint>
#include <glm/vec3.hpp>
#include <span>

#include "bounding_box.h"

namespace lvk::point_cloud {
// float3(4x3) + uint32_t(4x1) = 16 bytes
struct PointCloudVertex {
  glm::vec3 position{};
  std::uint32_t color{};

  [[nodiscard]] static VkVertexInputBindingDescription GetBindingDescription() noexcept;
  [[nodiscard]] static std::array<VkVertexInputAttributeDescription, 2> GetAttributeDescriptions() noexcept;
};

/// @brief 计算一组渲染顶点的紧密轴对齐包围盒。
/// @param vertices 调用期间必须保持有效且不能为空的只读顶点视图。
/// @return 包含全部顶点位置的包围盒。
[[nodiscard]] BoundingBox ComputeBoundingBox(std::span<const PointCloudVertex> vertices);
}  // namespace lvk::point_cloud
