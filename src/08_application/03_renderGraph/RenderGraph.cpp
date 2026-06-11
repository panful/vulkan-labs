
#include "RenderGraph.h"
#include "RenderPass.h"
#include "Window.h"

RenderGraph::RenderGraph(Window* window)
    : m_window(window)
{
}

RenderPass& RenderGraph::AddPass(const std::string& name)
{
    m_renderPasses.try_emplace(name, RenderPass {});
    return m_renderPasses.at(name);
}

void RenderGraph::SetPresentPass(const std::string& name)
{
    m_renderPasses.at(name).SetColorOutput(m_window->GetColors());
    m_renderPasses.at(name).SetColorFormat(m_window->GetFormat());
    m_renderPasses.at(name).SetExtent(m_window->GetExtent());
}

void RenderGraph::Compile()
{
    for (auto& [_, renderPass] : m_renderPasses)
    {
        renderPass.Compile();
    }
}

void RenderGraph::Execute(const VkCommandBuffer cmd, const size_t frameIndex, const uint32_t imageIndex)
{
    for (const auto& [_, renderPass] : m_renderPasses)
    {
        renderPass.Execute(cmd, frameIndex, imageIndex);
    }
}

RenderGraph::~RenderGraph() noexcept
{
}

const RenderPass& RenderGraph::GetRenderPass(const std::string& name) const noexcept
{
    return m_renderPasses.at(name);
}
