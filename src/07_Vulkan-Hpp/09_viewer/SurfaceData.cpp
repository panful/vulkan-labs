#include "SurfaceData.h"
#include <GLFW/glfw3.h>

SurfaceData::SurfaceData(vk::raii::Instance const& instance, GLFWwindow* window, vk::Extent2D const& extent_)
    : extent(extent_)
{
    VkSurfaceKHR _surface;
    VkResult err = glfwCreateWindowSurface(static_cast<VkInstance>(*instance), window, nullptr, &_surface);
    surface      = vk::raii::SurfaceKHR(instance, _surface);
}

SurfaceData::SurfaceData(std::nullptr_t)
{
}
