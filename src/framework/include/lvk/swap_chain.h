#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

namespace lvk {
class GlfwWindow;
class VulkanContext;

struct SwapChainSupportDetails {
  VkSurfaceCapabilitiesKHR capabilities{};
  std::vector<VkSurfaceFormatKHR> formats{};
  std::vector<VkPresentModeKHR> present_modes{};
};

class SwapChain final {
public:
  SwapChain(const VulkanContext& context, const GlfwWindow& window);
  ~SwapChain() noexcept;

  SwapChain(const SwapChain&) = delete;
  SwapChain& operator=(const SwapChain&) = delete;
  SwapChain(SwapChain&&) noexcept = delete;
  SwapChain& operator=(SwapChain&&) noexcept = delete;

  void Recreate();

  [[nodiscard]] VkSwapchainKHR GetHandle() const noexcept;
  [[nodiscard]] VkFormat GetImageFormat() const noexcept;
  [[nodiscard]] VkExtent2D GetExtent() const noexcept;
  [[nodiscard]] VkRenderPass GetRenderPass() const noexcept;
  [[nodiscard]] VkFramebuffer GetFramebuffer(uint32_t image_index) const;
  [[nodiscard]] uint32_t GetImageCount() const noexcept;

private:
  void Create();
  void Cleanup() noexcept;
  void CreateImageViews();
  void CreateDepthResources();
  void CreateRenderPass();
  void CreateFramebuffers();
  [[nodiscard]] SwapChainSupportDetails QuerySupport() const;
  [[nodiscard]] VkFormat FindDepthFormat() const;
  [[nodiscard]] VkFormat FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling,
                                             VkFormatFeatureFlags features) const;
  [[nodiscard]] VkSurfaceFormatKHR ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) const noexcept;
  [[nodiscard]] VkPresentModeKHR ChoosePresentMode(const std::vector<VkPresentModeKHR>& present_modes) const noexcept;
  [[nodiscard]] VkExtent2D ChooseExtent(const VkSurfaceCapabilitiesKHR& capabilities) const noexcept;

private:
  const VulkanContext& m_context;
  const GlfwWindow& m_window;
  VkSwapchainKHR m_swap_chain{VK_NULL_HANDLE};
  std::vector<VkImage> m_images{};
  std::vector<VkImageView> m_image_views{};
  std::vector<VkFramebuffer> m_framebuffers{};
  VkFormat m_image_format{};
  VkExtent2D m_extent{};
  VkRenderPass m_render_pass{VK_NULL_HANDLE};
  VkImage m_depth_image{VK_NULL_HANDLE};
  VkDeviceMemory m_depth_image_memory{VK_NULL_HANDLE};
  VkImageView m_depth_image_view{VK_NULL_HANDLE};
  VkFormat m_depth_format{};
};
}  // namespace lvk
