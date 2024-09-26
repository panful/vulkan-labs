#pragma once

#include <vulkan/vulkan.h>

#include <optional>
#include <string>
#include <vector>

class Window;

struct QueueFamilyIndices
{
    std::optional<uint32_t> graphicsFamily {};
    std::optional<uint32_t> presentFamily {};

    constexpr bool IsComplete() const noexcept
    {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

class Context
{
public:
    static Context* GetContext();

    void InitDevice(const Window* window);

    VkInstance GetInstance() const noexcept;

    VkPhysicalDevice GetPhysicalDevice() const noexcept;

    VkDevice GetDevice() const noexcept;

    VkQueue GetGraphicsQueue() const noexcept;

    VkQueue GetPresentQueue() const noexcept;

    const QueueFamilyIndices& GetQueueFamilyIndices() const noexcept;

    ~Context() noexcept;

private:
    Context();

    void CreateInstance();

    bool CheckValidationLayerSupport() const noexcept;

    std::vector<const char*> GetRequiredExtensions() const noexcept;

    void SetupDebugCallback();

    void PickPhysicalDevice(const Window* window);

    bool IsDeviceSuitable(const VkPhysicalDevice device, const Window* window) const noexcept;

    QueueFamilyIndices FindQueueFamilies(const VkPhysicalDevice device, const Window* window) const noexcept;

    void CreateLogicalDevice();

    bool CheckDeviceExtensionSupported(const VkPhysicalDevice device) const noexcept;

    void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) const noexcept;

private:
    VkInstance m_instance {nullptr};
    VkDebugUtilsMessengerEXT m_debugMessenger {nullptr};
    VkPhysicalDevice m_physicalDevice {nullptr};
    VkDevice m_device {nullptr};
    VkQueue m_graphicsQueue {nullptr};
    VkQueue m_presentQueue {nullptr};

    QueueFamilyIndices m_queueFamilyIndices {};

    const bool m_enableValidationLayers                 = true;
    const std::vector<const char*> m_validationLayers   = {"VK_LAYER_KHRONOS_validation"};
    const std::vector<const char*> m_deviceExtensions   = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    const std::vector<const char*> m_instanceExtensions = {"VK_KHR_surface", "VK_KHR_win32_surface"};
};
