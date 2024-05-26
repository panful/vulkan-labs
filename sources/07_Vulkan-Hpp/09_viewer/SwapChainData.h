#pragma once

#include <memory>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

struct Device;

struct SwapChainData
{
    SwapChainData(
        std::shared_ptr<Device> device,
        vk::raii::SurfaceKHR const& surface,
        vk::Extent2D const& extent,
        vk::ImageUsageFlags usage,
        vk::raii::SwapchainKHR const* pOldSwapchain,
        uint32_t graphicsQueueFamilyIndex,
        uint32_t presentQueueFamilyIndex
    );

    SwapChainData(std::nullptr_t);

    vk::Format colorFormat;
    vk::raii::SwapchainKHR swapChain = nullptr;
    std::vector<vk::Image> images;
    std::vector<vk::raii::ImageView> imageViews;
    vk::Extent2D swapchainExtent;
    uint32_t numberOfImages {};
};
