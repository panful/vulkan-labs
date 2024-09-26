#pragma once

#include <vulkan/vulkan.h>

#include "Common.h"

#include <array>
#include <stdexcept>
#include <vector>

class RenderPass
{
public:
    ~RenderPass() noexcept;

    void SetColorOutput(const std::vector<ImageData>& colors);

    void SetExtent(const VkExtent2D extent);

    void SetColorFormat(const VkFormat format);

    void Compile();

    void Execute(const VkCommandBuffer commandBuffer, const size_t frameIndex, const uint32_t imageIndex) const;

    VkRenderPass GetRenderPass() const noexcept;

    void CreateRenderPass();

    void CreateFramebuffers();

    void CreateGraphicsPipeline();

    VkShaderModule CreateShaderModule(const std::vector<char>& code) const;

    void RecordCommandBuffer(const VkCommandBuffer commandBuffer, const size_t frameIndex, const uint32_t imageIndex) const;

private:
    VkExtent2D m_extent {};

    VkPipeline m_pipeline {};
    VkPipelineLayout m_pipelineLayout {};

    VkRenderPass m_renderPass {};
    std::vector<VkFramebuffer> m_framebuffers {};

    std::vector<ImageData> m_outputColors {};
    VkFormat m_colorFormat {};
};
