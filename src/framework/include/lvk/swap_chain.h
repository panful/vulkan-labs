#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

namespace lvk {
class GlfwWindow;
class VulkanContext;

/// @brief 物理设备针对当前 surface 的 swapchain 支持信息。
/// @details formats 和 present modes 可能为空；调用方必须先检查后再创建 swapchain。
struct SwapChainSupportDetails {
  /// @brief surface 尺寸、图像数量和 transform 等能力限制。
  VkSurfaceCapabilitiesKHR capabilities{};
  /// @brief surface 支持的颜色格式与色彩空间组合。
  std::vector<VkSurfaceFormatKHR> formats{};
  /// @brief surface 支持的 present mode。
  std::vector<VkPresentModeKHR> present_modes{};
};

/// @brief 教学框架默认 swapchain 封装。
/// @details
/// - 拥有 swapchain images 对应的 image views、默认 render pass、depth attachment 和 framebuffer。
/// - 默认 render pass 固定为 color + depth，适合基础样例；更复杂的渲染流程应自定义渲染目标。
/// - 依赖的 `VulkanContext` 和 `GlfwWindow` 必须比 `SwapChain` 活得更久。
class SwapChain final {
public:
  /// @brief 创建当前窗口可用的 swapchain 和默认渲染资源。
  /// @throws std::runtime_error swapchain 支持不完整或 Vulkan 创建失败时抛出。
  SwapChain(const VulkanContext& context, const GlfwWindow& window);
  ~SwapChain() noexcept;

  SwapChain(const SwapChain&) = delete;
  SwapChain& operator=(const SwapChain&) = delete;
  SwapChain(SwapChain&&) noexcept = delete;
  SwapChain& operator=(SwapChain&&) noexcept = delete;

  /// @brief 在窗口尺寸变化或 swapchain out-of-date 后重建资源。
  /// @details 调用方应先确保 device idle 或相关资源不再被 GPU 使用。
  void Recreate();

  /// @brief 原始 Vulkan swapchain 句柄。
  [[nodiscard]] VkSwapchainKHR GetHandle() const noexcept;
  /// @brief swapchain color image 格式。
  [[nodiscard]] VkFormat GetImageFormat() const noexcept;
  /// @brief 当前 swapchain extent，单位为 framebuffer 像素。
  [[nodiscard]] VkExtent2D GetExtent() const noexcept;
  /// @brief 默认 render pass，包含 color attachment 和 depth attachment。
  [[nodiscard]] VkRenderPass GetRenderPass() const noexcept;
  /// @brief 获取指定 swapchain image 对应的 framebuffer。
  [[nodiscard]] VkFramebuffer GetFramebuffer(uint32_t image_index) const;
  /// @brief 当前 swapchain image 数量。
  [[nodiscard]] uint32_t GetImageCount() const noexcept;

private:
  void Create(VkSwapchainKHR old_swap_chain);
  void Cleanup(bool destroy_swap_chain) noexcept;
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
