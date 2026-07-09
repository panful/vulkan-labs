#include "lvk/imgui_layer.h"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include <array>
#include <stdexcept>
#include <string_view>

#include "lvk/glfw_window.h"
#include "lvk/swap_chain.h"
#include "lvk/vk_utils.h"
#include "lvk/vulkan_context.h"

namespace lvk {
namespace {
void CheckImGuiVkResult(VkResult result) { CheckVkResult(result, "ImGui Vulkan backend failure"); }
}  // namespace

void DrawImGuiText(std::string_view text) { ImGui::TextUnformatted(text.data(), text.data() + text.size()); }

ImGuiLayer::~ImGuiLayer() noexcept { Shutdown(); }

void ImGuiLayer::Initialize(const ImGuiLayerDesc& desc) {
  if (m_is_initialized) {
    Shutdown();
  }
  if (nullptr == desc.window || nullptr == desc.context || nullptr == desc.swap_chain) {
    throw std::invalid_argument("ImGuiLayerDesc contains null dependency");
  }

  m_window = desc.window;
  m_context = desc.context;
  m_swap_chain = desc.swap_chain;

  // ImGui Vulkan 后端需要一个 descriptor pool 存放字体和用户纹理描述符。
  CreateDescriptorPool();

  ImGui::CreateContext();
  ImGuiIO& io{ImGui::GetIO()};
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  ImGui::StyleColorsDark();

  // 传 false 表示不让 ImGui 后端直接安装 GLFW 回调，统一走 GlfwWindow 的订阅系统。
  ImGui_ImplGlfw_InitForVulkan(m_window->GetNativeWindow(), false);
  RegisterCallbacks();

  ImGui_ImplVulkan_InitInfo init_info{};
  init_info.Instance = m_context->GetInstance();
  init_info.PhysicalDevice = m_context->GetPhysicalDevice();
  init_info.Device = m_context->GetDevice();
  init_info.QueueFamily = m_context->GetGraphicsQueueFamily();
  init_info.Queue = m_context->GetGraphicsQueue();
  init_info.DescriptorPool = m_descriptor_pool;
  init_info.MinImageCount = m_swap_chain->GetImageCount();
  init_info.ImageCount = m_swap_chain->GetImageCount();
  init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
  init_info.CheckVkResultFn = CheckImGuiVkResult;

  if (!ImGui_ImplVulkan_Init(&init_info, m_swap_chain->GetRenderPass())) {
    throw std::runtime_error("failed to initialize ImGui Vulkan backend");
  }

  UploadFonts();
  m_is_initialized = true;
}

void ImGuiLayer::Shutdown() noexcept {
  if (m_is_initialized) {
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    m_is_initialized = false;
  }
  // 即使 Initialize 中途失败，也尝试解除已经注册的回调。
  UnregisterCallbacks();

  if (nullptr != m_context) {
    VkDevice device{m_context->GetDevice()};
    if (VK_NULL_HANDLE != m_upload_command_pool) {
      vkDestroyCommandPool(device, m_upload_command_pool, nullptr);
      m_upload_command_pool = VK_NULL_HANDLE;
    }
    if (VK_NULL_HANDLE != m_descriptor_pool) {
      vkDestroyDescriptorPool(device, m_descriptor_pool, nullptr);
      m_descriptor_pool = VK_NULL_HANDLE;
    }
  }

  m_window = nullptr;
  m_context = nullptr;
  m_swap_chain = nullptr;
}

void ImGuiLayer::BeginFrame() {
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
}

void ImGuiLayer::Render(VkCommandBuffer command_buffer) {
  ImGui::Render();
  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), command_buffer);
}

bool ImGuiLayer::WantsMouse() const { return ImGui::GetIO().WantCaptureMouse; }

bool ImGuiLayer::WantsKeyboard() const { return ImGui::GetIO().WantCaptureKeyboard; }

void ImGuiLayer::CreateDescriptorPool() {
  const std::array<VkDescriptorPoolSize, 11> pool_sizes{{
    {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
    {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
    {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
    {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
    {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
    {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
    {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
    {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
    {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
    {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
    {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000},
  }};

  // 使用 ImGui 官方示例常见的大 pool 配置，教学框架避免过早引入复杂的描述符池管理。
  VkDescriptorPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  pool_info.maxSets = 1000U * static_cast<uint32_t>(pool_sizes.size());
  pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
  pool_info.pPoolSizes = pool_sizes.data();

  CheckVkResult(vkCreateDescriptorPool(m_context->GetDevice(), &pool_info, nullptr, &m_descriptor_pool),
                "failed to create ImGui descriptor pool");
}

void ImGuiLayer::UploadFonts() {
  VkCommandPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  // 字体上传只执行一次，使用 transient command pool 表达短生命周期命令缓冲区。
  pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
  pool_info.queueFamilyIndex = m_context->GetGraphicsQueueFamily();
  CheckVkResult(vkCreateCommandPool(m_context->GetDevice(), &pool_info, nullptr, &m_upload_command_pool),
                "failed to create ImGui upload command pool");

  VkCommandBuffer command_buffer{VK_NULL_HANDLE};
  VkCommandBufferAllocateInfo allocate_info{};
  allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocate_info.commandPool = m_upload_command_pool;
  allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocate_info.commandBufferCount = 1;
  CheckVkResult(vkAllocateCommandBuffers(m_context->GetDevice(), &allocate_info, &command_buffer),
                "failed to allocate ImGui upload command buffer");

  VkCommandBufferBeginInfo begin_info{};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  CheckVkResult(vkBeginCommandBuffer(command_buffer, &begin_info), "failed to begin ImGui font upload command buffer");
  ImGui_ImplVulkan_CreateFontsTexture(command_buffer);
  CheckVkResult(vkEndCommandBuffer(command_buffer), "failed to end ImGui font upload command buffer");

  VkSubmitInfo submit_info{};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &command_buffer;
  CheckVkResult(vkQueueSubmit(m_context->GetGraphicsQueue(), 1, &submit_info, VK_NULL_HANDLE),
                "failed to submit ImGui font upload command buffer");
  // 初始化阶段直接等待设备空闲，简化教学代码中的一次性上传同步。
  CheckVkResult(vkDeviceWaitIdle(m_context->GetDevice()), "failed to wait for ImGui font upload");
  ImGui_ImplVulkan_DestroyFontUploadObjects();
}

void ImGuiLayer::RegisterCallbacks() {
  if (nullptr == m_window || GlfwWindow::k_invalid_callback_handle != m_callback_handles.front()) {
    return;
  }

  GLFWwindow* native_window{m_window->GetNativeWindow()};
  // ImGui 后端初始化时选择不安装 GLFW 回调，因此这里手动转发输入事件。
  // 保存句柄是为了 Shutdown 时解除订阅，避免 swapchain 重建后重复转发。
  m_callback_handles.at(0) = m_window->AddCursorPositionCallback(
    [native_window](double x, double y) { ImGui_ImplGlfw_CursorPosCallback(native_window, x, y); });
  m_callback_handles.at(1) = m_window->AddMouseButtonCallback([native_window](int button, int action, int mods) {
    ImGui_ImplGlfw_MouseButtonCallback(native_window, button, action, mods);
  });
  m_callback_handles.at(2) = m_window->AddScrollCallback([native_window](double x_offset, double y_offset) {
    ImGui_ImplGlfw_ScrollCallback(native_window, x_offset, y_offset);
  });
  m_callback_handles.at(3) = m_window->AddKeyCallback([native_window](int key, int scancode, int action, int mods) {
    ImGui_ImplGlfw_KeyCallback(native_window, key, scancode, action, mods);
  });
  m_callback_handles.at(4) = m_window->AddCharCallback(
    [native_window](uint32_t codepoint) { ImGui_ImplGlfw_CharCallback(native_window, codepoint); });
}

void ImGuiLayer::UnregisterCallbacks() noexcept {
  if (nullptr == m_window) {
    m_callback_handles.fill(GlfwWindow::k_invalid_callback_handle);
    return;
  }

  for (GlfwWindow::CallbackHandle callback_handle : m_callback_handles) {
    m_window->RemoveCallback(callback_handle);
  }
  m_callback_handles.fill(GlfwWindow::k_invalid_callback_handle);
}
}  // namespace lvk
