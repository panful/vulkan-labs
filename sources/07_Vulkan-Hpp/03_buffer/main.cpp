#pragma warning(disable : 4996) // 解决 stb_image_write.h 文件中的`sprintf`不安全警告

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <glm/glm.hpp>

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <fstream>
#include <iostream>

static std::string AppName {"Vulkan-Hpp"};
static std::string EngineName {"Vulkan-Hpp"};
static std::vector<const char*> EnableLayerNames {"VK_LAYER_KHRONOS_validation"};
static std::vector<const char*> EnableExtensionNames {VK_EXT_DEBUG_UTILS_EXTENSION_NAME};

constexpr static uint32_t MaxFramesInFlight {3};
static uint32_t CurrentFrameIndex {0};

struct Vertex
{
    glm::vec2 pos {0.f, 0.f};
    glm::vec3 color {0.f, 0.f, 0.f};
};

// clang-format off
const std::vector<Vertex> vertices {
    {{-0.5f, -0.5f}, {1.f, 0.f, 0.f}},
    {{-0.5f,  0.5f}, {1.f, 0.f, 0.f}},
    {{ 0.5f,  0.5f}, {0.f, 1.f, 0.f}},
    {{ 0.5f, -0.5f}, {0.f, 1.f, 0.f}},
};

const std::vector<uint16_t> indices {
    0, 1, 2,
    0, 2, 3,
};

// clang-format on

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
    std::array inputBinding {
        vk::VertexInputBindingDescription {0, sizeof(Vertex), vk::VertexInputRate::eVertex}
    };
    std::array inputAttribute {
        vk::VertexInputAttributeDescription {0, 0, vk::Format::eR32G32Sfloat,    offsetof(Vertex, Vertex::pos)  },
        vk::VertexInputAttributeDescription {1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, Vertex::color)}
    };

    vk::PipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo({}, inputBinding, inputAttribute);

    vk::PipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreateInfo(
        vk::PipelineInputAssemblyStateCreateFlags(), vk::PrimitiveTopology::eTriangleList
    );

    vk::PipelineViewportStateCreateInfo pipelineViewportStateCreateInfo(vk::PipelineViewportStateCreateFlags(), 1, nullptr, 1, nullptr);

    vk::PipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo(
        vk::PipelineRasterizationStateCreateFlags(),
        false,
        false,
        vk::PolygonMode::eFill,
        vk::CullModeFlagBits::eNone,
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

template <typename T>
void copyToDevice(vk::raii::DeviceMemory const& deviceMemory, T const* pData, size_t count, vk::DeviceSize stride = sizeof(T))
{
    assert(sizeof(T) <= stride);
    uint8_t* deviceData = static_cast<uint8_t*>(deviceMemory.mapMemory(0, count * stride));
    if (stride == sizeof(T))
    {
        memcpy(deviceData, pData, count * sizeof(T));
    }
    else
    {
        for (size_t i = 0; i < count; i++)
        {
            memcpy(deviceData, &pData[i], sizeof(T));
            deviceData += stride;
        }
    }
    deviceMemory.unmapMemory();
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

int main()
{
    try
    {
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

        //--------------------------------------------------------------------------------------
        // 选择一个合适的物理设备
        vk::raii::PhysicalDevices physicalDevices(instance); // 所有物理设备，继承自 std::vector
        uint32_t usePhysicalDeviceIndex {};
        uint32_t graphicsAndTransferQueueFamilyIndex {};
        for (size_t i = 0; i < physicalDevices.size(); ++i)
        {
            std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevices[i].getQueueFamilyProperties();
            for (size_t j = 0; j < queueFamilyProperties.size(); ++j)
            {
                if ((queueFamilyProperties[j].queueFlags & vk::QueueFlagBits::eGraphics)
                    && (queueFamilyProperties[j].queueFlags & vk::QueueFlagBits::eTransfer))
                {
                    usePhysicalDeviceIndex              = static_cast<uint32_t>(i);
                    graphicsAndTransferQueueFamilyIndex = static_cast<uint32_t>(j);
                }
            }
        }
        auto physicalDevice = physicalDevices[usePhysicalDeviceIndex];

        vk::FormatProperties formatProperties = physicalDevice.getFormatProperties(vk::Format::eB8G8R8A8Unorm);
        bool supportBlit {false};

        if ((formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eBlitSrc)
            && (formatProperties.linearTilingFeatures & vk::FormatFeatureFlagBits::eBlitDst))
        {
            std::cout << "Support blit\n";
            supportBlit = true;
        }

        //--------------------------------------------------------------------------------------
        // 创建一个逻辑设备
        float queuePriority = 0.0f;
        vk::DeviceQueueCreateInfo deviceQueueCreateInfo({}, graphicsAndTransferQueueFamilyIndex, 1, &queuePriority);
        vk::DeviceCreateInfo deviceCreateInfo({}, deviceQueueCreateInfo, {}, {}, {});
        vk::raii::Device device(physicalDevice, deviceCreateInfo);

        //--------------------------------------------------------------------------------------
        vk::raii::CommandPool commandPool =
            vk::raii::CommandPool(device, {{vk::CommandPoolCreateFlagBits::eResetCommandBuffer}, graphicsAndTransferQueueFamilyIndex});
        vk::raii::CommandBuffers commandBuffers(device, {commandPool, vk::CommandBufferLevel::ePrimary, MaxFramesInFlight});
        vk::raii::CommandBuffers blitImageCommandBuffers(device, {commandPool, vk::CommandBufferLevel::ePrimary, MaxFramesInFlight});

        vk::raii::Queue graphicsAndTransferQueue(device, graphicsAndTransferQueueFamilyIndex, 0);

        //--------------------------------------------------------------------------------------
        auto colorFormat = vk::Format::eB8G8R8A8Unorm;
        auto extent      = vk::Extent2D {800, 600};

        std::array<ImageData, MaxFramesInFlight> colorImageDatas {
            ImageData(
                physicalDevice,
                device,
                colorFormat,
                extent,
                vk::ImageTiling::eOptimal,
                vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc,
                vk::ImageLayout::eUndefined,
                vk::MemoryPropertyFlagBits::eDeviceLocal,
                vk::ImageAspectFlagBits::eColor
            ),
            ImageData(
                physicalDevice,
                device,
                colorFormat,
                extent,
                vk::ImageTiling::eOptimal,
                vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc,
                vk::ImageLayout::eUndefined,
                vk::MemoryPropertyFlagBits::eDeviceLocal,
                vk::ImageAspectFlagBits::eColor
            ),
            ImageData(
                physicalDevice,
                device,
                colorFormat,
                extent,
                vk::ImageTiling::eOptimal,
                vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc,
                vk::ImageLayout::eUndefined,
                vk::MemoryPropertyFlagBits::eDeviceLocal,
                vk::ImageAspectFlagBits::eColor
            )
        };

        //--------------------------------------------------------------------------------------
        std::array<ImageData, MaxFramesInFlight> saveImageDatas {
            ImageData(
                physicalDevice,
                device,
                colorFormat,
                extent,
                vk::ImageTiling::eLinear,
                vk::ImageUsageFlagBits::eTransferDst,
                vk::ImageLayout::eUndefined,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                vk::ImageAspectFlagBits::eColor,
                false
            ),
            ImageData(
                physicalDevice,
                device,
                colorFormat,
                extent,
                vk::ImageTiling::eLinear,
                vk::ImageUsageFlagBits::eTransferDst,
                vk::ImageLayout::eUndefined,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                vk::ImageAspectFlagBits::eColor,
                false
            ),
            ImageData(
                physicalDevice,
                device,
                colorFormat,
                extent,
                vk::ImageTiling::eLinear,
                vk::ImageUsageFlagBits::eTransferDst,
                vk::ImageLayout::eUndefined,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                vk::ImageAspectFlagBits::eColor,
                false
            )
        };

        //--------------------------------------------------------------------------------------
        vk::AttachmentReference colorAttachment(0, vk::ImageLayout::eColorAttachmentOptimal);
        vk::SubpassDescription subpassDescription(vk::SubpassDescriptionFlags(), vk::PipelineBindPoint::eGraphics, {}, {colorAttachment}, {}, {}, {});
        std::vector<vk::AttachmentDescription> attachmentDescriptions {
            {{},
             colorFormat, vk::SampleCountFlagBits::e1,
             vk::AttachmentLoadOp::eClear,
             vk::AttachmentStoreOp::eStore,
             vk::AttachmentLoadOp::eDontCare,
             vk::AttachmentStoreOp::eDontCare,
             vk::ImageLayout::eUndefined,
             vk::ImageLayout::eTransferSrcOptimal}
        };

        vk::RenderPassCreateInfo renderPassCreateInfo(vk::RenderPassCreateFlags(), attachmentDescriptions, subpassDescription);
        vk::raii::RenderPass renderPass(device, renderPassCreateInfo);

        //--------------------------------------------------------------------------------------
        std::vector<uint32_t> vertSPV = ReadFile("../resources/shaders/01_02_base_vert.spv");
        std::vector<uint32_t> fragSPV = ReadFile("../resources/shaders/01_02_base_frag.spv");
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
        BufferData vertexBufferData(
            physicalDevice, device, sizeof(Vertex) * vertices.size(), vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst
        );
        vertexBufferData.upload(physicalDevice, device, commandPool, graphicsAndTransferQueue, vertices, sizeof(Vertex));

        BufferData indexBufferData(
            physicalDevice,
            device,
            sizeof(std::remove_cvref_t<decltype(indices.front())>) * indices.size(),
            vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst
        );
        indexBufferData.upload(
            physicalDevice, device, commandPool, graphicsAndTransferQueue, indices, sizeof(std::remove_cvref_t<decltype(indices.front())>)
        );

        //--------------------------------------------------------------------------------------
        std::array<std::array<vk::ImageView, 1>, MaxFramesInFlight> imageViews {
            colorImageDatas[0].imageView, colorImageDatas[1].imageView, colorImageDatas[2].imageView
        };

        std::array<vk::raii::Framebuffer, MaxFramesInFlight> framebuffers {
            vk::raii::Framebuffer(device, vk::FramebufferCreateInfo({}, renderPass, imageViews[0], extent.width, extent.height, 1)),
            vk::raii::Framebuffer(device, vk::FramebufferCreateInfo({}, renderPass, imageViews[1], extent.width, extent.height, 1)),
            vk::raii::Framebuffer(device, vk::FramebufferCreateInfo({}, renderPass, imageViews[2], extent.width, extent.height, 1)),
        };

        //--------------------------------------------------------------------------------------
        std::array<vk::raii::Fence, MaxFramesInFlight> drawFences {
            vk::raii::Fence(device, vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled)),
            vk::raii::Fence(device, vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled)),
            vk::raii::Fence(device, vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled)),
        };

        std::array<vk::raii::Fence, MaxFramesInFlight> blitFences {
            vk::raii::Fence(device, vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled)),
            vk::raii::Fence(device, vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled)),
            vk::raii::Fence(device, vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled)),
        };

        std::array<vk::raii::Semaphore, MaxFramesInFlight> renderFinishedSemaphore {
            vk::raii::Semaphore(device, vk::SemaphoreCreateInfo()),
            vk::raii::Semaphore(device, vk::SemaphoreCreateInfo()),
            vk::raii::Semaphore(device, vk::SemaphoreCreateInfo()),
        };

        static uint32_t count {0};
        while (count++ < 9)
        {
            std::cout << "Index: " << count << "\tFrame: " << CurrentFrameIndex << '\n';

            auto result =
                device.waitForFences({drawFences[CurrentFrameIndex], blitFences[CurrentFrameIndex]}, VK_TRUE, std::numeric_limits<uint64_t>::max());
            device.resetFences({drawFences[CurrentFrameIndex], blitFences[CurrentFrameIndex]});

            // 确保命令执行完之后再保存图片，每个 commandbuffer 对应的第一张图片都还没有绘制就保存，所以都是黑色，图片格式是 BGRA
            auto subResourceLayout = saveImageDatas[CurrentFrameIndex].image.getSubresourceLayout({vk::ImageAspectFlagBits::eColor, 0, 0});
            auto saveImagePixels   = reinterpret_cast<uint8_t*>(saveImageDatas[CurrentFrameIndex].deviceMemory.mapMemory(0, vk::WholeSize, {}));
            saveImagePixels += subResourceLayout.offset;
            stbi_write_jpg(("raii_" + std::to_string(count) + ".jpg").c_str(), extent.width, extent.height, 4, saveImagePixels, 100);
            saveImageDatas[CurrentFrameIndex].deviceMemory.unmapMemory();

            commandBuffers[CurrentFrameIndex].reset();
            commandBuffers[CurrentFrameIndex].begin({});

            std::array<vk::ClearValue, 1> clearValues;
            clearValues[0].color = vk::ClearColorValue(0.0f + 1.f / count, 0.2f, 0.3f, 1.f);
            vk::RenderPassBeginInfo renderPassBeginInfo(
                renderPass, framebuffers[CurrentFrameIndex], vk::Rect2D(vk::Offset2D(0, 0), extent), clearValues
            );

            commandBuffers[CurrentFrameIndex].beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);
            commandBuffers[CurrentFrameIndex].bindPipeline(vk::PipelineBindPoint::eGraphics, graphicsPipeline);

            commandBuffers[CurrentFrameIndex].setViewport(
                0, vk::Viewport(0.0f, 0.0f, static_cast<float>(extent.width), static_cast<float>(extent.height), 0.0f, 1.0f)
            );
            commandBuffers[CurrentFrameIndex].setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), extent));

            commandBuffers[CurrentFrameIndex].bindVertexBuffers(0, {vertexBufferData.buffer}, {0});
            commandBuffers[CurrentFrameIndex].bindIndexBuffer(indexBufferData.buffer, 0, vk::IndexType::eUint16);
            commandBuffers[CurrentFrameIndex].drawIndexed(static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

            commandBuffers[CurrentFrameIndex].endRenderPass();

            commandBuffers[CurrentFrameIndex].end();

            //--------------------------------------------------------------------------------------
            vk::CommandBuffer pDrawCommandBuffer[] {commandBuffers[CurrentFrameIndex]};
            vk::Semaphore semaphore[] {renderFinishedSemaphore[CurrentFrameIndex]};
            vk::SubmitInfo drawSubmitInfo(0, nullptr, nullptr, 1, pDrawCommandBuffer, 1, semaphore);
            graphicsAndTransferQueue.submit(drawSubmitInfo, *drawFences[CurrentFrameIndex]);

            //--------------------------------------------------------------------------------------
            blitImageCommandBuffers[CurrentFrameIndex].reset();
            blitImageCommandBuffers[CurrentFrameIndex].begin({});

            setImageLayout(
                blitImageCommandBuffers[CurrentFrameIndex],
                saveImageDatas[CurrentFrameIndex].image,
                colorFormat,
                vk::ImageLayout::eUndefined,
                vk::ImageLayout::eTransferDstOptimal
            );

            if (supportBlit)
            {
                vk::ImageSubresourceLayers imageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1);
                std::array<vk::Offset3D, 2> offsets {
                    vk::Offset3D(0, 0, 0), vk::Offset3D {static_cast<int32_t>(extent.width), static_cast<int32_t>(extent.height), 1}
                };
                vk::ImageBlit imageBlit(imageSubresourceLayers, offsets, imageSubresourceLayers, offsets);
                blitImageCommandBuffers[CurrentFrameIndex].blitImage(
                    colorImageDatas[CurrentFrameIndex].image,
                    vk::ImageLayout::eTransferSrcOptimal,
                    saveImageDatas[CurrentFrameIndex].image,
                    vk::ImageLayout::eTransferDstOptimal,
                    imageBlit,
                    vk::Filter::eLinear
                );
            }
            else
            {
                vk::ImageSubresourceLayers imageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1);
                vk::ImageCopy imageCopy(imageSubresourceLayers, vk::Offset3D(), imageSubresourceLayers, vk::Offset3D(), vk::Extent3D {extent, 1});
                blitImageCommandBuffers[CurrentFrameIndex].copyImage(
                    colorImageDatas[CurrentFrameIndex].image,
                    vk::ImageLayout::eTransferSrcOptimal,
                    saveImageDatas[CurrentFrameIndex].image,
                    vk::ImageLayout::eTransferDstOptimal,
                    imageCopy
                );
            }

            setImageLayout(
                blitImageCommandBuffers[CurrentFrameIndex],
                saveImageDatas[CurrentFrameIndex].image,
                colorFormat,
                vk::ImageLayout::eTransferDstOptimal,
                vk::ImageLayout::eGeneral
            );

            blitImageCommandBuffers[CurrentFrameIndex].end();

            vk::CommandBuffer pBlitCommandBuffer[] {blitImageCommandBuffers[CurrentFrameIndex]};
            vk::PipelineStageFlags waitStages[] = {vk::PipelineStageFlagBits::eColorAttachmentOutput};
            vk::SubmitInfo blitSubmitInfo(1, semaphore, waitStages, 1, pBlitCommandBuffer);
            graphicsAndTransferQueue.submit(blitSubmitInfo, blitFences[CurrentFrameIndex]);

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
