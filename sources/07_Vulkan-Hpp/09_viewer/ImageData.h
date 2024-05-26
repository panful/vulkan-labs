#pragma once

#include <memory>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

struct Device;

struct ImageData
{
    ImageData(
        std::shared_ptr<Device> device,
        vk::Format format_,
        vk::Extent2D const& extent,
        vk::ImageTiling tiling,
        vk::ImageUsageFlags usage,
        vk::ImageLayout initialLayout,
        vk::MemoryPropertyFlags memoryProperties,
        vk::ImageAspectFlags aspectMask,
        bool createImageView = true
    );

    ImageData(std::nullptr_t);

    static void setImageLayout(
        vk::raii::CommandBuffer const& commandBuffer,
        vk::Image image,
        vk::Format format,
        vk::ImageLayout oldImageLayout,
        vk::ImageLayout newImageLayout
    );

    vk::Format format;
    vk::raii::DeviceMemory deviceMemory = nullptr;
    vk::raii::Image image               = nullptr;
    vk::raii::ImageView imageView       = nullptr;
};
