#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>

namespace lvk {
/// @brief 单帧渲染回调拿到的最小上下文。
/// @details
/// - `image_index` 用于访问当前 swapchain image 对应的 framebuffer。
/// - `frame_index` 用于访问按飞行帧数量分配的 uniform buffer、descriptor set 等资源。
/// - `command_buffer` 已经 begin，样例只需要录制本帧命令。
struct FrameContext {
  /// @brief 当前 acquire 到的 swapchain image 下标。
  uint32_t image_index{};
  /// @brief 当前飞行帧下标，范围为 `[0, k_max_frames_in_flight)`。
  uint32_t frame_index{};
  /// @brief 当前帧使用的主命令缓冲区。
  VkCommandBuffer command_buffer{VK_NULL_HANDLE};
};
}  // namespace lvk
