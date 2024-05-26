#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <GLFW/glfw3.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

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

template <typename Func>
void oneTimeSubmit(vk::raii::Device const& device, vk::raii::CommandPool const& commandPool, vk::raii::Queue const& queue, Func const& func)
{
    vk::raii::CommandBuffer commandBuffer = std::move(vk::raii::CommandBuffers(device, {*commandPool, vk::CommandBufferLevel::ePrimary, 1}).front());
    commandBuffer.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
    func(commandBuffer);
    commandBuffer.end();
    vk::SubmitInfo submitInfo(nullptr, nullptr, *commandBuffer);
    queue.submit(submitInfo, nullptr);
    queue.waitIdle();
}

void setImageLayout(
    vk::raii::CommandBuffer const& commandBuffer, vk::Image image, vk::Format format, vk::ImageLayout oldImageLayout, vk::ImageLayout newImageLayout
)
{
    vk::AccessFlags sourceAccessMask;
    switch (oldImageLayout)
    {
        case vk::ImageLayout::eTransferDstOptimal:
            sourceAccessMask = vk::AccessFlagBits::eTransferWrite;
            break;
        case vk::ImageLayout::ePreinitialized:
            sourceAccessMask = vk::AccessFlagBits::eHostWrite;
            break;
        case vk::ImageLayout::eGeneral: // sourceAccessMask is empty
        case vk::ImageLayout::eUndefined:
            break;
        default:
            assert(false);
            break;
    }

    vk::PipelineStageFlags sourceStage;
    switch (oldImageLayout)
    {
        case vk::ImageLayout::eGeneral:
        case vk::ImageLayout::ePreinitialized:
            sourceStage = vk::PipelineStageFlagBits::eHost;
            break;
        case vk::ImageLayout::eTransferDstOptimal:
            sourceStage = vk::PipelineStageFlagBits::eTransfer;
            break;
        case vk::ImageLayout::eUndefined:
            sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
            break;
        default:
            assert(false);
            break;
    }

    vk::AccessFlags destinationAccessMask;
    switch (newImageLayout)
    {
        case vk::ImageLayout::eColorAttachmentOptimal:
            destinationAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
            break;
        case vk::ImageLayout::eDepthStencilAttachmentOptimal:
            destinationAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite;
            break;
        case vk::ImageLayout::eGeneral: // empty destinationAccessMask
        case vk::ImageLayout::ePresentSrcKHR:
            break;
        case vk::ImageLayout::eShaderReadOnlyOptimal:
            destinationAccessMask = vk::AccessFlagBits::eShaderRead;
            break;
        case vk::ImageLayout::eTransferSrcOptimal:
            destinationAccessMask = vk::AccessFlagBits::eTransferRead;
            break;
        case vk::ImageLayout::eTransferDstOptimal:
            destinationAccessMask = vk::AccessFlagBits::eTransferWrite;
            break;
        default:
            assert(false);
            break;
    }

    vk::PipelineStageFlags destinationStage;
    switch (newImageLayout)
    {
        case vk::ImageLayout::eColorAttachmentOptimal:
            destinationStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
            break;
        case vk::ImageLayout::eDepthStencilAttachmentOptimal:
            destinationStage = vk::PipelineStageFlagBits::eEarlyFragmentTests;
            break;
        case vk::ImageLayout::eGeneral:
            destinationStage = vk::PipelineStageFlagBits::eHost;
            break;
        case vk::ImageLayout::ePresentSrcKHR:
            destinationStage = vk::PipelineStageFlagBits::eBottomOfPipe;
            break;
        case vk::ImageLayout::eShaderReadOnlyOptimal:
            destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
            break;
        case vk::ImageLayout::eTransferDstOptimal:
        case vk::ImageLayout::eTransferSrcOptimal:
            destinationStage = vk::PipelineStageFlagBits::eTransfer;
            break;
        default:
            assert(false);
            break;
    }

    vk::ImageAspectFlags aspectMask;
    if (newImageLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal)
    {
        aspectMask = vk::ImageAspectFlagBits::eDepth;
        if (format == vk::Format::eD32SfloatS8Uint || format == vk::Format::eD24UnormS8Uint)
        {
            aspectMask |= vk::ImageAspectFlagBits::eStencil;
        }
    }
    else
    {
        aspectMask = vk::ImageAspectFlagBits::eColor;
    }

    vk::ImageSubresourceRange imageSubresourceRange(aspectMask, 0, 1, 0, 1);
    vk::ImageMemoryBarrier imageMemoryBarrier(
        sourceAccessMask,
        destinationAccessMask,
        oldImageLayout,
        newImageLayout,
        VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED,
        image,
        imageSubresourceRange
    );
    return commandBuffer.pipelineBarrier(sourceStage, destinationStage, {}, nullptr, nullptr, imageMemoryBarrier);
}

uint32_t findMemoryType(vk::PhysicalDeviceMemoryProperties const& memoryProperties, uint32_t typeBits, vk::MemoryPropertyFlags requirementsMask)
{
    uint32_t typeIndex = uint32_t(~0);
    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++)
    {
        if ((typeBits & 1) && ((memoryProperties.memoryTypes[i].propertyFlags & requirementsMask) == requirementsMask))
        {
            typeIndex = i;
            break;
        }
        typeBits >>= 1;
    }
    assert(typeIndex != uint32_t(~0));
    return typeIndex;
}

vk::raii::DeviceMemory allocateDeviceMemory(
    vk::raii::Device const& device,
    vk::PhysicalDeviceMemoryProperties const& memoryProperties,
    vk::MemoryRequirements const& memoryRequirements,
    vk::MemoryPropertyFlags memoryPropertyFlags
)
{
    uint32_t memoryTypeIndex = findMemoryType(memoryProperties, memoryRequirements.memoryTypeBits, memoryPropertyFlags);
    vk::MemoryAllocateInfo memoryAllocateInfo(memoryRequirements.size, memoryTypeIndex);
    return vk::raii::DeviceMemory(device, memoryAllocateInfo);
}

struct BufferData
{
    BufferData(
        vk::raii::PhysicalDevice const& physicalDevice,
        vk::raii::Device const& device,
        vk::DeviceSize size,
        vk::BufferUsageFlags usage,
        vk::MemoryPropertyFlags propertyFlags = vk::MemoryPropertyFlagBits::eDeviceLocal
    )
        : buffer(device, vk::BufferCreateInfo({}, size, usage))
        , m_size(size)
        , m_usage(usage)
        , m_propertyFlags(propertyFlags)
    {
        deviceMemory = allocateDeviceMemory(device, physicalDevice.getMemoryProperties(), buffer.getMemoryRequirements(), propertyFlags);
        buffer.bindMemory(deviceMemory, 0);
    }

    BufferData(std::nullptr_t)
    {
    }

    /// @brief 将数据拷贝到暂存缓冲
    /// @tparam DataType
    /// @param data
    template <typename DataType>
    void upload(DataType const& data) const
    {
        assert((m_propertyFlags & vk::MemoryPropertyFlagBits::eHostCoherent) && (m_propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible));
        assert(sizeof(DataType) <= m_size);

        void* dataPtr = deviceMemory.mapMemory(0, sizeof(DataType));
        memcpy(dataPtr, &data, sizeof(DataType));
        deviceMemory.unmapMemory();
    }

    template <typename DataType>
    void upload(std::vector<DataType> const& data, size_t stride = 0) const
    {
        assert(m_propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible);

        size_t elementSize = stride ? stride : sizeof(DataType);
        assert(sizeof(DataType) <= elementSize);

        copyToDevice(deviceMemory, data.data(), data.size(), elementSize);
    }

    /// @brief 将数据从CPU先拷贝到暂存缓冲，再拷贝到GPU专用缓冲
    /// @tparam DataType
    /// @param physicalDevice
    /// @param device
    /// @param commandPool
    /// @param queue
    /// @param data
    /// @param stride
    template <typename DataType>
    void upload(
        vk::raii::PhysicalDevice const& physicalDevice,
        vk::raii::Device const& device,
        vk::raii::CommandPool const& commandPool,
        vk::raii::Queue const& queue,
        std::vector<DataType> const& data,
        size_t stride
    ) const
    {
        assert(m_usage & vk::BufferUsageFlagBits::eTransferDst);
        assert(m_propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal);

        size_t elementSize = stride ? stride : sizeof(DataType);
        assert(sizeof(DataType) <= elementSize);

        size_t dataSize = data.size() * elementSize;
        assert(dataSize <= m_size);

        BufferData stagingBuffer(
            physicalDevice,
            device,
            dataSize,
            vk::BufferUsageFlagBits::eTransferSrc,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
        );
        copyToDevice(stagingBuffer.deviceMemory, data.data(), data.size(), elementSize);

        oneTimeSubmit(device, commandPool, queue, [&](vk::raii::CommandBuffer const& commandBuffer) {
            commandBuffer.copyBuffer(*stagingBuffer.buffer, *this->buffer, vk::BufferCopy(0, 0, dataSize));
        });
    }

    vk::raii::DeviceMemory deviceMemory = nullptr;
    vk::raii::Buffer buffer             = nullptr;

private:
    vk::DeviceSize m_size;
    vk::BufferUsageFlags m_usage;
    vk::MemoryPropertyFlags m_propertyFlags;
};

struct ImageData
{
    ImageData(
        vk::raii::PhysicalDevice const& physicalDevice,
        vk::raii::Device const& device,
        vk::Format format_,
        vk::Extent2D const& extent,
        vk::ImageTiling tiling,
        vk::ImageUsageFlags usage,
        vk::ImageLayout initialLayout,
        vk::MemoryPropertyFlags memoryProperties,
        vk::ImageAspectFlags aspectMask,
        bool createImageView = true
    )
        : format(format_)
        , image(
              device,
              {vk::ImageCreateFlags(),
               vk::ImageType::e2D,
               format,
               vk::Extent3D(extent, 1),
               1,
               1,
               vk::SampleCountFlagBits::e1,
               tiling,
               usage | vk::ImageUsageFlagBits::eSampled,
               vk::SharingMode::eExclusive,
               {},
               initialLayout}
          )
    {
        deviceMemory = allocateDeviceMemory(device, physicalDevice.getMemoryProperties(), image.getMemoryRequirements(), memoryProperties);
        image.bindMemory(deviceMemory, 0);
        imageView = createImageView
            ? vk::raii::ImageView(device, vk::ImageViewCreateInfo({}, image, vk::ImageViewType::e2D, format, {}, {aspectMask, 0, 1, 0, 1}))
            : nullptr;
    }

    ImageData(std::nullptr_t)
    {
    }

    // the DeviceMemory should be destroyed before the Image it is bound to; to get that order with the standard destructor
    // of the ImageData, the order of DeviceMemory and Image here matters
    vk::Format format;
    vk::raii::DeviceMemory deviceMemory = nullptr;
    vk::raii::Image image               = nullptr;
    vk::raii::ImageView imageView       = nullptr;
};

struct TextureData
{
    TextureData(
        vk::raii::PhysicalDevice const& physicalDevice,
        vk::raii::Device const& device,
        vk::Extent2D const& extent_               = {256, 256},
        vk::ImageUsageFlags usageFlags            = {},
        vk::FormatFeatureFlags formatFeatureFlags = {},
        bool anisotropyEnable                     = false,
        bool forceStaging                         = false
    )
        : format(vk::Format::eR8G8B8A8Unorm)
        , extent(extent_)
        , sampler(
              device,
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
        vk::FormatProperties formatProperties = physicalDevice.getFormatProperties(format);

        formatFeatureFlags |= vk::FormatFeatureFlagBits::eSampledImage;
        needsStaging = forceStaging || ((formatProperties.linearTilingFeatures & formatFeatureFlags) != formatFeatureFlags);
        vk::ImageTiling imageTiling;
        vk::ImageLayout initialLayout;
        vk::MemoryPropertyFlags requirements;
        if (needsStaging)
        {
            assert((formatProperties.optimalTilingFeatures & formatFeatureFlags) == formatFeatureFlags);
            stagingBufferData = BufferData(physicalDevice, device, extent.width * extent.height * 4, vk::BufferUsageFlagBits::eTransferSrc);
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
            physicalDevice,
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

    template <typename ImageGenerator>
    void setImage(vk::raii::CommandBuffer const& commandBuffer, ImageGenerator const& imageGenerator)
    {
        void* data = needsStaging ? stagingBufferData.deviceMemory.mapMemory(0, stagingBufferData.buffer.getMemoryRequirements().size)
                                  : imageData.deviceMemory.mapMemory(0, imageData.image.getMemoryRequirements().size);
        imageGenerator(data, extent);
        needsStaging ? stagingBufferData.deviceMemory.unmapMemory() : imageData.deviceMemory.unmapMemory();

        if (needsStaging)
        {
            // Since we're going to blit to the texture image, set its layout to eTransferDstOptimal
            setImageLayout(commandBuffer, imageData.image, imageData.format, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
            vk::BufferImageCopy copyRegion(
                0,
                extent.width,
                extent.height,
                vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1),
                vk::Offset3D(0, 0, 0),
                vk::Extent3D(extent, 1)
            );
            commandBuffer.copyBufferToImage(stagingBufferData.buffer, imageData.image, vk::ImageLayout::eTransferDstOptimal, copyRegion);
            // Set the layout for the texture image from eTransferDstOptimal to eShaderReadOnlyOptimal
            setImageLayout(
                commandBuffer, imageData.image, imageData.format, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal
            );
        }
        else
        {
            // If we can use the linear tiled image as a texture, just do it
            setImageLayout(
                commandBuffer, imageData.image, imageData.format, vk::ImageLayout::ePreinitialized, vk::ImageLayout::eShaderReadOnlyOptimal
            );
        }
    }

    vk::Format format;
    vk::Extent2D extent;
    bool needsStaging;
    BufferData stagingBufferData = nullptr;
    ImageData imageData          = nullptr;
    vk::raii::Sampler sampler;
};

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
        std::vector<uint32_t> vertSPV = ReadFile("../resources/shaders/07_08_quad_vert.spv");
        std::vector<uint32_t> fragSPV = ReadFile("../resources/shaders/07_08_quad_frag.spv");
        vk::raii::ShaderModule vertexShaderModule(device, vk::ShaderModuleCreateInfo(vk::ShaderModuleCreateFlags(), vertSPV));
        vk::raii::ShaderModule fragmentShaderModule(device, vk::ShaderModuleCreateInfo(vk::ShaderModuleCreateFlags(), fragSPV));

        std::array descriptorSetLayoutBindings {
            vk::DescriptorSetLayoutBinding {0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment}
        };
        vk::DescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo({}, descriptorSetLayoutBindings);
        vk::raii::DescriptorSetLayout descriptorSetLayout(device, descriptorSetLayoutCreateInfo);

        std::array<vk::DescriptorSetLayout, 1> descriptorSetLayoursForPipeline {descriptorSetLayout};
        vk::raii::PipelineLayout pipelineLayout(device, {{}, descriptorSetLayoursForPipeline});
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
        int texWidth {0}, texHeight {0}, texChannels {0};
        auto pixels = stbi_load("../resources/textures/nightsky.png", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
        assert(pixels);

        TextureData textureData(physicalDevice, device, {static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight)});
        oneTimeSubmit(device, commandPool, graphicsTransferPresentQueue, [&textureData, pixels](vk::raii::CommandBuffer const& commandBuffer) {
            textureData.setImage(commandBuffer, [pixels](void* data, vk::Extent2D extent) {
                std::memcpy(data, pixels, static_cast<size_t>(extent.width * extent.height * 4));
            });
        });
        stbi_image_free(pixels);

        //--------------------------------------------------------------------------------------
        std::array descriptorPoolSizes {
            vk::DescriptorPoolSize {vk::DescriptorType::eUniformBuffer,        MaxFramesInFlight},
            vk::DescriptorPoolSize {vk::DescriptorType::eCombinedImageSampler, MaxFramesInFlight},
        };

        vk::raii::DescriptorPool descriptorPool(
            device, {vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, MaxFramesInFlight, descriptorPoolSizes}
        );

        std::array<vk::DescriptorSetLayout, MaxFramesInFlight> descriptorSetLayouts {
            *descriptorSetLayout, *descriptorSetLayout, *descriptorSetLayout
        };
        vk::raii::DescriptorSets descriptorSets(device, {descriptorPool, descriptorSetLayouts});

        vk::DescriptorImageInfo descriptorImageInfo(textureData.sampler, textureData.imageData.imageView, vk::ImageLayout::eShaderReadOnlyOptimal);

        std::array<std::array<vk::WriteDescriptorSet, 1>, MaxFramesInFlight> writeDescriptorSets {
            std::array {vk::WriteDescriptorSet {descriptorSets[0], 0, 0, vk::DescriptorType::eCombinedImageSampler, descriptorImageInfo, nullptr}},
            std::array {vk::WriteDescriptorSet {descriptorSets[1], 0, 0, vk::DescriptorType::eCombinedImageSampler, descriptorImageInfo, nullptr}},
            std::array {vk::WriteDescriptorSet {descriptorSets[2], 0, 0, vk::DescriptorType::eCombinedImageSampler, descriptorImageInfo, nullptr}}
        };
        device.updateDescriptorSets(writeDescriptorSets[0], nullptr);
        device.updateDescriptorSets(writeDescriptorSets[1], nullptr);
        device.updateDescriptorSets(writeDescriptorSets[2], nullptr);

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
            cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 0, {descriptorSets[CurrentFrameIndex]}, nullptr);
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
