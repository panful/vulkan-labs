#pragma warning(disable : 4996)  // 解决 stb_image_write.h 文件中的`sprintf`不安全警告

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image/stb_image_write.h>

#include <array>
#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

static std::string g_app_name{"Vulkan-Hpp"};
static std::string g_engine_name{"Vulkan-Hpp"};
static std::vector<const char*> g_enable_layer_names{"VK_LAYER_KHRONOS_validation"};
static std::vector<const char*> g_enable_extension_names{VK_EXT_DEBUG_UTILS_EXTENSION_NAME};

constexpr static uint32_t k_max_frames_in_flight{3};
static uint32_t g_current_frame_index{0};

struct Vertex {
  glm::vec2 pos{0.F, 0.F};
  glm::vec3 color{0.F, 0.F, 0.F};
};

struct UniformBufferObject {
  glm::mat4 model{glm::mat4(1.F)};
  glm::mat4 view{glm::mat4(1.F)};
  glm::mat4 proj{glm::mat4(1.F)};
};

// clang-format off
const std::vector<Vertex> k_vertices {
    {{-0.5F, -0.5F}, {1.F, 0.F, 0.F}},
    {{-0.5F,  0.5F}, {1.F, 0.F, 0.F}},
    {{ 0.5F,  0.5F}, {0.F, 1.F, 0.F}},
    {{ 0.5F, -0.5F}, {0.F, 1.F, 0.F}},
};

const std::vector<uint16_t> k_indices {
    0, 1, 2,
    0, 2, 3,
};

// clang-format on

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
  file.seekg(0, std::ios::beg);
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
  std::array input_binding{vk::VertexInputBindingDescription{0, sizeof(Vertex), vk::VertexInputRate::eVertex}};
  std::array input_attribute{
    vk::VertexInputAttributeDescription{0, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, pos)},
    vk::VertexInputAttributeDescription{1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color)}};

  vk::PipelineVertexInputStateCreateInfo pipeline_vertex_input_state_create_info({}, input_binding, input_attribute);

  vk::PipelineInputAssemblyStateCreateInfo pipeline_input_assembly_state_create_info(
    vk::PipelineInputAssemblyStateCreateFlags(), vk::PrimitiveTopology::eTriangleList);

  vk::PipelineViewportStateCreateInfo pipeline_viewport_state_create_info(vk::PipelineViewportStateCreateFlags(), 1,
                                                                          nullptr, 1, nullptr);

  vk::PipelineRasterizationStateCreateInfo pipeline_rasterization_state_create_info(
    vk::PipelineRasterizationStateCreateFlags(), false, false, vk::PolygonMode::eFill, vk::CullModeFlagBits::eNone,
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

vk::raii::DeviceMemory AllocateDeviceMemory(vk::raii::Device const& device,
                                            vk::PhysicalDeviceMemoryProperties const& memory_properties,
                                            vk::MemoryRequirements const& memory_requirements,
                                            vk::MemoryPropertyFlags memory_property_flags) {
  uint32_t memory_type_index =
    FindMemoryType(memory_properties, memory_requirements.memoryTypeBits, memory_property_flags);
  vk::MemoryAllocateInfo memory_allocate_info(memory_requirements.size, memory_type_index);
  return vk::raii::DeviceMemory(device, memory_allocate_info);
}

template <typename T>
void CopyToDevice(vk::raii::DeviceMemory const& device_memory, T const* p_data, size_t count,
                  vk::DeviceSize stride = sizeof(T)) {
  assert(sizeof(T) <= stride);
  uint8_t* device_data = static_cast<uint8_t*>(device_memory.mapMemory(0, count * stride));
  if (stride == sizeof(T)) {
    memcpy(device_data, p_data, count * sizeof(T));
  } else {
    for (size_t i = 0; i < count; i++) {
      memcpy(device_data, &p_data[i], sizeof(T));
      device_data += stride;
    }
  }
  device_memory.unmapMemory();
}

template <typename T>
void CopyToDevice(vk::raii::DeviceMemory const& device_memory, T const& data) {
  CopyToDevice<T>(device_memory, &data, 1);
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

    CopyToDevice(device_memory, data.data(), data.size(), element_size);
  }

  /// @brief 将数据从CPU先拷贝到暂存缓冲，再拷贝到GPU专用缓冲
  /// @tparam DataType
  /// @param physical_device
  /// @param device
  /// @param command_pool
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
    CopyToDevice(staging_buffer.device_memory, data.data(), data.size(), element_size);

    OneTimeSubmit(device, command_pool, queue, [&](vk::raii::CommandBuffer const& command_buffer) {
      command_buffer.copyBuffer(*staging_buffer.buffer, *this->buffer, vk::BufferCopy(0, 0, data_size));
    });
  }

  vk::raii::DeviceMemory device_memory = nullptr;
  vk::raii::Buffer buffer = nullptr;
  vk::DeviceSize m_size;
  vk::BufferUsageFlags m_usage;
  vk::MemoryPropertyFlags m_property_flags;
};

struct ImageData {
  ImageData(vk::raii::PhysicalDevice const& physical_device, vk::raii::Device const& device, vk::Format fmt,
            vk::Extent2D const& extent, vk::ImageTiling tiling, vk::ImageUsageFlags usage,
            vk::ImageLayout initial_layout, vk::MemoryPropertyFlags memory_properties, vk::ImageAspectFlags aspect_mask,
            bool create_image_view = true)
      : format(fmt),
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

int main() {
  try {
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
    uint32_t graphics_and_transfer_queue_family_index{};
    for (size_t i = 0; i < physical_devices.size(); ++i) {
      std::vector<vk::QueueFamilyProperties> queue_family_properties = physical_devices[i].getQueueFamilyProperties();
      for (size_t j = 0; j < queue_family_properties.size(); ++j) {
        if ((queue_family_properties[j].queueFlags & vk::QueueFlagBits::eGraphics) &&
            (queue_family_properties[j].queueFlags & vk::QueueFlagBits::eTransfer)) {
          use_physical_device_index = static_cast<uint32_t>(i);
          graphics_and_transfer_queue_family_index = static_cast<uint32_t>(j);
        }
      }
    }
    vk::raii::PhysicalDevice const& physical_device = physical_devices[use_physical_device_index];

    vk::FormatProperties format_properties = physical_device.getFormatProperties(vk::Format::eB8G8R8A8Unorm);
    bool support_blit{false};

    if ((format_properties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eBlitSrc) &&
        (format_properties.linearTilingFeatures & vk::FormatFeatureFlagBits::eBlitDst)) {
      std::cout << "Support blit\n";
      support_blit = true;
    }

    //--------------------------------------------------------------------------------------
    // 创建一个逻辑设备
    float queue_priority = 0.0F;
    vk::DeviceQueueCreateInfo device_queue_create_info({}, graphics_and_transfer_queue_family_index, 1,
                                                       &queue_priority);
    vk::DeviceCreateInfo device_create_info({}, device_queue_create_info, {}, {}, {});
    vk::raii::Device device(physical_device, device_create_info);

    //--------------------------------------------------------------------------------------
    vk::raii::CommandPool command_pool = vk::raii::CommandPool(
      device, {{vk::CommandPoolCreateFlagBits::eResetCommandBuffer}, graphics_and_transfer_queue_family_index});
    vk::raii::CommandBuffers command_buffers(device,
                                             {command_pool, vk::CommandBufferLevel::ePrimary, k_max_frames_in_flight});
    vk::raii::CommandBuffers blit_image_command_buffers(
      device, {command_pool, vk::CommandBufferLevel::ePrimary, k_max_frames_in_flight});

    vk::raii::Queue graphics_and_transfer_queue(device, graphics_and_transfer_queue_family_index, 0);

    //--------------------------------------------------------------------------------------
    auto color_format = vk::Format::eB8G8R8A8Unorm;
    auto extent = vk::Extent2D{800, 600};

    std::array<ImageData, k_max_frames_in_flight> color_image_datas{
      ImageData(physical_device, device, color_format, extent, vk::ImageTiling::eOptimal,
                vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc,
                vk::ImageLayout::eUndefined, vk::MemoryPropertyFlagBits::eDeviceLocal, vk::ImageAspectFlagBits::eColor),
      ImageData(physical_device, device, color_format, extent, vk::ImageTiling::eOptimal,
                vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc,
                vk::ImageLayout::eUndefined, vk::MemoryPropertyFlagBits::eDeviceLocal, vk::ImageAspectFlagBits::eColor),
      ImageData(physical_device, device, color_format, extent, vk::ImageTiling::eOptimal,
                vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc,
                vk::ImageLayout::eUndefined, vk::MemoryPropertyFlagBits::eDeviceLocal,
                vk::ImageAspectFlagBits::eColor)};

    //--------------------------------------------------------------------------------------
    std::array<ImageData, k_max_frames_in_flight> save_image_datas{
      ImageData(physical_device, device, color_format, extent, vk::ImageTiling::eLinear,
                vk::ImageUsageFlagBits::eTransferDst, vk::ImageLayout::eUndefined,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                vk::ImageAspectFlagBits::eColor, false),
      ImageData(physical_device, device, color_format, extent, vk::ImageTiling::eLinear,
                vk::ImageUsageFlagBits::eTransferDst, vk::ImageLayout::eUndefined,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                vk::ImageAspectFlagBits::eColor, false),
      ImageData(physical_device, device, color_format, extent, vk::ImageTiling::eLinear,
                vk::ImageUsageFlagBits::eTransferDst, vk::ImageLayout::eUndefined,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                vk::ImageAspectFlagBits::eColor, false)};

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
    std::vector<uint32_t> vert_spv = ReadFile(PROJECT_SHADER_DIR "07_04_base_vert.spv");
    std::vector<uint32_t> frag_spv = ReadFile(PROJECT_SHADER_DIR "07_04_base_frag.spv");
    vk::raii::ShaderModule vertex_shader_module(device,
                                                vk::ShaderModuleCreateInfo(vk::ShaderModuleCreateFlags(), vert_spv));
    vk::raii::ShaderModule fragment_shader_module(device,
                                                  vk::ShaderModuleCreateInfo(vk::ShaderModuleCreateFlags(), frag_spv));

    std::array descriptor_set_layout_bindings{
      vk::DescriptorSetLayoutBinding{0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex}};
    vk::DescriptorSetLayoutCreateInfo descriptor_set_layout_create_info({}, descriptor_set_layout_bindings);
    vk::raii::DescriptorSetLayout descriptor_set_layout(device, descriptor_set_layout_create_info);

    std::array<vk::DescriptorSetLayout, 1> descriptor_set_layouts_for_pipeline{descriptor_set_layout};
    vk::raii::PipelineLayout pipeline_layout(device, {{}, descriptor_set_layouts_for_pipeline});
    vk::raii::PipelineCache pipeline_cache(device, vk::PipelineCacheCreateInfo());

    vk::raii::Pipeline graphics_pipeline =
      MakeGraphicsPipeline(device, pipeline_cache, vertex_shader_module, nullptr, fragment_shader_module, nullptr, 0,
                           {}, vk::FrontFace::eClockwise, false, pipeline_layout, render_pass);

    //--------------------------------------------------------------------------------------
    BufferData vertex_buffer_data(physical_device, device, sizeof(Vertex) * k_vertices.size(),
                                  vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst);
    vertex_buffer_data.Upload(physical_device, device, command_pool, graphics_and_transfer_queue, k_vertices,
                              sizeof(Vertex));

    BufferData index_buffer_data(physical_device, device,
                                 sizeof(std::remove_cvref_t<decltype(k_indices.front())>) * k_indices.size(),
                                 vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst);
    index_buffer_data.Upload(physical_device, device, command_pool, graphics_and_transfer_queue, k_indices,
                             sizeof(std::remove_cvref_t<decltype(k_indices.front())>));

    std::array<BufferData, k_max_frames_in_flight> uniform_buffer_objects{
      BufferData(physical_device, device, sizeof(UniformBufferObject), vk::BufferUsageFlagBits::eUniformBuffer,
                 vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent),
      BufferData(physical_device, device, sizeof(UniformBufferObject), vk::BufferUsageFlagBits::eUniformBuffer,
                 vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent),
      BufferData(physical_device, device, sizeof(UniformBufferObject), vk::BufferUsageFlagBits::eUniformBuffer,
                 vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent)};

    UniformBufferObject default_ubo{};
    CopyToDevice(uniform_buffer_objects[0].device_memory, default_ubo);
    CopyToDevice(uniform_buffer_objects[1].device_memory, default_ubo);
    CopyToDevice(uniform_buffer_objects[2].device_memory, default_ubo);

    //--------------------------------------------------------------------------------------
    std::array descriptor_pool_sizes{
      vk::DescriptorPoolSize{vk::DescriptorType::eUniformBuffer, k_max_frames_in_flight}};

    vk::raii::DescriptorPool descriptor_pool(
      device, {vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, k_max_frames_in_flight, descriptor_pool_sizes});

    std::array<vk::DescriptorSetLayout, k_max_frames_in_flight> descriptor_set_layouts{
      *descriptor_set_layout, *descriptor_set_layout, *descriptor_set_layout};
    vk::raii::DescriptorSets descriptor_sets(device, {descriptor_pool, descriptor_set_layouts});

    vk::DescriptorBufferInfo descriptor_buffer_info_0(uniform_buffer_objects[0].buffer, 0, sizeof(UniformBufferObject));
    vk::DescriptorBufferInfo descriptor_buffer_info_1(uniform_buffer_objects[1].buffer, 0, sizeof(UniformBufferObject));
    vk::DescriptorBufferInfo descriptor_buffer_info_2(uniform_buffer_objects[2].buffer, 0, sizeof(UniformBufferObject));

    std::array<std::array<vk::WriteDescriptorSet, 1>, k_max_frames_in_flight> write_descriptor_sets{
      std::array{vk::WriteDescriptorSet{descriptor_sets[0], 0, 0, vk::DescriptorType::eUniformBuffer, nullptr,
                                        descriptor_buffer_info_0}},
      std::array{vk::WriteDescriptorSet{descriptor_sets[1], 0, 0, vk::DescriptorType::eUniformBuffer, nullptr,
                                        descriptor_buffer_info_1}},
      std::array{vk::WriteDescriptorSet{descriptor_sets[2], 0, 0, vk::DescriptorType::eUniformBuffer, nullptr,
                                        descriptor_buffer_info_2}},
    };
    device.updateDescriptorSets(write_descriptor_sets[0], nullptr);
    device.updateDescriptorSets(write_descriptor_sets[1], nullptr);
    device.updateDescriptorSets(write_descriptor_sets[2], nullptr);

    //--------------------------------------------------------------------------------------
    std::array<std::array<vk::ImageView, 1>, k_max_frames_in_flight> image_views{
      color_image_datas[0].image_view, color_image_datas[1].image_view, color_image_datas[2].image_view};

    std::array<vk::raii::Framebuffer, k_max_frames_in_flight> framebuffers{
      vk::raii::Framebuffer(device,
                            vk::FramebufferCreateInfo({}, render_pass, image_views[0], extent.width, extent.height, 1)),
      vk::raii::Framebuffer(device,
                            vk::FramebufferCreateInfo({}, render_pass, image_views[1], extent.width, extent.height, 1)),
      vk::raii::Framebuffer(device,
                            vk::FramebufferCreateInfo({}, render_pass, image_views[2], extent.width, extent.height, 1)),
    };

    //--------------------------------------------------------------------------------------
    std::array<vk::raii::Fence, k_max_frames_in_flight> draw_fences{
      vk::raii::Fence(device, vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled)),
      vk::raii::Fence(device, vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled)),
      vk::raii::Fence(device, vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled)),
    };

    std::array<vk::raii::Fence, k_max_frames_in_flight> blit_fences{
      vk::raii::Fence(device, vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled)),
      vk::raii::Fence(device, vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled)),
      vk::raii::Fence(device, vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled)),
    };

    std::array<vk::raii::Semaphore, k_max_frames_in_flight> render_finished_semaphore{
      vk::raii::Semaphore(device, vk::SemaphoreCreateInfo()),
      vk::raii::Semaphore(device, vk::SemaphoreCreateInfo()),
      vk::raii::Semaphore(device, vk::SemaphoreCreateInfo()),
    };

    static uint32_t s_count{0};
    while (s_count++ < 9) {
      std::cout << "Index: " << s_count << "\tFrame: " << g_current_frame_index << '\n';

      static_cast<void>(device.waitForFences({draw_fences[g_current_frame_index], blit_fences[g_current_frame_index]},
                                             VK_TRUE, std::numeric_limits<uint64_t>::max()));
      device.resetFences({draw_fences[g_current_frame_index], blit_fences[g_current_frame_index]});

      // 确保命令执行完之后再保存图片，每个 commandbuffer 对应的第一张图片都还没有绘制就保存，所以都是黑色，图片格式是
      // BGRA
      auto sub_resource_layout =
        save_image_datas[g_current_frame_index].image.getSubresourceLayout({vk::ImageAspectFlagBits::eColor, 0, 0});
      auto save_image_pixels = reinterpret_cast<uint8_t*>(
        save_image_datas[g_current_frame_index].device_memory.mapMemory(0, vk::WholeSize, {}));
      save_image_pixels += sub_resource_layout.offset;
      stbi_write_jpg(("raii_" + std::to_string(s_count) + ".jpg").c_str(), static_cast<int>(extent.width),
                     static_cast<int>(extent.height), 4, save_image_pixels, 100);
      save_image_datas[g_current_frame_index].device_memory.unmapMemory();

      // 更新 Uniform
      UniformBufferObject temp_ubo{};
      temp_ubo.model =
        glm::rotate(glm::mat4(1.F), glm::radians(30.F * static_cast<float>(s_count)), glm::vec3(0.F, 0.F, 1.F));
      CopyToDevice(uniform_buffer_objects[g_current_frame_index].device_memory, temp_ubo);

      //--------------------------------------------------------------------------------------
      auto&& cmd = command_buffers[g_current_frame_index];
      cmd.reset();
      cmd.begin({});

      std::array<vk::ClearValue, 1> clear_values;
      clear_values[0].color = vk::ClearColorValue(0.0F + 1.F / static_cast<float>(s_count), 0.2F, 0.3F, 1.F);
      vk::RenderPassBeginInfo render_pass_begin_info(render_pass, framebuffers[g_current_frame_index],
                                                     vk::Rect2D(vk::Offset2D(0, 0), extent), clear_values);

      cmd.beginRenderPass(render_pass_begin_info, vk::SubpassContents::eInline);

      cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, graphics_pipeline);
      cmd.setViewport(
        0, vk::Viewport(0.0F, 0.0F, static_cast<float>(extent.width), static_cast<float>(extent.height), 0.0F, 1.0F));
      cmd.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), extent));

      cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline_layout, 0,
                             {descriptor_sets[g_current_frame_index]}, nullptr);
      cmd.bindVertexBuffers(0, {vertex_buffer_data.buffer}, {0});
      cmd.bindIndexBuffer(index_buffer_data.buffer, 0, vk::IndexType::eUint16);
      cmd.drawIndexed(static_cast<uint32_t>(k_indices.size()), 1, 0, 0, 0);

      cmd.endRenderPass();

      cmd.end();

      std::array<vk::CommandBuffer, 1> draw_command_buffers{cmd};
      std::array<vk::Semaphore, 1> draw_semaphores{render_finished_semaphore[g_current_frame_index]};
      vk::SubmitInfo draw_submit_info(0, nullptr, nullptr, 1, draw_command_buffers.data(), 1, draw_semaphores.data());
      graphics_and_transfer_queue.submit(draw_submit_info, *draw_fences[g_current_frame_index]);

      //--------------------------------------------------------------------------------------
      auto&& blit_cmd = blit_image_command_buffers[g_current_frame_index];
      blit_cmd.reset();
      blit_cmd.begin({});

      SetImageLayout(blit_cmd, save_image_datas[g_current_frame_index].image, color_format, vk::ImageLayout::eUndefined,
                     vk::ImageLayout::eTransferDstOptimal);

      if (support_blit) {
        vk::ImageSubresourceLayers image_subresource_layers(vk::ImageAspectFlagBits::eColor, 0, 0, 1);
        std::array<vk::Offset3D, 2> offsets{
          vk::Offset3D(0, 0, 0),
          vk::Offset3D{static_cast<int32_t>(extent.width), static_cast<int32_t>(extent.height), 1}};
        vk::ImageBlit image_blit(image_subresource_layers, offsets, image_subresource_layers, offsets);
        blit_cmd.blitImage(color_image_datas[g_current_frame_index].image, vk::ImageLayout::eTransferSrcOptimal,
                           save_image_datas[g_current_frame_index].image, vk::ImageLayout::eTransferDstOptimal,
                           image_blit, vk::Filter::eLinear);
      } else {
        vk::ImageSubresourceLayers image_subresource_layers(vk::ImageAspectFlagBits::eColor, 0, 0, 1);
        vk::ImageCopy image_copy(image_subresource_layers, vk::Offset3D(), image_subresource_layers, vk::Offset3D(),
                                 vk::Extent3D{extent, 1});
        blit_cmd.copyImage(color_image_datas[g_current_frame_index].image, vk::ImageLayout::eTransferSrcOptimal,
                           save_image_datas[g_current_frame_index].image, vk::ImageLayout::eTransferDstOptimal,
                           image_copy);
      }

      SetImageLayout(blit_cmd, save_image_datas[g_current_frame_index].image, color_format,
                     vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eGeneral);

      blit_cmd.end();

      std::array<vk::CommandBuffer, 1> blit_command_buffers{blit_cmd};
      std::array<vk::PipelineStageFlags, 1> wait_stages = {vk::PipelineStageFlagBits::eColorAttachmentOutput};
      vk::SubmitInfo blit_submit_info(1, draw_semaphores.data(), wait_stages.data(), 1, blit_command_buffers.data());
      graphics_and_transfer_queue.submit(blit_submit_info, blit_fences[g_current_frame_index]);

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
