#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <string>
#include <vector>

namespace lvk {
struct ApplicationDesc {
  std::string title{"Vulkan Sample"};
  uint32_t width{800};
  uint32_t height{600};
  uint32_t api_version{VK_API_VERSION_1_0};
  bool enable_validation_layers{
#ifdef NDEBUG
    false
#else
    true
#endif
  };
  std::vector<const char*> instance_extensions{};
  std::vector<const char*> device_extensions{};
  VkPhysicalDeviceFeatures device_features{};
};
}  // namespace lvk
