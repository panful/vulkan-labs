#pragma once

#include "Event.h"
#include "ImageData.h"
#include "InteractorStyle.h"
#include "View.h"
#include <memory>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

struct Device;
struct Window;

class Viewer
{
    friend class View;
    friend class Actor;

public:
    Viewer(const vk::Extent2D& extent = {800, 600});

    Viewer(const std::shared_ptr<Device>& device, const uint32_t numberOfFrames, const bool useSwapChain, const vk::Extent2D& extent);

    std::vector<vk::ImageView> GetImageViews();

    ~Viewer();

    void ResizeFramebuffer(const vk::Extent2D& extent);

    void Record(const vk::raii::CommandBuffer& commadnBuffer);

    void Render();

    void AddView(const std::shared_ptr<View>& view);

    void ProcessEvent(const Event& event);

    void SetInteractorStyle(const std::shared_ptr<InteractorStyle>& interactorStyle);

    void SetPresentWindow(Window* window);

private:
    std::shared_ptr<Device> m_device {};
    Window* m_presentWindow {nullptr};

    uint32_t numberOfFrames {0};
    uint32_t currentFrameIndex {0};
    vk::Extent2D extent {800, 600};
    vk::raii::DescriptorPool descriptorPool {nullptr};
    vk::raii::RenderPass renderPass {nullptr};

    vk::Format m_colorFormat {vk::Format::eB8G8R8A8Unorm};
    vk::Format m_depthFormat {vk::Format::eD16Unorm};

    bool m_useSwapChain {false};
    bool m_supportBlit {false};

    std::vector<ImageData> m_colorImageDatas {};
    std::vector<ImageData> m_saveImageDatas {};
    DepthBufferData m_depthImagedata {nullptr};

    std::vector<vk::raii::Framebuffer> m_framebuffers {};

    std::vector<vk::raii::Fence> m_drawFences {};
    std::vector<vk::raii::Fence> m_blitFences {};
    std::vector<vk::raii::Semaphore> m_renderFinishedSemaphores {};

    vk::raii::CommandBuffers m_commandBuffers {nullptr};
    vk::raii::CommandBuffers m_blitImageCommandBuffers {nullptr};

    std::vector<std::shared_ptr<View>> m_views {};

    std::shared_ptr<InteractorStyle> m_interactorStyle {};
};
