#pragma once

#include <vulkan/vulkan.h>

#define GLM_FORCE_RADIANS           // glm函数的参数使用弧度
#define GLM_FORCE_DEPTH_ZERO_TO_ONE // 透视矩阵深度值范围 [-1, 1] => [0, 1]
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

#include "Common.h"
#include "RenderGraph.h"

#include <atomic>
#include <vector>

struct GLFWwindow;

struct SwapChainSupportDetails
{
    VkSurfaceCapabilitiesKHR capabilities {};
    std::vector<VkSurfaceFormatKHR> foramts {};
    std::vector<VkPresentModeKHR> presentModes {};
};

class WindowHelper
{
public:
    static WindowHelper* GetHelper();

    GLFWwindow* CreateWindow(int width = 800, int height = 600);

    void DestroyWindow(GLFWwindow* window);

    ~WindowHelper() noexcept;

private:
    WindowHelper();

private:
    std::atomic_uint8_t m_numberOfWindows {0};
};

class Window
{
public:
    Window();

    ~Window() noexcept;

    VkSurfaceKHR GetSurface() const noexcept;

    bool CheckPhysicalDeviceSupport(const VkPhysicalDevice physicalDevice) const noexcept;

    std::tuple<glm::vec3, glm::vec3, glm::vec3> GetCameraParams() const noexcept;

    void MainLoop();

    RenderGraph& GetRenderGraph() noexcept;

    const std::vector<ImageData>& GetColors() const noexcept;

    VkFormat GetFormat() const noexcept;

    VkExtent2D GetExtent() const noexcept;

private:
    void CreateSurface();

    SwapChainSupportDetails QuerySwapChainSupport(const VkPhysicalDevice device) const noexcept;

    VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) const noexcept;

    VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) const noexcept;

    VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) const noexcept;

    void CreateSwapChain();

    void CreateImageViews();

    void CreateCommandPool();

    void CreateCommandBuffers();

    void CreateSyncObjects();

    void CreateDescriptorPool();

    void CleanupSwapChain() noexcept;

    void DrawFrame();

private:
    static void FramebufferResizeCallback(GLFWwindow* window, int widht, int height) noexcept;
    static void CursorPosCallback(GLFWwindow* window, double xpos, double ypos) noexcept;
    static void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset) noexcept;
    static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods) noexcept;
    static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) noexcept;

private:
    GLFWwindow* m_window {nullptr};
    VkSurfaceKHR m_surface {};
    VkSwapchainKHR m_swapChain {nullptr};

    VkFormat m_swapChainImageFormat {};
    VkExtent2D m_swapChainExtent {};

    VkDescriptorPool m_descriptorPool {nullptr};
    VkCommandPool m_commandPool {nullptr};

    std::vector<VkCommandBuffer> m_commandBuffers {};
    std::vector<VkSemaphore> m_imageAvailableSemaphores {};
    std::vector<VkSemaphore> m_renderFinishedSemaphores {};
    std::vector<VkFence> m_inFlightFences {};
    size_t m_currentFrame {0};
    bool m_framebufferResized {false};

    glm::vec3 m_viewUp {0.f, 1.f, 0.f};
    glm::vec3 m_eyePos {0.f, 0.f, -3.f};
    glm::vec3 m_lookAt {0.f};
    double m_mouseLastX {0.0};
    double m_mouseLastY {0.0};
    bool m_isRotating {false};
    bool m_isTranslating {false};

    uint32_t m_swapChainImageCount {0};

    std::vector<ImageData> m_colors {};

    RenderGraph m_renderGraph {this};
};
