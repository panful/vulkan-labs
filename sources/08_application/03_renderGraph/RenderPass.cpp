#include "RenderPass.h"
#include "Context.h"

#include <fstream>

namespace {
std::vector<char> ReadFile(const std::string& fileName)
{
    std::ifstream file(fileName, std::ios::ate | std::ios::binary);
    if (!file.is_open())
    {
        throw std::runtime_error("failed to open file: " + fileName);
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();
    return buffer;
}
} // namespace

RenderPass::~RenderPass() noexcept
{
    vkDestroyPipeline(Context::GetContext()->GetDevice(), m_pipeline, nullptr);
    vkDestroyPipelineLayout(Context::GetContext()->GetDevice(), m_pipelineLayout, nullptr);
    vkDestroyRenderPass(Context::GetContext()->GetDevice(), m_renderPass, nullptr);

    for (auto framebuffer : m_framebuffers)
    {
        vkDestroyFramebuffer(Context::GetContext()->GetDevice(), framebuffer, nullptr);
    }
}

void RenderPass::SetColorOutput(const std::vector<ImageData>& colors)
{
    m_outputColors = colors;
}

void RenderPass::SetExtent(const VkExtent2D extent)
{
    m_extent = extent;
}

void RenderPass::SetColorFormat(const VkFormat format)
{
    m_colorFormat = format;
}

void RenderPass::Compile()
{
    CreateRenderPass();
    CreateFramebuffers();
    CreateGraphicsPipeline();
}

void RenderPass::Execute(const VkCommandBuffer commandBuffer, const size_t frameIndex, const uint32_t imageIndex) const
{
    RecordCommandBuffer(commandBuffer, frameIndex, imageIndex);
}

VkRenderPass RenderPass::GetRenderPass() const noexcept
{
    return m_renderPass;
}

void RenderPass::CreateRenderPass()
{
    // 附着描述
    VkAttachmentDescription colorAttachment = {};
    colorAttachment.format                  = m_colorFormat;
    colorAttachment.samples                 = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp                  = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp                 = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp           = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp          = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout             = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    // 子流程引用的附着
    VkAttachmentReference colorAttachmentRef = {};
    colorAttachmentRef.attachment            = 0;
    colorAttachmentRef.layout                = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // 子流程
    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments    = &colorAttachmentRef;

    // 渲染流程使用的依赖信息
    VkSubpassDependency dependency = {};
    dependency.srcSubpass          = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass          = 0;
    dependency.srcStageMask        = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask       = 0;
    dependency.dstStageMask        = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask       = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType                  = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount        = 1;
    renderPassInfo.pAttachments           = &colorAttachment;
    renderPassInfo.subpassCount           = 1;
    renderPassInfo.pSubpasses             = &subpass;
    renderPassInfo.dependencyCount        = 1;
    renderPassInfo.pDependencies          = &dependency;

    if (VK_SUCCESS != vkCreateRenderPass(Context::GetContext()->GetDevice(), &renderPassInfo, nullptr, &m_renderPass))
    {
        throw std::runtime_error("failed to create render pass");
    }
}

void RenderPass::CreateFramebuffers()
{
    m_framebuffers.resize(m_outputColors.size());

    for (size_t i = 0; i < m_outputColors.size(); ++i)
    {
        std::array<VkImageView, 1> attachments {m_outputColors[i].imageView};

        VkFramebufferCreateInfo framebufferInfo = {};
        framebufferInfo.sType                   = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass              = m_renderPass;
        framebufferInfo.attachmentCount         = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments            = attachments.data();
        framebufferInfo.width                   = m_extent.width;
        framebufferInfo.height                  = m_extent.height;
        framebufferInfo.layers                  = 1;

        if (VK_SUCCESS != vkCreateFramebuffer(Context::GetContext()->GetDevice(), &framebufferInfo, nullptr, &m_framebuffers[i]))
        {
            throw std::runtime_error("failed to create framebuffer");
        }
    }
}

void RenderPass::CreateGraphicsPipeline()
{
    auto vertShaderCode = ReadFile("../resources/shaders/01_01_base_vert.spv");
    auto fragShaderCode = ReadFile("../resources/shaders/01_01_base_frag.spv");

    VkShaderModule vertShaderModule = CreateShaderModule(vertShaderCode);
    VkShaderModule fragShaderModule = CreateShaderModule(fragShaderCode);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
    vertShaderStageInfo.sType                           = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage                           = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module                          = vertShaderModule;
    vertShaderStageInfo.pName                           = "main";  // 指定调用的着色器函数，同一份代码可以实现多个着色器
    vertShaderStageInfo.pSpecializationInfo             = nullptr; // 设置着色器常量

    VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
    fragShaderStageInfo.sType                           = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage                           = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module                          = fragShaderModule;
    fragShaderStageInfo.pName                           = "main";
    fragShaderStageInfo.pSpecializationInfo             = nullptr;

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    // 顶点信息
    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
    vertexInputInfo.sType                                = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount        = 0;
    vertexInputInfo.pVertexBindingDescriptions           = nullptr; // 可选的
    vertexInputInfo.vertexAttributeDescriptionCount      = 0;
    vertexInputInfo.pVertexAttributeDescriptions         = nullptr; // 可选的

    // 拓扑信息
    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
    inputAssembly.sType                                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology                               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable                 = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType                             = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount                     = 1;
    viewportState.scissorCount                      = 1;

    // 光栅化
    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType                                  = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable                       = VK_FALSE;
    rasterizer.rasterizerDiscardEnable                = VK_FALSE;                // 设置为true会禁止一切片段输出到帧缓冲
    rasterizer.polygonMode                            = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth                              = 1.f;
    rasterizer.cullMode                               = VK_CULL_MODE_BACK_BIT;   // 表面剔除类型，正面、背面、双面剔除
    rasterizer.frontFace                              = VK_FRONT_FACE_CLOCKWISE; // 指定顺时针的顶点序是正面还是反面
    rasterizer.depthBiasEnable                        = VK_FALSE;
    rasterizer.depthBiasConstantFactor                = 1.f;
    rasterizer.depthBiasClamp                         = 0.f;
    rasterizer.depthBiasSlopeFactor                   = 0.f;

    // 多重采样
    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType                                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable                  = VK_FALSE;
    multisampling.rasterizationSamples                 = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading                     = 1.f;
    multisampling.pSampleMask                          = nullptr;
    multisampling.alphaToCoverageEnable                = VK_FALSE;
    multisampling.alphaToOneEnable                     = VK_FALSE;

    // 深度和模板测试

    // 颜色混合，可以对指定帧缓冲单独设置，也可以设置全局颜色混合方式
    VkPipelineColorBlendAttachmentState colorBlendAttachment {};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable    = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending {};
    colorBlending.sType             = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable     = VK_FALSE;
    colorBlending.logicOp           = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount   = 1;
    colorBlending.pAttachments      = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f;
    colorBlending.blendConstants[1] = 0.0f;
    colorBlending.blendConstants[2] = 0.0f;
    colorBlending.blendConstants[3] = 0.0f;

    // 动态状态，视口大小、线宽、混合常量等
    std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState {};
    dynamicState.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates    = dynamicStates.data();

    // 管线布局，在着色器中使用 uniform 变量
    VkPipelineLayoutCreateInfo pipelineLayoutInfo {};
    pipelineLayoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount         = 0;
    pipelineLayoutInfo.pushConstantRangeCount = 0;

    if (VK_SUCCESS != vkCreatePipelineLayout(Context::GetContext()->GetDevice(), &pipelineLayoutInfo, nullptr, &m_pipelineLayout))
    {
        throw std::runtime_error("failed to create pipeline layout");
    }

    // 完整的图形管线包括：着色器阶段、固定功能状态、管线布局、渲染流程
    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType                        = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount                   = 2;
    pipelineInfo.pStages                      = shaderStages;
    pipelineInfo.pVertexInputState            = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState          = &inputAssembly;
    pipelineInfo.pViewportState               = &viewportState;
    pipelineInfo.pRasterizationState          = &rasterizer;
    pipelineInfo.pMultisampleState            = &multisampling;
    pipelineInfo.pDepthStencilState           = nullptr;
    pipelineInfo.pColorBlendState             = &colorBlending;
    pipelineInfo.pDynamicState                = &dynamicState;
    pipelineInfo.layout                       = m_pipelineLayout;
    pipelineInfo.renderPass                   = m_renderPass;
    pipelineInfo.subpass                      = 0;
    pipelineInfo.basePipelineHandle           = nullptr;
    pipelineInfo.basePipelineIndex            = -1;

    if (VK_SUCCESS != vkCreateGraphicsPipelines(Context::GetContext()->GetDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline))
    {
        throw std::runtime_error("failed to create graphics pipeline");
    }

    vkDestroyShaderModule(Context::GetContext()->GetDevice(), fragShaderModule, nullptr);
    vkDestroyShaderModule(Context::GetContext()->GetDevice(), vertShaderModule, nullptr);
}

VkShaderModule RenderPass::CreateShaderModule(const std::vector<char>& code) const
{
    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType                    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize                 = code.size();
    createInfo.pCode                    = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (VK_SUCCESS != vkCreateShaderModule(Context::GetContext()->GetDevice(), &createInfo, nullptr, &shaderModule))
    {
        throw std::runtime_error("failed to create shader module");
    }

    return shaderModule;
}

void RenderPass::RecordCommandBuffer(const VkCommandBuffer commandBuffer, const size_t frameIndex, const uint32_t imageIndex) const
{
    // 清除色，相当于背景色
    std::array<VkClearValue, 2> clearValues {};
    clearValues.at(0).color = {
        {.1f, .2f, .3f, 1.f}
    };
    clearValues.at(1).depthStencil = {1.f, 0};

    VkRenderPassBeginInfo renderPassInfo = {};
    renderPassInfo.sType                 = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass            = m_renderPass;
    renderPassInfo.framebuffer           = m_framebuffers[imageIndex];
    renderPassInfo.renderArea.offset     = {0, 0};
    renderPassInfo.renderArea.extent     = m_extent;
    renderPassInfo.clearValueCount       = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues          = clearValues.data();

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

    VkViewport viewport = {};
    viewport.x          = 0.f;
    viewport.y          = 0.f;
    viewport.width      = static_cast<float>(m_extent.width);
    viewport.height     = static_cast<float>(m_extent.height);
    viewport.minDepth   = 0.f;
    viewport.maxDepth   = 1.f;

    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor = {};
    scissor.offset   = {0, 0};
    scissor.extent   = m_extent;

    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    vkCmdDraw(commandBuffer, 3, 1, 0, 0);

    vkCmdEndRenderPass(commandBuffer);
}
