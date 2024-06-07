#pragma once

#include "SurfaceData.h"
#include "SwapChainData.h"
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

struct GLFWwindow;
struct Device;
class Viewer;
class View;
class InteractorStyle;

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

    inline static std::mutex mutex {};
    inline static std::atomic_uint32_t numberOfWindows {0};

    GLFWwindow* window {};
    vk::Extent2D extent {};
    SurfaceData surfaceData {nullptr};
};

struct Window
{
    Window(const std::string& name, const vk::Extent2D& extent);
    Window(const std::shared_ptr<Device>& device, const std::string& name, const vk::Extent2D& extent);
    ~Window();

    void Render();

    void ResizeWindow(const vk::Extent2D& extent);

    void AddView(const std::shared_ptr<View>& view);

    void SetInteractorStyle(const std::shared_ptr<InteractorStyle>& interactorStyle);

    void WaitIdle() const noexcept;

    std::shared_ptr<Device> GetDevice() const noexcept;
    const std::unique_ptr<WindowHelper>& GetWindowHelper() const noexcept;
    const std::unique_ptr<Viewer>& GetViewer() const noexcept;

private:
    void UpdateDescriptorSets();
    void RecreateSwapChain();
    void InitWindow();

private:
    std::shared_ptr<Device> m_device {};
    std::unique_ptr<WindowHelper> m_windowHelper {};
    SwapChainData m_swapChainData {nullptr};

    vk::raii::CommandBuffers m_commandBuffers {nullptr};

    uint32_t m_numberOfFrames {0};
    uint32_t m_currentFrameIndex {0};

    std::unique_ptr<Viewer> m_viewer {};

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
