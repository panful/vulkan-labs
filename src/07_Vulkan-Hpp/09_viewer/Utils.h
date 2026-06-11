#pragma once

#include "Device.h"
#include <memory>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

class Utils
{
public:
    template <typename T>
    static void CopyToDevice(const vk::raii::DeviceMemory& deviceMemory, const T* pData, size_t count, vk::DeviceSize stride = sizeof(T))
    {
        assert(sizeof(T) <= stride);
        uint8_t* deviceData = static_cast<uint8_t*>(deviceMemory.mapMemory(0, count * stride));
        if (stride == sizeof(T))
        {
            std::memcpy(deviceData, pData, count * sizeof(T));
        }
        else
        {
            for (size_t i = 0; i < count; i++)
            {
                std::memcpy(deviceData, &pData[i], sizeof(T));
                deviceData += stride;
            }
        }
        deviceMemory.unmapMemory();
    }

    template <typename T>
    static void CopyToDevice(const vk::raii::DeviceMemory& deviceMemory, const T& data)
    {
        CopyToDevice<T>(deviceMemory, &data, 1);
    }

    template <typename Func>
    static void OneTimeSubmit(const std::shared_ptr<Device>& device, const Func& func)
    {
        vk::raii::CommandBuffer commandBuffer =
            std::move(vk::raii::CommandBuffers(device->device, {device->commandPoolTransient, vk::CommandBufferLevel::ePrimary, 1}).front());

        commandBuffer.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
        func(commandBuffer);
        commandBuffer.end();

        vk::SubmitInfo submitInfo(nullptr, nullptr, *commandBuffer);
        device->graphicsQueue.submit(submitInfo, nullptr);
        device->graphicsQueue.waitIdle();
    }

    static vk::SurfaceFormatKHR PickSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& formats);

    static vk::PresentModeKHR PickPresentMode(const std::vector<vk::PresentModeKHR>& presentModes);

    static uint32_t
    FindMemoryType(const vk::PhysicalDeviceMemoryProperties& memoryProperties, uint32_t typeBits, vk::MemoryPropertyFlags requirementsMask) noexcept;

    static vk::raii::DeviceMemory AllocateDeviceMemory(
        const std::shared_ptr<Device> device, const vk::MemoryRequirements& memoryRequirements, vk::MemoryPropertyFlags memoryPropertyFlags
    ) noexcept;

    static std::vector<uint32_t> ReadSPVShader(const std::string& fileName);
};
