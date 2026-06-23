#include "lvk/vulkan_sample.h"

#include <array>
#include <chrono>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <limits>
#include <stdexcept>

#include "lvk/glfw_window.h"
#include "lvk/swap_chain.h"
#include "lvk/vk_utils.h"
#include "lvk/vulkan_context.h"

namespace lvk {
VulkanSample::VulkanSample() = default;

VulkanSample::~VulkanSample() = default;

int VulkanSample::Run() {
  try {
    ApplicationDesc desc{};
    Configure(desc);
    Initialize(desc);
    OnCreate();
    m_is_created = true;
    MainLoop();
    Cleanup();
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    Cleanup();
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

void VulkanSample::Configure([[maybe_unused]] ApplicationDesc& desc) {}

void VulkanSample::OnDestroy() noexcept {}

void VulkanSample::OnUpdate([[maybe_unused]] float delta_seconds) {}

void VulkanSample::OnUpdate(float delta_seconds, [[maybe_unused]] const InputState& input_state) {
  OnUpdate(delta_seconds);
}

void VulkanSample::OnSwapChainCleanup() noexcept {}

void VulkanSample::OnSwapChainRecreated() {}

VulkanContext& VulkanSample::GetContext() {
  if (!m_context) {
    throw std::runtime_error("Vulkan context is not initialized");
  }
  return *m_context;
}

const VulkanContext& VulkanSample::GetContext() const {
  if (!m_context) {
    throw std::runtime_error("Vulkan context is not initialized");
  }
  return *m_context;
}

SwapChain& VulkanSample::GetSwapChain() {
  if (!m_swap_chain) {
    throw std::runtime_error("swap chain is not initialized");
  }
  return *m_swap_chain;
}

const SwapChain& VulkanSample::GetSwapChain() const {
  if (!m_swap_chain) {
    throw std::runtime_error("swap chain is not initialized");
  }
  return *m_swap_chain;
}

GlfwWindow& VulkanSample::GetWindow() {
  if (!m_window) {
    throw std::runtime_error("window is not initialized");
  }
  return *m_window;
}

const GlfwWindow& VulkanSample::GetWindow() const {
  if (!m_window) {
    throw std::runtime_error("window is not initialized");
  }
  return *m_window;
}

void VulkanSample::BeginDefaultRenderPass(FrameContext& frame_context, const std::array<float, 4>& clear_color) const {
  VkClearValue clear_value{};
  clear_value.color.float32[0] = clear_color.at(0);
  clear_value.color.float32[1] = clear_color.at(1);
  clear_value.color.float32[2] = clear_color.at(2);
  clear_value.color.float32[3] = clear_color.at(3);

  VkClearValue depth_clear_value{};
  depth_clear_value.depthStencil = {1.0F, 0};
  const std::array<VkClearValue, 2> clear_values{clear_value, depth_clear_value};

  VkRenderPassBeginInfo render_pass_info{};
  render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  render_pass_info.renderPass = GetSwapChain().GetRenderPass();
  render_pass_info.framebuffer = GetSwapChain().GetFramebuffer(frame_context.image_index);
  render_pass_info.renderArea.offset = {0, 0};
  render_pass_info.renderArea.extent = GetSwapChain().GetExtent();
  render_pass_info.clearValueCount = static_cast<uint32_t>(clear_values.size());
  render_pass_info.pClearValues = clear_values.data();

  vkCmdBeginRenderPass(frame_context.command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
}

void VulkanSample::EndDefaultRenderPass(FrameContext& frame_context) noexcept {
  vkCmdEndRenderPass(frame_context.command_buffer);
}

void VulkanSample::Initialize(const ApplicationDesc& desc) {
  m_window = std::make_unique<GlfwWindow>(desc.width, desc.height, desc.title);
  m_window->SetResizeCallback([this](uint32_t width, uint32_t height) { OnFramebufferResize(width, height); });
  m_window->AddScrollCallback(
    [this](double x_offset, double y_offset) { m_input_adapter.AddScroll(x_offset, y_offset); });

  m_context = std::make_unique<VulkanContext>(desc, *m_window);
  m_swap_chain = std::make_unique<SwapChain>(*m_context, *m_window);
  CreateCommandPool();
  CreateFrameResources();
  CreateSwapChainSemaphores();
}

void VulkanSample::MainLoop() {
  auto previous_time{std::chrono::steady_clock::now()};
  while (!m_window->ShouldClose()) {
    m_window->PollEvents();

    const auto current_time{std::chrono::steady_clock::now()};
    const std::chrono::duration<float> delta_time{current_time - previous_time};
    previous_time = current_time;

    m_input_adapter.BeginFrame(m_window->GetNativeWindow());
    OnUpdate(delta_time.count(), m_input_adapter.GetInputState());
    DrawFrame();
  }

  vkDeviceWaitIdle(GetContext().GetDevice());
}

void VulkanSample::Cleanup() noexcept {
  if (m_context) {
    vkDeviceWaitIdle(m_context->GetDevice());
  }

  if (m_is_created) {
    OnDestroy();
    m_is_created = false;
  }

  DestroyFrameResources();
  DestroySwapChainSemaphores();

  if (m_context && VK_NULL_HANDLE != m_command_pool) {
    vkDestroyCommandPool(m_context->GetDevice(), m_command_pool, nullptr);
    m_command_pool = VK_NULL_HANDLE;
  }

  m_swap_chain.reset();
  m_context.reset();
  m_window.reset();
}

void VulkanSample::CreateCommandPool() {
  VkCommandPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  pool_info.queueFamilyIndex = GetContext().GetGraphicsQueueFamily();

  CheckVkResult(vkCreateCommandPool(GetContext().GetDevice(), &pool_info, nullptr, &m_command_pool),
                "failed to create command pool");
}

void VulkanSample::CreateFrameResources() {
  std::array<VkCommandBuffer, k_max_frames_in_flight> command_buffers{};

  VkCommandBufferAllocateInfo allocate_info{};
  allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocate_info.commandPool = m_command_pool;
  allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocate_info.commandBufferCount = static_cast<uint32_t>(command_buffers.size());

  CheckVkResult(vkAllocateCommandBuffers(GetContext().GetDevice(), &allocate_info, command_buffers.data()),
                "failed to allocate command buffers");

  VkSemaphoreCreateInfo semaphore_info{};
  semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  VkFenceCreateInfo fence_info{};
  fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  for (size_t i{}; i < m_frame_resources.size(); ++i) {
    FrameResources& resources{m_frame_resources.at(i)};
    resources.command_buffer = command_buffers.at(i);
    CheckVkResult(
      vkCreateSemaphore(GetContext().GetDevice(), &semaphore_info, nullptr, &resources.image_available_semaphore),
      "failed to create image available semaphore");
    CheckVkResult(vkCreateFence(GetContext().GetDevice(), &fence_info, nullptr, &resources.in_flight_fence),
                  "failed to create in-flight fence");
  }
}

void VulkanSample::DestroyFrameResources() noexcept {
  if (!m_context) {
    return;
  }

  VkDevice device{m_context->GetDevice()};
  for (FrameResources& resources : m_frame_resources) {
    if (VK_NULL_HANDLE != resources.in_flight_fence) {
      vkDestroyFence(device, resources.in_flight_fence, nullptr);
      resources.in_flight_fence = VK_NULL_HANDLE;
    }

    if (VK_NULL_HANDLE != resources.image_available_semaphore) {
      vkDestroySemaphore(device, resources.image_available_semaphore, nullptr);
      resources.image_available_semaphore = VK_NULL_HANDLE;
    }
  }
}

void VulkanSample::CreateSwapChainSemaphores() {
  VkSemaphoreCreateInfo semaphore_info{};
  semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  m_render_finished_semaphores.resize(GetSwapChain().GetImageCount(), VK_NULL_HANDLE);
  for (VkSemaphore& semaphore : m_render_finished_semaphores) {
    CheckVkResult(vkCreateSemaphore(GetContext().GetDevice(), &semaphore_info, nullptr, &semaphore),
                  "failed to create render finished semaphore");
  }
}

void VulkanSample::DestroySwapChainSemaphores() noexcept {
  if (!m_context) {
    return;
  }

  VkDevice device{m_context->GetDevice()};
  for (VkSemaphore semaphore : m_render_finished_semaphores) {
    if (VK_NULL_HANDLE != semaphore) {
      vkDestroySemaphore(device, semaphore, nullptr);
    }
  }
  m_render_finished_semaphores.clear();
}

void VulkanSample::DrawFrame() {
  FrameResources& resources{m_frame_resources.at(m_current_frame)};
  VkDevice device{GetContext().GetDevice()};

  CheckVkResult(vkWaitForFences(device, 1, &resources.in_flight_fence, VK_TRUE, std::numeric_limits<uint64_t>::max()),
                "failed to wait for in-flight fence");

  uint32_t image_index{};
  VkResult result{vkAcquireNextImageKHR(device, GetSwapChain().GetHandle(), std::numeric_limits<uint64_t>::max(),
                                        resources.image_available_semaphore, VK_NULL_HANDLE, &image_index)};
  if (VK_ERROR_OUT_OF_DATE_KHR == result) {
    RecreateSwapChain();
    return;
  }
  if (VK_SUCCESS != result && VK_SUBOPTIMAL_KHR != result) {
    CheckVkResult(result, "failed to acquire swap chain image");
  }

  VkSemaphore render_finished_semaphore{m_render_finished_semaphores.at(image_index)};

  CheckVkResult(vkResetFences(device, 1, &resources.in_flight_fence), "failed to reset in-flight fence");
  CheckVkResult(vkResetCommandBuffer(resources.command_buffer, 0), "failed to reset command buffer");

  VkCommandBufferBeginInfo begin_info{};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  CheckVkResult(vkBeginCommandBuffer(resources.command_buffer, &begin_info), "failed to begin command buffer");

  FrameContext frame_context{
    image_index,
    m_current_frame,
    resources.command_buffer,
  };
  OnRender(frame_context);

  CheckVkResult(vkEndCommandBuffer(resources.command_buffer), "failed to end command buffer");

  const std::array<VkSemaphore, 1> wait_semaphores{resources.image_available_semaphore};
  const std::array<VkPipelineStageFlags, 1> wait_stages{VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
  const std::array<VkSemaphore, 1> signal_semaphores{render_finished_semaphore};

  VkSubmitInfo submit_info{};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.waitSemaphoreCount = static_cast<uint32_t>(wait_semaphores.size());
  submit_info.pWaitSemaphores = wait_semaphores.data();
  submit_info.pWaitDstStageMask = wait_stages.data();
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &resources.command_buffer;
  submit_info.signalSemaphoreCount = static_cast<uint32_t>(signal_semaphores.size());
  submit_info.pSignalSemaphores = signal_semaphores.data();

  CheckVkResult(vkQueueSubmit(GetContext().GetGraphicsQueue(), 1, &submit_info, resources.in_flight_fence),
                "failed to submit command buffer");

  const std::array<VkSwapchainKHR, 1> swap_chains{GetSwapChain().GetHandle()};

  VkPresentInfoKHR present_info{};
  present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present_info.waitSemaphoreCount = static_cast<uint32_t>(signal_semaphores.size());
  present_info.pWaitSemaphores = signal_semaphores.data();
  present_info.swapchainCount = static_cast<uint32_t>(swap_chains.size());
  present_info.pSwapchains = swap_chains.data();
  present_info.pImageIndices = &image_index;

  result = vkQueuePresentKHR(GetContext().GetPresentQueue(), &present_info);
  if (VK_ERROR_OUT_OF_DATE_KHR == result || VK_SUBOPTIMAL_KHR == result || m_framebuffer_resized) {
    m_framebuffer_resized = false;
    RecreateSwapChain();
  } else if (VK_SUCCESS != result) {
    CheckVkResult(result, "failed to present swap chain image");
  }

  m_current_frame = (m_current_frame + 1U) % k_max_frames_in_flight;
}

void VulkanSample::RecreateSwapChain() {
  vkDeviceWaitIdle(GetContext().GetDevice());
  OnSwapChainCleanup();
  DestroySwapChainSemaphores();
  m_swap_chain->Recreate();
  CreateSwapChainSemaphores();
  OnSwapChainRecreated();
}

void VulkanSample::OnFramebufferResize([[maybe_unused]] uint32_t width, [[maybe_unused]] uint32_t height) noexcept {
  m_framebuffer_resized = true;
}
}  // namespace lvk
