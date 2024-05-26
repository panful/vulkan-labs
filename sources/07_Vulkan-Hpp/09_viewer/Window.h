#pragma once

#include "SurfaceData.h"
#include "SwapChainData.h"
#include <memory>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

struct GLFWwindow;
struct Device;
class Viewer;

struct WindowHelper
{
    WindowHelper(const std::string& name, const vk::Extent2D& extent);

    ~WindowHelper();

    WindowHelper(const WindowHelper&)            = delete;
    WindowHelper& operator=(const WindowHelper&) = delete;

    std::vector<const char*> GetExtensions() const noexcept;

    vk::SurfaceKHR InitSurface(const vk::raii::Instance& instance);

    bool ShouldExit() const noexcept;

    void PollEvents() const noexcept;

    void WaitWindowNotMinimized();

    static void FramebufferResizeCallback(GLFWwindow* window, int width, int height) noexcept;

    GLFWwindow* window {};
    vk::Extent2D extent {};
    SurfaceData surfaceData {nullptr};
};

struct Window
{
    Window(const std::string& name, const vk::Extent2D& extent);
    ~Window();

    void Run();

    void UpdateDescriptorSets();

private:
    void RecreateSwapChain();

    std::shared_ptr<Device> m_device {};
    std::unique_ptr<WindowHelper> m_windowHelper {};
    SwapChainData m_swapChainData {nullptr};

    vk::raii::CommandBuffers m_commandBuffers {nullptr};

    uint32_t m_numberOfFrames {0};
    uint32_t m_currentFrameIndex {0};

public:
    std::unique_ptr<Viewer> viewer {};

private:
    vk::raii::RenderPass m_renderPass {nullptr};
    vk::raii::DescriptorSetLayout m_descriptorSetLayout {nullptr};
    vk::raii::PipelineLayout m_pipelineLayout {nullptr};
    vk::raii::Pipeline m_graphicsPipeline {nullptr};
    vk::raii::Sampler m_sampler {nullptr};

    vk::raii::DescriptorPool m_descriptorPool {nullptr};
    vk::raii::DescriptorSets m_descriptorSets {nullptr};

    std::vector<vk::raii::Framebuffer> m_framebuffers {};

    std::vector<vk::raii::Fence> m_drawFences {};
    std::vector<vk::raii::Semaphore> m_renderFinishedSemaphores {};
    std::vector<vk::raii::Semaphore> m_imageAcquiredSemaphores {};
};
