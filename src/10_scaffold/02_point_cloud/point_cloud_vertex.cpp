#include "point_cloud_vertex.h"

#include <cstddef>

namespace lvk::point_cloud {
static_assert(sizeof(PointCloudVertex) == 16);
static_assert(offsetof(PointCloudVertex, color) == 12);

VkVertexInputBindingDescription PointCloudVertex::GetBindingDescription() noexcept {
  VkVertexInputBindingDescription binding_description{};
  binding_description.binding = 0;
  binding_description.stride = sizeof(PointCloudVertex);
  binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
  return binding_description;
}

std::array<VkVertexInputAttributeDescription, 2> PointCloudVertex::GetAttributeDescriptions() noexcept {
  std::array<VkVertexInputAttributeDescription, 2> attribute_descriptions{};
  attribute_descriptions[0].binding = 0;
  attribute_descriptions[0].location = 0;
  attribute_descriptions[0].format = VK_FORMAT_R32G32B32_SINT;
  attribute_descriptions[0].offset = offsetof(PointCloudVertex, position);
  attribute_descriptions[1].binding = 0;
  attribute_descriptions[1].location = 1;
  attribute_descriptions[1].format = VK_FORMAT_R8G8B8A8_UNORM;
  attribute_descriptions[1].offset = offsetof(PointCloudVertex, color);
  return attribute_descriptions;
}
}  // namespace lvk::point_cloud
