#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

#include "lvk/application_desc.h"

namespace lvk {
class GlfwWindow;

struct QueueFamilyIndices {
  uint32_t graphics_family{};
  uint32_t present_family{};
  bool has_graphics_family{false};
  bool has_present_family{false};

  [[nodiscard]] bool IsComplete() const noexcept;
};

class VulkanContext final {
public:
  VulkanContext(const ApplicationDesc& desc, const GlfwWindow& window);
  ~VulkanContext() noexcept;

  VulkanContext(const VulkanContext&) = delete;
  VulkanContext& operator=(const VulkanContext&) = delete;
  VulkanContext(VulkanContext&&) noexcept = delete;
  VulkanContext& operator=(VulkanContext&&) noexcept = delete;

  [[nodiscard]] VkInstance GetInstance() const noexcept;
  [[nodiscard]] VkSurfaceKHR GetSurface() const noexcept;
  [[nodiscard]] VkPhysicalDevice GetPhysicalDevice() const noexcept;
  [[nodiscard]] VkDevice GetDevice() const noexcept;
  [[nodiscard]] VkQueue GetGraphicsQueue() const noexcept;
  [[nodiscard]] VkQueue GetPresentQueue() const noexcept;
  [[nodiscard]] uint32_t GetGraphicsQueueFamily() const noexcept;
  [[nodiscard]] uint32_t GetPresentQueueFamily() const noexcept;
  [[nodiscard]] const std::vector<const char*>& GetDeviceExtensions() const noexcept;
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
