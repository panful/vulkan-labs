#pragma once

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

struct GLFWwindow;

struct SurfaceData
{
    SurfaceData(vk::raii::Instance const& instance, GLFWwindow* window, vk::Extent2D const& extent_);

    SurfaceData(std::nullptr_t);

    vk::Extent2D extent;
    vk::raii::SurfaceKHR surface = nullptr;
};
