#pragma once

#include <vulkan/vulkan.h>

namespace lvk {
class GlfwWindow;
class SwapChain;
class VulkanContext;

struct ImGuiLayerDesc {
  GlfwWindow* window{nullptr};
  VulkanContext* context{nullptr};
  SwapChain* swap_chain{nullptr};
};

class ImGuiLayer final {
public:
  ImGuiLayer() = default;
  ~ImGuiLayer() noexcept;

  ImGuiLayer(const ImGuiLayer&) = delete;
  ImGuiLayer& operator=(const ImGuiLayer&) = delete;
  ImGuiLayer(ImGuiLayer&&) noexcept = delete;
  ImGuiLayer& operator=(ImGuiLayer&&) noexcept = delete;

  void Initialize(const ImGuiLayerDesc& desc);
  void Shutdown() noexcept;
  void BeginFrame();
  void Render(VkCommandBuffer command_buffer);
  [[nodiscard]] bool WantsMouse() const;
  [[nodiscard]] bool WantsKeyboard() const;

private:
  void CreateDescriptorPool();
  void UploadFonts();
  void RegisterCallbacks();

private:
  GlfwWindow* m_window{nullptr};
  VulkanContext* m_context{nullptr};
  SwapChain* m_swap_chain{nullptr};
  VkDescriptorPool m_descriptor_pool{VK_NULL_HANDLE};
  VkCommandPool m_upload_command_pool{VK_NULL_HANDLE};
  bool m_is_initialized{false};
  bool m_callbacks_registered{false};
};
}  // namespace lvk
