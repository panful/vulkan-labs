#pragma once

#include "Device.h"
#include "Utils.h"
#include <memory>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

struct BufferData
{
    BufferData(
        std::shared_ptr<Device> device,
        vk::DeviceSize size,
        vk::BufferUsageFlags usage,
        vk::MemoryPropertyFlags propertyFlags = vk::MemoryPropertyFlagBits::eDeviceLocal
    );

    BufferData(std::nullptr_t);

    template <typename DataType>
    void upload(DataType const& data) const
    {
        assert((m_propertyFlags & vk::MemoryPropertyFlagBits::eHostCoherent) && (m_propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible));
        assert(sizeof(DataType) <= m_size);

        void* dataPtr = deviceMemory.mapMemory(0, sizeof(DataType));
        std::memcpy(dataPtr, &data, sizeof(DataType));
        deviceMemory.unmapMemory();
    }

    template <typename DataType>
    void upload(std::vector<DataType> const& data, size_t stride = 0) const
    {
        assert(m_propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible);

        size_t elementSize = stride ? stride : sizeof(DataType);
        assert(sizeof(DataType) <= elementSize);

        Utils::CopyToDevice(deviceMemory, data.data(), data.size(), elementSize);
    }

    template <typename DataType>
    void upload(std::shared_ptr<Device> const device, std::vector<DataType> const& data, size_t stride) const
    {
        assert(m_usage & vk::BufferUsageFlagBits::eTransferDst);
        assert(m_propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal);

        size_t elementSize = stride ? stride : sizeof(DataType);
        assert(sizeof(DataType) <= elementSize);

        size_t dataSize = data.size() * elementSize;
        assert(dataSize <= m_size);

        BufferData stagingBuffer(
            device,
            dataSize,
            vk::BufferUsageFlagBits::eTransferSrc,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
        );

        Utils::CopyToDevice(stagingBuffer.deviceMemory, data.data(), data.size(), elementSize);

        Utils::OneTimeSubmit(device, [&](vk::raii::CommandBuffer const& commandBuffer) {
            commandBuffer.copyBuffer(*stagingBuffer.buffer, *this->buffer, vk::BufferCopy(0, 0, dataSize));
        });
    }

    vk::raii::DeviceMemory deviceMemory = nullptr;
    vk::raii::Buffer buffer             = nullptr;

private:
    vk::DeviceSize m_size;
    vk::BufferUsageFlags m_usage;
    vk::MemoryPropertyFlags m_propertyFlags;
};
