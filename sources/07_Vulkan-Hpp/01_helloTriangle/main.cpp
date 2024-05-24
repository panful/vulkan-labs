#pragma warning(disable : 4996) // 解决 stb_image_write.h 文件中的`sprintf`不安全警告

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <fstream>
#include <iostream>

static std::string AppName {"Vulkan-Hpp"};
static std::string EngineName {"Vulkan-Hpp"};
static std::vector<const char*> EnableLayerNames {"VK_LAYER_KHRONOS_validation"};
static std::vector<const char*> EnableExtensionNames {VK_EXT_DEBUG_UTILS_EXTENSION_NAME};

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

int main()
{
    try
    {
        //--------------------------------------------------------------------------------------
        // 初始化 VkInstance
        vk::DebugUtilsMessageSeverityFlagsEXT severityFlags {
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning
            | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError
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
        uint32_t graphicsQueueFamilyIndex {};
        for (size_t i = 0; i < physicalDevices.size(); ++i)
        {
            std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevices[i].getQueueFamilyProperties();
            for (size_t j = 0; j < queueFamilyProperties.size(); ++j)
            {
                if (queueFamilyProperties[j].queueFlags & vk::QueueFlagBits::eGraphics)
                {
                    usePhysicalDeviceIndex   = static_cast<uint32_t>(i);
                    graphicsQueueFamilyIndex = static_cast<uint32_t>(j);
                }
            }
        }
        auto physicalDevice = physicalDevices[usePhysicalDeviceIndex];

        //--------------------------------------------------------------------------------------
        // 创建一个逻辑设备
        float queuePriority = 0.0f;
        vk::DeviceQueueCreateInfo deviceQueueCreateInfo({}, graphicsQueueFamilyIndex, 1, &queuePriority);
        vk::DeviceCreateInfo deviceCreateInfo({}, deviceQueueCreateInfo, {}, {}, {});
        vk::raii::Device device(physicalDevice, deviceCreateInfo);

        //--------------------------------------------------------------------------------------
        vk::raii::CommandPool commandPool = vk::raii::CommandPool(device, {{}, graphicsQueueFamilyIndex});
        vk::CommandBufferAllocateInfo commandBufferAllocateInfo(commandPool, vk::CommandBufferLevel::ePrimary, 1);
        vk::raii::CommandBuffers commandBuffers(device, commandBufferAllocateInfo);
        auto commandBuffer = std::move(commandBuffers[0]);

        vk::raii::Queue graphicsQueue(device, graphicsQueueFamilyIndex, 0);

        //--------------------------------------------------------------------------------------
        auto colorFormat = vk::Format::eR8G8B8A8Unorm;
        auto extent      = vk::Extent3D(800, 600, 1);
        vk::ImageCreateInfo imageCreateInfo(
            {},
            vk::ImageType::e2D,
            colorFormat,
            extent,
            1,
            1,
            vk::SampleCountFlagBits::e1,
            vk::ImageTiling::eOptimal,
            vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc
        );
        vk::raii::Image colorImage(device, imageCreateInfo);
        vk::MemoryRequirements memoryRequirements = colorImage.getMemoryRequirements();
        uint32_t memoryTypeIndex =
            findMemoryType(physicalDevice.getMemoryProperties(), memoryRequirements.memoryTypeBits, {vk::MemoryPropertyFlagBits::eDeviceLocal});
        vk::MemoryAllocateInfo memoryAllocateInfo(memoryRequirements.size, memoryTypeIndex);
        vk::raii::DeviceMemory colorImageMemory = vk::raii::DeviceMemory(device, memoryAllocateInfo);
        colorImage.bindMemory(colorImageMemory, 0);

        vk::ImageViewCreateInfo imageViewCreateInfo(
            {}, colorImage, vk::ImageViewType::e2D, colorFormat, {}, {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}
        );
        vk::raii::ImageView colorImageView(device, imageViewCreateInfo);

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
             vk::ImageLayout::eTransferSrcOptimal}
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
        std::array<vk::ImageView, 1> attachments;
        attachments[0] = colorImageView;
        vk::FramebufferCreateInfo framebufferCreateInfo(vk::FramebufferCreateFlags(), renderPass, attachments, extent.width, extent.height, 1);
        vk::raii::Framebuffer framebuffer {vk::raii::Framebuffer(device, framebufferCreateInfo)};

        //--------------------------------------------------------------------------------------
        // 绘制三角形（顶点、颜色数据由着色器设置）
        commandBuffer.begin({});

        std::array<vk::ClearValue, 1> clearValues;
        clearValues[0].color = vk::ClearColorValue(0.3f, 0.2f, 0.3f, 1.f);
        vk::RenderPassBeginInfo renderPassBeginInfo(
            renderPass, framebuffer, vk::Rect2D(vk::Offset2D(0, 0), {extent.width, extent.height}), clearValues
        );

        commandBuffer.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, graphicsPipeline);

        commandBuffer.setViewport(0, vk::Viewport(0.0f, 0.0f, static_cast<float>(extent.width), static_cast<float>(extent.height), 0.0f, 1.0f));
        commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), {extent.width, extent.height}));

        commandBuffer.draw(3, 1, 0, 0);
        commandBuffer.endRenderPass();

        commandBuffer.end();

        //--------------------------------------------------------------------------------------
        vk::raii::Fence drawFence(device, vk::FenceCreateInfo());
        vk::SubmitInfo submitInfo({}, {}, *commandBuffer);
        graphicsQueue.submit(submitInfo, *drawFence);
        auto waitFence = device.waitForFences({drawFence}, VK_TRUE, std::numeric_limits<uint64_t>::max());

        //--------------------------------------------------------------------------------------
        vk::ImageCreateInfo imageCreateInfo2(
            {},
            vk::ImageType::e2D,
            colorFormat,
            extent,
            1,
            1,
            vk::SampleCountFlagBits::e1,
            vk::ImageTiling::eLinear,
            vk::ImageUsageFlagBits::eTransferDst
        );
        vk::raii::Image colorImage2(device, imageCreateInfo2);
        vk::MemoryRequirements memoryRequirements2 = colorImage2.getMemoryRequirements();

        uint32_t memoryTypeIndex2 = findMemoryType(
            physicalDevice.getMemoryProperties(),
            memoryRequirements2.memoryTypeBits,
            {vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent}
        );

        vk::MemoryAllocateInfo memoryAllocateInfo2(memoryRequirements2.size, memoryTypeIndex2);
        vk::raii::DeviceMemory colorImageMemory2 = vk::raii::DeviceMemory(device, memoryAllocateInfo2);
        colorImage2.bindMemory(colorImageMemory2, 0);

        //--------------------------------------------------------------------------------------
        commandPool.reset();
        commandBuffer.begin({});

        setImageLayout(commandBuffer, colorImage2, colorFormat, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
        vk::ImageSubresourceLayers imageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1);
        vk::ImageCopy imageCopy(imageSubresourceLayers, vk::Offset3D(), imageSubresourceLayers, vk::Offset3D(), extent);
        commandBuffer.copyImage(colorImage, vk::ImageLayout::eTransferSrcOptimal, colorImage2, vk::ImageLayout::eTransferDstOptimal, imageCopy);
        setImageLayout(commandBuffer, colorImage2, colorFormat, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eGeneral);

        commandBuffer.end();

        //--------------------------------------------------------------------------------------
        vk::raii::Fence drawFence2(device, vk::FenceCreateInfo());
        auto submitInfo2 = vk::SubmitInfo({}, {}, *commandBuffer);
        graphicsQueue.submit(submitInfo2, drawFence2);
        waitFence = device.waitForFences({drawFence2}, true, std::numeric_limits<uint64_t>::max());

        //--------------------------------------------------------------------------------------
        auto subResourceLayout = colorImage2.getSubresourceLayout({vk::ImageAspectFlagBits::eColor, 0, 0});
        auto saveImagePixels   = reinterpret_cast<uint8_t*>(colorImageMemory2.mapMemory(0, vk::WholeSize, {}));
        saveImagePixels += subResourceLayout.offset;
        stbi_write_jpg("test_raii.jpg", extent.width, extent.height, 4, saveImagePixels, 100);
        colorImageMemory2.unmapMemory();
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
