#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>

namespace lvk {
struct FrameContext {
  uint32_t image_index{};
  uint32_t frame_index{};
  VkCommandBuffer command_buffer{VK_NULL_HANDLE};
};
}  // namespace lvk
