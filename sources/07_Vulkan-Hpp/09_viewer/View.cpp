#include "View.h"
#include "Viewer.h"

View::View()
    : m_camera(std::make_unique<Camera>())
{
}

void View::AddActor(const std::shared_ptr<Actor>& actor)
{
    m_actors.emplace_back(actor);
}

void View::Update(const std::shared_ptr<Device> device, const Viewer* viewer)
{
    for (const auto& actor : m_actors)
    {
        actor->Update(device, viewer);
    }
}

void View::Render(const vk::raii::CommandBuffer& commandBuffer, const Viewer* viewer)
{
    auto offset =
        vk::Offset2D {static_cast<int32_t>(viewer->extent.width * m_viewport[0]), static_cast<int32_t>(viewer->extent.height * m_viewport[1])};
    auto extent =
        vk::Extent2D {static_cast<uint32_t>(viewer->extent.width * m_viewport[2]), static_cast<uint32_t>(viewer->extent.height * m_viewport[3])};
    auto m_viewport = vk::Viewport {
        static_cast<float>(offset.x), static_cast<float>(offset.y), static_cast<float>(extent.width), static_cast<float>(extent.height), 0.0f, 1.0f
    };

    vk::ClearValue clearValue {m_background};
    vk::ClearAttachment attachment {vk::ImageAspectFlagBits::eColor, 0, clearValue};
    vk::ClearRect rect {
        vk::Rect2D {offset, extent},
         0, 1
    };

    commandBuffer.setViewport(0, m_viewport);
    commandBuffer.setScissor(0, vk::Rect2D(offset, extent));
    commandBuffer.clearAttachments(attachment, rect);

    auto aspect = static_cast<float>(extent.width) / static_cast<float>(extent.height);

    for (const auto& actor : m_actors)
    {
        actor->Render(commandBuffer, viewer->currentFrameIndex, m_camera->GetViewMatrix(), m_camera->GetProjectMatrix(aspect));
    }
}

void View::SetViewport(const std::array<double, 4>& viewport)
{
    m_viewport = viewport;
}

void View::SetBackground(const std::array<float, 4>& background)
{
    m_background = background;
}

const std::unique_ptr<Camera>& View::GetCamera() const noexcept
{
    return m_camera;
}
