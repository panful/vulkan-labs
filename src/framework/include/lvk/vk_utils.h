#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <string_view>

namespace lvk {
void CheckVkResult(VkResult result, std::string_view message);
[[nodiscard]] const char* GetVkResultName(VkResult result) noexcept;
[[nodiscard]] uint32_t FindMemoryType(VkPhysicalDevice physical_device, uint32_t type_filter,
                                      VkMemoryPropertyFlags properties);

template <typename T>
[[nodiscard]] T LoadInstanceProcAddress(VkInstance instance, const char* name) noexcept {
  return reinterpret_cast<T>(vkGetInstanceProcAddr(instance, name));
}

template <typename T>
[[nodiscard]] T LoadDeviceProcAddress(VkDevice device, const char* name) noexcept {
  return reinterpret_cast<T>(vkGetDeviceProcAddr(device, name));
}
}  // namespace lvk
