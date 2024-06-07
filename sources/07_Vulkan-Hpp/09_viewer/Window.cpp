#include "Window.h"
#include "Device.h"
#include "Utils.h"
#include "Viewer.h"
#include <GLFW/glfw3.h>
#include <fstream>
#include <iostream>
#include <mutex>

namespace {

vk::raii::Pipeline makeGraphicsPipelineForQuad(
    vk::raii::Device const& m_device,
    vk::raii::PipelineCache const& pipelineCache,
    vk::raii::ShaderModule const& vertexShaderModule,
    vk::SpecializationInfo const* vertexShaderSpecializationInfo,
    vk::raii::ShaderModule const& fragmentShaderModule,
    vk::SpecializationInfo const* fragmentShaderSpecializationInfo,
    uint32_t vertexStride,
    std::vector<std::pair<vk::Format, uint32_t>> const& vertexInputAttributeFormatOffset,
    vk::FrontFace frontFace,
    bool depthBuffered,
    vk::raii::PipelineLayout const& pipelineLayout,
    vk::raii::RenderPass const& renderPass
)
{
    std::array<vk::PipelineShaderStageCreateInfo, 2> pipelineShaderStageCreateInfos = {
        vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eVertex, vertexShaderModule, "main", vertexShaderSpecializationInfo),
        vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eFragment, fragmentShaderModule, "main", fragmentShaderSpecializationInfo)
    };

    std::vector<vk::VertexInputAttributeDescription> vertexInputAttributeDescriptions;
    vk::PipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo;

    vk::PipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreateInfo(
        vk::PipelineInputAssemblyStateCreateFlags(), vk::PrimitiveTopology::eTriangleList
    );

    vk::PipelineViewportStateCreateInfo pipelineViewportStateCreateInfo(vk::PipelineViewportStateCreateFlags(), 1, nullptr, 1, nullptr);

    vk::PipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo(
        vk::PipelineRasterizationStateCreateFlags(),
        false,
        false,
        vk::PolygonMode::eFill,
        vk::CullModeFlagBits::eBack,
        frontFace,
        false,
        0.0f,
        0.0f,
        0.0f,
        1.0f
    );

    vk::PipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo({}, vk::SampleCountFlagBits::e1);

    vk::PipelineDepthStencilStateCreateInfo pipelineDepthStencilStateCreateInfo(
        vk::PipelineDepthStencilStateCreateFlags(), depthBuffered, depthBuffered, vk::CompareOp::eLessOrEqual, false, false, {}, {}
    );

    vk::ColorComponentFlags colorComponentFlags(
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
    );
    vk::PipelineColorBlendAttachmentState pipelineColorBlendAttachmentState(
        false,
        vk::BlendFactor::eZero,
        vk::BlendFactor::eZero,
        vk::BlendOp::eAdd,
        vk::BlendFactor::eZero,
        vk::BlendFactor::eZero,
        vk::BlendOp::eAdd,
        colorComponentFlags
    );
    vk::PipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo(
        vk::PipelineColorBlendStateCreateFlags(),
        false,
        vk::LogicOp::eNoOp,
        pipelineColorBlendAttachmentState,
        {
            {1.0f, 1.0f, 1.0f, 1.0f}
    }
    );

    std::array<vk::DynamicState, 2> dynamicStates = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};
    vk::PipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo(vk::PipelineDynamicStateCreateFlags(), dynamicStates);

    vk::GraphicsPipelineCreateInfo graphicsPipelineCreateInfo(
        vk::PipelineCreateFlags(),
        pipelineShaderStageCreateInfos,
        &pipelineVertexInputStateCreateInfo,
        &pipelineInputAssemblyStateCreateInfo,
        nullptr,
        &pipelineViewportStateCreateInfo,
        &pipelineRasterizationStateCreateInfo,
        &pipelineMultisampleStateCreateInfo,
        &pipelineDepthStencilStateCreateInfo,
        &pipelineColorBlendStateCreateInfo,
        &pipelineDynamicStateCreateInfo,
        pipelineLayout,
        renderPass
    );

    return vk::raii::Pipeline(m_device, pipelineCache, graphicsPipelineCreateInfo);
}
} // namespace

WindowHelper::WindowHelper(const std::string& name, const vk::Extent2D& extent_)
    : extent(extent_)
{
    static std::once_flag init_flag {};
    std::call_once(init_flag, []() {
        if (!glfwInit())
        {
            throw std::runtime_error("failed to init glfw");
        }
        glfwSetErrorCallback([](int error, const char* msg) { std::cerr << "glfw: " << "(" << error << ") " << msg << std::endl; });
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    });

    {
        std::lock_guard lk(mutex);
        window = glfwCreateWindow(extent.width, extent.height, name.c_str(), nullptr, nullptr);
    }

    ++numberOfWindows;
}

WindowHelper::~WindowHelper()
{
    glfwDestroyWindow(window);
    if (--numberOfWindows == 0)
    {
        glfwTerminate();
    }
}

vk::SurfaceKHR WindowHelper::InitSurface(const vk::raii::Instance& instance)
{
    surfaceData = SurfaceData(instance, window, extent);
    return surfaceData.surface;
}

std::vector<const char*> WindowHelper::GetExtensions() const noexcept
{
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    return std::vector<const char*>(glfwExtensions, glfwExtensions + glfwExtensionCount);
}

bool WindowHelper::ShouldExit() const noexcept
{
    return glfwWindowShouldClose(window);
}

void WindowHelper::PollEvents() const noexcept
{
    glfwPollEvents();
}

void WindowHelper::WaitWindowNotMinimized()
{
    int width {0}, height {0};
    glfwGetFramebufferSize(window, &width, &height);
    while (0 == width || 0 == height)
    {
        std::cout << "window size: " << width << ' ' << height << '\n';
        glfwGetFramebufferSize(window, &width, &height);
        glfwWaitEvents();
    }

    extent = vk::Extent2D {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
    std::cout << "WindowHelper size: " << width << ' ' << height << '\n';
}

Window::Window(const std::shared_ptr<Device>& device, const std::string& name, const vk::Extent2D& extent)
{
    m_windowHelper = std::make_unique<WindowHelper>(name, extent);
    m_device       = device;
    m_windowHelper->InitSurface(m_device->GetInstance());
    InitWindow();
}

Window::Window(const std::string& name, const vk::Extent2D& extent)
{
    m_windowHelper = std::make_unique<WindowHelper>(name, extent);
    m_device       = std::make_shared<Device>(m_windowHelper);
    InitWindow();
}

Window::~Window()
{
    m_device->device.waitIdle();
}

void Window::Render()
{
    static int index = 0;
    std::cout << "Window render: " << index++ << std::endl;

    auto waitResult = m_device->device.waitForFences({m_drawFences[m_currentFrameIndex]}, VK_TRUE, std::numeric_limits<uint64_t>::max());
    auto [result, imageIndex] =
        m_swapChainData.swapChain.acquireNextImage(std::numeric_limits<uint64_t>::max(), m_imageAcquiredSemaphores[m_currentFrameIndex]);
    assert(imageIndex < m_swapChainData.images.size());

    m_device->device.resetFences({m_drawFences[m_currentFrameIndex]});

    auto&& cmd = m_commandBuffers[m_currentFrameIndex];
    cmd.reset();

    std::array<vk::ClearValue, 1> clearValues;
    clearValues[0].color = vk::ClearColorValue(0.1f, 0.2f, 0.3f, 1.f);
    vk::RenderPassBeginInfo renderPassBeginInfo(
        *m_renderPass, m_framebuffers[m_currentFrameIndex], vk::Rect2D(vk::Offset2D(0, 0), m_swapChainData.swapchainExtent), clearValues
    );

    cmd.begin({});

    m_viewer->Record(cmd);

    cmd.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);

    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *m_graphicsPipeline);
    cmd.setViewport(
        0,
        vk::Viewport(
            0.0f,
            0.0f,
            static_cast<float>(m_swapChainData.swapchainExtent.width),
            static_cast<float>(m_swapChainData.swapchainExtent.height),
            0.0f,
            1.0f
        )
    );
    cmd.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), m_swapChainData.swapchainExtent));
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelineLayout, 0, {m_descriptorSets[m_currentFrameIndex]}, nullptr);
    cmd.draw(3, 1, 0, 0);

    cmd.endRenderPass();
    cmd.end();

    //--------------------------------------------------------------------------------------
    std::array<vk::CommandBuffer, 1> drawCommandBuffers {cmd};
    std::array<vk::Semaphore, 1> signalSemaphores {m_renderFinishedSemaphores[m_currentFrameIndex]};
    std::array<vk::Semaphore, 1> waitSemaphores {m_imageAcquiredSemaphores[m_currentFrameIndex]};
    std::array<vk::PipelineStageFlags, 1> waitStages = {vk::PipelineStageFlagBits::eColorAttachmentOutput};
    vk::SubmitInfo drawSubmitInfo(waitSemaphores, waitStages, drawCommandBuffers, signalSemaphores);
    m_device->graphicsQueue.submit(drawSubmitInfo, m_drawFences[m_currentFrameIndex]);

    //--------------------------------------------------------------------------------------
    std::array<vk::Semaphore, 1> presentWait {m_renderFinishedSemaphores[m_currentFrameIndex]};
    std::array<vk::SwapchainKHR, 1> swapchains {m_swapChainData.swapChain};
    vk::PresentInfoKHR presentInfoKHR(presentWait, swapchains, imageIndex);

    try
    {
        auto presentResult = m_device->presentQueue.presentKHR(presentInfoKHR);
    }
    catch (vk::SystemError& err)
    {
        std::cout << "vk::SystemError\n\twhat: " << err.what() << "\n\tcode: " << err.code() << '\n';
    }

    m_currentFrameIndex = (m_currentFrameIndex + 1) % m_numberOfFrames;
}

void Window::AddView(const std::shared_ptr<View>& view)
{
    m_viewer->AddView(view);
}

void Window::UpdateDescriptorSets()
{
    auto viewer_imageViews = m_viewer->GetImageViews();

    for (uint32_t i = 0; i < m_numberOfFrames; ++i)
    {
        vk::DescriptorImageInfo descriptorImageInfo(m_sampler, viewer_imageViews[i], vk::ImageLayout::eShaderReadOnlyOptimal);
        std::array writeDescriptorSets {
            vk::WriteDescriptorSet {m_descriptorSets[i], 0, 0, vk::DescriptorType::eCombinedImageSampler, descriptorImageInfo, nullptr}
        };
        m_device->device.updateDescriptorSets(writeDescriptorSets, nullptr);
    }
}

void Window::RecreateSwapChain()
{
    std::cout << "recreate swap chain: " << m_windowHelper->extent.width << ' ' << m_windowHelper->extent.height << '\n';

    m_currentFrameIndex = 0; // 保证下一帧的序号从 0 开始

    m_swapChainData = SwapChainData(
        m_device,
        m_windowHelper->surfaceData.surface,
        m_windowHelper->extent,
        vk::ImageUsageFlagBits::eColorAttachment,
        &m_swapChainData.swapChain,
        m_device->graphicsQueueIndex,
        m_device->presentQueueIndex
    );

    assert(m_numberOfFrames == m_swapChainData.numberOfImages);

    auto w = m_swapChainData.swapchainExtent.width;
    auto h = m_swapChainData.swapchainExtent.height;

    for (uint32_t i = 0; i < m_numberOfFrames; ++i)
    {
        std::array<vk::ImageView, 1> imageViews {m_swapChainData.imageViews[i]};
        m_framebuffers[i] = vk::raii::Framebuffer(m_device->device, vk::FramebufferCreateInfo({}, m_renderPass, imageViews, w, h, 1));
    }
}

void Window::ResizeWindow(const vk::Extent2D& extent)
{
    RecreateSwapChain();
    UpdateDescriptorSets();
}

void Window::InitWindow()
{
    m_swapChainData = SwapChainData(
        m_device,
        m_windowHelper->surfaceData.surface,
        m_windowHelper->surfaceData.extent,
        vk::ImageUsageFlagBits::eColorAttachment,
        nullptr,
        m_device->graphicsQueueIndex,
        m_device->presentQueueIndex
    );

    m_numberOfFrames = m_swapChainData.numberOfImages;

    m_viewer = std::make_unique<Viewer>(m_device, m_numberOfFrames, true, m_windowHelper->surfaceData.extent);
    m_viewer->SetPresentWindow(this);

    m_commandBuffers = vk::raii::CommandBuffers(
        m_device->device, vk::CommandBufferAllocateInfo {m_device->commandPoolReset, vk::CommandBufferLevel::ePrimary, m_numberOfFrames}
    );

    vk::AttachmentReference colorAttachment(0, vk::ImageLayout::eColorAttachmentOptimal);
    std::array colorAttachments {colorAttachment};
    vk::SubpassDescription subpassDescription(vk::SubpassDescriptionFlags(), vk::PipelineBindPoint::eGraphics, {}, colorAttachments, {}, {}, {});
    std::array attachmentDescriptions {
        vk::AttachmentDescription {
                                   {},
                                   m_swapChainData.colorFormat,
                                   vk::SampleCountFlagBits::e1,
                                   vk::AttachmentLoadOp::eClear,
                                   vk::AttachmentStoreOp::eStore,
                                   vk::AttachmentLoadOp::eDontCare,
                                   vk::AttachmentStoreOp::eDontCare,
                                   vk::ImageLayout::eUndefined,
                                   vk::ImageLayout::ePresentSrcKHR
        }
    };

    vk::RenderPassCreateInfo renderPassCreateInfo(vk::RenderPassCreateFlags(), attachmentDescriptions, subpassDescription);
    m_renderPass = vk::raii::RenderPass(m_device->device, renderPassCreateInfo);

    //--------------------------------------------------------------------------------------
    std::vector<uint32_t> vertSPV = Utils::ReadSPVShader("../resources/shaders/07_08_quad_vert.spv");
    std::vector<uint32_t> fragSPV = Utils::ReadSPVShader("../resources/shaders/07_08_quad_frag.spv");
    vk::raii::ShaderModule vertexShaderModule(m_device->device, vk::ShaderModuleCreateInfo(vk::ShaderModuleCreateFlags(), vertSPV));
    vk::raii::ShaderModule fragmentShaderModule(m_device->device, vk::ShaderModuleCreateInfo(vk::ShaderModuleCreateFlags(), fragSPV));

    std::array descriptorSetLayoutBindings {
        vk::DescriptorSetLayoutBinding {0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment}
    };
    vk::DescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo({}, descriptorSetLayoutBindings);
    m_descriptorSetLayout = vk::raii::DescriptorSetLayout(m_device->device, descriptorSetLayoutCreateInfo);

    std::array<vk::DescriptorSetLayout, 1> descriptorSetLayoursForPipeline {m_descriptorSetLayout};
    m_pipelineLayout = vk::raii::PipelineLayout(m_device->device, vk::PipelineLayoutCreateInfo {{}, descriptorSetLayoursForPipeline});
    vk::raii::PipelineCache pipelineCache(m_device->device, vk::PipelineCacheCreateInfo());

    m_graphicsPipeline = makeGraphicsPipelineForQuad(
        m_device->device,
        pipelineCache,
        vertexShaderModule,
        nullptr,
        fragmentShaderModule,
        nullptr,
        0,
        {},
        vk::FrontFace::eClockwise,
        false,
        m_pipelineLayout,
        m_renderPass
    );

    //--------------------------------------------------------------------------------------
    std::array descriptorPoolSizes {
        vk::DescriptorPoolSize {vk::DescriptorType::eUniformBuffer,        m_numberOfFrames},
        vk::DescriptorPoolSize {vk::DescriptorType::eCombinedImageSampler, m_numberOfFrames},
    };

    m_descriptorPool = vk::raii::DescriptorPool(
        m_device->device, vk::DescriptorPoolCreateInfo {vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, m_numberOfFrames, descriptorPoolSizes}
    );

    std::vector<vk::DescriptorSetLayout> descriptorSetLayouts(m_numberOfFrames, m_descriptorSetLayout);

    m_descriptorSets = vk::raii::DescriptorSets(m_device->device, vk::DescriptorSetAllocateInfo {m_descriptorPool, descriptorSetLayouts});

    m_sampler = vk::raii::Sampler(
        m_device->device,
        vk::SamplerCreateInfo {
            {},
            vk::Filter::eLinear,
            vk::Filter::eLinear,
            vk::SamplerMipmapMode::eLinear,
            vk::SamplerAddressMode::eRepeat,
            vk::SamplerAddressMode::eRepeat,
            vk::SamplerAddressMode::eRepeat,
            0.0f,
            false,
            16.0f,
            false,
            vk::CompareOp::eNever,
            0.0f,
            0.0f,
            vk::BorderColor::eFloatOpaqueBlack
        }
    );

    UpdateDescriptorSets();

    auto w = m_swapChainData.swapchainExtent.width;
    auto h = m_swapChainData.swapchainExtent.height;
    m_framebuffers.reserve(m_numberOfFrames);
    for (uint32_t i = 0; i < m_numberOfFrames; ++i)
    {
        std::array<vk::ImageView, 1> imageViews {m_swapChainData.imageViews[i]};
        m_framebuffers.emplace_back(vk::raii::Framebuffer(m_device->device, vk::FramebufferCreateInfo({}, m_renderPass, imageViews, w, h, 1)));
    }

    //--------------------------------------------------------------------------------------
    m_drawFences.reserve(m_numberOfFrames);
    m_renderFinishedSemaphores.reserve(m_numberOfFrames);
    m_imageAcquiredSemaphores.reserve(m_numberOfFrames);
    for (uint32_t i = 0; i < m_numberOfFrames; ++i)
    {
        m_drawFences.emplace_back(vk::raii::Fence(m_device->device, {vk::FenceCreateFlagBits::eSignaled}));
        m_renderFinishedSemaphores.emplace_back(vk::raii::Semaphore(m_device->device, vk::SemaphoreCreateInfo()));
        m_imageAcquiredSemaphores.emplace_back(vk::raii::Semaphore(m_device->device, vk::SemaphoreCreateInfo()));
    }
}

void Window::WaitIdle() const noexcept
{
    m_device->device.waitIdle();
}

std::shared_ptr<Device> Window::GetDevice() const noexcept
{
    return m_device;
}

const std::unique_ptr<WindowHelper>& Window::GetWindowHelper() const noexcept
{
    return m_windowHelper;
}

const std::unique_ptr<Viewer>& Window::GetViewer() const noexcept
{
    return m_viewer;
}

void Window::SetInteractorStyle(const std::shared_ptr<InteractorStyle>& interactorStyle)
{
    m_viewer->SetInteractorStyle(interactorStyle);
}
