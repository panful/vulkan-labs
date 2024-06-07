#include "Actor.h"
#include "Device.h"
#include "Utils.h"
#include "Viewer.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace {

struct Vertex
{
    glm::vec3 pos {0.f, 0.f, 0.f};
    glm::vec3 color {0.f, 0.f, 0.f};
};

struct UniformBufferObject
{
    glm::mat4 model {glm::mat4(1.f)};
    glm::mat4 view {glm::mat4(1.f)};
    glm::mat4 proj {glm::mat4(1.f)};
};

// clang-format off
const std::vector<Vertex> vertices  {
    {{-0.5f,  0.5f, -0.5f}, {1.f, 0.f, 0.f}},
    {{ 0.5f,  0.5f, -0.5f}, {1.f, 1.f, 0.f}},
    {{ 0.5f, -0.5f, -0.5f}, {0.f, 1.f, 0.f}},
    {{-0.5f, -0.5f, -0.5f}, {0.f, 1.f, 1.f}},

    {{-0.5f,  0.5f,  0.5f}, {0.f, 0.f, 1.f}},
    {{ 0.5f,  0.5f,  0.5f}, {1.f, 0.f, 1.f}},
    {{ 0.5f, -0.5f,  0.5f}, {0.f, 0.f, 0.f}},
    {{-0.5f, -0.5f,  0.5f}, {1.f, 1.f, 1.f}},
};

const std::vector<uint16_t> indices{
    0, 1, 2, 0, 2, 3, // 前
    1, 5, 6, 1, 6, 2, // 右
    5, 4, 7, 5, 7, 6, // 后
    4, 0, 3, 4, 3, 7, // 左
    3, 2, 6, 3, 6, 7, // 上
    4, 5, 1, 4, 1, 0, // 下
};

// clang-format on

vk::raii::Pipeline makeGraphicsPipelineForViewer(
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
    vk::raii::PipelineLayout const& m_pipelineLayout,
    vk::raii::RenderPass const& renderPass
)
{
    std::array<vk::PipelineShaderStageCreateInfo, 2> pipelineShaderStageCreateInfos = {
        vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eVertex, vertexShaderModule, "main", vertexShaderSpecializationInfo),
        vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eFragment, fragmentShaderModule, "main", fragmentShaderSpecializationInfo)
    };

    std::array inputBinding {
        vk::VertexInputBindingDescription {0, sizeof(Vertex), vk::VertexInputRate::eVertex}
    };
    std::array inputAttribute {
        vk::VertexInputAttributeDescription {0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, Vertex::pos)  },
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
        m_pipelineLayout,
        renderPass
    );

    return vk::raii::Pipeline(device, pipelineCache, graphicsPipelineCreateInfo);
}

} // namespace

void Actor::Update(const std::shared_ptr<Device> device, const Viewer* viewer)
{
    if (!m_needUpdate)
    {
        return;
    }
    m_needUpdate = false;

    std::vector<uint32_t> vertSPV = Utils::ReadSPVShader("../resources/shaders/07_09_base_vert.spv");
    std::vector<uint32_t> fragSPV = Utils::ReadSPVShader("../resources/shaders/07_09_base_frag.spv");
    vk::raii::ShaderModule vertexShaderModule(device->device, vk::ShaderModuleCreateInfo(vk::ShaderModuleCreateFlags(), vertSPV));
    vk::raii::ShaderModule fragmentShaderModule(device->device, vk::ShaderModuleCreateInfo(vk::ShaderModuleCreateFlags(), fragSPV));

    std::array descriptorSetLayoutBindings {
        vk::DescriptorSetLayoutBinding {0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex}
    };
    vk::DescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo({}, descriptorSetLayoutBindings);
    vk::raii::DescriptorSetLayout descriptorSetLayout(device->device, descriptorSetLayoutCreateInfo);

    std::array<vk::DescriptorSetLayout, 1> descriptorSetLayoursForPipeline {descriptorSetLayout};
    m_pipelineLayout = vk::raii::PipelineLayout(device->device, vk::PipelineLayoutCreateInfo {{}, descriptorSetLayoursForPipeline});
    vk::raii::PipelineCache pipelineCache(device->device, vk::PipelineCacheCreateInfo());

    m_graphicsPipeline = makeGraphicsPipelineForViewer(
        device->device,
        pipelineCache,
        vertexShaderModule,
        nullptr,
        fragmentShaderModule,
        nullptr,
        0,
        {},
        vk::FrontFace::eClockwise,
        true,
        m_pipelineLayout,
        viewer->renderPass
    );
    //--------------------------------------------------------------------------------------
    m_vertexBufferData =
        BufferData(device, sizeof(Vertex) * vertices.size(), vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst);
    m_vertexBufferData.upload(device, vertices, sizeof(Vertex));

    m_indexBufferData = BufferData(
        device,
        sizeof(std::remove_cvref_t<decltype(indices.front())>) * indices.size(),
        vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst
    );
    m_indexBufferData.upload(device, indices, sizeof(std::remove_cvref_t<decltype(indices.front())>));

    UniformBufferObject defaultUBO {};
    m_uniformBufferObjects.reserve(viewer->numberOfFrames);
    for (uint32_t i = 0; i < viewer->numberOfFrames; ++i)
    {
        m_uniformBufferObjects.emplace_back(BufferData(
            device,
            sizeof(UniformBufferObject),
            vk::BufferUsageFlagBits::eUniformBuffer,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
        ));
        Utils::CopyToDevice(m_uniformBufferObjects[i].deviceMemory, defaultUBO);
    }

    std::vector<vk::DescriptorSetLayout> descriptorSetLayouts(3, descriptorSetLayout);

    m_descriptorSets = vk::raii::DescriptorSets(device->device, vk::DescriptorSetAllocateInfo {viewer->descriptorPool, descriptorSetLayouts});

    for (uint32_t i = 0; i < viewer->numberOfFrames; ++i)
    {
        vk::DescriptorBufferInfo descriptorBufferInfo0(m_uniformBufferObjects[i].buffer, 0, sizeof(UniformBufferObject));
        std::array<vk::WriteDescriptorSet, 1> writeDescriptorSets {
            vk::WriteDescriptorSet {m_descriptorSets[i], 0, 0, vk::DescriptorType::eUniformBuffer, nullptr, descriptorBufferInfo0}
        };
        device->device.updateDescriptorSets(writeDescriptorSets, nullptr);
    }
}

void Actor::Render(const vk::raii::CommandBuffer& cmd, const uint32_t currentFrameIndex, const glm::mat4& viewMat, const glm::mat4& projMat)
{
    UniformBufferObject defaultUBO {};
    defaultUBO.model = glm::mat4(1.f);
    defaultUBO.view  = viewMat;
    defaultUBO.proj  = projMat;
    Utils::CopyToDevice(m_uniformBufferObjects[currentFrameIndex].deviceMemory, defaultUBO);

    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_graphicsPipeline);

    cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelineLayout, 0, {m_descriptorSets[currentFrameIndex]}, nullptr);
    cmd.bindVertexBuffers(0, {m_vertexBufferData.buffer}, {0});
    cmd.bindIndexBuffer(m_indexBufferData.buffer, 0, vk::IndexType::eUint16);
    cmd.drawIndexed(static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);
}
