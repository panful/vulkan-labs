#define GLFW_INCLUDE_VULKAN  // 定义这个宏之后 glfw3.h 文件就会包含 Vulkan 的头文件
#include <GLFW/glfw3.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <optional>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

static std::string g_app_name{"Vulkan-Hpp"};
static std::string g_engine_name{"Vulkan-Hpp"};
static std::vector<const char*> g_enable_layer_names{"VK_LAYER_KHRONOS_validation"};
static std::vector<const char*> g_enable_extension_names{VK_EXT_DEBUG_UTILS_EXTENSION_NAME};

static vk::Extent2D g_extent{800, 600};
static vk::Format g_color_format{};

constexpr static uint64_t k_timeout{std::numeric_limits<uint64_t>::max()};
constexpr static uint32_t k_max_frames_in_flight{3};
static uint32_t g_current_frame_index{0};

static VKAPI_ATTR vk::Bool32 VKAPI_CALL DebugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT /*message_severity*/,
                                                      vk::DebugUtilsMessageTypeFlagsEXT /*message_type*/,
                                                      const vk::DebugUtilsMessengerCallbackDataEXT* p_callback_data,
                                                      void* /*p_user_data*/
                                                      ) noexcept {
  std::clog << "===========================================\n"
            << "Debug::validation layer: " << p_callback_data->pMessage << '\n';

  return VK_FALSE;
}

static std::vector<uint32_t> ReadFile(const std::string& file_name) {
  std::ifstream file(file_name, std::ios::ate | std::ios::binary);
  if (!file.is_open()) {
    throw std::runtime_error("failed to open file: " + file_name);
  }

  size_t file_size = static_cast<size_t>(file.tellg());
  std::vector<uint32_t> buffer(file_size / (sizeof(uint32_t) / sizeof(char)));
  file.seekg(0);
  file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(file_size));
  file.close();

  return buffer;
}

vk::raii::Pipeline MakeGraphicsPipeline(
  vk::raii::Device const& device, vk::raii::PipelineCache const& pipeline_cache,
  vk::raii::ShaderModule const& vertex_shader_module, vk::SpecializationInfo const* vertex_shader_specialization_info,
  vk::raii::ShaderModule const& fragment_shader_module,
  vk::SpecializationInfo const* fragment_shader_specialization_info, uint32_t /*vertex_stride*/,
  std::vector<std::pair<vk::Format, uint32_t>> const& /*vertex_input_attribute_format_offset*/,
  vk::FrontFace front_face, bool depth_buffered, vk::raii::PipelineLayout const& pipeline_layout,
  vk::raii::RenderPass const& render_pass) {
  std::array<vk::PipelineShaderStageCreateInfo, 2> pipeline_shader_stage_create_infos = {
    vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eVertex, vertex_shader_module, "main",
                                      vertex_shader_specialization_info),
    vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eFragment, fragment_shader_module, "main",
                                      fragment_shader_specialization_info)};

  std::vector<vk::VertexInputAttributeDescription> vertex_input_attribute_descriptions;
  vk::PipelineVertexInputStateCreateInfo pipeline_vertex_input_state_create_info;

  vk::PipelineInputAssemblyStateCreateInfo pipeline_input_assembly_state_create_info(
    vk::PipelineInputAssemblyStateCreateFlags(), vk::PrimitiveTopology::eTriangleList);

  vk::PipelineViewportStateCreateInfo pipeline_viewport_state_create_info(vk::PipelineViewportStateCreateFlags(), 1,
                                                                          nullptr, 1, nullptr);

  vk::PipelineRasterizationStateCreateInfo pipeline_rasterization_state_create_info(
    vk::PipelineRasterizationStateCreateFlags(), false, false, vk::PolygonMode::eFill, vk::CullModeFlagBits::eBack,
    front_face, false, 0.0F, 0.0F, 0.0F, 1.0F);

  vk::PipelineMultisampleStateCreateInfo pipeline_multisample_state_create_info({}, vk::SampleCountFlagBits::e1);

  vk::PipelineDepthStencilStateCreateInfo pipeline_depth_stencil_state_create_info(
    vk::PipelineDepthStencilStateCreateFlags(), depth_buffered, depth_buffered, vk::CompareOp::eLessOrEqual, false,
    false, {}, {});

  vk::ColorComponentFlags color_component_flags(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                                                vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);
  vk::PipelineColorBlendAttachmentState pipeline_color_blend_attachment_state(
    false, vk::BlendFactor::eZero, vk::BlendFactor::eZero, vk::BlendOp::eAdd, vk::BlendFactor::eZero,
    vk::BlendFactor::eZero, vk::BlendOp::eAdd, color_component_flags);
  vk::PipelineColorBlendStateCreateInfo pipeline_color_blend_state_create_info(
    vk::PipelineColorBlendStateCreateFlags(), false, vk::LogicOp::eNoOp, pipeline_color_blend_attachment_state,
    {{1.0F, 1.0F, 1.0F, 1.0F}});

  std::array<vk::DynamicState, 2> dynamic_states = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};
  vk::PipelineDynamicStateCreateInfo pipeline_dynamic_state_create_info(vk::PipelineDynamicStateCreateFlags(),
                                                                        dynamic_states);

  vk::GraphicsPipelineCreateInfo graphics_pipeline_create_info(
    vk::PipelineCreateFlags(), pipeline_shader_stage_create_infos, &pipeline_vertex_input_state_create_info,
    &pipeline_input_assembly_state_create_info, nullptr, &pipeline_viewport_state_create_info,
    &pipeline_rasterization_state_create_info, &pipeline_multisample_state_create_info,
    &pipeline_depth_stencil_state_create_info, &pipeline_color_blend_state_create_info,
    &pipeline_dynamic_state_create_info, pipeline_layout, render_pass);

  return vk::raii::Pipeline(device, pipeline_cache, graphics_pipeline_create_info);
}

struct Window {
  Window(std::string const& name, vk::Extent2D const& extent)
      : extent(extent), window([&]() {
          glfwInit();
          glfwSetErrorCallback(
            [](int error, const char* msg) { std::cerr << "glfw: " << "(" << error << ") " << msg << '\n'; });
          glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
          auto* w = glfwCreateWindow(static_cast<int>(extent.width), static_cast<int>(extent.height), name.c_str(),
                                     nullptr, nullptr);
          glfwSetWindowUserPointer(w, this);
          glfwSetFramebufferSizeCallback(w, ResizeCallback);
          return w;
        }()) {}

  static void ResizeCallback(GLFWwindow* window, int width, int height) {
    auto instance = static_cast<Window*>(glfwGetWindowUserPointer(window));
    instance->resized = true;
    instance->extent = vk::Extent2D{static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
  }

  ~Window() {
    glfwDestroyWindow(window);
    glfwTerminate();
  }

  Window(const Window&) = delete;
  Window& operator=(const Window&) = delete;
  Window(Window&&) = delete;
  Window& operator=(Window&&) = delete;

  std::vector<const char*> GetExtensions() const noexcept {
    uint32_t glfw_extension_count = 0;
    const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

    return std::vector<const char*>(glfw_extensions, glfw_extensions + glfw_extension_count);
  }

  bool ShouldExit() const noexcept { return glfwWindowShouldClose(window); }

  void PollEvents() const noexcept { glfwPollEvents(); }

  GLFWwindow* window{};
  vk::Extent2D extent{};
  bool resized{};
};

struct SurfaceData {
  SurfaceData(vk::raii::Instance const& instance, GLFWwindow* window, vk::Extent2D const& extent) : extent(extent) {
    VkSurfaceKHR surface = nullptr;
    static_cast<void>(glfwCreateWindowSurface(static_cast<VkInstance>(*instance), window, nullptr, &surface));
    this->surface = vk::raii::SurfaceKHR(instance, surface);
  }

  vk::Extent2D extent;
  vk::raii::SurfaceKHR surface = nullptr;
};

vk::SurfaceFormatKHR PickSurfaceFormat(std::vector<vk::SurfaceFormatKHR> const& formats) {
  assert(!formats.empty());
  vk::SurfaceFormatKHR picked_format = formats[0];
  if (formats.size() == 1) {
    if (formats[0].format == vk::Format::eUndefined) {
      picked_format.format = vk::Format::eB8G8R8A8Unorm;
      picked_format.colorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;
    }
  } else {
    // request several formats, the first found will be used
    std::array<vk::Format, 4> requested_formats{vk::Format::eB8G8R8A8Unorm, vk::Format::eR8G8B8A8Unorm,
                                                vk::Format::eB8G8R8Unorm, vk::Format::eR8G8B8Unorm};
    vk::ColorSpaceKHR requested_color_space = vk::ColorSpaceKHR::eSrgbNonlinear;
    for (auto requested_format : requested_formats) {
      auto it = std::find_if(formats.begin(), formats.end(),
                             [requested_format, requested_color_space](vk::SurfaceFormatKHR const& f) {
                               return (f.format == requested_format) && (f.colorSpace == requested_color_space);
                             });
      if (it != formats.end()) {
        picked_format = *it;
        break;
      }
    }
  }
  assert(picked_format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear);
  return picked_format;
}

vk::PresentModeKHR PickPresentMode(std::vector<vk::PresentModeKHR> const& present_modes) {
  vk::PresentModeKHR picked_mode = vk::PresentModeKHR::eFifo;
  for (const auto& present_mode : present_modes) {
    if (present_mode == vk::PresentModeKHR::eMailbox) {
      picked_mode = present_mode;
      break;
    }

    if (present_mode == vk::PresentModeKHR::eImmediate) {
      picked_mode = present_mode;
    }
  }
  return picked_mode;
}

struct SwapChainData {
  SwapChainData(vk::raii::PhysicalDevice const& physical_device, vk::raii::Device const& device,
                vk::raii::SurfaceKHR const& surface, vk::Extent2D const& extent, vk::ImageUsageFlags usage,
                vk::raii::SwapchainKHR const* p_old_swapchain, uint32_t graphics_queue_family_index,
                uint32_t present_queue_family_index) {
    vk::SurfaceFormatKHR surface_format = PickSurfaceFormat(physical_device.getSurfaceFormatsKHR(surface));
    color_format = surface_format.format;

    vk::SurfaceCapabilitiesKHR surface_capabilities = physical_device.getSurfaceCapabilitiesKHR(surface);
    if (surface_capabilities.currentExtent.width == std::numeric_limits<uint32_t>::max()) {
      // If the surface size is undefined, the size is set to the size of the images requested.
      swapchain_extent.width =
        std::clamp(extent.width, surface_capabilities.minImageExtent.width, surface_capabilities.maxImageExtent.width);
      swapchain_extent.height = std::clamp(extent.height, surface_capabilities.minImageExtent.height,
                                           surface_capabilities.maxImageExtent.height);
    } else {
      // If the surface size is defined, the swap chain size must match
      swapchain_extent = surface_capabilities.currentExtent;
    }
    vk::SurfaceTransformFlagBitsKHR pre_transform =
      (surface_capabilities.supportedTransforms & vk::SurfaceTransformFlagBitsKHR::eIdentity)
        ? vk::SurfaceTransformFlagBitsKHR::eIdentity
        : surface_capabilities.currentTransform;
    vk::CompositeAlphaFlagBitsKHR composite_alpha =
      (surface_capabilities.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::ePreMultiplied)
        ? vk::CompositeAlphaFlagBitsKHR::ePreMultiplied
      : (surface_capabilities.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::ePostMultiplied)
        ? vk::CompositeAlphaFlagBitsKHR::ePostMultiplied
      : (surface_capabilities.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::eInherit)
        ? vk::CompositeAlphaFlagBitsKHR::eInherit
        : vk::CompositeAlphaFlagBitsKHR::eOpaque;

    vk::PresentModeKHR present_mode = PickPresentMode(physical_device.getSurfacePresentModesKHR(surface));
    vk::SwapchainCreateInfoKHR swap_chain_create_info(
      {}, surface, std::clamp(3U, surface_capabilities.minImageCount, surface_capabilities.maxImageCount), color_format,
      surface_format.colorSpace, swapchain_extent, 1, usage, vk::SharingMode::eExclusive, {}, pre_transform,
      composite_alpha, present_mode, true, p_old_swapchain ? **p_old_swapchain : nullptr);
    if (graphics_queue_family_index != present_queue_family_index) {
      std::array<uint32_t, 2> queue_family_indices{graphics_queue_family_index, present_queue_family_index};
      // If the graphics and present queues are from different queue families, we either have to explicitly
      // transfer ownership of images between the queues, or we have to create the swapchain with imageSharingMode
      // as vk::SharingMode::eConcurrent
      swap_chain_create_info.imageSharingMode = vk::SharingMode::eConcurrent;
      swap_chain_create_info.queueFamilyIndexCount = 2;
      swap_chain_create_info.pQueueFamilyIndices = queue_family_indices.data();
    }
    swap_chain = vk::raii::SwapchainKHR(device, swap_chain_create_info);

    images = swap_chain.getImages();

    image_views.reserve(images.size());
    vk::ImageViewCreateInfo image_view_create_info({}, {}, vk::ImageViewType::e2D, color_format, {},
                                                   {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});
    for (auto image : images) {
      image_view_create_info.image = image;
      image_views.emplace_back(device, image_view_create_info);
    }
  }

  vk::Format color_format;
  vk::raii::SwapchainKHR swap_chain = nullptr;
  std::vector<vk::Image> images;
  std::vector<vk::raii::ImageView> image_views;
  vk::Extent2D swapchain_extent;
};

void RecreateSwapChain(uint32_t& index, GLFWwindow* window, SwapChainData& swap_chain_data,
                       std::array<vk::raii::Framebuffer, k_max_frames_in_flight>& framebuffers,
                       vk::raii::RenderPass const& render_pass, vk::raii::PhysicalDevice const& physical_device,
                       vk::raii::Device const& device, vk::raii::SurfaceKHR const& surface, vk::Extent2D const& extent,
                       vk::ImageUsageFlags /*usage*/, vk::raii::SwapchainKHR const* p_old_swapchain,
                       uint32_t graphics_queue_family_index, uint32_t present_queue_family_index) {
  index = k_max_frames_in_flight -
          1;  // XXX: 验证层报错：交换链图像布局不符合展示需要的布局，窗口大小改变时当前帧序号从0开始就不会报错

  int width{0};
  int height{0};
  glfwGetFramebufferSize(window, &width, &height);
  while (0 == width || 0 == height) {
    glfwGetFramebufferSize(window, &width, &height);
    glfwWaitEvents();
  }

  device.waitIdle();

  try {
    swap_chain_data = SwapChainData(physical_device, device, surface, extent, vk::ImageUsageFlagBits::eColorAttachment,
                                    p_old_swapchain, graphics_queue_family_index, present_queue_family_index);
  } catch (vk::SystemError& err) {
    std::cout << "swapChain vk::SystemError: " << err.what() << '\n';
  }

  std::array<std::array<vk::ImageView, 1>, k_max_frames_in_flight> image_views{
    std::array<vk::ImageView, 1>{swap_chain_data.image_views[0]},
    std::array<vk::ImageView, 1>{swap_chain_data.image_views[1]},
    std::array<vk::ImageView, 1>{swap_chain_data.image_views[2]},
  };

  auto w = swap_chain_data.swapchain_extent.width;
  auto h = swap_chain_data.swapchain_extent.height;

  framebuffers[0] = vk::raii::Framebuffer(device, vk::FramebufferCreateInfo({}, render_pass, image_views[0], w, h, 1));
  framebuffers[1] = vk::raii::Framebuffer(device, vk::FramebufferCreateInfo({}, render_pass, image_views[1], w, h, 1));
  framebuffers[2] = vk::raii::Framebuffer(device, vk::FramebufferCreateInfo({}, render_pass, image_views[2], w, h, 1));
}

template <typename Func>
void OneTimeSubmit(vk::raii::Device const& device, vk::raii::CommandPool const& command_pool,
                   vk::raii::Queue const& queue, Func const& func) {
  vk::raii::CommandBuffer command_buffer =
    std::move(vk::raii::CommandBuffers(device, {*command_pool, vk::CommandBufferLevel::ePrimary, 1}).front());
  command_buffer.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
  func(command_buffer);
  command_buffer.end();
  vk::SubmitInfo submit_info(nullptr, nullptr, *command_buffer);
  queue.submit(submit_info, nullptr);
  queue.waitIdle();
}

void SetImageLayout(vk::raii::CommandBuffer const& command_buffer, vk::Image image, vk::Format format,
                    vk::ImageLayout old_image_layout, vk::ImageLayout new_image_layout) {
  vk::AccessFlags source_access_mask;
  switch (old_image_layout) {
    case vk::ImageLayout::eTransferDstOptimal:
      source_access_mask = vk::AccessFlagBits::eTransferWrite;
      break;
    case vk::ImageLayout::ePreinitialized:
      source_access_mask = vk::AccessFlagBits::eHostWrite;
      break;
    case vk::ImageLayout::eGeneral:  // sourceAccessMask is empty
    case vk::ImageLayout::eUndefined:
      break;
    default:
      assert(false);
      break;
  }

  vk::PipelineStageFlags source_stage;
  switch (old_image_layout) {
    case vk::ImageLayout::eGeneral:
    case vk::ImageLayout::ePreinitialized:
      source_stage = vk::PipelineStageFlagBits::eHost;
      break;
    case vk::ImageLayout::eTransferDstOptimal:
      source_stage = vk::PipelineStageFlagBits::eTransfer;
      break;
    case vk::ImageLayout::eUndefined:
      source_stage = vk::PipelineStageFlagBits::eTopOfPipe;
      break;
    default:
      assert(false);
      break;
  }

  vk::AccessFlags destination_access_mask;
  switch (new_image_layout) {
    case vk::ImageLayout::eColorAttachmentOptimal:
      destination_access_mask = vk::AccessFlagBits::eColorAttachmentWrite;
      break;
    case vk::ImageLayout::eDepthStencilAttachmentOptimal:
      destination_access_mask =
        vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite;
      break;
    case vk::ImageLayout::eGeneral:  // empty destinationAccessMask
    case vk::ImageLayout::ePresentSrcKHR:
      break;
    case vk::ImageLayout::eShaderReadOnlyOptimal:
      destination_access_mask = vk::AccessFlagBits::eShaderRead;
      break;
    case vk::ImageLayout::eTransferSrcOptimal:
      destination_access_mask = vk::AccessFlagBits::eTransferRead;
      break;
    case vk::ImageLayout::eTransferDstOptimal:
      destination_access_mask = vk::AccessFlagBits::eTransferWrite;
      break;
    default:
      assert(false);
      break;
  }

  vk::PipelineStageFlags destination_stage;
  switch (new_image_layout) {
    case vk::ImageLayout::eColorAttachmentOptimal:
      destination_stage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
      break;
    case vk::ImageLayout::eDepthStencilAttachmentOptimal:
      destination_stage = vk::PipelineStageFlagBits::eEarlyFragmentTests;
      break;
    case vk::ImageLayout::eGeneral:
      destination_stage = vk::PipelineStageFlagBits::eHost;
      break;
    case vk::ImageLayout::ePresentSrcKHR:
      destination_stage = vk::PipelineStageFlagBits::eBottomOfPipe;
      break;
    case vk::ImageLayout::eShaderReadOnlyOptimal:
      destination_stage = vk::PipelineStageFlagBits::eFragmentShader;
      break;
    case vk::ImageLayout::eTransferDstOptimal:
    case vk::ImageLayout::eTransferSrcOptimal:
      destination_stage = vk::PipelineStageFlagBits::eTransfer;
      break;
    default:
      assert(false);
      break;
  }

  vk::ImageAspectFlags aspect_mask;
  if (new_image_layout == vk::ImageLayout::eDepthStencilAttachmentOptimal) {
    aspect_mask = vk::ImageAspectFlagBits::eDepth;
    if (format == vk::Format::eD32SfloatS8Uint || format == vk::Format::eD24UnormS8Uint) {
      aspect_mask |= vk::ImageAspectFlagBits::eStencil;
    }
  } else {
    aspect_mask = vk::ImageAspectFlagBits::eColor;
  }

  vk::ImageSubresourceRange image_subresource_range(aspect_mask, 0, 1, 0, 1);
  vk::ImageMemoryBarrier image_memory_barrier(source_access_mask, destination_access_mask, old_image_layout,
                                              new_image_layout, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, image,
                                              image_subresource_range);
  return command_buffer.pipelineBarrier(source_stage, destination_stage, {}, nullptr, nullptr, image_memory_barrier);
}

uint32_t FindMemoryType(vk::PhysicalDeviceMemoryProperties const& memory_properties, uint32_t type_bits,
                        vk::MemoryPropertyFlags requirements_mask) {
  uint32_t type_index = uint32_t(~0);
  for (uint32_t i = 0; i < memory_properties.memoryTypeCount; i++) {
    if ((type_bits & 1) &&
        ((memory_properties.memoryTypes[i].propertyFlags & requirements_mask) == requirements_mask)) {
      type_index = i;
      break;
    }
    type_bits >>= 1;
  }
  assert(type_index != uint32_t(~0));
  return type_index;
}

vk::raii::DeviceMemory AllocateDeviceMemory(vk::raii::Device const& device,
                                            vk::PhysicalDeviceMemoryProperties const& memory_properties,
                                            vk::MemoryRequirements const& memory_requirements,
                                            vk::MemoryPropertyFlags memory_property_flags) {
  uint32_t memory_type_index =
    FindMemoryType(memory_properties, memory_requirements.memoryTypeBits, memory_property_flags);
  vk::MemoryAllocateInfo memory_allocate_info(memory_requirements.size, memory_type_index);
  return vk::raii::DeviceMemory(device, memory_allocate_info);
}

struct BufferData {
  BufferData(vk::raii::PhysicalDevice const& physical_device, vk::raii::Device const& device, vk::DeviceSize size,
             vk::BufferUsageFlags usage,
             vk::MemoryPropertyFlags property_flags = vk::MemoryPropertyFlagBits::eDeviceLocal)
      : buffer(device, vk::BufferCreateInfo({}, size, usage)),
        m_size(size),
        m_usage(usage),
        m_property_flags(property_flags) {
    device_memory = AllocateDeviceMemory(device, physical_device.getMemoryProperties(), buffer.getMemoryRequirements(),
                                         property_flags);
    buffer.bindMemory(device_memory, 0);
  }

  BufferData(std::nullptr_t) {}

  /// @brief 将数据拷贝到暂存缓冲
  /// @tparam DataType
  /// @param data
  template <typename DataType>
  void Upload(DataType const& data) const {
    assert((m_property_flags & vk::MemoryPropertyFlagBits::eHostCoherent) &&
           (m_property_flags & vk::MemoryPropertyFlagBits::eHostVisible));
    assert(sizeof(DataType) <= m_size);

    void* data_ptr = device_memory.mapMemory(0, sizeof(DataType));
    memcpy(data_ptr, &data, sizeof(DataType));
    device_memory.unmapMemory();
  }

  template <typename DataType>
  void Upload(std::vector<DataType> const& data, size_t stride = 0) const {
    assert(m_property_flags & vk::MemoryPropertyFlagBits::eHostVisible);

    size_t element_size = stride ? stride : sizeof(DataType);
    assert(sizeof(DataType) <= element_size);

    copyToDevice(device_memory, data.data(), data.size(), element_size);
  }

  /// @brief 将数据从CPU先拷贝到暂存缓冲，再拷贝到GPU专用缓冲
  /// @tparam DataType
  /// @param physicalDevice
  /// @param device
  /// @param commandPool
  /// @param queue
  /// @param data
  /// @param stride
  template <typename DataType>
  void Upload(vk::raii::PhysicalDevice const& physical_device, vk::raii::Device const& device,
              vk::raii::CommandPool const& command_pool, vk::raii::Queue const& queue,
              std::vector<DataType> const& data, size_t stride) const {
    assert(m_usage & vk::BufferUsageFlagBits::eTransferDst);
    assert(m_property_flags & vk::MemoryPropertyFlagBits::eDeviceLocal);

    size_t element_size = stride ? stride : sizeof(DataType);
    assert(sizeof(DataType) <= element_size);

    size_t data_size = data.size() * element_size;
    assert(data_size <= m_size);

    BufferData staging_buffer(physical_device, device, data_size, vk::BufferUsageFlagBits::eTransferSrc,
                              vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    copyToDevice(staging_buffer.device_memory, data.data(), data.size(), element_size);

    OneTimeSubmit(device, command_pool, queue, [&](vk::raii::CommandBuffer const& command_buffer) {
      command_buffer.copyBuffer(*staging_buffer.buffer, *this->buffer, vk::BufferCopy(0, 0, data_size));
    });
  }

  // NOLINTNEXTLINE(cppcoreguidelines-non-private-member-variables-in-classes)
  vk::raii::DeviceMemory device_memory = nullptr;
  // NOLINTNEXTLINE(cppcoreguidelines-non-private-member-variables-in-classes)
  vk::raii::Buffer buffer = nullptr;

private:
  vk::DeviceSize m_size;
  vk::BufferUsageFlags m_usage;
  vk::MemoryPropertyFlags m_property_flags;
};

struct ImageData {
  ImageData(vk::raii::PhysicalDevice const& physical_device, vk::raii::Device const& device, vk::Format format,
            vk::Extent2D const& extent, vk::ImageTiling tiling, vk::ImageUsageFlags usage,
            vk::ImageLayout initial_layout, vk::MemoryPropertyFlags memory_properties, vk::ImageAspectFlags aspect_mask,
            bool create_image_view = true)
      : format(format),
        image(device, {vk::ImageCreateFlags(),
                       vk::ImageType::e2D,
                       format,
                       vk::Extent3D(extent, 1),
                       1,
                       1,
                       vk::SampleCountFlagBits::e1,
                       tiling,
                       usage | vk::ImageUsageFlagBits::eSampled,
                       vk::SharingMode::eExclusive,
                       {},
                       initial_layout}) {
    device_memory = AllocateDeviceMemory(device, physical_device.getMemoryProperties(), image.getMemoryRequirements(),
                                         memory_properties);
    image.bindMemory(device_memory, 0);
    image_view = create_image_view
                   ? vk::raii::ImageView(device, vk::ImageViewCreateInfo({}, image, vk::ImageViewType::e2D, format, {},
                                                                         {aspect_mask, 0, 1, 0, 1}))
                   : nullptr;
  }

  ImageData(std::nullptr_t) {}

  // the DeviceMemory should be destroyed before the Image it is bound to; to get that order with the standard
  // destructor of the ImageData, the order of DeviceMemory and Image here matters
  vk::Format format;
  vk::raii::DeviceMemory device_memory = nullptr;
  vk::raii::Image image = nullptr;
  vk::raii::ImageView image_view = nullptr;
};

void RecreateOffscreenResources(const vk::raii::PhysicalDevice& physical_device, const vk::raii::Device& device,
                                const vk::Extent2D& extent, const vk::raii::RenderPass& render_pass,
                                const vk::Sampler& sampler, const vk::raii::DescriptorSets& descriptor_sets,
                                std::array<ImageData, k_max_frames_in_flight>& color_images,
                                std::array<vk::raii::Framebuffer, k_max_frames_in_flight>& framebuffers) {
  std::cout << "recreate offscreen: " << extent.width << '\t' << extent.height << '\n';

  for (size_t i = 0; i < k_max_frames_in_flight; ++i) {
    color_images[i] =
      ImageData(physical_device, device, g_color_format, extent, vk::ImageTiling::eOptimal,
                vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
                vk::ImageLayout::eUndefined, vk::MemoryPropertyFlagBits::eDeviceLocal, vk::ImageAspectFlagBits::eColor);
  }

  for (size_t i = 0; i < k_max_frames_in_flight; ++i) {
    std::array<vk::ImageView, 1> image_views{color_images[i].image_view};
    framebuffers[i] = vk::raii::Framebuffer(
      device, vk::FramebufferCreateInfo({}, render_pass, image_views, extent.width, extent.height, 1));
  }

  for (size_t i = 0; i < k_max_frames_in_flight; ++i) {
    vk::DescriptorImageInfo descriptor_image_info(sampler, color_images[i].image_view,
                                                  vk::ImageLayout::eShaderReadOnlyOptimal);

    std::array write_descriptor_sets{vk::WriteDescriptorSet{
      descriptor_sets[i], 0, 0, vk::DescriptorType::eCombinedImageSampler, descriptor_image_info, nullptr}};
    device.updateDescriptorSets(write_descriptor_sets, nullptr);
  }
}

int main() {
  try {
    Window window{"Test", g_extent};

    //--------------------------------------------------------------------------------------
    // 初始化 VkInstance
    vk::DebugUtilsMessageSeverityFlagsEXT severity_flags{vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                                                         vk::DebugUtilsMessageSeverityFlagBitsEXT::eError};
    vk::DebugUtilsMessageTypeFlagsEXT message_type_flags{vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                                                         vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
                                                         vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation};

    vk::DebugUtilsMessengerCreateInfoEXT debug_utils_messenger_create_info_ext{
      {}, severity_flags, message_type_flags, DebugCallback};

    vk::ApplicationInfo application_info{g_app_name.c_str(), 1, g_engine_name.c_str(), 1, VK_API_VERSION_1_1};
    auto&& window_extensions = window.GetExtensions();
    g_enable_extension_names.insert(g_enable_extension_names.cend(), window_extensions.begin(),
                                    window_extensions.end());
    vk::InstanceCreateInfo instance_create_info{
      {}, &application_info, g_enable_layer_names, g_enable_extension_names, &debug_utils_messenger_create_info_ext};

    vk::raii::Context context{};
    vk::raii::Instance instance{context, instance_create_info};
    vk::raii::DebugUtilsMessengerEXT debug_utils_messenger{
      instance,
      {{},
       vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
       vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
         vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation,
       &DebugCallback}};

    SurfaceData surface_data(instance, window.window, g_extent);

    //--------------------------------------------------------------------------------------
    // 选择一个合适的物理设备
    vk::raii::PhysicalDevices physical_devices(instance);  // 所有物理设备，继承自 std::vector
    std::optional<uint32_t> opt_use_physical_device_index{};
    uint32_t graphics_transfer_present_queue_index{};
    for (size_t i = 0; i < physical_devices.size(); ++i) {
      std::vector<vk::QueueFamilyProperties> queue_family_properties = physical_devices[i].getQueueFamilyProperties();
      for (size_t j = 0; j < queue_family_properties.size(); ++j) {
        if ((queue_family_properties[j].queueFlags & vk::QueueFlagBits::eGraphics) &&
            (queue_family_properties[j].queueFlags & vk::QueueFlagBits::eTransfer) &&
            physical_devices[i].getSurfaceSupportKHR(static_cast<uint32_t>(j), surface_data.surface)) {
          opt_use_physical_device_index = static_cast<uint32_t>(i);
          graphics_transfer_present_queue_index = static_cast<uint32_t>(j);
        }
      }
    }
    assert(opt_use_physical_device_index);
    auto use_physical_device_index = *opt_use_physical_device_index;
    auto const& physical_device = physical_devices[use_physical_device_index];
    g_color_format = PickSurfaceFormat(physical_device.getSurfaceFormatsKHR(surface_data.surface)).format;

    //--------------------------------------------------------------------------------------
    // 创建一个逻辑设备
    float queue_priority = 0.0F;
    vk::DeviceQueueCreateInfo device_queue_create_info({}, graphics_transfer_present_queue_index, 1, &queue_priority);
    std::array<const char*, 1> device_extensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    vk::DeviceCreateInfo device_create_info({}, device_queue_create_info, {}, device_extensions, {});
    vk::raii::Device device(physical_device, device_create_info);

    //--------------------------------------------------------------------------------------
    SwapChainData swap_chain_data(physical_device, device, surface_data.surface, surface_data.extent,
                                  vk::ImageUsageFlagBits::eColorAttachment, {}, graphics_transfer_present_queue_index,
                                  graphics_transfer_present_queue_index);
    assert(swap_chain_data.image_views.size() == k_max_frames_in_flight);

    //--------------------------------------------------------------------------------------
    vk::raii::CommandPool command_pool = vk::raii::CommandPool(
      device, {{vk::CommandPoolCreateFlagBits::eResetCommandBuffer}, graphics_transfer_present_queue_index});
    vk::raii::CommandBuffers command_buffers(device,
                                             {command_pool, vk::CommandBufferLevel::ePrimary, k_max_frames_in_flight});

    vk::raii::Queue graphics_transfer_present_queue(device, graphics_transfer_present_queue_index, 0);

    //--------------------------------------------------------------------------------------
    // offscreen
    std::array<ImageData, k_max_frames_in_flight> color_image_datas_offscreen{
      ImageData(physical_device, device, g_color_format, g_extent, vk::ImageTiling::eOptimal,
                vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
                vk::ImageLayout::eUndefined, vk::MemoryPropertyFlagBits::eDeviceLocal, vk::ImageAspectFlagBits::eColor),
      ImageData(physical_device, device, g_color_format, g_extent, vk::ImageTiling::eOptimal,
                vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
                vk::ImageLayout::eUndefined, vk::MemoryPropertyFlagBits::eDeviceLocal, vk::ImageAspectFlagBits::eColor),
      ImageData(physical_device, device, g_color_format, g_extent, vk::ImageTiling::eOptimal,
                vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
                vk::ImageLayout::eUndefined, vk::MemoryPropertyFlagBits::eDeviceLocal,
                vk::ImageAspectFlagBits::eColor)};

    //--------------------------------------------------------------------------------------
    vk::AttachmentReference color_attachment_offscreen(0, vk::ImageLayout::eColorAttachmentOptimal);
    std::array color_attachments_offscreen{color_attachment_offscreen};
    vk::SubpassDescription subpass_description_offscreen(
      vk::SubpassDescriptionFlags(), vk::PipelineBindPoint::eGraphics, {}, color_attachments_offscreen, {}, {}, {});
    std::vector<vk::AttachmentDescription> attachment_descriptions_offscreen{{{},
                                                                              g_color_format,
                                                                              vk::SampleCountFlagBits::e1,
                                                                              vk::AttachmentLoadOp::eClear,
                                                                              vk::AttachmentStoreOp::eStore,
                                                                              vk::AttachmentLoadOp::eDontCare,
                                                                              vk::AttachmentStoreOp::eDontCare,
                                                                              vk::ImageLayout::eUndefined,
                                                                              vk::ImageLayout::eShaderReadOnlyOptimal}};

    vk::RenderPassCreateInfo render_pass_create_info_offscreen(
      vk::RenderPassCreateFlags(), attachment_descriptions_offscreen, subpass_description_offscreen);
    vk::raii::RenderPass render_pass_offscreen(device, render_pass_create_info_offscreen);

    //--------------------------------------------------------------------------------------
    std::vector<uint32_t> vert_spv_offscreen = ReadFile(PROJECT_ASSETS_DIR "shaders/01_01_base_vert.spv");
    std::vector<uint32_t> frag_spv_offscreen = ReadFile(PROJECT_ASSETS_DIR "shaders/01_01_base_frag.spv");
    vk::raii::ShaderModule vertex_shader_module_offscreen(
      device, vk::ShaderModuleCreateInfo(vk::ShaderModuleCreateFlags(), vert_spv_offscreen));
    vk::raii::ShaderModule fragment_shader_module_offscreen(
      device, vk::ShaderModuleCreateInfo(vk::ShaderModuleCreateFlags(), frag_spv_offscreen));

    vk::raii::PipelineLayout pipeline_layout_offscreen(device, {{}, {}});
    vk::raii::PipelineCache pipeline_cache_offscreen(device, vk::PipelineCacheCreateInfo());

    vk::raii::Pipeline graphics_pipeline_offscreen = MakeGraphicsPipeline(
      device, pipeline_cache_offscreen, vertex_shader_module_offscreen, nullptr, fragment_shader_module_offscreen,
      nullptr, 0, {}, vk::FrontFace::eClockwise, false, pipeline_layout_offscreen, render_pass_offscreen);

    //--------------------------------------------------------------------------------------
    std::array<std::array<vk::ImageView, 1>, k_max_frames_in_flight> image_views_offscreen{
      color_image_datas_offscreen[0].image_view, color_image_datas_offscreen[1].image_view,
      color_image_datas_offscreen[2].image_view};

    std::array<vk::raii::Framebuffer, k_max_frames_in_flight> framebuffers_offscreen{
      vk::raii::Framebuffer(device, vk::FramebufferCreateInfo({}, render_pass_offscreen, image_views_offscreen[0],
                                                              g_extent.width, g_extent.height, 1)),
      vk::raii::Framebuffer(device, vk::FramebufferCreateInfo({}, render_pass_offscreen, image_views_offscreen[1],
                                                              g_extent.width, g_extent.height, 1)),
      vk::raii::Framebuffer(device, vk::FramebufferCreateInfo({}, render_pass_offscreen, image_views_offscreen[2],
                                                              g_extent.width, g_extent.height, 1)),
    };

    //--------------------------------------------------------------------------------------
    // quad
    vk::AttachmentReference color_attachment(0, vk::ImageLayout::eColorAttachmentOptimal);
    std::array color_attachments{color_attachment};
    vk::SubpassDescription subpass_description(vk::SubpassDescriptionFlags(), vk::PipelineBindPoint::eGraphics, {},
                                               color_attachments, {}, {}, {});
    std::vector<vk::AttachmentDescription> attachment_descriptions{{{},
                                                                    g_color_format,
                                                                    vk::SampleCountFlagBits::e1,
                                                                    vk::AttachmentLoadOp::eClear,
                                                                    vk::AttachmentStoreOp::eStore,
                                                                    vk::AttachmentLoadOp::eDontCare,
                                                                    vk::AttachmentStoreOp::eDontCare,
                                                                    vk::ImageLayout::eUndefined,
                                                                    vk::ImageLayout::ePresentSrcKHR}};

    vk::RenderPassCreateInfo render_pass_create_info(vk::RenderPassCreateFlags(), attachment_descriptions,
                                                     subpass_description);
    vk::raii::RenderPass render_pass(device, render_pass_create_info);

    //--------------------------------------------------------------------------------------
    std::vector<uint32_t> vert_spv = ReadFile(PROJECT_ASSETS_DIR "shaders/07_08_quad_vert.spv");
    std::vector<uint32_t> frag_spv = ReadFile(PROJECT_ASSETS_DIR "shaders/07_08_quad_frag.spv");
    vk::raii::ShaderModule vertex_shader_module(device,
                                                vk::ShaderModuleCreateInfo(vk::ShaderModuleCreateFlags(), vert_spv));
    vk::raii::ShaderModule fragment_shader_module(device,
                                                  vk::ShaderModuleCreateInfo(vk::ShaderModuleCreateFlags(), frag_spv));

    std::array descriptor_set_layout_bindings{vk::DescriptorSetLayoutBinding{
      0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment}};
    vk::DescriptorSetLayoutCreateInfo descriptor_set_layout_create_info({}, descriptor_set_layout_bindings);
    vk::raii::DescriptorSetLayout descriptor_set_layout(device, descriptor_set_layout_create_info);

    std::array<vk::DescriptorSetLayout, 1> descriptor_set_layours_for_pipeline{descriptor_set_layout};
    vk::raii::PipelineLayout pipeline_layout(device, {{}, descriptor_set_layours_for_pipeline});
    vk::raii::PipelineCache pipeline_cache(device, vk::PipelineCacheCreateInfo());

    vk::raii::Pipeline graphics_pipeline =
      MakeGraphicsPipeline(device, pipeline_cache, vertex_shader_module, nullptr, fragment_shader_module, nullptr, 0,
                           {}, vk::FrontFace::eClockwise, false, pipeline_layout, render_pass);

    //--------------------------------------------------------------------------------------
    vk::raii::Sampler sampler(device, vk::SamplerCreateInfo{{},
                                                            vk::Filter::eLinear,
                                                            vk::Filter::eLinear,
                                                            vk::SamplerMipmapMode::eLinear,
                                                            vk::SamplerAddressMode::eRepeat,
                                                            vk::SamplerAddressMode::eRepeat,
                                                            vk::SamplerAddressMode::eRepeat,
                                                            0.0F,
                                                            false,
                                                            16.0F,
                                                            false,
                                                            vk::CompareOp::eNever,
                                                            0.0F,
                                                            0.0F,
                                                            vk::BorderColor::eFloatOpaqueBlack});

    //--------------------------------------------------------------------------------------
    std::array descriptor_pool_sizes{
      vk::DescriptorPoolSize{vk::DescriptorType::eUniformBuffer, k_max_frames_in_flight},
      vk::DescriptorPoolSize{vk::DescriptorType::eCombinedImageSampler, k_max_frames_in_flight},
    };

    vk::raii::DescriptorPool descriptor_pool(
      device, {vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, k_max_frames_in_flight, descriptor_pool_sizes});

    std::array<vk::DescriptorSetLayout, k_max_frames_in_flight> descriptor_set_layouts{
      *descriptor_set_layout, *descriptor_set_layout, *descriptor_set_layout};
    vk::raii::DescriptorSets descriptor_sets(device, {descriptor_pool, descriptor_set_layouts});

    vk::DescriptorImageInfo descriptor_image_info0(sampler, color_image_datas_offscreen[0].image_view,
                                                   vk::ImageLayout::eShaderReadOnlyOptimal);
    vk::DescriptorImageInfo descriptor_image_info1(sampler, color_image_datas_offscreen[1].image_view,
                                                   vk::ImageLayout::eShaderReadOnlyOptimal);
    vk::DescriptorImageInfo descriptor_image_info2(sampler, color_image_datas_offscreen[2].image_view,
                                                   vk::ImageLayout::eShaderReadOnlyOptimal);

    std::array<std::array<vk::WriteDescriptorSet, 1>, k_max_frames_in_flight> write_descriptor_sets{
      std::array{vk::WriteDescriptorSet{descriptor_sets[0], 0, 0, vk::DescriptorType::eCombinedImageSampler,
                                        descriptor_image_info0, nullptr}},
      std::array{vk::WriteDescriptorSet{descriptor_sets[1], 0, 0, vk::DescriptorType::eCombinedImageSampler,
                                        descriptor_image_info1, nullptr}},
      std::array{vk::WriteDescriptorSet{descriptor_sets[2], 0, 0, vk::DescriptorType::eCombinedImageSampler,
                                        descriptor_image_info2, nullptr}}};
    device.updateDescriptorSets(write_descriptor_sets[0], nullptr);
    device.updateDescriptorSets(write_descriptor_sets[1], nullptr);
    device.updateDescriptorSets(write_descriptor_sets[2], nullptr);

    //--------------------------------------------------------------------------------------
    std::array<std::array<vk::ImageView, 1>, k_max_frames_in_flight> image_views{
      std::array<vk::ImageView, 1>{swap_chain_data.image_views[0]},
      std::array<vk::ImageView, 1>{swap_chain_data.image_views[1]},
      std::array<vk::ImageView, 1>{swap_chain_data.image_views[2]},
    };

    auto w = swap_chain_data.swapchain_extent.width;
    auto h = swap_chain_data.swapchain_extent.height;
    std::array<vk::raii::Framebuffer, k_max_frames_in_flight> framebuffers{
      vk::raii::Framebuffer(device, vk::FramebufferCreateInfo({}, render_pass, image_views[0], w, h, 1)),
      vk::raii::Framebuffer(device, vk::FramebufferCreateInfo({}, render_pass, image_views[1], w, h, 1)),
      vk::raii::Framebuffer(device, vk::FramebufferCreateInfo({}, render_pass, image_views[2], w, h, 1)),
    };

    //--------------------------------------------------------------------------------------
    std::array<vk::raii::Fence, k_max_frames_in_flight> draw_fences{
      vk::raii::Fence(device, vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled)),
      vk::raii::Fence(device, vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled)),
      vk::raii::Fence(device, vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled)),
    };

    std::array<vk::raii::Semaphore, k_max_frames_in_flight> render_finished_semaphore{
      vk::raii::Semaphore(device, vk::SemaphoreCreateInfo()),
      vk::raii::Semaphore(device, vk::SemaphoreCreateInfo()),
      vk::raii::Semaphore(device, vk::SemaphoreCreateInfo()),
    };

    std::array<vk::raii::Semaphore, k_max_frames_in_flight> image_acquired_semaphores{
      vk::raii::Semaphore(device, vk::SemaphoreCreateInfo()),
      vk::raii::Semaphore(device, vk::SemaphoreCreateInfo()),
      vk::raii::Semaphore(device, vk::SemaphoreCreateInfo()),
    };

    while (!window.ShouldExit()) {
      window.PollEvents();

      static_cast<void>(device.waitForFences({draw_fences[g_current_frame_index]}, VK_TRUE, k_timeout));
      auto [result, imageIndex] =
        swap_chain_data.swap_chain.acquireNextImage(k_timeout, image_acquired_semaphores[g_current_frame_index]);
      assert(imageIndex < swap_chain_data.images.size());

      if (vk::Result::eErrorOutOfDateKHR == result) {
        RecreateSwapChain(g_current_frame_index, window.window, swap_chain_data, framebuffers, render_pass,
                          physical_device, device, surface_data.surface, window.extent,
                          vk::ImageUsageFlagBits::eColorAttachment, &swap_chain_data.swap_chain,
                          graphics_transfer_present_queue_index, graphics_transfer_present_queue_index);

        // 离屏渲染和窗口显示大小始终一致
        RecreateOffscreenResources(physical_device, device, swap_chain_data.swapchain_extent, render_pass_offscreen,
                                   sampler, descriptor_sets, color_image_datas_offscreen, framebuffers_offscreen);

        continue;
      }
      if (vk::Result::eSuccess != result && vk::Result::eSuboptimalKHR != result) {
        throw std::runtime_error("failed to acquire swap chain image");
      }

      device.resetFences({draw_fences[g_current_frame_index]});

      auto&& cmd = command_buffers[g_current_frame_index];
      cmd.reset();
      cmd.begin({});

      //---------------------------------------------------------------------------
      // offscreen
      std::array<vk::ClearValue, 1> clear_values_offscreen;
      clear_values_offscreen[0].color = vk::ClearColorValue(0.1F, 0.2F, 0.3F, 1.F);
      vk::RenderPassBeginInfo render_pass_begin_info_offscreen(
        render_pass_offscreen, framebuffers_offscreen[g_current_frame_index],
        vk::Rect2D(vk::Offset2D(0, 0), swap_chain_data.swapchain_extent), clear_values_offscreen);

      command_buffers[g_current_frame_index].beginRenderPass(render_pass_begin_info_offscreen,
                                                             vk::SubpassContents::eInline);
      command_buffers[g_current_frame_index].bindPipeline(vk::PipelineBindPoint::eGraphics,
                                                          graphics_pipeline_offscreen);

      command_buffers[g_current_frame_index].setViewport(
        0, vk::Viewport(0.0F, 0.0F, static_cast<float>(swap_chain_data.swapchain_extent.width),
                        static_cast<float>(swap_chain_data.swapchain_extent.height), 0.0F, 1.0F));
      command_buffers[g_current_frame_index].setScissor(
        0, vk::Rect2D(vk::Offset2D(0, 0), swap_chain_data.swapchain_extent));

      command_buffers[g_current_frame_index].draw(3, 1, 0, 0);
      command_buffers[g_current_frame_index].endRenderPass();

      //---------------------------------------------------------------------------
      // quad
      std::array<vk::ClearValue, 1> clear_values;
      clear_values[0].color = vk::ClearColorValue(0.F, 0.F, 0.F, 1.F);
      vk::RenderPassBeginInfo render_pass_begin_info(render_pass, framebuffers[g_current_frame_index],
                                                     vk::Rect2D(vk::Offset2D(0, 0), swap_chain_data.swapchain_extent),
                                                     clear_values);
      cmd.beginRenderPass(render_pass_begin_info, vk::SubpassContents::eInline);

      cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, graphics_pipeline);
      cmd.setViewport(0, vk::Viewport(0.0F, 0.0F, static_cast<float>(swap_chain_data.swapchain_extent.width),
                                      static_cast<float>(swap_chain_data.swapchain_extent.height), 0.0F, 1.0F));
      cmd.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swap_chain_data.swapchain_extent));
      cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline_layout, 0,
                             {descriptor_sets[g_current_frame_index]}, nullptr);
      cmd.draw(3, 1, 0, 0);

      cmd.endRenderPass();
      cmd.end();

      //--------------------------------------------------------------------------------------
      std::array<vk::CommandBuffer, 1> draw_command_buffers{cmd};
      std::array<vk::Semaphore, 1> signal_semaphores{render_finished_semaphore[g_current_frame_index]};
      std::array<vk::Semaphore, 1> wait_semaphores{image_acquired_semaphores[g_current_frame_index]};
      std::array<vk::PipelineStageFlags, 1> wait_stages = {vk::PipelineStageFlagBits::eColorAttachmentOutput};
      vk::SubmitInfo draw_submit_info(wait_semaphores, wait_stages, draw_command_buffers, signal_semaphores);
      graphics_transfer_present_queue.submit(draw_submit_info, draw_fences[g_current_frame_index]);

      //--------------------------------------------------------------------------------------
      std::array<vk::Semaphore, 1> present_wait{render_finished_semaphore[g_current_frame_index]};
      std::array<vk::SwapchainKHR, 1> swapchains{swap_chain_data.swap_chain};
      vk::PresentInfoKHR present_info_khr(present_wait, swapchains, imageIndex);
      try {
        auto present_result = graphics_transfer_present_queue.presentKHR(present_info_khr);
        if (vk::Result::eErrorOutOfDateKHR == present_result || vk::Result::eSuboptimalKHR == present_result ||
            window.resized) {
          window.resized = false;

          RecreateSwapChain(g_current_frame_index, window.window, swap_chain_data, framebuffers, render_pass,
                            physical_device, device, surface_data.surface, window.extent,
                            vk::ImageUsageFlagBits::eColorAttachment, &swap_chain_data.swap_chain,
                            graphics_transfer_present_queue_index, graphics_transfer_present_queue_index);

          RecreateOffscreenResources(physical_device, device, swap_chain_data.swapchain_extent, render_pass_offscreen,
                                     sampler, descriptor_sets, color_image_datas_offscreen, framebuffers_offscreen);
        }
      } catch (vk::SystemError& err) {
        std::cout << err.what() << '\n';
      }

      g_current_frame_index = (g_current_frame_index + 1) % k_max_frames_in_flight;
    }

    device.waitIdle();
  }

  catch (vk::SystemError& err) {
    std::cout << "vk::SystemError: " << err.what() << '\n';
    exit(-1);
  }

  catch (std::exception& err) {
    std::cout << "std::exception: " << err.what() << '\n';
    exit(-1);
  }

  catch (...) {
    std::cout << "unknown error\n";
    exit(-1);
  }

  std::cout << "Success\n";
  return 0;
}
