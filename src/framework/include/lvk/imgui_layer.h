#pragma once

#include <vulkan/vulkan.h>

#include <array>
#include <string_view>

#include "lvk/glfw_window.h"

namespace lvk {
class SwapChain;
class VulkanContext;

/// @brief 绘制一段无需格式化的 ImGui 文本。
/// @details 避免样例直接调用 `ImGui::Text` 这类 C 风格可变参数函数。
void DrawImGuiText(std::string_view text);

/// @brief 初始化 ImGui 层所需的非拥有依赖。
/// @details 这些指针必须在 `ImGuiLayer::Shutdown` 前保持有效。
struct ImGuiLayerDesc {
  /// @brief 用于接收 GLFW 输入事件的窗口。
  GlfwWindow* window{nullptr};
  /// @brief 提供 Vulkan instance/device/queue 的上下文。
  VulkanContext* context{nullptr};
  /// @brief 提供 render pass、image count 等初始化信息的 swapchain。
  SwapChain* swap_chain{nullptr};
};

/// @brief Dear ImGui 的 GLFW + Vulkan 后端封装。
/// @details
/// - 该类拥有 ImGui context、descriptor pool 和一次性字体上传 command pool。
/// - swapchain 重建时需要先 `Shutdown`，新 render pass 创建后再 `Initialize`。
/// - 输入回调由本类手动注册到 `GlfwWindow`，关闭时会解除订阅。
class ImGuiLayer final {
public:
  ImGuiLayer() = default;
  ~ImGuiLayer() noexcept;

  ImGuiLayer(const ImGuiLayer&) = delete;
  ImGuiLayer& operator=(const ImGuiLayer&) = delete;
  ImGuiLayer(ImGuiLayer&&) noexcept = delete;
  ImGuiLayer& operator=(ImGuiLayer&&) noexcept = delete;

  /// @brief 初始化 ImGui context 和 Vulkan 后端。
  /// @throws std::invalid_argument 依赖为空时抛出。
  /// @throws std::runtime_error Vulkan/ImGui 初始化失败时抛出。
  void Initialize(const ImGuiLayerDesc& desc);
  /// @brief 释放 ImGui 后端和本层拥有的 Vulkan 资源。
  void Shutdown() noexcept;
  /// @brief 开始一帧 ImGui 录制。
  void BeginFrame();
  /// @brief 将 ImGui draw data 录入当前 render pass 内的命令缓冲区。
  void Render(VkCommandBuffer command_buffer);
  /// @brief ImGui 当前是否希望捕获鼠标输入。
  [[nodiscard]] bool WantsMouse() const;
  /// @brief ImGui 当前是否希望捕获键盘输入。
  [[nodiscard]] bool WantsKeyboard() const;

private:
  void CreateDescriptorPool();
  void UploadFonts();
  void RegisterCallbacks();
  void UnregisterCallbacks() noexcept;

private:
  GlfwWindow* m_window{nullptr};
  VulkanContext* m_context{nullptr};
  SwapChain* m_swap_chain{nullptr};
  std::array<GlfwWindow::CallbackHandle, 5> m_callback_handles{};
  VkDescriptorPool m_descriptor_pool{VK_NULL_HANDLE};
  VkCommandPool m_upload_command_pool{VK_NULL_HANDLE};
  bool m_is_initialized{false};
};
}  // namespace lvk
