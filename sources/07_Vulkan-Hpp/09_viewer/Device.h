#pragma once

#include <memory>
#include <optional>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

struct WindowHelper;

struct Device
{
    Device();

    explicit Device(std::unique_ptr<WindowHelper>& windowHelper);

private:
    void CreateInstance() noexcept;
    void CreateDebugUtilsMessengerEXT() noexcept;
    void PickPhysicalDevice(const vk::SurfaceKHR surface = nullptr) noexcept;
    void CreateDevice() noexcept;
    void CreateQueues() noexcept;
    void CreateCommandPools() noexcept;

    bool IsDeviceSuitable(const vk::raii::PhysicalDevice& physicalDevice, const vk::SurfaceKHR surface = nullptr) noexcept;

private:
    vk::raii::Context m_context {};
    vk::raii::Instance m_instance {nullptr};
    vk::raii::DebugUtilsMessengerEXT m_debugUtilsMessengerEXT {nullptr};

public:
    uint32_t graphicsQueueIndex {};
    uint32_t presentQueueIndex {};

    vk::raii::PhysicalDevice physicalDevice {nullptr};
    vk::raii::Device device {nullptr};

    vk::raii::CommandPool commandPoolReset {nullptr};
    vk::raii::CommandPool commandPoolTransient {nullptr};

    vk::raii::Queue graphicsQueue {nullptr};
    vk::raii::Queue presentQueue {nullptr};

private:
    const std::string m_appName {"Vulkan-Hpp"};
    const std::string m_engineName {"Vulkan-Hpp"};
    std::vector<const char*> m_enableLayerNames {"VK_LAYER_KHRONOS_validation"};
    std::vector<const char*> m_enableInstanceExtensionNames {VK_EXT_DEBUG_UTILS_EXTENSION_NAME};
    std::vector<const char*> m_enableDeviceExtensionNames {};
};
