#pragma warning(disable : 4996)  // 解决 stb_image_write.h 文件中的`sprintf`不安全警告

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image/stb_image_write.h>

#include <fstream>
#include <iostream>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

static std::string g_app_name{"Vulkan-Hpp"};
static std::string g_engine_name{"Vulkan-Hpp"};
static std::vector<const char*> g_enable_layer_names{"VK_LAYER_KHRONOS_validation"};
static std::vector<const char*> g_enable_extension_names{VK_EXT_DEBUG_UTILS_EXTENSION_NAME};

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT /*message_severity*/,
                                                    VkDebugUtilsMessageTypeFlagsEXT /*message_type*/,
                                                    const VkDebugUtilsMessengerCallbackDataEXT* p_callback_data,
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
    case vk::ImageLayout::eGeneral:  // source_access_mask is empty
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
    case vk::ImageLayout::eGeneral:  // empty destination_access_mask
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

int main() {
  try {
    //--------------------------------------------------------------------------------------
    // 初始化 VkInstance
    vk::DebugUtilsMessageSeverityFlagsEXT severity_flags{vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
                                                         vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                                                         vk::DebugUtilsMessageSeverityFlagBitsEXT::eError};
    vk::DebugUtilsMessageTypeFlagsEXT message_type_flags{vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                                                         vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
                                                         vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation};

    vk::DebugUtilsMessengerCreateInfoEXT debug_utils_messenger_create_info_ext{
      {}, severity_flags, message_type_flags, DebugCallback};

    vk::ApplicationInfo application_info{g_app_name.c_str(), 1, g_engine_name.c_str(), 1, VK_API_VERSION_1_1};
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

    //--------------------------------------------------------------------------------------
    // 选择一个合适的物理设备
    vk::raii::PhysicalDevices physical_devices(instance);  // 所有物理设备，继承自 std::vector
    uint32_t use_physical_device_index{};
    uint32_t graphics_queue_family_index{};
    for (size_t i = 0; i < physical_devices.size(); ++i) {
      std::vector<vk::QueueFamilyProperties> queue_family_properties = physical_devices[i].getQueueFamilyProperties();
      for (size_t j = 0; j < queue_family_properties.size(); ++j) {
        if (queue_family_properties[j].queueFlags & vk::QueueFlagBits::eGraphics) {
          use_physical_device_index = static_cast<uint32_t>(i);
          graphics_queue_family_index = static_cast<uint32_t>(j);
        }
      }
    }
    vk::raii::PhysicalDevice const& physical_device = physical_devices[use_physical_device_index];

    //--------------------------------------------------------------------------------------
    // 创建一个逻辑设备
    float queue_priority = 0.0F;
    vk::DeviceQueueCreateInfo device_queue_create_info({}, graphics_queue_family_index, 1, &queue_priority);
    vk::DeviceCreateInfo device_create_info({}, device_queue_create_info, {}, {}, {});
    vk::raii::Device device(physical_device, device_create_info);

    //--------------------------------------------------------------------------------------
    vk::raii::CommandPool command_pool = vk::raii::CommandPool(device, {{}, graphics_queue_family_index});
    vk::CommandBufferAllocateInfo command_buffer_allocate_info(command_pool, vk::CommandBufferLevel::ePrimary, 1);
    vk::raii::CommandBuffers command_buffers(device, command_buffer_allocate_info);
    auto command_buffer = std::move(command_buffers[0]);

    vk::raii::Queue graphics_queue(device, graphics_queue_family_index, 0);

    //--------------------------------------------------------------------------------------
    auto color_format = vk::Format::eR8G8B8A8Unorm;
    auto extent = vk::Extent3D(800, 600, 1);
    vk::ImageCreateInfo image_create_info(
      {}, vk::ImageType::e2D, color_format, extent, 1, 1, vk::SampleCountFlagBits::e1, vk::ImageTiling::eOptimal,
      vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc);
    vk::raii::Image color_image(device, image_create_info);
    vk::MemoryRequirements memory_requirements = color_image.getMemoryRequirements();
    uint32_t memory_type_index =
      FindMemoryType(physical_device.getMemoryProperties(), memory_requirements.memoryTypeBits,
                     {vk::MemoryPropertyFlagBits::eDeviceLocal});
    vk::MemoryAllocateInfo memory_allocate_info(memory_requirements.size, memory_type_index);
    vk::raii::DeviceMemory color_image_memory = vk::raii::DeviceMemory(device, memory_allocate_info);
    color_image.bindMemory(color_image_memory, 0);

    vk::ImageViewCreateInfo image_view_create_info({}, color_image, vk::ImageViewType::e2D, color_format, {},
                                                   {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});
    vk::raii::ImageView color_image_view(device, image_view_create_info);

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
                                                                    vk::ImageLayout::eTransferSrcOptimal}};

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
    std::array<vk::ImageView, 1> attachments;
    attachments[0] = color_image_view;
    vk::FramebufferCreateInfo framebuffer_create_info(vk::FramebufferCreateFlags(), render_pass, attachments,
                                                      extent.width, extent.height, 1);
    vk::raii::Framebuffer framebuffer{vk::raii::Framebuffer(device, framebuffer_create_info)};

    //--------------------------------------------------------------------------------------
    // 绘制三角形（顶点、颜色数据由着色器设置）
    command_buffer.begin({});

    std::array<vk::ClearValue, 1> clear_values;
    clear_values[0].color = vk::ClearColorValue(0.3F, 0.2F, 0.3F, 1.F);
    vk::RenderPassBeginInfo render_pass_begin_info(
      render_pass, framebuffer, vk::Rect2D(vk::Offset2D(0, 0), {extent.width, extent.height}), clear_values);

    command_buffer.beginRenderPass(render_pass_begin_info, vk::SubpassContents::eInline);
    command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, graphics_pipeline);

    command_buffer.setViewport(
      0, vk::Viewport(0.0F, 0.0F, static_cast<float>(extent.width), static_cast<float>(extent.height), 0.0F, 1.0F));
    command_buffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), {extent.width, extent.height}));

    command_buffer.draw(3, 1, 0, 0);
    command_buffer.endRenderPass();

    command_buffer.end();

    //--------------------------------------------------------------------------------------
    vk::raii::Fence draw_fence(device, vk::FenceCreateInfo());
    vk::SubmitInfo submit_info({}, {}, *command_buffer);
    graphics_queue.submit(submit_info, *draw_fence);
    static_cast<void>(device.waitForFences({draw_fence}, VK_TRUE, std::numeric_limits<uint64_t>::max()));

    //--------------------------------------------------------------------------------------
    vk::ImageCreateInfo image_create_info_2({}, vk::ImageType::e2D, color_format, extent, 1, 1,
                                            vk::SampleCountFlagBits::e1, vk::ImageTiling::eLinear,
                                            vk::ImageUsageFlagBits::eTransferDst);
    vk::raii::Image color_image_2(device, image_create_info_2);
    vk::MemoryRequirements memory_requirements_2 = color_image_2.getMemoryRequirements();

    uint32_t memory_type_index_2 =
      FindMemoryType(physical_device.getMemoryProperties(), memory_requirements_2.memoryTypeBits,
                     {vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent});

    vk::MemoryAllocateInfo memory_allocate_info_2(memory_requirements_2.size, memory_type_index_2);
    vk::raii::DeviceMemory color_image_memory_2 = vk::raii::DeviceMemory(device, memory_allocate_info_2);
    color_image_2.bindMemory(color_image_memory_2, 0);

    //--------------------------------------------------------------------------------------
    command_pool.reset();
    command_buffer.begin({});

    SetImageLayout(command_buffer, color_image_2, color_format, vk::ImageLayout::eUndefined,
                   vk::ImageLayout::eTransferDstOptimal);
    vk::ImageSubresourceLayers image_subresource_layers(vk::ImageAspectFlagBits::eColor, 0, 0, 1);
    vk::ImageCopy image_copy(image_subresource_layers, vk::Offset3D(), image_subresource_layers, vk::Offset3D(),
                             extent);
    command_buffer.copyImage(color_image, vk::ImageLayout::eTransferSrcOptimal, color_image_2,
                             vk::ImageLayout::eTransferDstOptimal, image_copy);
    SetImageLayout(command_buffer, color_image_2, color_format, vk::ImageLayout::eTransferDstOptimal,
                   vk::ImageLayout::eGeneral);

    command_buffer.end();

    //--------------------------------------------------------------------------------------
    vk::raii::Fence draw_fence_2(device, vk::FenceCreateInfo());
    auto submit_info_2 = vk::SubmitInfo({}, {}, *command_buffer);
    graphics_queue.submit(submit_info_2, draw_fence_2);
    static_cast<void>(device.waitForFences({draw_fence_2}, true, std::numeric_limits<uint64_t>::max()));

    //--------------------------------------------------------------------------------------
    auto sub_resource_layout = color_image_2.getSubresourceLayout({vk::ImageAspectFlagBits::eColor, 0, 0});
    auto save_image_pixels = reinterpret_cast<uint8_t*>(color_image_memory_2.mapMemory(0, vk::WholeSize, {}));
    save_image_pixels += sub_resource_layout.offset;
    stbi_write_jpg("test_raii.jpg", static_cast<int>(extent.width), static_cast<int>(extent.height), 4,
                   save_image_pixels, 100);
    color_image_memory_2.unmapMemory();
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
