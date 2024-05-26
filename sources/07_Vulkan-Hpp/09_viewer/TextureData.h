#pragma once

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include "BufferData.h"
#include "ImageData.h"

struct TextureData
{
    TextureData(
        std::shared_ptr<Device> device,
        vk::Extent2D const& extent_               = {256, 256},
        vk::ImageUsageFlags usageFlags            = {},
        vk::FormatFeatureFlags formatFeatureFlags = {},
        bool anisotropyEnable                     = false,
        bool forceStaging                         = false
    );

    template <typename ImageGenerator>
    void setImage(vk::raii::CommandBuffer const& commandBuffer, ImageGenerator const& imageGenerator)
    {
        void* data = needsStaging ? stagingBufferData.deviceMemory.mapMemory(0, stagingBufferData.buffer.getMemoryRequirements().size)
                                  : imageData.deviceMemory.mapMemory(0, imageData.image.getMemoryRequirements().size);
        imageGenerator(data, extent);
        needsStaging ? stagingBufferData.deviceMemory.unmapMemory() : imageData.deviceMemory.unmapMemory();

        if (needsStaging)
        {
            // Since we're going to blit to the texture image, set its layout to eTransferDstOptimal
            ImageData::setImageLayout(
                commandBuffer, imageData.image, imageData.format, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal
            );
            vk::BufferImageCopy copyRegion(
                0,
                extent.width,
                extent.height,
                vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1),
                vk::Offset3D(0, 0, 0),
                vk::Extent3D(extent, 1)
            );
            commandBuffer.copyBufferToImage(stagingBufferData.buffer, imageData.image, vk::ImageLayout::eTransferDstOptimal, copyRegion);
            // Set the layout for the texture image from eTransferDstOptimal to eShaderReadOnlyOptimal
            ImageData::setImageLayout(
                commandBuffer, imageData.image, imageData.format, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal
            );
        }
        else
        {
            // If we can use the linear tiled image as a texture, just do it
            ImageData::setImageLayout(
                commandBuffer, imageData.image, imageData.format, vk::ImageLayout::ePreinitialized, vk::ImageLayout::eShaderReadOnlyOptimal
            );
        }
    }

    vk::Format format;
    vk::Extent2D extent;
    bool needsStaging;
    BufferData stagingBufferData = nullptr;
    ImageData imageData          = nullptr;
    vk::raii::Sampler sampler;
};
