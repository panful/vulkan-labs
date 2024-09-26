#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>

class Common
{
public:
    static inline constexpr uint32_t MaxFramesInFlight {2};
};

struct Drawable
{
    VkBuffer vertexBuffer {nullptr};
    VkDeviceMemory vertexBufferMemory {nullptr};
    VkBuffer indexBuffer {nullptr};
    VkDeviceMemory indexBufferMemory {nullptr};
    uint32_t indexCount {0};

    void Draw(const VkCommandBuffer commandBuffer) const noexcept
    {
        VkBuffer vertexBuffers[] = {vertexBuffer};
        VkDeviceSize offsets[]   = {0};

        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT16);
        vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);
    }
};

struct ImageData
{
    VkImage image {};
    VkImageView imageView {};
    VkDeviceMemory deviceMemory {};
};
