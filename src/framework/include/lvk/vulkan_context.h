#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

#include "lvk/application_desc.h"

namespace lvk {
class GlfwWindow;

/// @brief 图形队列与呈现队列的族下标查询结果。
/// @details Vulkan 不保证 graphics queue 与 present queue 位于同一个 queue family。
struct QueueFamilyIndices {
  /// @brief 支持 `VK_QUEUE_GRAPHICS_BIT` 的 queue family。
  uint32_t graphics_family{};
  /// @brief 支持向当前窗口 surface present 的 queue family。
  uint32_t present_family{};
  /// @brief graphics family 是否已经找到。
  bool has_graphics_family{false};
  /// @brief present family 是否已经找到。
  bool has_present_family{false};

  /// @brief 是否已经具备创建当前教学样例所需的队列。
  [[nodiscard]] bool IsComplete() const noexcept;
};

/// @brief 持有 Vulkan instance、surface、physical device、logical device 和基础队列。
/// @details
/// - `VulkanContext` 拥有底层 Vulkan handle，并在析构时按依赖顺序释放。
/// - 构造期间会创建 window surface，因此传入的 `GlfwWindow` 必须已经创建成功。
/// - 该类不负责 swapchain、command pool 或每帧同步对象，这些由 `VulkanSample`/`SwapChain` 管理。
class VulkanContext final {
public:
  /// @brief 按应用配置和窗口创建 Vulkan 基础上下文。
  /// @throws std::runtime_error Vulkan 初始化任一步骤失败时抛出。
  VulkanContext(const ApplicationDesc& desc, const GlfwWindow& window);
  ~VulkanContext() noexcept;

  VulkanContext(const VulkanContext&) = delete;
  VulkanContext& operator=(const VulkanContext&) = delete;
  VulkanContext(VulkanContext&&) noexcept = delete;
  VulkanContext& operator=(VulkanContext&&) noexcept = delete;

  /// @brief Vulkan instance，生命周期由 `VulkanContext` 拥有。
  [[nodiscard]] VkInstance GetInstance() const noexcept;
  /// @brief 与 GLFW 窗口绑定的 Vulkan surface。
  [[nodiscard]] VkSurfaceKHR GetSurface() const noexcept;
  /// @brief 当前选择的物理设备。
  [[nodiscard]] VkPhysicalDevice GetPhysicalDevice() const noexcept;
  /// @brief 创建出的逻辑设备。
  [[nodiscard]] VkDevice GetDevice() const noexcept;
  /// @brief graphics queue，用于提交渲染命令。
  [[nodiscard]] VkQueue GetGraphicsQueue() const noexcept;
  /// @brief present queue，用于提交 swapchain present。
  [[nodiscard]] VkQueue GetPresentQueue() const noexcept;
  /// @brief graphics queue family 下标。
  [[nodiscard]] uint32_t GetGraphicsQueueFamily() const noexcept;
  /// @brief present queue family 下标。
  [[nodiscard]] uint32_t GetPresentQueueFamily() const noexcept;
  /// @brief 创建 logical device 时启用的 device extensions。
  [[nodiscard]] const std::vector<const char*>& GetDeviceExtensions() const noexcept;
  /// @brief 当前是否启用了 validation layer。
  [[nodiscard]] bool IsValidationEnabled() const noexcept;

private:
  void CreateInstance(const ApplicationDesc& desc);
  void SetupDebugMessenger();
  void CreateSurface(const GlfwWindow& window);
  void PickPhysicalDevice();
  void CreateLogicalDevice();
  [[nodiscard]] bool CheckValidationLayerSupport() const;
  [[nodiscard]] std::vector<const char*> GetRequiredInstanceExtensions(const ApplicationDesc& desc) const;
  [[nodiscard]] QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice physical_device) const;
  [[nodiscard]] bool IsDeviceSuitable(VkPhysicalDevice physical_device) const;
  [[nodiscard]] bool CheckDeviceExtensionSupport(VkPhysicalDevice physical_device) const;
  static void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& create_info) noexcept;
  static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
                                                      VkDebugUtilsMessageTypeFlagsEXT message_type,
                                                      const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
                                                      void* user_data) noexcept;
  static VkResult CreateDebugUtilsMessenger(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* create_info,
                                            const VkAllocationCallbacks* allocator,
                                            VkDebugUtilsMessengerEXT* debug_messenger) noexcept;
  static void DestroyDebugUtilsMessenger(VkInstance instance, VkDebugUtilsMessengerEXT debug_messenger,
                                         const VkAllocationCallbacks* allocator) noexcept;

private:
  bool m_enable_validation_layers{false};
  VkPhysicalDeviceFeatures m_device_features{};
  std::vector<const char*> m_validation_layers{};
  std::vector<const char*> m_device_extensions{};
  VkInstance m_instance{VK_NULL_HANDLE};
  VkDebugUtilsMessengerEXT m_debug_messenger{VK_NULL_HANDLE};
  VkSurfaceKHR m_surface{VK_NULL_HANDLE};
  VkPhysicalDevice m_physical_device{VK_NULL_HANDLE};
  VkDevice m_device{VK_NULL_HANDLE};
  VkQueue m_graphics_queue{VK_NULL_HANDLE};
  VkQueue m_present_queue{VK_NULL_HANDLE};
  QueueFamilyIndices m_queue_family_indices{};
};
}  // namespace lvk
