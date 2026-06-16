#define GLFW_INCLUDE_VULKAN  // 定义这个宏之后 glfw3.h 文件就会包含 Vulkan 的头文件
#include <GLFW/glfw3.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <optional>
#include <utility>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

static std::string g_app_name{"Vulkan-Hpp"};
static std::string g_engine_name{"Vulkan-Hpp"};
static std::vector<const char*> g_enable_layer_names{"VK_LAYER_KHRONOS_validation"};
static std::vector<const char*> g_enable_extension_names{VK_EXT_DEBUG_UTILS_EXTENSION_NAME};

static vk::Extent2D g_extent{800, 600};

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
  Window(std::string const& name, vk::Extent2D const& in_extent) : extent(in_extent) {
    glfwInit();
    glfwSetErrorCallback(
      [](int error, const char* msg) { std::cerr << "glfw: " << "(" << error << ") " << msg << '\n'; });
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    window =
      glfwCreateWindow(static_cast<int>(extent.width), static_cast<int>(extent.height), name.c_str(), nullptr, nullptr);

    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, [](GLFWwindow* /*glfw_window*/, int /*width*/, int /*height*/) {});
  }

  ~Window() {
    glfwDestroyWindow(window);
    glfwTerminate();
  }

  Window(const Window&) = delete;
  Window& operator=(const Window&) = delete;

  Window(Window&& other) noexcept : window(std::exchange(other.window, nullptr)), extent(other.extent) {
    if (window) {
      glfwSetWindowUserPointer(window, this);
    }
  }

  Window& operator=(Window&& other) noexcept {
    if (this != &other) {
      if (window) {
        glfwDestroyWindow(window);
      }
      window = std::exchange(other.window, nullptr);
      extent = other.extent;
      if (window) {
        glfwSetWindowUserPointer(window, this);
      }
    }
    return *this;
  }

  std::vector<const char*> GetExtensions() const noexcept {
    uint32_t glfw_extension_count = 0;
    const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

    return std::vector<const char*>(glfw_extensions, glfw_extensions + glfw_extension_count);
  }

  bool ShouldExit() const noexcept { return glfwWindowShouldClose(window); }

  void PollEvents() const noexcept { glfwPollEvents(); }

  GLFWwindow* window{};
  vk::Extent2D extent{};
};

struct SurfaceData {
  SurfaceData(vk::raii::Instance const& instance, GLFWwindow* window, vk::Extent2D const& in_extent)
      : extent(in_extent) {
    VkSurfaceKHR raw_surface{};
    static_cast<void>(glfwCreateWindowSurface(static_cast<VkInstance>(*instance), window, nullptr, &raw_surface));
    surface = vk::raii::SurfaceKHR(instance, raw_surface);
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
    std::array<vk::Format, 4> requested_formats = {vk::Format::eB8G8R8A8Unorm, vk::Format::eR8G8B8A8Unorm,
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
      swapchainExtent.width =
        std::clamp(extent.width, surface_capabilities.minImageExtent.width, surface_capabilities.maxImageExtent.width);
      swapchainExtent.height = std::clamp(extent.height, surface_capabilities.minImageExtent.height,
                                          surface_capabilities.maxImageExtent.height);
    } else {
      // If the surface size is defined, the swap chain size must match
      swapchainExtent = surface_capabilities.currentExtent;
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
      surface_format.colorSpace, swapchainExtent, 1, usage, vk::SharingMode::eExclusive, {}, pre_transform,
      composite_alpha, present_mode, true, p_old_swapchain ? **p_old_swapchain : nullptr);
    if (graphics_queue_family_index != present_queue_family_index) {
      std::array<uint32_t, 2> queue_family_indices = {graphics_queue_family_index, present_queue_family_index};
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
  vk::Extent2D swapchainExtent;
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

  auto w = swap_chain_data.swapchainExtent.width;
  auto h = swap_chain_data.swapchainExtent.height;

  framebuffers[0] = vk::raii::Framebuffer(device, vk::FramebufferCreateInfo({}, render_pass, image_views[0], w, h, 1));
  framebuffers[1] = vk::raii::Framebuffer(device, vk::FramebufferCreateInfo({}, render_pass, image_views[1], w, h, 1));
  framebuffers[2] = vk::raii::Framebuffer(device, vk::FramebufferCreateInfo({}, render_pass, image_views[2], w, h, 1));
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
    if (!opt_use_physical_device_index.has_value()) {
      throw std::runtime_error("no suitable physical device found");
    }
    auto use_physical_device_index = opt_use_physical_device_index.value();
    auto const& physical_device = physical_devices[use_physical_device_index];
    auto color_format = PickSurfaceFormat(physical_device.getSurfaceFormatsKHR(surface_data.surface)).format;

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
    vk::AttachmentReference color_attachment(0, vk::ImageLayout::eColorAttachmentOptimal);
    std::array color_attachments{color_attachment};
    vk::SubpassDescription subpass_description(vk::SubpassDescriptionFlags(), vk::PipelineBindPoint::eGraphics, {},
                                               color_attachments, {}, {}, {});
    std::vector<vk::AttachmentDescription> attachment_descriptions{{{},
                                                                    color_format,
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
    std::vector<uint32_t> vert_spv = ReadFile(PROJECT_ASSETS_DIR "shaders/01_01_base_vert.spv");
    std::vector<uint32_t> frag_spv = ReadFile(PROJECT_ASSETS_DIR "shaders/01_01_base_frag.spv");
    vk::raii::ShaderModule vertex_shader_module(device,
                                                vk::ShaderModuleCreateInfo(vk::ShaderModuleCreateFlags(), vert_spv));
    vk::raii::ShaderModule fragment_shader_module(device,
                                                  vk::ShaderModuleCreateInfo(vk::ShaderModuleCreateFlags(), frag_spv));

    vk::raii::PipelineLayout pipeline_layout(device, {{}, {}});
    vk::raii::PipelineCache pipeline_cache(device, vk::PipelineCacheCreateInfo());

    vk::raii::Pipeline graphics_pipeline =
      MakeGraphicsPipeline(device, pipeline_cache, vertex_shader_module, nullptr, fragment_shader_module, nullptr, 0,
                           {}, vk::FrontFace::eClockwise, false, pipeline_layout, render_pass);

    //--------------------------------------------------------------------------------------
    std::array<std::array<vk::ImageView, 1>, k_max_frames_in_flight> image_views{
      std::array<vk::ImageView, 1>{swap_chain_data.image_views[0]},
      std::array<vk::ImageView, 1>{swap_chain_data.image_views[1]},
      std::array<vk::ImageView, 1>{swap_chain_data.image_views[2]},
    };

    auto w = swap_chain_data.swapchainExtent.width;
    auto h = swap_chain_data.swapchainExtent.height;
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
      auto [result, image_index] =
        swap_chain_data.swap_chain.acquireNextImage(k_timeout, image_acquired_semaphores[g_current_frame_index]);
      assert(image_index < swap_chain_data.images.size());

      if (vk::Result::eErrorOutOfDateKHR == result) {
        RecreateSwapChain(g_current_frame_index, window.window, swap_chain_data, framebuffers, render_pass,
                          physical_device, device, surface_data.surface, window.extent,
                          vk::ImageUsageFlagBits::eColorAttachment, &swap_chain_data.swap_chain,
                          graphics_transfer_present_queue_index, graphics_transfer_present_queue_index);
        continue;
      }
      if (vk::Result::eSuccess != result && vk::Result::eSuboptimalKHR != result) {
        throw std::runtime_error("failed to acquire swap chain image");
      }

      device.resetFences({draw_fences[g_current_frame_index]});

      auto&& cmd = command_buffers[g_current_frame_index];
      cmd.reset();

      std::array<vk::ClearValue, 1> clear_values;
      clear_values[0].color = vk::ClearColorValue(0.1F, 0.2F, 0.3F, 1.F);
      vk::RenderPassBeginInfo render_pass_begin_info(render_pass, framebuffers[g_current_frame_index],
                                                     vk::Rect2D(vk::Offset2D(0, 0), swap_chain_data.swapchainExtent),
                                                     clear_values);

      cmd.begin({});
      cmd.beginRenderPass(render_pass_begin_info, vk::SubpassContents::eInline);

      cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, graphics_pipeline);
      cmd.setViewport(0, vk::Viewport(0.0F, 0.0F, static_cast<float>(swap_chain_data.swapchainExtent.width),
                                      static_cast<float>(swap_chain_data.swapchainExtent.height), 0.0F, 1.0F));
      cmd.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swap_chain_data.swapchainExtent));
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
      vk::PresentInfoKHR present_info_khr(present_wait, swapchains, image_index);
      try {
        static_cast<void>(graphics_transfer_present_queue.presentKHR(present_info_khr));
      } catch (vk::SystemError& err) {
        RecreateSwapChain(g_current_frame_index, window.window, swap_chain_data, framebuffers, render_pass,
                          physical_device, device, surface_data.surface, window.extent,
                          vk::ImageUsageFlagBits::eColorAttachment, &swap_chain_data.swap_chain,
                          graphics_transfer_present_queue_index, graphics_transfer_present_queue_index);

        std::cout << err.what() << '\n';
      }

      g_current_frame_index = (g_current_frame_index + 1) % k_max_frames_in_flight;
    }

    device.waitIdle();
  } catch (vk::SystemError& err) {
    std::cout << "vk::SystemError: " << err.what() << '\n';
    exit(-1);
  } catch (std::exception& err) {
    std::cout << "std::exception: " << err.what() << '\n';
    exit(-1);
  } catch (...) {
    std::cout << "unknown error\n";
    exit(-1);
  }

  std::cout << "Success\n";
  return 0;
}
