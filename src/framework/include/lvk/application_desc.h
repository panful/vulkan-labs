#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <string>
#include <vector>

namespace lvk {
/// @brief 创建教学样例时使用的应用级配置。
/// @details
/// - 由 `VulkanSample::Configure` 填写。
/// - 字符串指针扩展名只在创建 Vulkan instance/device 期间读取，调用方需要保证指针在配置阶段有效。
/// - 这里刻意保留较薄的配置面，避免教学框架一开始就暴露完整引擎级配置系统。
struct ApplicationDesc {
  /// @brief GLFW 窗口标题，同时作为 Vulkan application name。
  std::string title{"Vulkan Sample"};
  /// @brief 窗口初始宽度，单位为屏幕坐标像素。
  uint32_t width{800};
  /// @brief 窗口初始高度，单位为屏幕坐标像素。
  uint32_t height{600};
  /// @brief 请求的 Vulkan API 版本。
  uint32_t api_version{VK_API_VERSION_1_0};
  /// @brief 是否启用 Vulkan validation layer；Debug 默认开启，Release 默认关闭。
  bool enable_validation_layers{
#ifdef NDEBUG
    false
#else
    true
#endif
  };
  /// @brief 额外 instance extensions；GLFW 和 debug utils 扩展由框架自动补齐。
  std::vector<const char*> instance_extensions{};
  /// @brief 额外 device extensions；swapchain 扩展由框架自动补齐。
  std::vector<const char*> device_extensions{};
  /// @brief 创建逻辑设备时请求的物理设备特性。
  VkPhysicalDeviceFeatures device_features{};
};
}  // namespace lvk
