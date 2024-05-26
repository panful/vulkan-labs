#include "TextureData.h"

TextureData::TextureData(
    std::shared_ptr<Device> device,
    vk::Extent2D const& extent_,
    vk::ImageUsageFlags usageFlags,
    vk::FormatFeatureFlags formatFeatureFlags,
    bool anisotropyEnable,
    bool forceStaging
)
    : format(vk::Format::eR8G8B8A8Unorm)
    , extent(extent_)
    , sampler(
          device->device,
          {{},
           vk::Filter::eLinear,
           vk::Filter::eLinear,
           vk::SamplerMipmapMode::eLinear,
           vk::SamplerAddressMode::eRepeat,
           vk::SamplerAddressMode::eRepeat,
           vk::SamplerAddressMode::eRepeat,
           0.0f,
           anisotropyEnable,
           16.0f,
           false,
           vk::CompareOp::eNever,
           0.0f,
           0.0f,
           vk::BorderColor::eFloatOpaqueBlack}
      )
{
    vk::FormatProperties formatProperties = device->physicalDevice.getFormatProperties(format);

    formatFeatureFlags |= vk::FormatFeatureFlagBits::eSampledImage;
    needsStaging = forceStaging || ((formatProperties.linearTilingFeatures & formatFeatureFlags) != formatFeatureFlags);
    vk::ImageTiling imageTiling;
    vk::ImageLayout initialLayout;
    vk::MemoryPropertyFlags requirements;
    if (needsStaging)
    {
        assert((formatProperties.optimalTilingFeatures & formatFeatureFlags) == formatFeatureFlags);
        stagingBufferData = BufferData(device, extent.width * extent.height * 4, vk::BufferUsageFlagBits::eTransferSrc);
        imageTiling       = vk::ImageTiling::eOptimal;
        usageFlags |= vk::ImageUsageFlagBits::eTransferDst;
        initialLayout = vk::ImageLayout::eUndefined;
    }
    else
    {
        imageTiling   = vk::ImageTiling::eLinear;
        initialLayout = vk::ImageLayout::ePreinitialized;
        requirements  = vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible;
    }
    imageData = ImageData(
        device,
        format,
        extent,
        imageTiling,
        usageFlags | vk::ImageUsageFlagBits::eSampled,
        initialLayout,
        requirements,
        vk::ImageAspectFlagBits::eColor
    );
}
