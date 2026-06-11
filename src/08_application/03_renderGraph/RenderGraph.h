#pragma once

#include <map>
#include <string>

#include "RenderPass.h"

class Window;

class RenderGraph
{
public:
    explicit RenderGraph(Window* window);

    ~RenderGraph() noexcept;

    RenderPass& AddPass(const std::string& name);

    void Compile();

    void Execute(const VkCommandBuffer cmd, const size_t frameIndex, const uint32_t imageIndex);

    const RenderPass& GetRenderPass(const std::string& name) const noexcept;

    void SetPresentPass(const std::string& name);

private:
    Window* m_window {};
    std::map<std::string, RenderPass> m_renderPasses {};
};
