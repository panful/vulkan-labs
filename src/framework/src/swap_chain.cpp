#include "lvk/swap_chain.h"

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>

#include "lvk/glfw_window.h"
#include "lvk/vk_utils.h"
#include "lvk/vulkan_context.h"

namespace lvk {
SwapChain::SwapChain(const VulkanContext& context, const GlfwWindow& window) : m_context(context), m_window(window) {
  Create(VK_NULL_HANDLE);
}

SwapChain::~SwapChain() noexcept { Cleanup(true); }

void SwapChain::Recreate() {
  // Windows 最小化时 framebuffer 可能为 0x0，此时创建 swapchain 会失败；先等待窗口恢复。
  m_window.WaitForVisibleFramebuffer();
  VkSwapchainKHR old_swap_chain{m_swap_chain};
  Cleanup(false);
  Create(old_swap_chain);
}

VkSwapchainKHR SwapChain::GetHandle() const noexcept { return m_swap_chain; }

VkFormat SwapChain::GetImageFormat() const noexcept { return m_image_format; }

VkExtent2D SwapChain::GetExtent() const noexcept { return m_extent; }

VkRenderPass SwapChain::GetRenderPass() const noexcept { return m_render_pass; }

VkFramebuffer SwapChain::GetFramebuffer(uint32_t image_index) const {
  return m_framebuffers.at(static_cast<size_t>(image_index));
}

uint32_t SwapChain::GetImageCount() const noexcept { return static_cast<uint32_t>(m_images.size()); }

void SwapChain::Create(VkSwapchainKHR old_swap_chain) {
  const SwapChainSupportDetails support{QuerySupport()};
  if (support.formats.empty() || support.present_modes.empty()) {
    throw std::runtime_error("swap chain support is incomplete");
  }

  const VkSurfaceFormatKHR surface_format{ChooseSurfaceFormat(support.formats)};
  const VkPresentModeKHR present_mode{ChoosePresentMode(support.present_modes)};
  const VkExtent2D extent{ChooseExtent(support.capabilities)};

  uint32_t image_count{support.capabilities.minImageCount + 1U};
  if (0U != support.capabilities.maxImageCount) {
    // Vulkan 允许 maxImageCount 为 0 表示“不限制”，否则必须夹到驱动允许范围内。
    image_count = std::min(image_count, support.capabilities.maxImageCount);
  }

  VkSwapchainCreateInfoKHR create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  create_info.surface = m_context.GetSurface();
  create_info.minImageCount = image_count;
  create_info.imageFormat = surface_format.format;
  create_info.imageColorSpace = surface_format.colorSpace;
  create_info.imageExtent = extent;
  create_info.imageArrayLayers = 1;
  create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

  const std::array<uint32_t, 2> queue_family_indices{
    m_context.GetGraphicsQueueFamily(),
    m_context.GetPresentQueueFamily(),
  };
  if (m_context.GetGraphicsQueueFamily() != m_context.GetPresentQueueFamily()) {
    // 教学框架优先选择简单的 concurrent 模式，避免在入门阶段引入显式队列族所有权转移。
    create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    create_info.queueFamilyIndexCount = static_cast<uint32_t>(queue_family_indices.size());
    create_info.pQueueFamilyIndices = queue_family_indices.data();
  } else {
    create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  }

  create_info.preTransform = support.capabilities.currentTransform;
  create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  create_info.presentMode = present_mode;
  create_info.clipped = VK_TRUE;
  // 重建时把旧 swapchain 交给驱动，便于驱动复用内部资源；新对象创建成功后再销毁旧对象。
  create_info.oldSwapchain = old_swap_chain;

  CheckVkResult(vkCreateSwapchainKHR(m_context.GetDevice(), &create_info, nullptr, &m_swap_chain),
                "failed to create swap chain");
  if (VK_NULL_HANDLE != old_swap_chain) {
    vkDestroySwapchainKHR(m_context.GetDevice(), old_swap_chain, nullptr);
  }

  CheckVkResult(vkGetSwapchainImagesKHR(m_context.GetDevice(), m_swap_chain, &image_count, nullptr),
                "failed to get swap chain image count");
  m_images.resize(image_count);
  CheckVkResult(vkGetSwapchainImagesKHR(m_context.GetDevice(), m_swap_chain, &image_count, m_images.data()),
                "failed to get swap chain images");

  m_image_format = surface_format.format;
  m_extent = extent;

  CreateImageViews();
  CreateDepthResources();
  CreateRenderPass();
  CreateFramebuffers();
}

void SwapChain::Cleanup(bool destroy_swap_chain) noexcept {
  VkDevice device{m_context.GetDevice()};

  // 销毁顺序与创建依赖相反：framebuffer 依赖 render pass/image view，depth image 依赖 memory。
  for (VkFramebuffer framebuffer : m_framebuffers) {
    vkDestroyFramebuffer(device, framebuffer, nullptr);
  }
  m_framebuffers.clear();

  if (VK_NULL_HANDLE != m_render_pass) {
    vkDestroyRenderPass(device, m_render_pass, nullptr);
    m_render_pass = VK_NULL_HANDLE;
  }

  if (VK_NULL_HANDLE != m_depth_image_view) {
    vkDestroyImageView(device, m_depth_image_view, nullptr);
    m_depth_image_view = VK_NULL_HANDLE;
  }

  if (VK_NULL_HANDLE != m_depth_image) {
    vkDestroyImage(device, m_depth_image, nullptr);
    m_depth_image = VK_NULL_HANDLE;
  }

  if (VK_NULL_HANDLE != m_depth_image_memory) {
    vkFreeMemory(device, m_depth_image_memory, nullptr);
    m_depth_image_memory = VK_NULL_HANDLE;
  }

  for (VkImageView image_view : m_image_views) {
    vkDestroyImageView(device, image_view, nullptr);
  }
  m_image_views.clear();

  if (destroy_swap_chain && VK_NULL_HANDLE != m_swap_chain) {
    vkDestroySwapchainKHR(device, m_swap_chain, nullptr);
    m_swap_chain = VK_NULL_HANDLE;
  }

  m_images.clear();
}

void SwapChain::CreateImageViews() {
  m_image_views.resize(m_images.size());
  for (size_t i{}; i < m_images.size(); ++i) {
    VkImageViewCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    create_info.image = m_images.at(i);
    create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    create_info.format = m_image_format;
    create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    create_info.subresourceRange.baseMipLevel = 0;
    create_info.subresourceRange.levelCount = 1;
    create_info.subresourceRange.baseArrayLayer = 0;
    create_info.subresourceRange.layerCount = 1;

    CheckVkResult(vkCreateImageView(m_context.GetDevice(), &create_info, nullptr, &m_image_views.at(i)),
                  "failed to create swap chain image view");
  }
}

void SwapChain::CreateDepthResources() {
  m_depth_format = FindDepthFormat();

  // 默认 render pass 总是带 depth attachment，方便 3D 教学样例直接使用。
  VkImageCreateInfo image_info{};
  image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  image_info.imageType = VK_IMAGE_TYPE_2D;
  image_info.extent.width = m_extent.width;
  image_info.extent.height = m_extent.height;
  image_info.extent.depth = 1;
  image_info.mipLevels = 1;
  image_info.arrayLayers = 1;
  image_info.format = m_depth_format;
  image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
  image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  image_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
  image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  image_info.samples = VK_SAMPLE_COUNT_1_BIT;

  CheckVkResult(vkCreateImage(m_context.GetDevice(), &image_info, nullptr, &m_depth_image),
                "failed to create depth image");

  VkMemoryRequirements memory_requirements{};
  vkGetImageMemoryRequirements(m_context.GetDevice(), m_depth_image, &memory_requirements);

  VkMemoryAllocateInfo allocate_info{};
  allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocate_info.allocationSize = memory_requirements.size;
  allocate_info.memoryTypeIndex = FindMemoryType(m_context.GetPhysicalDevice(), memory_requirements.memoryTypeBits,
                                                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  CheckVkResult(vkAllocateMemory(m_context.GetDevice(), &allocate_info, nullptr, &m_depth_image_memory),
                "failed to allocate depth image memory");
  CheckVkResult(vkBindImageMemory(m_context.GetDevice(), m_depth_image, m_depth_image_memory, 0),
                "failed to bind depth image memory");

  VkImageViewCreateInfo view_info{};
  view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  view_info.image = m_depth_image;
  view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  view_info.format = m_depth_format;
  view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  view_info.subresourceRange.baseMipLevel = 0;
  view_info.subresourceRange.levelCount = 1;
  view_info.subresourceRange.baseArrayLayer = 0;
  view_info.subresourceRange.layerCount = 1;

  CheckVkResult(vkCreateImageView(m_context.GetDevice(), &view_info, nullptr, &m_depth_image_view),
                "failed to create depth image view");
}

void SwapChain::CreateRenderPass() {
  VkAttachmentDescription color_attachment{};
  color_attachment.format = m_image_format;
  color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
  color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkAttachmentDescription depth_attachment{};
  depth_attachment.format = m_depth_format;
  depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
  depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkAttachmentReference color_attachment_ref{};
  color_attachment_ref.attachment = 0;
  color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &color_attachment_ref;

  VkAttachmentReference depth_attachment_ref{};
  depth_attachment_ref.attachment = 1;
  depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
  subpass.pDepthStencilAttachment = &depth_attachment_ref;

  VkSubpassDependency dependency{};
  dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  dependency.dstSubpass = 0;
  dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  // 这个依赖覆盖 color/depth 首次写入，避免 layout transition 与附件写入之间缺同步。

  VkRenderPassCreateInfo render_pass_info{};
  const std::array<VkAttachmentDescription, 2> attachments{color_attachment, depth_attachment};
  render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  render_pass_info.attachmentCount = static_cast<uint32_t>(attachments.size());
  render_pass_info.pAttachments = attachments.data();
  render_pass_info.subpassCount = 1;
  render_pass_info.pSubpasses = &subpass;
  render_pass_info.dependencyCount = 1;
  render_pass_info.pDependencies = &dependency;

  CheckVkResult(vkCreateRenderPass(m_context.GetDevice(), &render_pass_info, nullptr, &m_render_pass),
                "failed to create render pass");
}

void SwapChain::CreateFramebuffers() {
  m_framebuffers.resize(m_image_views.size());
  for (size_t i{}; i < m_image_views.size(); ++i) {
    const std::array<VkImageView, 2> attachments{m_image_views.at(i), m_depth_image_view};

    VkFramebufferCreateInfo framebuffer_info{};
    framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebuffer_info.renderPass = m_render_pass;
    framebuffer_info.attachmentCount = static_cast<uint32_t>(attachments.size());
    framebuffer_info.pAttachments = attachments.data();
    framebuffer_info.width = m_extent.width;
    framebuffer_info.height = m_extent.height;
    framebuffer_info.layers = 1;

    CheckVkResult(vkCreateFramebuffer(m_context.GetDevice(), &framebuffer_info, nullptr, &m_framebuffers.at(i)),
                  "failed to create framebuffer");
  }
}

VkFormat SwapChain::FindDepthFormat() const {
  return FindSupportedFormat({VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
                             VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

VkFormat SwapChain::FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling,
                                        VkFormatFeatureFlags features) const {
  for (VkFormat format : candidates) {
    VkFormatProperties properties{};
    vkGetPhysicalDeviceFormatProperties(m_context.GetPhysicalDevice(), format, &properties);
    if (VK_IMAGE_TILING_LINEAR == tiling && (properties.linearTilingFeatures & features) == features) {
      return format;
    }
    if (VK_IMAGE_TILING_OPTIMAL == tiling && (properties.optimalTilingFeatures & features) == features) {
      return format;
    }
  }

  throw std::runtime_error("failed to find supported format");
}

SwapChainSupportDetails SwapChain::QuerySupport() const {
  SwapChainSupportDetails details{};
  VkPhysicalDevice physical_device{m_context.GetPhysicalDevice()};
  VkSurfaceKHR surface{m_context.GetSurface()};

  CheckVkResult(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &details.capabilities),
                "failed to get surface capabilities");

  uint32_t format_count{};
  CheckVkResult(vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, nullptr),
                "failed to get surface formats");
  details.formats.resize(format_count);
  if (0U != format_count) {
    CheckVkResult(vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, details.formats.data()),
                  "failed to get surface formats");
  }

  uint32_t present_mode_count{};
  CheckVkResult(vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, nullptr),
                "failed to get present modes");
  details.present_modes.resize(present_mode_count);
  if (0U != present_mode_count) {
    CheckVkResult(vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count,
                                                            details.present_modes.data()),
                  "failed to get present modes");
  }

  return details;
}

VkSurfaceFormatKHR SwapChain::ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) const noexcept {
  for (const VkSurfaceFormatKHR& format : formats) {
    if (VK_FORMAT_B8G8R8A8_SRGB == format.format && VK_COLOR_SPACE_SRGB_NONLINEAR_KHR == format.colorSpace) {
      return format;
    }
  }

  return formats.front();
}

VkPresentModeKHR SwapChain::ChoosePresentMode(const std::vector<VkPresentModeKHR>& present_modes) const noexcept {
  for (VkPresentModeKHR present_mode : present_modes) {
    if (VK_PRESENT_MODE_MAILBOX_KHR == present_mode) {
      return present_mode;
    }
  }

  return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D SwapChain::ChooseExtent(const VkSurfaceCapabilitiesKHR& capabilities) const noexcept {
  if (std::numeric_limits<uint32_t>::max() != capabilities.currentExtent.width) {
    // 部分平台固定 surface extent，必须直接使用驱动给出的尺寸。
    return capabilities.currentExtent;
  }

  VkExtent2D actual_extent{
    m_window.GetFramebufferWidth(),
    m_window.GetFramebufferHeight(),
  };

  actual_extent.width =
    std::clamp(actual_extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
  actual_extent.height =
    std::clamp(actual_extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
  return actual_extent;
}
}  // namespace lvk
