#include "BufferData.h"

BufferData::BufferData(std::shared_ptr<Device> device, vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags propertyFlags)
    : buffer(device->device, vk::BufferCreateInfo({}, size, usage))
    , m_size(size)
    , m_usage(usage)
    , m_propertyFlags(propertyFlags)
{
    deviceMemory = Utils::AllocateDeviceMemory(device, buffer.getMemoryRequirements(), propertyFlags);
    buffer.bindMemory(deviceMemory, 0);
}

BufferData::BufferData(std::nullptr_t)
{
}
