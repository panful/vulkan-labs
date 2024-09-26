#include "Context.h"
#include "RenderGraph.h"
#include "RenderPass.h"
#include "Window.h"

int main()
{
    Window window {};

    auto& renderGraph = window.GetRenderGraph();
    auto& renderPass  = renderGraph.AddPass("present");
    renderGraph.SetPresentPass("present");
    renderGraph.Compile();

    window.MainLoop();
}
