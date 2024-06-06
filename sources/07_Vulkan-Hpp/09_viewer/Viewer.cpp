#include "Viewer.h"
#include "Device.h"
#include "ImageData.h"
#include "Utils.h"
#include <fstream>
#include <iostream>

Viewer::Viewer(const vk::Extent2D& extent)
    : Viewer(std::make_shared<Device>(), 3, false, extent)
{
}

Viewer::Viewer(const std::shared_ptr<Device>& device, const uint32_t numberOfFrames_, const bool useWindow, const vk::Extent2D& extent_)
    : m_device(device)
    , m_useSwapChain(useWindow)
    , numberOfFrames(numberOfFrames_)
    , extent(extent_)
{
    if (!m_useSwapChain)
    {
        auto formatProperties = m_device->physicalDevice.getFormatProperties(m_colorFormat);
        if ((formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eBlitSrc)
            && (formatProperties.linearTilingFeatures & vk::FormatFeatureFlagBits::eBlitDst))
        {
            m_supportBlit = true;
            std::cout << "Support blit\n";
        }
        else
        {
            m_supportBlit = false;
            std::cout << "does not support blit\n";
        }
    }

    m_colorImageDatas.reserve(numberOfFrames);
    if (!m_useSwapChain)
    {
        m_saveImageDatas.reserve(numberOfFrames);
    }
    for (uint32_t i = 0; i < numberOfFrames; ++i)
    {
        m_colorImageDatas.emplace_back(ImageData(
            m_device,
            m_colorFormat,
            extent,
            vk::ImageTiling::eOptimal,
            vk::ImageUsageFlagBits::eColorAttachment | (m_useSwapChain ? vk::ImageUsageFlagBits::eSampled : vk::ImageUsageFlagBits::eTransferSrc),
            vk::ImageLayout::eUndefined,
            vk::MemoryPropertyFlagBits::eDeviceLocal,
            vk::ImageAspectFlagBits::eColor
        ));

        if (!m_useSwapChain)
        {
            m_saveImageDatas.emplace_back(ImageData(
                m_device,
                m_colorFormat,
                extent,
                vk::ImageTiling::eLinear,
                vk::ImageUsageFlagBits::eTransferDst,
                vk::ImageLayout::eUndefined,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                vk::ImageAspectFlagBits::eColor,
                false
            ));
        }
    }

    m_depthImagedata = DepthBufferData(m_device, m_depthFormat, extent);

    //--------------------------------------------------------------------------------------
    vk::AttachmentReference colorAttachment(0, vk::ImageLayout::eColorAttachmentOptimal);
    vk::AttachmentReference depthAttachment(1, vk::ImageLayout::eDepthStencilAttachmentOptimal);
    std::array colorAttachments {colorAttachment};
    vk::SubpassDescription subpassDescription(
        vk::SubpassDescriptionFlags(), vk::PipelineBindPoint::eGraphics, {}, colorAttachments, {}, &depthAttachment, {}
    );
    std::array attachmentDescriptions {
        vk::AttachmentDescription {
                                   {},
                                   m_colorFormat, vk::SampleCountFlagBits::e1,
                                   vk::AttachmentLoadOp::eClear,
                                   vk::AttachmentStoreOp::eStore,
                                   vk::AttachmentLoadOp::eDontCare,
                                   vk::AttachmentStoreOp::eDontCare,
                                   vk::ImageLayout::eUndefined,
                                   m_useSwapChain ? vk::ImageLayout::eShaderReadOnlyOptimal : vk::ImageLayout::eTransferSrcOptimal
        },
        vk::AttachmentDescription {
                                   {},
                                   m_depthFormat, vk::SampleCountFlagBits::e1,
                                   vk::AttachmentLoadOp::eClear,
                                   vk::AttachmentStoreOp::eDontCare,
                                   vk::AttachmentLoadOp::eDontCare,
                                   vk::AttachmentStoreOp::eDontCare,
                                   vk::ImageLayout::eUndefined,
                                   vk::ImageLayout::eDepthStencilAttachmentOptimal
        }
    };

    vk::RenderPassCreateInfo renderPassCreateInfo(vk::RenderPassCreateFlags(), attachmentDescriptions, subpassDescription);
    renderPass = vk::raii::RenderPass(m_device->device, renderPassCreateInfo);

    std::array descriptorPoolSizes {
        vk::DescriptorPoolSize {vk::DescriptorType::eUniformBuffer, numberOfFrames}
    };

    descriptorPool = vk::raii::DescriptorPool(
        m_device->device, vk::DescriptorPoolCreateInfo {vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, numberOfFrames, descriptorPoolSizes}
    );

    //--------------------------------------------------------------------------------------
    m_framebuffers.reserve(numberOfFrames);
    for (uint32_t i = 0; i < numberOfFrames; ++i)
    {
        std::array<vk::ImageView, 2> imageViews {m_colorImageDatas[i].imageView, m_depthImagedata.imageView};

        m_framebuffers.emplace_back(
            vk::raii::Framebuffer(m_device->device, vk::FramebufferCreateInfo({}, renderPass, imageViews, extent.width, extent.height, 1))
        );
    }

    //--------------------------------------------------------------------------------------
    m_drawFences.reserve(numberOfFrames);
    if (!m_useSwapChain)
    {
        m_blitFences.reserve(numberOfFrames);
    }
    m_renderFinishedSemaphores.reserve(numberOfFrames);
    for (uint32_t i = 0; i < numberOfFrames; ++i)
    {
        m_drawFences.emplace_back(vk::raii::Fence(m_device->device, vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled)));
        if (!m_useSwapChain)
        {
            m_blitFences.emplace_back(vk::raii::Fence(m_device->device, vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled)));
        }
        m_renderFinishedSemaphores.emplace_back(vk::raii::Semaphore(m_device->device, vk::SemaphoreCreateInfo()));
    }

    if (!m_useSwapChain)
    {
        m_commandBuffers = vk::raii::CommandBuffers(m_device->device, {m_device->commandPoolReset, vk::CommandBufferLevel::ePrimary, numberOfFrames});
        m_blitImageCommandBuffers =
            vk::raii::CommandBuffers(m_device->device, {m_device->commandPoolReset, vk::CommandBufferLevel::ePrimary, numberOfFrames});
    }
}

std::vector<vk::ImageView> Viewer::GetImageViews()
{
    return {m_colorImageDatas[0].imageView, m_colorImageDatas[1].imageView, m_colorImageDatas[2].imageView};
}

Viewer::~Viewer()
{
    m_device->device.waitIdle();
}

void Viewer::ResizeFramebuffer(const vk::Extent2D& extent_)
{
    std::cout << "resize frame buffer: " << extent.width << ' ' << extent.height << '\n';

    if (extent == extent_)
    {
        return;
    }

    currentFrameIndex = 0;

    extent = extent_;
    m_device->device.waitIdle();

    m_colorImageDatas.clear();
    m_saveImageDatas.clear();
    for (uint32_t i = 0; i < numberOfFrames; ++i)
    {
        m_colorImageDatas.emplace_back(ImageData(
            m_device,
            m_colorFormat,
            extent,
            vk::ImageTiling::eOptimal,
            vk::ImageUsageFlagBits::eColorAttachment | (m_useSwapChain ? vk::ImageUsageFlagBits::eSampled : vk::ImageUsageFlagBits::eTransferSrc),
            vk::ImageLayout::eUndefined,
            vk::MemoryPropertyFlagBits::eDeviceLocal,
            vk::ImageAspectFlagBits::eColor
        ));

        m_depthImagedata = DepthBufferData(m_device, m_depthFormat, extent);

        if (!m_useSwapChain)
        {
            m_saveImageDatas.emplace_back(ImageData(
                m_device,
                m_colorFormat,
                extent,
                vk::ImageTiling::eLinear,
                vk::ImageUsageFlagBits::eTransferDst,
                vk::ImageLayout::eUndefined,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                vk::ImageAspectFlagBits::eColor,
                false
            ));
        }
    }

    m_framebuffers.clear();
    for (uint32_t i = 0; i < numberOfFrames; ++i)
    {
        std::array<vk::ImageView, 2> imageViews {m_colorImageDatas[i].imageView, m_depthImagedata.imageView};

        m_framebuffers.emplace_back(
            vk::raii::Framebuffer(m_device->device, vk::FramebufferCreateInfo({}, renderPass, imageViews, extent.width, extent.height, 1))
        );
    }
}

void Viewer::AddView(const std::shared_ptr<View>& view)
{
    m_views.emplace_back(view);
}

void Viewer::Record(const vk::raii::CommandBuffer& commandBuffer)
{
    std::array<vk::ClearValue, 2> clearValues;
    clearValues[0].color        = vk::ClearColorValue(0.1f, 0.2f, 0.3f, 1.f);
    clearValues[1].depthStencil = vk::ClearDepthStencilValue(1.f, 0);
    vk::RenderPassBeginInfo renderPassBeginInfo(renderPass, m_framebuffers[currentFrameIndex], vk::Rect2D({0, 0}, extent), clearValues);
    commandBuffer.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);

    for (const auto& view : m_views)
    {
        view->Update(m_device, this);
        view->Render(commandBuffer, this);
    }

    commandBuffer.endRenderPass();

    currentFrameIndex = (currentFrameIndex + 1) % numberOfFrames;
}

void Viewer::Render()
{
    static uint32_t count {0};
    count++;
    std::cout << "Index: " << count << "\tFrame: " << currentFrameIndex << '\n';

    auto result = m_device->device.waitForFences(
        {m_drawFences[currentFrameIndex], m_blitFences[currentFrameIndex]}, VK_TRUE, std::numeric_limits<uint64_t>::max()
    );
    m_device->device.resetFences({m_drawFences[currentFrameIndex], m_blitFences[currentFrameIndex]});

    // 确保命令执行完之后再保存图片，每个 commandbuffer 对应的第一张图片都还没有绘制就保存，所以都是黑色，图片格式是 BGRA
    auto subResourceLayout = m_saveImageDatas[currentFrameIndex].image.getSubresourceLayout({vk::ImageAspectFlagBits::eColor, 0, 0});
    auto saveImagePixels   = reinterpret_cast<uint8_t*>(m_saveImageDatas[currentFrameIndex].deviceMemory.mapMemory(0, vk::WholeSize, {}));
    saveImagePixels += subResourceLayout.offset;

    auto w = extent.width;
    auto h = extent.height;
    std::ofstream ppmFile(("raii_" + std::to_string(count) + ".pgm").c_str(), std::ios::out | std::ios::binary);
    ppmFile << "P6\n" << w << "\n" << h << "\n" << 255 << "\n";

    for (uint32_t y = 0; y < h; y++)
    {
        auto row = reinterpret_cast<uint32_t*>(saveImagePixels);
        for (uint32_t x = 0; x < w; x++)
        {
            ppmFile.write((char*)row + 2, 1);
            ppmFile.write((char*)row + 1, 1);
            ppmFile.write((char*)row + 0, 1);
            row++;
        }
        saveImagePixels += subResourceLayout.rowPitch;
    }
    ppmFile.close();

    m_saveImageDatas[currentFrameIndex].deviceMemory.unmapMemory();

    //--------------------------------------------------------------------------------------
    auto&& cmd = m_commandBuffers[currentFrameIndex];
    cmd.reset();
    cmd.begin({});

    std::array<vk::ClearValue, 2> clearValues;
    clearValues[0].color        = vk::ClearColorValue(0.1f, 0.2f, 0.3f, 1.f);
    clearValues[1].depthStencil = vk::ClearDepthStencilValue(1.f, 0);
    vk::RenderPassBeginInfo renderPassBeginInfo(renderPass, m_framebuffers[currentFrameIndex], vk::Rect2D({0, 0}, extent), clearValues);
    cmd.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);

    for (const auto& view : m_views)
    {
        view->Update(m_device, this);
        view->Render(cmd, this);
    }

    cmd.endRenderPass();
    cmd.end();

    std::array<vk::CommandBuffer, 1> drawCommandBuffers {cmd};
    std::array<vk::Semaphore, 1> semaphores {m_renderFinishedSemaphores[currentFrameIndex]};
    vk::SubmitInfo drawSubmitInfo({}, {}, drawCommandBuffers, semaphores);
    m_device->graphicsQueue.submit(drawSubmitInfo, m_drawFences[currentFrameIndex]);

    //--------------------------------------------------------------------------------------
    auto&& blitCmd = m_blitImageCommandBuffers[currentFrameIndex];
    blitCmd.reset();
    blitCmd.begin({});

    ImageData::setImageLayout(
        blitCmd, m_saveImageDatas[currentFrameIndex].image, m_colorFormat, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal
    );

    if (m_supportBlit)
    {
        vk::ImageSubresourceLayers imageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1);
        std::array<vk::Offset3D, 2> offsets {
            vk::Offset3D(0, 0, 0), vk::Offset3D {static_cast<int32_t>(extent.width), static_cast<int32_t>(extent.height), 1}
        };
        vk::ImageBlit imageBlit(imageSubresourceLayers, offsets, imageSubresourceLayers, offsets);
        blitCmd.blitImage(
            m_colorImageDatas[currentFrameIndex].image,
            vk::ImageLayout::eTransferSrcOptimal,
            m_saveImageDatas[currentFrameIndex].image,
            vk::ImageLayout::eTransferDstOptimal,
            imageBlit,
            vk::Filter::eLinear
        );
    }
    else
    {
        vk::ImageSubresourceLayers imageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1);
        vk::ImageCopy imageCopy(imageSubresourceLayers, vk::Offset3D(), imageSubresourceLayers, vk::Offset3D(), vk::Extent3D {extent, 1});
        blitCmd.copyImage(
            m_colorImageDatas[currentFrameIndex].image,
            vk::ImageLayout::eTransferSrcOptimal,
            m_saveImageDatas[currentFrameIndex].image,
            vk::ImageLayout::eTransferDstOptimal,
            imageCopy
        );
    }

    ImageData::setImageLayout(
        blitCmd, m_saveImageDatas[currentFrameIndex].image, m_colorFormat, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eGeneral
    );

    blitCmd.end();

    std::array<vk::CommandBuffer, 1> blitCommandBuffers {blitCmd};
    std::array<vk::PipelineStageFlags, 1> waitStages = {vk::PipelineStageFlagBits::eColorAttachmentOutput};
    vk::SubmitInfo blitSubmitInfo(semaphores, waitStages, blitCommandBuffers, {});
    m_device->graphicsQueue.submit(blitSubmitInfo, m_blitFences[currentFrameIndex]);

    currentFrameIndex = (currentFrameIndex + 1) % numberOfFrames;
}
