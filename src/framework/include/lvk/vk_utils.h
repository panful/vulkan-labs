#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <string_view>

namespace lvk {
/// @brief 检查 Vulkan 调用结果，失败时抛出带结果名的异常。
/// @details 这个工具用于样例初始化和渲染主路径；析构和 noexcept 清理路径不应调用它。
void CheckVkResult(VkResult result, std::string_view message);

/// @brief 将常见 VkResult 转成人类可读名称。
/// @return 未覆盖的结果码返回 `VK_UNKNOWN_RESULT`。
[[nodiscard]] const char* GetVkResultName(VkResult result) noexcept;

/// @brief 在物理设备内存类型中查找满足属性要求的类型下标。
/// @throws std::runtime_error 找不到满足条件的内存类型时抛出。
[[nodiscard]] uint32_t FindMemoryType(VkPhysicalDevice physical_device, uint32_t type_filter,
                                      VkMemoryPropertyFlags properties);

/// @brief 加载 instance 级扩展函数指针。
/// @details 返回值可能为 nullptr，调用方必须按扩展是否存在处理。
template <typename T>
[[nodiscard]] T LoadInstanceProcAddress(VkInstance instance, const char* name) noexcept {
  return reinterpret_cast<T>(vkGetInstanceProcAddr(instance, name));
}

/// @brief 加载 device 级扩展函数指针。
/// @details 返回值可能为 nullptr，调用方必须按扩展是否存在处理。
template <typename T>
[[nodiscard]] T LoadDeviceProcAddress(VkDevice device, const char* name) noexcept {
  return reinterpret_cast<T>(vkGetDeviceProcAddr(device, name));
}
}  // namespace lvk
