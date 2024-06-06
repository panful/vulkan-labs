#pragma once

#include "BufferData.h"
#include <glm/glm.hpp>
#include <vulkan/vulkan_raii.hpp>

struct Device;
class Viewer;

class Actor
{
public:
    void Update(const std::shared_ptr<Device> device, const Viewer* viewer);
    void Render(const vk::raii::CommandBuffer& cmd, const uint32_t currentFrameIndex, const glm::mat4& viewMat, const glm::mat4& projMat);

private:
    bool m_needUpdate {true};

    vk::raii::PipelineLayout m_pipelineLayout {nullptr};
    vk::raii::Pipeline m_graphicsPipeline {nullptr};

    vk::raii::DescriptorSets m_descriptorSets {nullptr};

    BufferData m_vertexBufferData {nullptr};
    BufferData m_indexBufferData {nullptr};
    std::vector<BufferData> m_uniformBufferObjects {};
};
