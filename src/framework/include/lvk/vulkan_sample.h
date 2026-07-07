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

/// @brief Vulkan 教学样例的基础应用骨架。
/// @details
/// - 派生类只需要实现资源创建、每帧更新和渲染录制。
/// - 基类负责窗口、VulkanContext、SwapChain、命令缓冲区、同步对象和主循环。
/// - 这是教学框架，不是完整引擎；复杂资源系统应在派生样例或后续框架层中逐步引入。
class VulkanSample {
public:
  VulkanSample();
  virtual ~VulkanSample();

  VulkanSample(const VulkanSample&) = delete;
  VulkanSample& operator=(const VulkanSample&) = delete;
  VulkanSample(VulkanSample&&) noexcept = delete;
  VulkanSample& operator=(VulkanSample&&) noexcept = delete;

  /// @brief 运行样例主流程。
  /// @return 进程退出码；初始化或运行异常时返回 `EXIT_FAILURE`。
  [[nodiscard]] int Run();

protected:
  /// @brief 派生类可覆盖该函数以修改窗口和 Vulkan 初始化配置。
  virtual void Configure(ApplicationDesc& desc);
  /// @brief 创建样例私有资源；基类资源已经初始化完成。
  virtual void OnCreate() = 0;
  /// @brief 销毁样例私有资源；函数不能抛出异常。
  virtual void OnDestroy() noexcept;
  /// @brief 无输入版本的每帧更新回调，保留给最简单样例使用。
  virtual void OnUpdate(float delta_seconds);
  /// @brief 带输入快照的每帧更新回调。
  virtual void OnUpdate(float delta_seconds, const InputState& input_state);
  /// @brief 录制当前帧渲染命令；传入的 command buffer 已经 begin。
  virtual void OnRender(FrameContext& frame_context) = 0;
  /// @brief swapchain 重建前释放依赖旧 render pass/framebuffer 的资源。
  virtual void OnSwapChainCleanup() noexcept;
  /// @brief swapchain 重建后重新创建依赖新 render pass/framebuffer 的资源。
  virtual void OnSwapChainRecreated();

  /// @brief 获取 Vulkan 上下文；只能在 `OnCreate` 之后调用。
  [[nodiscard]] VulkanContext& GetContext();
  [[nodiscard]] const VulkanContext& GetContext() const;
  /// @brief 获取 swapchain；只能在 `OnCreate` 之后调用。
  [[nodiscard]] SwapChain& GetSwapChain();
  [[nodiscard]] const SwapChain& GetSwapChain() const;
  /// @brief 获取窗口；只能在 `OnCreate` 之后调用。
  [[nodiscard]] GlfwWindow& GetWindow();
  [[nodiscard]] const GlfwWindow& GetWindow() const;
  /// @brief 开始框架默认 render pass。
  /// @details 默认 render pass 会清理 color 和 depth attachment。
  void BeginDefaultRenderPass(FrameContext& frame_context, const std::array<float, 4>& clear_color) const;
  /// @brief 结束框架默认 render pass。
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
  std::vector<VkFence> m_image_in_flight_fences{};
  uint32_t m_current_frame{};
  bool m_framebuffer_resized{false};
  bool m_is_created{false};
};
}  // namespace lvk
