#include "SwapChainData.h"
#include "Device.h"
#include "Utils.h"

SwapChainData::SwapChainData(
    std::shared_ptr<Device> device,
    vk::raii::SurfaceKHR const& surface,
    vk::Extent2D const& extent,
    vk::ImageUsageFlags usage,
    vk::raii::SwapchainKHR const* pOldSwapchain,
    uint32_t graphicsQueueFamilyIndex,
    uint32_t presentQueueFamilyIndex
)
{
    vk::SurfaceFormatKHR surfaceFormat = Utils::PickSurfaceFormat(device->physicalDevice.getSurfaceFormatsKHR(surface));
    colorFormat                        = surfaceFormat.format;

    vk::SurfaceCapabilitiesKHR surfaceCapabilities = device->physicalDevice.getSurfaceCapabilitiesKHR(surface);
    if (surfaceCapabilities.currentExtent.width == std::numeric_limits<uint32_t>::max())
    {
        // If the surface size is undefined, the size is set to the size of the images requested.
        swapchainExtent.width  = std::clamp(extent.width, surfaceCapabilities.minImageExtent.width, surfaceCapabilities.maxImageExtent.width);
        swapchainExtent.height = std::clamp(extent.height, surfaceCapabilities.minImageExtent.height, surfaceCapabilities.maxImageExtent.height);
    }
    else
    {
        // If the surface size is defined, the swap chain size must match
        swapchainExtent = surfaceCapabilities.currentExtent;
    }
    vk::SurfaceTransformFlagBitsKHR preTransform = (surfaceCapabilities.supportedTransforms & vk::SurfaceTransformFlagBitsKHR::eIdentity)
        ? vk::SurfaceTransformFlagBitsKHR::eIdentity
        : surfaceCapabilities.currentTransform;
    vk::CompositeAlphaFlagBitsKHR compositeAlpha = (surfaceCapabilities.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::ePreMultiplied)
        ? vk::CompositeAlphaFlagBitsKHR::ePreMultiplied
        : (surfaceCapabilities.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::ePostMultiplied)
        ? vk::CompositeAlphaFlagBitsKHR::ePostMultiplied
        : (surfaceCapabilities.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::eInherit) ? vk::CompositeAlphaFlagBitsKHR::eInherit
                                                                                                  : vk::CompositeAlphaFlagBitsKHR::eOpaque;

    numberOfImages = std::clamp(3u, surfaceCapabilities.minImageCount, surfaceCapabilities.maxImageCount);

    vk::PresentModeKHR presentMode = Utils::PickPresentMode(device->physicalDevice.getSurfacePresentModesKHR(surface));
    vk::SwapchainCreateInfoKHR swapChainCreateInfo(
        {},
        surface,
        numberOfImages,
        colorFormat,
        surfaceFormat.colorSpace,
        swapchainExtent,
        1,
        usage,
        vk::SharingMode::eExclusive,
        {},
        preTransform,
        compositeAlpha,
        presentMode,
        true,
        pOldSwapchain ? **pOldSwapchain : nullptr
    );
    if (graphicsQueueFamilyIndex != presentQueueFamilyIndex)
    {
        uint32_t queueFamilyIndices[2] = {graphicsQueueFamilyIndex, presentQueueFamilyIndex};
        // If the graphics and present queues are from different queue families, we either have to explicitly
        // transfer ownership of images between the queues, or we have to create the swapchain with imageSharingMode
        // as vk::SharingMode::eConcurrent
        swapChainCreateInfo.imageSharingMode      = vk::SharingMode::eConcurrent;
        swapChainCreateInfo.queueFamilyIndexCount = 2;
        swapChainCreateInfo.pQueueFamilyIndices   = queueFamilyIndices;
    }
    swapChain = vk::raii::SwapchainKHR(device->device, swapChainCreateInfo);

    images = swapChain.getImages();

    imageViews.reserve(images.size());
    vk::ImageViewCreateInfo imageViewCreateInfo({}, {}, vk::ImageViewType::e2D, colorFormat, {}, {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});
    for (auto image : images)
    {
        imageViewCreateInfo.image = image;
        imageViews.emplace_back(device->device, imageViewCreateInfo);
    }
}

SwapChainData::SwapChainData(std::nullptr_t)
{
}
