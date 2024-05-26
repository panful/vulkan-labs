#include "ImageData.h"
#include "Device.h"
#include "Utils.h"

ImageData::ImageData(
    std::shared_ptr<Device> device,
    vk::Format format_,
    vk::Extent2D const& extent,
    vk::ImageTiling tiling,
    vk::ImageUsageFlags usage,
    vk::ImageLayout initialLayout,
    vk::MemoryPropertyFlags memoryProperties,
    vk::ImageAspectFlags aspectMask,
    bool createImageView
)
    : format(format_)
    , image(
          device->device,
          {vk::ImageCreateFlags(),
           vk::ImageType::e2D,
           format,
           vk::Extent3D(extent, 1),
           1,
           1,
           vk::SampleCountFlagBits::e1,
           tiling,
           usage,
           vk::SharingMode::eExclusive,
           {},
           initialLayout}
      )
{
    deviceMemory = Utils::AllocateDeviceMemory(device, image.getMemoryRequirements(), memoryProperties);
    image.bindMemory(deviceMemory, 0);
    imageView = createImageView
        ? vk::raii::ImageView(device->device, vk::ImageViewCreateInfo({}, image, vk::ImageViewType::e2D, format, {}, {aspectMask, 0, 1, 0, 1}))
        : nullptr;
}

ImageData::ImageData(std::nullptr_t)
{
}

void ImageData::setImageLayout(
    vk::raii::CommandBuffer const& commandBuffer, vk::Image image, vk::Format format, vk::ImageLayout oldImageLayout, vk::ImageLayout newImageLayout
)
{
    vk::AccessFlags sourceAccessMask;
    switch (oldImageLayout)
    {
        case vk::ImageLayout::eTransferDstOptimal:
            sourceAccessMask = vk::AccessFlagBits::eTransferWrite;
            break;
        case vk::ImageLayout::ePreinitialized:
            sourceAccessMask = vk::AccessFlagBits::eHostWrite;
            break;
        case vk::ImageLayout::eGeneral: // sourceAccessMask is empty
        case vk::ImageLayout::eUndefined:
            break;
        default:
            assert(false);
            break;
    }

    vk::PipelineStageFlags sourceStage;
    switch (oldImageLayout)
    {
        case vk::ImageLayout::eGeneral:
        case vk::ImageLayout::ePreinitialized:
            sourceStage = vk::PipelineStageFlagBits::eHost;
            break;
        case vk::ImageLayout::eTransferDstOptimal:
            sourceStage = vk::PipelineStageFlagBits::eTransfer;
            break;
        case vk::ImageLayout::eUndefined:
            sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
            break;
        default:
            assert(false);
            break;
    }

    vk::AccessFlags destinationAccessMask;
    switch (newImageLayout)
    {
        case vk::ImageLayout::eColorAttachmentOptimal:
            destinationAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
            break;
        case vk::ImageLayout::eDepthStencilAttachmentOptimal:
            destinationAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite;
            break;
        case vk::ImageLayout::eGeneral: // empty destinationAccessMask
        case vk::ImageLayout::ePresentSrcKHR:
            break;
        case vk::ImageLayout::eShaderReadOnlyOptimal:
            destinationAccessMask = vk::AccessFlagBits::eShaderRead;
            break;
        case vk::ImageLayout::eTransferSrcOptimal:
            destinationAccessMask = vk::AccessFlagBits::eTransferRead;
            break;
        case vk::ImageLayout::eTransferDstOptimal:
            destinationAccessMask = vk::AccessFlagBits::eTransferWrite;
            break;
        default:
            assert(false);
            break;
    }

    vk::PipelineStageFlags destinationStage;
    switch (newImageLayout)
    {
        case vk::ImageLayout::eColorAttachmentOptimal:
            destinationStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
            break;
        case vk::ImageLayout::eDepthStencilAttachmentOptimal:
            destinationStage = vk::PipelineStageFlagBits::eEarlyFragmentTests;
            break;
        case vk::ImageLayout::eGeneral:
            destinationStage = vk::PipelineStageFlagBits::eHost;
            break;
        case vk::ImageLayout::ePresentSrcKHR:
            destinationStage = vk::PipelineStageFlagBits::eBottomOfPipe;
            break;
        case vk::ImageLayout::eShaderReadOnlyOptimal:
            destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
            break;
        case vk::ImageLayout::eTransferDstOptimal:
        case vk::ImageLayout::eTransferSrcOptimal:
            destinationStage = vk::PipelineStageFlagBits::eTransfer;
            break;
        default:
            assert(false);
            break;
    }

    vk::ImageAspectFlags aspectMask;
    if (newImageLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal)
    {
        aspectMask = vk::ImageAspectFlagBits::eDepth;
        if (format == vk::Format::eD32SfloatS8Uint || format == vk::Format::eD24UnormS8Uint)
        {
            aspectMask |= vk::ImageAspectFlagBits::eStencil;
        }
    }
    else
    {
        aspectMask = vk::ImageAspectFlagBits::eColor;
    }

    vk::ImageSubresourceRange imageSubresourceRange(aspectMask, 0, 1, 0, 1);
    vk::ImageMemoryBarrier imageMemoryBarrier(
        sourceAccessMask,
        destinationAccessMask,
        oldImageLayout,
        newImageLayout,
        VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED,
        image,
        imageSubresourceRange
    );
    return commandBuffer.pipelineBarrier(sourceStage, destinationStage, {}, nullptr, nullptr, imageMemoryBarrier);
}
