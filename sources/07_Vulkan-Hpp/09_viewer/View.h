#pragma once

#include "Actor.h"
#include "Camera.h"
#include <array>
#include <memory>
#include <vector>
#include <vulkan/vulkan_raii.hpp>

struct Device;
class Viewer;
class Camera;

class View
{
public:
    View();

    void Update(const std::shared_ptr<Device> device, const Viewer* viewer);

    void Render(const vk::raii::CommandBuffer& commandBuffer, const Viewer* viewer);

    void AddActor(const std::shared_ptr<Actor>& actor);

    void SetViewport(const std::array<double, 4>& viewport);

    void SetBackground(const std::array<float, 4>& background);

private:
    std::unique_ptr<Camera> m_camera {};
    std::vector<std::shared_ptr<Actor>> m_actors {};
    std::array<double, 4> m_viewport {0.1, 0.1, .8, .8}; // 起始位置和宽高
    std::array<float, 4> m_background {.1f, .2f, .3f, 1.f};
};
