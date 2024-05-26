#include "Utils.h"
#include "Device.h"
#include <fstream>

vk::SurfaceFormatKHR Utils::PickSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& formats)
{
    assert(!formats.empty());
    vk::SurfaceFormatKHR pickedFormat = formats[0];
    if (formats.size() == 1)
    {
        if (formats[0].format == vk::Format::eUndefined)
        {
            pickedFormat.format     = vk::Format::eB8G8R8A8Unorm;
            pickedFormat.colorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;
        }
    }
    else
    {
        // request several formats, the first found will be used
        vk::Format requestedFormats[] = {vk::Format::eB8G8R8A8Unorm, vk::Format::eR8G8B8A8Unorm, vk::Format::eB8G8R8Unorm, vk::Format::eR8G8B8Unorm};
        vk::ColorSpaceKHR requestedColorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;
        for (size_t i = 0; i < sizeof(requestedFormats) / sizeof(requestedFormats[0]); i++)
        {
            vk::Format requestedFormat = requestedFormats[i];
            auto it = std::find_if(formats.begin(), formats.end(), [requestedFormat, requestedColorSpace](vk::SurfaceFormatKHR const& f) {
                return (f.format == requestedFormat) && (f.colorSpace == requestedColorSpace);
            });
            if (it != formats.end())
            {
                pickedFormat = *it;
                break;
            }
        }
    }
    assert(pickedFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear);
    return pickedFormat;
}

vk::PresentModeKHR Utils::PickPresentMode(const std::vector<vk::PresentModeKHR>& presentModes)
{
    vk::PresentModeKHR pickedMode = vk::PresentModeKHR::eFifo;
    for (const auto& presentMode : presentModes)
    {
        if (presentMode == vk::PresentModeKHR::eMailbox)
        {
            pickedMode = presentMode;
            break;
        }

        if (presentMode == vk::PresentModeKHR::eImmediate)
        {
            pickedMode = presentMode;
        }
    }
    return pickedMode;
}

uint32_t Utils::FindMemoryType(
    const vk::PhysicalDeviceMemoryProperties& memoryProperties, uint32_t typeBits, vk::MemoryPropertyFlags requirementsMask
) noexcept
{
    uint32_t typeIndex = uint32_t(~0);
    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++)
    {
        if ((typeBits & 1) && ((memoryProperties.memoryTypes[i].propertyFlags & requirementsMask) == requirementsMask))
        {
            typeIndex = i;
            break;
        }
        typeBits >>= 1;
    }
    assert(typeIndex != uint32_t(~0));
    return typeIndex;
}

vk::raii::DeviceMemory Utils::AllocateDeviceMemory(
    const std::shared_ptr<Device> device, const vk::MemoryRequirements& memoryRequirements, vk::MemoryPropertyFlags memoryPropertyFlags
) noexcept
{
    uint32_t memoryTypeIndex = FindMemoryType(device->physicalDevice.getMemoryProperties(), memoryRequirements.memoryTypeBits, memoryPropertyFlags);
    vk::MemoryAllocateInfo memoryAllocateInfo(memoryRequirements.size, memoryTypeIndex);
    return vk::raii::DeviceMemory(device->device, memoryAllocateInfo);
}

std::vector<uint32_t> Utils::ReadSPVShader(const std::string& fileName)
{
    std::ifstream file(fileName, std::ios::ate | std::ios::binary);
    if (!file.is_open())
    {
        throw std::runtime_error("failed to open file: " + fileName);
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<uint32_t> buffer(fileSize / (sizeof(uint32_t) / sizeof(char)));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(buffer.data()), fileSize);
    file.close();

    return buffer;
}
