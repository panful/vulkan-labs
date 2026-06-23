#pragma once

#include <vulkan/vulkan.h>

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

#include "lvk/application_desc.h"
#include "lvk/frame_context.h"
#include "lvk/glfw_input_adapter.h"
#include "lvk/input_state.h"

namespace lvk {
class GlfwWindow;
class SwapChain;
class VulkanContext;

class VulkanSample {
public:
  VulkanSample();
  virtual ~VulkanSample();

  VulkanSample(const VulkanSample&) = delete;
  VulkanSample& operator=(const VulkanSample&) = delete;
  VulkanSample(VulkanSample&&) noexcept = delete;
  VulkanSample& operator=(VulkanSample&&) noexcept = delete;

  [[nodiscard]] int Run();

protected:
  virtual void Configure(ApplicationDesc& desc);
  virtual void OnCreate() = 0;
  virtual void OnDestroy() noexcept;
  virtual void OnUpdate(float delta_seconds);
  virtual void OnUpdate(float delta_seconds, const InputState& input_state);
  virtual void OnRender(FrameContext& frame_context) = 0;
  virtual void OnSwapChainCleanup() noexcept;
  virtual void OnSwapChainRecreated();

  [[nodiscard]] VulkanContext& GetContext();
  [[nodiscard]] const VulkanContext& GetContext() const;
  [[nodiscard]] SwapChain& GetSwapChain();
  [[nodiscard]] const SwapChain& GetSwapChain() const;
  [[nodiscard]] GlfwWindow& GetWindow();
  [[nodiscard]] const GlfwWindow& GetWindow() const;
  void BeginDefaultRenderPass(FrameContext& frame_context, const std::array<float, 4>& clear_color) const;
  static void EndDefaultRenderPass(FrameContext& frame_context) noexcept;

private:
  struct FrameResources {
    VkCommandBuffer command_buffer{VK_NULL_HANDLE};
    VkSemaphore image_available_semaphore{VK_NULL_HANDLE};
    VkFence in_flight_fence{VK_NULL_HANDLE};
  };

private:
  void Initialize(const ApplicationDesc& desc);
  void MainLoop();
  void Cleanup() noexcept;
  void CreateCommandPool();
  void CreateFrameResources();
  void DestroyFrameResources() noexcept;
  void CreateSwapChainSemaphores();
  void DestroySwapChainSemaphores() noexcept;
  void DrawFrame();
  void RecreateSwapChain();
  void OnFramebufferResize(uint32_t width, uint32_t height) noexcept;

private:
  static constexpr uint32_t k_max_frames_in_flight{2};

private:
  std::unique_ptr<GlfwWindow> m_window{};
  GlfwInputAdapter m_input_adapter{};
  std::unique_ptr<VulkanContext> m_context{};
  std::unique_ptr<SwapChain> m_swap_chain{};
  VkCommandPool m_command_pool{VK_NULL_HANDLE};
  std::array<FrameResources, k_max_frames_in_flight> m_frame_resources{};
  std::vector<VkSemaphore> m_render_finished_semaphores{};
  uint32_t m_current_frame{};
  bool m_framebuffer_resized{false};
  bool m_is_created{false};
};
}  // namespace lvk
