#include "Window.h"
#include "Context.h"
#include "RenderGraph.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <format>
#include <iostream>
#include <numbers>
#include <set>
#include <stdexcept>

WindowHelper* WindowHelper::GetHelper()
{
    static WindowHelper helper {};
    return &helper;
}

GLFWwindow* WindowHelper::CreateWindow(int width, int height)
{
    m_numberOfWindows++;
    return glfwCreateWindow(width, height, "Vulkan", nullptr, nullptr);
}

void WindowHelper::DestroyWindow(GLFWwindow* window)
{
    glfwDestroyWindow(window);
}

WindowHelper::~WindowHelper() noexcept
{
    glfwTerminate();
}

WindowHelper::WindowHelper()
{
    if (GLFW_FALSE == glfwInit())
    {
        throw std::runtime_error("failed to init GLFW");
    }

    glfwSetErrorCallback([](int error, const char* msg) { std::cerr << std::format("glfw: ({}) {}\n", error, msg); });
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
}

Window::Window()
{
    m_window = WindowHelper::GetHelper()->CreateWindow();

    glfwSetWindowUserPointer(m_window, this);
    glfwSetFramebufferSizeCallback(m_window, FramebufferResizeCallback);
    glfwSetKeyCallback(m_window, KeyCallback);
    glfwSetCursorPosCallback(m_window, CursorPosCallback);
    glfwSetScrollCallback(m_window, ScrollCallback);
    glfwSetMouseButtonCallback(m_window, MouseButtonCallback);

    CreateSurface();
    Context::GetContext()->InitDevice(this);

    CreateCommandPool();
    CreateDescriptorPool();
    CreateCommandBuffers();
    CreateSyncObjects();

    CreateSwapChain();
}

Window::~Window() noexcept
{
    vkDeviceWaitIdle(Context::GetContext()->GetDevice());

    CleanupSwapChain();

    vkDestroyDescriptorPool(Context::GetContext()->GetDevice(), m_descriptorPool, nullptr);
    vkDestroyCommandPool(Context::GetContext()->GetDevice(), m_commandPool, nullptr);

    for (uint32_t i = 0u; i < Common::MaxFramesInFlight; ++i)
    {
        vkDestroySemaphore(Context::GetContext()->GetDevice(), m_imageAvailableSemaphores.at(i), nullptr);
        vkDestroySemaphore(Context::GetContext()->GetDevice(), m_renderFinishedSemaphores.at(i), nullptr);
        vkDestroyFence(Context::GetContext()->GetDevice(), m_inFlightFences.at(i), nullptr);
    }

    vkDestroySurfaceKHR(Context::GetContext()->GetInstance(), m_surface, nullptr);
    WindowHelper::GetHelper()->DestroyWindow(m_window);
}

VkSurfaceKHR Window::GetSurface() const noexcept
{
    return m_surface;
}

const std::vector<ImageData>& Window::GetColors() const noexcept
{
    return m_colors;
}

VkFormat Window::GetFormat() const noexcept
{
    return m_swapChainImageFormat;
}

VkExtent2D Window::GetExtent() const noexcept
{
    return m_swapChainExtent;
}

RenderGraph& Window::GetRenderGraph() noexcept
{
    return m_renderGraph;
}

bool Window::CheckPhysicalDeviceSupport(const VkPhysicalDevice physicalDevice) const noexcept
{
    SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(physicalDevice);
    return !swapChainSupport.foramts.empty() & !swapChainSupport.presentModes.empty();
}

void Window::MainLoop()
{
    while (!glfwWindowShouldClose(m_window))
    {
        glfwPollEvents();
        DrawFrame();
    }
}

void Window::DrawFrame()
{
    vkWaitForFences(Context::GetContext()->GetDevice(), 1, &m_inFlightFences.at(m_currentFrame), VK_TRUE, std::numeric_limits<uint64_t>::max());

    uint32_t imageIndex {0};
    VkResult result = vkAcquireNextImageKHR(
        Context::GetContext()->GetDevice(),
        m_swapChain,
        std::numeric_limits<uint64_t>::max(),
        m_imageAvailableSemaphores.at(m_currentFrame),
        VK_NULL_HANDLE,
        &imageIndex
    );

    if (VK_ERROR_OUT_OF_DATE_KHR == result)
    {
        // RecreateSwapChain();
        return;
    }
    else if (VK_SUCCESS != result && VK_SUBOPTIMAL_KHR != result)
    {
        throw std::runtime_error("failed to acquire swap chain image");
    }

    vkResetFences(Context::GetContext()->GetDevice(), 1, &m_inFlightFences.at(m_currentFrame));
    vkResetCommandBuffer(m_commandBuffers.at(m_currentFrame), 0);

    //----------------------------------------
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType                    = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags                    = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
    beginInfo.pInheritanceInfo         = nullptr;

    if (VK_SUCCESS != vkBeginCommandBuffer(m_commandBuffers.at(m_currentFrame), &beginInfo))
    {
        throw std::runtime_error("failed to begin recording command buffer");
    }

    m_renderGraph.Execute(m_commandBuffers.at(m_currentFrame), 0, imageIndex);

    if (VK_SUCCESS != vkEndCommandBuffer(m_commandBuffers.at(m_currentFrame)))
    {
        throw std::runtime_error("failed to record command buffer");
    }

    //-----------------------------------

    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

    VkSubmitInfo submitInfo         = {};
    submitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount   = 1;
    submitInfo.pWaitSemaphores      = &m_imageAvailableSemaphores.at(m_currentFrame);
    submitInfo.pWaitDstStageMask    = waitStages;
    submitInfo.commandBufferCount   = 1;
    submitInfo.pCommandBuffers      = &m_commandBuffers[m_currentFrame];
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores    = &m_renderFinishedSemaphores.at(m_currentFrame);

    if (VK_SUCCESS != vkQueueSubmit(Context::GetContext()->GetGraphicsQueue(), 1, &submitInfo, m_inFlightFences.at(m_currentFrame)))
    {
        throw std::runtime_error("failed to submit draw command buffer");
    }

    VkPresentInfoKHR presentInfo   = {};
    presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores    = &m_renderFinishedSemaphores.at(m_currentFrame);
    presentInfo.swapchainCount     = 1;
    presentInfo.pSwapchains        = &m_swapChain;
    presentInfo.pImageIndices      = &imageIndex;
    presentInfo.pResults           = nullptr;

    result = vkQueuePresentKHR(Context::GetContext()->GetPresentQueue(), &presentInfo);

    if (VK_ERROR_OUT_OF_DATE_KHR == result || VK_SUBOPTIMAL_KHR == result || m_framebufferResized)
    {
        m_framebufferResized = false;
        // RecreateSwapChain();
    }
    else if (VK_SUCCESS != result)
    {
        throw std::runtime_error("failed to presend swap chain image");
    }

    m_currentFrame = (m_currentFrame + 1) % Common::MaxFramesInFlight;
}

void Window::CreateSurface()
{
    if (VK_SUCCESS != glfwCreateWindowSurface(Context::GetContext()->GetInstance(), m_window, nullptr, &m_surface))
    {
        throw std::runtime_error("failed to create window surface");
    }
}

SwapChainSupportDetails Window::QuerySwapChainSupport(const VkPhysicalDevice device) const noexcept
{
    SwapChainSupportDetails details {};

    // 查询基础表面特性
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_surface, &details.capabilities);

    // 查询表面支持的格式，确保集合对于所有有效的格式可扩充
    uint32_t formatCount {0};
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, nullptr);
    if (0 != formatCount)
    {
        details.foramts.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, details.foramts.data());
    }

    // 查询表面支持的呈现模式
    uint32_t presentModeCount {0};
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount, nullptr);
    if (0 != presentModeCount)
    {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount, details.presentModes.data());
    }

    return details;
}

VkSurfaceFormatKHR Window::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) const noexcept
{
    // 表面没有自己的首选格式，直接返回指定的格式
    if (1 == availableFormats.size() && availableFormats.front().format == VK_FORMAT_UNDEFINED)
    {
        return {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    }

    // 遍历格式列表，如果想要的格式存在则直接返回
    for (const auto& availableFormat : availableFormats)
    {
        if (VK_FORMAT_B8G8R8A8_UNORM == availableFormat.format && VK_COLOR_SPACE_SRGB_NONLINEAR_KHR == availableFormat.colorSpace)
        {
            return availableFormat;
        }
    }

    // 如果以上两种方式都失效，通过“优良”进行打分排序，大多数情况下会选择第一个格式作为理想的选择
    return availableFormats.front();
}

VkPresentModeKHR Window::ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) const noexcept
{
    VkPresentModeKHR bestMode {VK_PRESENT_MODE_FIFO_KHR};

    for (const auto& availablePresentMode : availablePresentModes)
    {
        if (VK_PRESENT_MODE_MAILBOX_KHR == availablePresentMode)
        {
            return availablePresentMode;
        }
        else if (VK_PRESENT_MODE_IMMEDIATE_KHR == availablePresentMode)
        {
            bestMode = availablePresentMode;
        }
    }

    return bestMode;
}

VkExtent2D Window::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) const noexcept
{
    // 如果是最大值则表示允许我们自己选择对于窗口最合适的交换范围
    if (std::numeric_limits<uint32_t>::max() != capabilities.currentExtent.width)
    {
        return capabilities.currentExtent;
    }
    else
    {
        int width {0}, height {0};
        glfwGetFramebufferSize(m_window, &width, &height);

        // 使用GLFW窗口的大小来设置交换链中图像的分辨率
        VkExtent2D actualExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};

        actualExtent.width  = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return actualExtent;
    }
}

void Window::CreateSwapChain()
{
    SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(Context::GetContext()->GetPhysicalDevice());
    VkSurfaceFormatKHR surfaceFormat         = ChooseSwapSurfaceFormat(swapChainSupport.foramts);
    VkPresentModeKHR presentMode             = ChooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D extent                        = ChooseSwapExtent(swapChainSupport.capabilities);

    auto minImageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 && minImageCount > swapChainSupport.capabilities.maxImageCount)
    {
        minImageCount = swapChainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo = {};
    createInfo.sType                    = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface                  = m_surface;
    createInfo.minImageCount            = minImageCount;
    createInfo.imageFormat              = surfaceFormat.format;
    createInfo.imageColorSpace          = surfaceFormat.colorSpace;
    createInfo.imageExtent              = extent;
    createInfo.imageArrayLayers         = 1;
    createInfo.imageUsage               = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    const auto& queueFamilyIndices = Context::GetContext()->GetQueueFamilyIndices();
    std::set<uint32_t> indices {queueFamilyIndices.graphicsFamily.value(), queueFamilyIndices.presentFamily.value()};
    std::vector<uint32_t> uinqueIndices {indices.cbegin(), indices.cend()};

    if (uinqueIndices.size() > 1)
    {
        createInfo.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = static_cast<uint32_t>(uinqueIndices.size());
        createInfo.pQueueFamilyIndices   = uinqueIndices.data();
    }
    else
    {
        createInfo.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices   = nullptr;
    }

    createInfo.preTransform   = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode    = presentMode;
    createInfo.clipped        = VK_TRUE;
    createInfo.oldSwapchain   = VK_NULL_HANDLE;

    if (VK_SUCCESS != vkCreateSwapchainKHR(Context::GetContext()->GetDevice(), &createInfo, nullptr, &m_swapChain))
    {
        throw std::runtime_error("failed to create swap chain");
    }

    vkGetSwapchainImagesKHR(Context::GetContext()->GetDevice(), m_swapChain, &m_swapChainImageCount, nullptr);

    m_colors.resize(m_swapChainImageCount);
    std::vector<VkImage> swapChainImages;
    for (auto i = 0u; i < m_swapChainImageCount; ++i)
    {
        swapChainImages.emplace_back(m_colors[i].image);
    }
    vkGetSwapchainImagesKHR(Context::GetContext()->GetDevice(), m_swapChain, &m_swapChainImageCount, swapChainImages.data());
    for (auto i = 0u; i < m_swapChainImageCount; ++i)
    {
        m_colors[i].image = swapChainImages[i];
    }

    m_swapChainImageFormat = surfaceFormat.format;
    m_swapChainExtent      = extent;

    CreateImageViews();
}

void Window::CreateImageViews()
{
    for (auto i = 0u; i < m_swapChainImageCount; ++i)
    {
        VkImageViewCreateInfo createInfo           = {};
        createInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image                           = m_colors[i].image;
        createInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format                          = m_swapChainImageFormat;
        createInfo.components.r                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel   = 0;
        createInfo.subresourceRange.levelCount     = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount     = 1;

        if (VK_SUCCESS != vkCreateImageView(Context::GetContext()->GetDevice(), &createInfo, nullptr, &m_colors[i].imageView))
        {
            throw std::runtime_error("failed to create image views");
        }
    }
}

void Window::CreateCommandPool()
{
    auto indices = Context::GetContext()->GetQueueFamilyIndices();

    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType                   = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags                   = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex        = indices.graphicsFamily.value();

    if (VK_SUCCESS != vkCreateCommandPool(Context::GetContext()->GetDevice(), &poolInfo, nullptr, &m_commandPool))
    {
        throw std::runtime_error("failed to create command pool");
    }
}

void Window::CreateCommandBuffers()
{
    m_commandBuffers.resize(Common::MaxFramesInFlight);

    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType                       = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool                 = m_commandPool;
    allocInfo.level                       = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount          = static_cast<uint32_t>(m_commandBuffers.size());

    if (VK_SUCCESS != vkAllocateCommandBuffers(Context::GetContext()->GetDevice(), &allocInfo, m_commandBuffers.data()))
    {
        throw std::runtime_error("failed to allocate command buffers");
    }
}

void Window::CreateSyncObjects()
{
    m_imageAvailableSemaphores.resize(Common::MaxFramesInFlight);
    m_renderFinishedSemaphores.resize(Common::MaxFramesInFlight);
    m_inFlightFences.resize(Common::MaxFramesInFlight);

    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType                 = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType             = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags             = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < Common::MaxFramesInFlight; ++i)
    {
        if (VK_SUCCESS != vkCreateSemaphore(Context::GetContext()->GetDevice(), &semaphoreInfo, nullptr, &m_imageAvailableSemaphores.at(i))
            || VK_SUCCESS != vkCreateSemaphore(Context::GetContext()->GetDevice(), &semaphoreInfo, nullptr, &m_renderFinishedSemaphores.at(i))
            || VK_SUCCESS != vkCreateFence(Context::GetContext()->GetDevice(), &fenceInfo, nullptr, &m_inFlightFences.at(i)))
        {
            throw std::runtime_error("failed to create synchronization objects for a frame");
        }
    }
}

void Window::CleanupSwapChain() noexcept
{
    for (auto& color : m_colors)
    {
        vkDestroyImageView(Context::GetContext()->GetDevice(), color.imageView, nullptr);
    }

    vkDestroySwapchainKHR(Context::GetContext()->GetDevice(), m_swapChain, nullptr);
}

void Window::CreateDescriptorPool()
{
    VkDescriptorPoolSize poolSize {};
    poolSize.type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = static_cast<uint32_t>(Common::MaxFramesInFlight);

    VkDescriptorPoolCreateInfo poolInfo {};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes    = &poolSize;
    poolInfo.maxSets       = static_cast<uint32_t>(Common::MaxFramesInFlight);
    poolInfo.flags         = 0;

    if (VK_SUCCESS != vkCreateDescriptorPool(Context::GetContext()->GetDevice(), &poolInfo, nullptr, &m_descriptorPool))
    {
        throw std::runtime_error("failed to create descriptor pool");
    }
}

void Window::FramebufferResizeCallback(GLFWwindow* window, int widht, int height) noexcept
{
    if (auto app = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window)))
    {
        app->m_framebufferResized = true;
    }
}

void Window::CursorPosCallback(GLFWwindow* window, double xpos, double ypos) noexcept
{
    if (auto app = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window)))
    {
        // 计算鼠标移动的偏移量
        double xOffset = xpos - app->m_mouseLastX;
        double yOffset = ypos - app->m_mouseLastY;

        if (app->m_isRotating)
        {
            // 设置灵敏度，可以根据需要调整
            constexpr auto sensitivity = 0.5 / 180.0 * std::numbers::pi_v<double>;
            xOffset *= sensitivity;
            yOffset *= sensitivity;

            // 构造旋转四元数
            glm::vec3 xAxis        = glm::normalize(glm::cross(app->m_viewUp, app->m_eyePos));
            glm::vec3 yAxis        = glm::normalize(app->m_viewUp);
            glm::quat xRotation    = glm::angleAxis(static_cast<float>(-yOffset), xAxis);
            glm::quat yRotation    = glm::angleAxis(static_cast<float>(-xOffset), yAxis);
            glm::quat rotationQuat = xRotation * yRotation;

            app->m_eyePos = glm::vec3(glm::mat4(rotationQuat) * glm::vec4(app->m_eyePos, 1.f));
            app->m_viewUp = glm::vec3(glm::mat4(rotationQuat) * glm::vec4(app->m_viewUp, 1.f));
            // app->m_viewUp = glm::normalize(glm::cross(xAxis, -app->m_eyePos));
        }

        if (app->m_isTranslating)
        {
            xOffset /= (app->m_swapChainExtent.width / 2);
            yOffset /= (app->m_swapChainExtent.height / 2);

            glm::vec3 xAxis        = glm::normalize(glm::cross(app->m_viewUp, app->m_eyePos));
            glm::vec3 yAxis        = glm::normalize(app->m_viewUp);
            glm::mat4 xTranslation = glm::translate(glm::mat4(1.f), xAxis * static_cast<float>(-xOffset));
            glm::mat4 yTranslation = glm::translate(glm::mat4(1.f), yAxis * static_cast<float>(yOffset));
            glm::mat4 translation  = xTranslation * yTranslation;

            app->m_eyePos = glm::vec3(translation * glm::vec4(app->m_eyePos, 1.f));
            app->m_lookAt = glm::vec3(translation * glm::vec4(app->m_lookAt, 1.f));
        }

        // 更新上一次的鼠标位置
        app->m_mouseLastX = xpos;
        app->m_mouseLastY = ypos;
    }
}

void Window::ScrollCallback(GLFWwindow* window, double xoffset, double yoffset) noexcept
{
    if (auto app = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window)))
    {
        glm::vec3 direction = glm::normalize(app->m_eyePos - app->m_lookAt);
        float distance      = glm::distance(app->m_eyePos, app->m_lookAt);
        glm::vec3 translate = direction * distance * 0.1f;

        if (yoffset < 0.0)
        {
            glm::mat4 mat = glm::translate(glm::mat4(1.f), translate);
            app->m_eyePos = glm::vec3(mat * glm::vec4(app->m_eyePos, 1.f));
        }
        else if (yoffset > 0.0)
        {
            glm::mat4 mat = glm::translate(glm::mat4(1.f), translate * -1.f);
            app->m_eyePos = glm::vec3(mat * glm::vec4(app->m_eyePos, 1.f));
        }
    }
}

void Window::MouseButtonCallback(GLFWwindow* window, int button, int action, int mods) noexcept
{
    if (auto app = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window)))
    {
        if (GLFW_PRESS == action && GLFW_MOUSE_BUTTON_RIGHT == button)
        {
            app->m_isRotating = true;
            glfwGetCursorPos(window, &app->m_mouseLastX, &app->m_mouseLastY);
        }
        else if (GLFW_RELEASE == action && GLFW_MOUSE_BUTTON_RIGHT == button)
        {
            app->m_isRotating = false;
        }

        if (GLFW_PRESS == action && GLFW_MOUSE_BUTTON_LEFT == button)
        {
            app->m_isTranslating = true;
            glfwGetCursorPos(window, &app->m_mouseLastX, &app->m_mouseLastY);
        }
        else if (GLFW_RELEASE == action && GLFW_MOUSE_BUTTON_LEFT == button)
        {
            app->m_isTranslating = false;
        }
    }
}

void Window::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) noexcept
{
    if (auto app = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window)))
    {
        // 按下'r'/'R'恢复视角
        if (GLFW_PRESS == action && GLFW_KEY_R == key)
        {
            app->m_viewUp = glm::vec3 {0.f, 1.f, 0.f};
            app->m_eyePos = glm::vec3 {0.f, 0.f, -3.f};
            app->m_lookAt = glm::vec3 {0.f};
        }
    }
}

std::tuple<glm::vec3, glm::vec3, glm::vec3> Window::GetCameraParams() const noexcept
{
    return std::forward_as_tuple(m_eyePos, m_viewUp, m_lookAt);
}
