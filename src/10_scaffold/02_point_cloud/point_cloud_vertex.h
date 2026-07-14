#pragma once

#include <vulkan/vulkan.h>

#include <array>
#include <cstdint>

namespace lvk::point_cloud {
// int32x3(4x3) + uint32_t(4x1) = 16 bytes
struct PointCloudVertex {
  std::array<std::int32_t, 3> position{};
  std::uint32_t color{};

  [[nodiscard]] static VkVertexInputBindingDescription GetBindingDescription() noexcept;
  [[nodiscard]] static std::array<VkVertexInputAttributeDescription, 2> GetAttributeDescriptions() noexcept;
};
}  // namespace lvk::point_cloud
