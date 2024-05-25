#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <GLFW/glfw3.h>

#include <algorithm>
#include <fstream>
#include <iostream>

static std::string AppName {"Vulkan-Hpp"};
static std::string EngineName {"Vulkan-Hpp"};
static std::vector<const char*> EnableLayerNames {"VK_LAYER_KHRONOS_validation"};
static std::vector<const char*> EnableExtensionNames {VK_EXT_DEBUG_UTILS_EXTENSION_NAME};

static vk::Extent2D extent {800, 600};

constexpr static uint64_t TimeOut {std::numeric_limits<uint64_t>::max()};
constexpr static uint32_t MaxFramesInFlight {3};
static uint32_t CurrentFrameIndex {0};

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData
) noexcept
{
    std::clog << "===========================================\n"
              << "Debug::validation layer: " << pCallbackData->pMessage << '\n';

    return VK_FALSE;
}

static std::vector<uint32_t> ReadFile(const std::string& fileName)
{
    std::ifstream file(fileName, std::ios::ate | std::ios::binary);
    if (!file.is_open())
    {
        throw std::runtime_error("failed to open file: " + fileName);
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<uint32_t> buffer(fileSize / (sizeof(uint32_t) / sizeof(char)));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(buffer.data()), fileSize);
    file.close();

    return buffer;
}

vk::raii::Pipeline makeGraphicsPipeline(
    vk::raii::Device const& device,
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

    return vk::raii::Pipeline(device, pipelineCache, graphicsPipelineCreateInfo);
}

struct Window
{
    Window(std::string const& _name, vk::Extent2D const& _extent)
        : extent(_extent)
    {
        glfwInit();
        glfwSetErrorCallback([](int error, const char* msg) { std::cerr << "glfw: " << "(" << error << ") " << msg << std::endl; });
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        window = glfwCreateWindow(extent.width, extent.height, _name.c_str(), nullptr, nullptr);

        glfwSetWindowUserPointer(window, this);
        glfwSetFramebufferSizeCallback(window, [](GLFWwindow* window, int width, int height) {});
    }

    ~Window()
    {
        glfwDestroyWindow(window);
        glfwTerminate();
    }

    Window(const Window&)            = delete;
    Window& operator=(const Window&) = delete;

    std::vector<const char*> GetExtensions() const noexcept
    {
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        return std::vector<const char*>(glfwExtensions, glfwExtensions + glfwExtensionCount);
    }

    bool ShouldExit() const noexcept
    {
        return glfwWindowShouldClose(window);
    }

    void PollEvents() const noexcept
    {
        glfwPollEvents();
    }

    GLFWwindow* window {};
    vk::Extent2D extent {};
};

struct SurfaceData
{
    SurfaceData(vk::raii::Instance const& instance, GLFWwindow* window, vk::Extent2D const& extent_)
        : extent(extent_)
    {
        VkSurfaceKHR _surface;
        VkResult err = glfwCreateWindowSurface(static_cast<VkInstance>(*instance), window, nullptr, &_surface);
        surface      = vk::raii::SurfaceKHR(instance, _surface);
    }

    vk::Extent2D extent;
    vk::raii::SurfaceKHR surface = nullptr;
};

vk::SurfaceFormatKHR pickSurfaceFormat(std::vector<vk::SurfaceFormatKHR> const& formats)
{
    assert(!formats.empty());
    vk::SurfaceFormatKHR pickedFormat = formats[0];
    if (formats.size() == 1)
    {
        if (formats[0].format == vk::Format::eUndefined)
        {
            pickedFormat.format     = vk::Format::eB8G8R8A8Unorm;
            pickedFormat.colorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;
        }
    }
    else
    {
        // request several formats, the first found will be used
        vk::Format requestedFormats[] = {vk::Format::eB8G8R8A8Unorm, vk::Format::eR8G8B8A8Unorm, vk::Format::eB8G8R8Unorm, vk::Format::eR8G8B8Unorm};
        vk::ColorSpaceKHR requestedColorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;
        for (size_t i = 0; i < sizeof(requestedFormats) / sizeof(requestedFormats[0]); i++)
        {
            vk::Format requestedFormat = requestedFormats[i];
            auto it = std::find_if(formats.begin(), formats.end(), [requestedFormat, requestedColorSpace](vk::SurfaceFormatKHR const& f) {
                return (f.format == requestedFormat) && (f.colorSpace == requestedColorSpace);
            });
            if (it != formats.end())
            {
                pickedFormat = *it;
                break;
            }
        }
    }
    assert(pickedFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear);
    return pickedFormat;
}

vk::PresentModeKHR pickPresentMode(std::vector<vk::PresentModeKHR> const& presentModes)
{
    vk::PresentModeKHR pickedMode = vk::PresentModeKHR::eFifo;
    for (const auto& presentMode : presentModes)
    {
        if (presentMode == vk::PresentModeKHR::eMailbox)
        {
            pickedMode = presentMode;
            break;
        }

        if (presentMode == vk::PresentModeKHR::eImmediate)
        {
            pickedMode = presentMode;
        }
    }
    return pickedMode;
}

struct SwapChainData
{
    SwapChainData(
        vk::raii::PhysicalDevice const& physicalDevice,
        vk::raii::Device const& device,
        vk::raii::SurfaceKHR const& surface,
        vk::Extent2D const& extent,
        vk::ImageUsageFlags usage,
        vk::raii::SwapchainKHR const* pOldSwapchain,
        uint32_t graphicsQueueFamilyIndex,
        uint32_t presentQueueFamilyIndex
    )
    {
        vk::SurfaceFormatKHR surfaceFormat = pickSurfaceFormat(physicalDevice.getSurfaceFormatsKHR(surface));
        colorFormat                        = surfaceFormat.format;

        vk::SurfaceCapabilitiesKHR surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(surface);
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

        vk::PresentModeKHR presentMode = pickPresentMode(physicalDevice.getSurfacePresentModesKHR(surface));
        vk::SwapchainCreateInfoKHR swapChainCreateInfo(
            {},
            surface,
            std::clamp(3u, surfaceCapabilities.minImageCount, surfaceCapabilities.maxImageCount),
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
        swapChain = vk::raii::SwapchainKHR(device, swapChainCreateInfo);

        images = swapChain.getImages();

        imageViews.reserve(images.size());
        vk::ImageViewCreateInfo imageViewCreateInfo({}, {}, vk::ImageViewType::e2D, colorFormat, {}, {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});
        for (auto image : images)
        {
            imageViewCreateInfo.image = image;
            imageViews.emplace_back(device, imageViewCreateInfo);
        }
    }

    vk::Format colorFormat;
    vk::raii::SwapchainKHR swapChain = nullptr;
    std::vector<vk::Image> images;
    std::vector<vk::raii::ImageView> imageViews;
    vk::Extent2D swapchainExtent;
};

void RecreateSwapChain(
    uint32_t& index,
    GLFWwindow* window,
    SwapChainData& swapChainData,
    std::array<vk::raii::Framebuffer, MaxFramesInFlight>& framebuffers,
    vk::raii::RenderPass const& renderPass,
    vk::raii::PhysicalDevice const& physicalDevice,
    vk::raii::Device const& device,
    vk::raii::SurfaceKHR const& surface,
    vk::Extent2D const& extent,
    vk::ImageUsageFlags usage,
    vk::raii::SwapchainKHR const* pOldSwapchain,
    uint32_t graphicsQueueFamilyIndex,
    uint32_t presentQueueFamilyIndex
)
{
    index = MaxFramesInFlight - 1; // XXX: 验证层报错：交换链图像布局不符合展示需要的布局，窗口大小改变时当前帧序号从0开始就不会报错

    int width {0}, height {0};
    glfwGetFramebufferSize(window, &width, &height);
    while (0 == width || 0 == height)
    {
        glfwGetFramebufferSize(window, &width, &height);
        glfwWaitEvents();
    }

    device.waitIdle();

    try
    {
        swapChainData = SwapChainData(
            physicalDevice,
            device,
            surface,
            extent,
            vk::ImageUsageFlagBits::eColorAttachment,
            pOldSwapchain,
            graphicsQueueFamilyIndex,
            presentQueueFamilyIndex
        );
    }
    catch (vk::SystemError& err)
    {
        std::cout << "swapChain vk::SystemError: " << err.what() << std::endl;
    }

    std::array<std::array<vk::ImageView, 1>, MaxFramesInFlight> imageViews {
        std::array<vk::ImageView, 1> {swapChainData.imageViews[0]},
        std::array<vk::ImageView, 1> {swapChainData.imageViews[1]},
        std::array<vk::ImageView, 1> {swapChainData.imageViews[2]},
    };

    auto w = swapChainData.swapchainExtent.width;
    auto h = swapChainData.swapchainExtent.height;

    framebuffers[0] = vk::raii::Framebuffer(device, vk::FramebufferCreateInfo({}, renderPass, imageViews[0], w, h, 1));
    framebuffers[1] = vk::raii::Framebuffer(device, vk::FramebufferCreateInfo({}, renderPass, imageViews[1], w, h, 1));
    framebuffers[2] = vk::raii::Framebuffer(device, vk::FramebufferCreateInfo({}, renderPass, imageViews[2], w, h, 1));
}

int main()
{
    try
    {
        Window window {"Test", extent};

        //--------------------------------------------------------------------------------------
        // 初始化 VkInstance
        vk::DebugUtilsMessageSeverityFlagsEXT severityFlags {
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError
        };
        vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags {
            vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance
            | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation
        };

        vk::DebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfoExt {{}, severityFlags, messageTypeFlags, DebugCallback};

        vk::ApplicationInfo applicationInfo {AppName.c_str(), 1, EngineName.c_str(), 1, VK_API_VERSION_1_1};
        auto&& windowExtensions = window.GetExtensions();
        EnableExtensionNames.insert(EnableExtensionNames.cend(), windowExtensions.begin(), windowExtensions.end());
        vk::InstanceCreateInfo instanceCreateInfo {{}, &applicationInfo, EnableLayerNames, EnableExtensionNames, &debugUtilsMessengerCreateInfoExt};

        vk::raii::Context context {};
        vk::raii::Instance instance {context, instanceCreateInfo};
        vk::raii::DebugUtilsMessengerEXT debugUtilsMessenger {
            instance,
            {{},
              vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
              vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance
                 | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation,
              &DebugCallback}
        };

        SurfaceData surfaceData(instance, window.window, extent);

        //--------------------------------------------------------------------------------------
        // 选择一个合适的物理设备
        vk::raii::PhysicalDevices physicalDevices(instance); // 所有物理设备，继承自 std::vector
        std::optional<uint32_t> optUsePhysicalDeviceIndex {};
        uint32_t graphicsTransferPresentQueueIndex {};
        for (size_t i = 0; i < physicalDevices.size(); ++i)
        {
            std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevices[i].getQueueFamilyProperties();
            for (size_t j = 0; j < queueFamilyProperties.size(); ++j)
            {
                if ((queueFamilyProperties[j].queueFlags & vk::QueueFlagBits::eGraphics)
                    && (queueFamilyProperties[j].queueFlags & vk::QueueFlagBits::eTransfer)
                    && physicalDevices[i].getSurfaceSupportKHR(j, surfaceData.surface))
                {
                    optUsePhysicalDeviceIndex         = static_cast<uint32_t>(i);
                    graphicsTransferPresentQueueIndex = static_cast<uint32_t>(j);
                }
            }
        }
        assert(optUsePhysicalDeviceIndex);
        auto usePhysicalDeviceIndex = optUsePhysicalDeviceIndex.value();
        auto physicalDevice         = physicalDevices[usePhysicalDeviceIndex];
        auto colorFormat            = pickSurfaceFormat(physicalDevice.getSurfaceFormatsKHR(surfaceData.surface)).format;

        //--------------------------------------------------------------------------------------
        // 创建一个逻辑设备
        float queuePriority = 0.0f;
        vk::DeviceQueueCreateInfo deviceQueueCreateInfo({}, graphicsTransferPresentQueueIndex, 1, &queuePriority);
        std::array<const char*, 1> deviceExtensions {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
        vk::DeviceCreateInfo deviceCreateInfo({}, deviceQueueCreateInfo, {}, deviceExtensions, {});
        vk::raii::Device device(physicalDevice, deviceCreateInfo);

        //--------------------------------------------------------------------------------------
        SwapChainData swapChainData(
            physicalDevice,
            device,
            surfaceData.surface,
            surfaceData.extent,
            vk::ImageUsageFlagBits::eColorAttachment,
            {},
            graphicsTransferPresentQueueIndex,
            graphicsTransferPresentQueueIndex
        );
        assert(swapChainData.imageViews.size() == MaxFramesInFlight);

        //--------------------------------------------------------------------------------------
        vk::raii::CommandPool commandPool =
            vk::raii::CommandPool(device, {{vk::CommandPoolCreateFlagBits::eResetCommandBuffer}, graphicsTransferPresentQueueIndex});
        vk::raii::CommandBuffers commandBuffers(device, {commandPool, vk::CommandBufferLevel::ePrimary, MaxFramesInFlight});

        vk::raii::Queue graphicsTransferPresentQueue(device, graphicsTransferPresentQueueIndex, 0);

        //--------------------------------------------------------------------------------------
        vk::AttachmentReference colorAttachment(0, vk::ImageLayout::eColorAttachmentOptimal);
        std::array colorAttachments {colorAttachment};
        vk::SubpassDescription subpassDescription(vk::SubpassDescriptionFlags(), vk::PipelineBindPoint::eGraphics, {}, colorAttachments, {}, {}, {});
        std::vector<vk::AttachmentDescription> attachmentDescriptions {
            {{},
             colorFormat, vk::SampleCountFlagBits::e1,
             vk::AttachmentLoadOp::eClear,
             vk::AttachmentStoreOp::eStore,
             vk::AttachmentLoadOp::eDontCare,
             vk::AttachmentStoreOp::eDontCare,
             vk::ImageLayout::eUndefined,
             vk::ImageLayout::ePresentSrcKHR}
        };

        vk::RenderPassCreateInfo renderPassCreateInfo(vk::RenderPassCreateFlags(), attachmentDescriptions, subpassDescription);
        vk::raii::RenderPass renderPass(device, renderPassCreateInfo);

        //--------------------------------------------------------------------------------------
        std::vector<uint32_t> vertSPV = ReadFile("../resources/shaders/01_01_base_vert.spv");
        std::vector<uint32_t> fragSPV = ReadFile("../resources/shaders/01_01_base_frag.spv");
        vk::raii::ShaderModule vertexShaderModule(device, vk::ShaderModuleCreateInfo(vk::ShaderModuleCreateFlags(), vertSPV));
        vk::raii::ShaderModule fragmentShaderModule(device, vk::ShaderModuleCreateInfo(vk::ShaderModuleCreateFlags(), fragSPV));

        vk::raii::PipelineLayout pipelineLayout(device, {{}, {}});
        vk::raii::PipelineCache pipelineCache(device, vk::PipelineCacheCreateInfo());

        vk::raii::Pipeline graphicsPipeline = makeGraphicsPipeline(
            device,
            pipelineCache,
            vertexShaderModule,
            nullptr,
            fragmentShaderModule,
            nullptr,
            0,
            {},
            vk::FrontFace::eClockwise,
            false,
            pipelineLayout,
            renderPass
        );

        //--------------------------------------------------------------------------------------
        std::array<std::array<vk::ImageView, 1>, MaxFramesInFlight> imageViews {
            std::array<vk::ImageView, 1> {swapChainData.imageViews[0]},
            std::array<vk::ImageView, 1> {swapChainData.imageViews[1]},
            std::array<vk::ImageView, 1> {swapChainData.imageViews[2]},
        };

        auto w = swapChainData.swapchainExtent.width;
        auto h = swapChainData.swapchainExtent.height;
        std::array<vk::raii::Framebuffer, MaxFramesInFlight> framebuffers {
            vk::raii::Framebuffer(device, vk::FramebufferCreateInfo({}, renderPass, imageViews[0], w, h, 1)),
            vk::raii::Framebuffer(device, vk::FramebufferCreateInfo({}, renderPass, imageViews[1], w, h, 1)),
            vk::raii::Framebuffer(device, vk::FramebufferCreateInfo({}, renderPass, imageViews[2], w, h, 1)),
        };

        //--------------------------------------------------------------------------------------
        std::array<vk::raii::Fence, MaxFramesInFlight> drawFences {
            vk::raii::Fence(device, vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled)),
            vk::raii::Fence(device, vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled)),
            vk::raii::Fence(device, vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled)),
        };

        std::array<vk::raii::Semaphore, MaxFramesInFlight> renderFinishedSemaphore {
            vk::raii::Semaphore(device, vk::SemaphoreCreateInfo()),
            vk::raii::Semaphore(device, vk::SemaphoreCreateInfo()),
            vk::raii::Semaphore(device, vk::SemaphoreCreateInfo()),
        };

        std::array<vk::raii::Semaphore, MaxFramesInFlight> imageAcquiredSemaphores {
            vk::raii::Semaphore(device, vk::SemaphoreCreateInfo()),
            vk::raii::Semaphore(device, vk::SemaphoreCreateInfo()),
            vk::raii::Semaphore(device, vk::SemaphoreCreateInfo()),
        };

        while (!window.ShouldExit())
        {
            window.PollEvents();

            auto waitResult           = device.waitForFences({drawFences[CurrentFrameIndex]}, VK_TRUE, TimeOut);
            auto [result, imageIndex] = swapChainData.swapChain.acquireNextImage(TimeOut, imageAcquiredSemaphores[CurrentFrameIndex]);
            assert(imageIndex < swapChainData.images.size());

            if (vk::Result::eErrorOutOfDateKHR == result)
            {
                RecreateSwapChain(
                    CurrentFrameIndex,
                    window.window,
                    swapChainData,
                    framebuffers,
                    renderPass,
                    physicalDevice,
                    device,
                    surfaceData.surface,
                    window.extent,
                    vk::ImageUsageFlagBits::eColorAttachment,
                    &swapChainData.swapChain,
                    graphicsTransferPresentQueueIndex,
                    graphicsTransferPresentQueueIndex
                );
                continue;
            }
            else if (vk::Result::eSuccess != result && vk::Result::eSuboptimalKHR != result)
            {
                throw std::runtime_error("failed to acquire swap chain image");
            }

            device.resetFences({drawFences[CurrentFrameIndex]});

            auto&& cmd = commandBuffers[CurrentFrameIndex];
            cmd.reset();

            std::array<vk::ClearValue, 1> clearValues;
            clearValues[0].color = vk::ClearColorValue(0.1f, 0.2f, 0.3f, 1.f);
            vk::RenderPassBeginInfo renderPassBeginInfo(
                renderPass, framebuffers[CurrentFrameIndex], vk::Rect2D(vk::Offset2D(0, 0), swapChainData.swapchainExtent), clearValues
            );

            cmd.begin({});
            cmd.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);

            cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, graphicsPipeline);
            cmd.setViewport(
                0,
                vk::Viewport(
                    0.0f,
                    0.0f,
                    static_cast<float>(swapChainData.swapchainExtent.width),
                    static_cast<float>(swapChainData.swapchainExtent.height),
                    0.0f,
                    1.0f
                )
            );
            cmd.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swapChainData.swapchainExtent));
            cmd.draw(3, 1, 0, 0);

            cmd.endRenderPass();
            cmd.end();

            //--------------------------------------------------------------------------------------
            std::array<vk::CommandBuffer, 1> drawCommandBuffers {cmd};
            std::array<vk::Semaphore, 1> signalSemaphores {renderFinishedSemaphore[CurrentFrameIndex]};
            std::array<vk::Semaphore, 1> waitSemaphores {imageAcquiredSemaphores[CurrentFrameIndex]};
            std::array<vk::PipelineStageFlags, 1> waitStages = {vk::PipelineStageFlagBits::eColorAttachmentOutput};
            vk::SubmitInfo drawSubmitInfo(waitSemaphores, waitStages, drawCommandBuffers, signalSemaphores);
            graphicsTransferPresentQueue.submit(drawSubmitInfo, drawFences[CurrentFrameIndex]);

            //--------------------------------------------------------------------------------------
            std::array<vk::Semaphore, 1> presentWait {renderFinishedSemaphore[CurrentFrameIndex]};
            std::array<vk::SwapchainKHR, 1> swapchains {swapChainData.swapChain};
            vk::PresentInfoKHR presentInfoKHR(presentWait, swapchains, imageIndex);
            try
            {
                auto presentResult = graphicsTransferPresentQueue.presentKHR(presentInfoKHR);
            }
            catch (vk::SystemError& err)
            {
                RecreateSwapChain(
                    CurrentFrameIndex,
                    window.window,
                    swapChainData,
                    framebuffers,
                    renderPass,
                    physicalDevice,
                    device,
                    surfaceData.surface,
                    window.extent,
                    vk::ImageUsageFlagBits::eColorAttachment,
                    &swapChainData.swapChain,
                    graphicsTransferPresentQueueIndex,
                    graphicsTransferPresentQueueIndex
                );
            }

            CurrentFrameIndex = (CurrentFrameIndex + 1) % MaxFramesInFlight;
        }

        device.waitIdle();
    }

    catch (vk::SystemError& err)
    {
        std::cout << "vk::SystemError: " << err.what() << std::endl;
        exit(-1);
    }

    catch (std::exception& err)
    {
        std::cout << "std::exception: " << err.what() << std::endl;
        exit(-1);
    }

    catch (...)
    {
        std::cout << "unknown error\n";
        exit(-1);
    }

    std::cout << "Success\n";
    return 0;
}
