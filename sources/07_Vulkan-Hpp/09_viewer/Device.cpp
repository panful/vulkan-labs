
#include "Device.h"
#include "Window.h"
#include <iostream>
#include <set>

#include <vulkan/vulkan.h>

namespace {
VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData
) noexcept
{
    std::clog << "===========================================\n"
              << "Debug::validation layer: " << pCallbackData->pMessage << '\n';

    return VK_FALSE;
}
} // namespace

Device::Device(std::unique_ptr<WindowHelper>& windowHelper)
{
    auto ext = windowHelper->GetExtensions();
    m_enableInstanceExtensionNames.insert(m_enableInstanceExtensionNames.cend(), ext.begin(), ext.end());
    m_enableDeviceExtensionNames.emplace_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

    CreateInstance();
    CreateDebugUtilsMessengerEXT();
    PickPhysicalDevice(windowHelper->InitSurface(m_instance));
    CreateDevice();
    CreateQueues();
    CreateCommandPools();
}

Device::Device()
{
    CreateInstance();
    CreateDebugUtilsMessengerEXT();
    PickPhysicalDevice();
    CreateDevice();
    CreateQueues();
    CreateCommandPools();
}

void Device::CreateInstance() noexcept
{
    vk::DebugUtilsMessageSeverityFlagsEXT severityFlags {
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError
    };
    vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags {
        vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance
        | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation
    };

    vk::DebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfoExt {{}, severityFlags, messageTypeFlags, DebugCallback};

    vk::ApplicationInfo applicationInfo {m_appName.c_str(), 1, m_engineName.c_str(), 1, VK_API_VERSION_1_1};

    vk::InstanceCreateInfo instanceCreateInfo {
        {}, &applicationInfo, m_enableLayerNames, m_enableInstanceExtensionNames, &debugUtilsMessengerCreateInfoExt
    };

    m_instance = vk::raii::Instance(m_context, instanceCreateInfo);
}

void Device::CreateDebugUtilsMessengerEXT() noexcept
{
    m_debugUtilsMessengerEXT = vk::raii::DebugUtilsMessengerEXT(
        m_instance,
        vk::DebugUtilsMessengerCreateInfoEXT {
            {},
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
            vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance
                | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation,
            &DebugCallback
        }
    );
}

void Device::PickPhysicalDevice(const vk::SurfaceKHR surface) noexcept
{
    vk::raii::PhysicalDevices physicalDevices(m_instance);
    for (size_t i = 0; i < physicalDevices.size(); ++i)
    {
        if (IsDeviceSuitable(physicalDevices[i], surface))
        {
            physicalDevice = physicalDevices[i];
            break;
        }
    }

    auto properties = physicalDevice.getProperties();
    std::cout << "pick device name: " << properties.deviceName.data() << '\n';
    assert(*physicalDevice);
}

bool Device::IsDeviceSuitable(const vk::raii::PhysicalDevice& physicalDevice, const vk::SurfaceKHR surface) noexcept
{
    std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties();

    std::optional<uint32_t> optPresentIndex {}, optGraphicsIndex {};
    for (uint32_t i = 0; i < queueFamilyProperties.size(); ++i)
    {
        if (surface != nullptr)
        {
            optPresentIndex = physicalDevice.getSurfaceSupportKHR(i, surface) ? i : optPresentIndex;
        }

        if ((vk::QueueFlagBits::eGraphics & queueFamilyProperties[i].queueFlags)
            && (vk::QueueFlagBits::eTransfer & queueFamilyProperties[i].queueFlags))
        {
            optGraphicsIndex = i;
        }

        if (surface != nullptr && optPresentIndex && optGraphicsIndex)
        {
            graphicsQueueIndex = optGraphicsIndex.value();
            presentQueueIndex  = optPresentIndex.value();
            break;
        }
        else if (surface == nullptr && optGraphicsIndex)
        {
            graphicsQueueIndex = optGraphicsIndex.value();
            presentQueueIndex  = optGraphicsIndex.value();
            break;
        }
    }

    if (surface != nullptr)
    {
        auto supportSurface = !physicalDevice.getSurfacePresentModesKHR(surface).empty() && !physicalDevice.getSurfaceFormatsKHR(surface).empty();
        return optPresentIndex && optGraphicsIndex && supportSurface;
    }
    else
    {
        return optGraphicsIndex.has_value();
    }
}

void Device::CreateDevice() noexcept
{
    float queuePriority = 0.f;

    std::set<uint32_t> uniqueQueueFamilyIndices {graphicsQueueIndex, presentQueueIndex}; // 不需要展示时，展示队列簇和图形队列簇一样

    std::vector<vk::DeviceQueueCreateInfo> deviceQueueCreateInfos {};
    for (const auto queueIndex : uniqueQueueFamilyIndices)
    {
        deviceQueueCreateInfos.emplace_back(vk::DeviceQueueCreateInfo {{}, queueIndex, 1, &queuePriority});
    }

    vk::DeviceCreateInfo deviceCreateInfo({}, deviceQueueCreateInfos, {}, m_enableDeviceExtensionNames, {});
    device = vk::raii::Device(physicalDevice, deviceCreateInfo);
}

void Device::CreateQueues() noexcept
{
    graphicsQueue = vk::raii::Queue(device, graphicsQueueIndex, 0);
    presentQueue  = vk::raii::Queue(device, presentQueueIndex, 0);
}

void Device::CreateCommandPools() noexcept
{
    commandPoolReset     = vk::raii::CommandPool(device, {{vk::CommandPoolCreateFlagBits::eResetCommandBuffer}, graphicsQueueIndex});
    commandPoolTransient = vk::raii::CommandPool(device, {{vk::CommandPoolCreateFlagBits::eTransient}, graphicsQueueIndex});
}
