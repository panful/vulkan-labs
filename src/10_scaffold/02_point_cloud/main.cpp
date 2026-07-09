// clang-format off
#include <imgui.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "glm/ext/vector_double3.hpp"
#include <vulkan/vulkan.h>
// clang-format on

#include <lvk/camera/camera.h>
#include <lvk/camera/orbit_camera_controller.h>
#include <lvk/frame_context.h>
#include <lvk/imgui_layer.h>
#include <lvk/input_state.h>
#include <lvk/shader_loader.h>
#include <lvk/swap_chain.h>
#include <lvk/vk_utils.h>
#include <lvk/vulkan_context.h>
#include <lvk/vulkan_sample.h>
#define TINYPLY_IMPLEMENTATION
#include <tiny_ply/tinyply.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <format>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
// float3(4x3) + uint32_t(4x1) = 16 bytes
struct Vertex {
  glm::vec3 position{};
  uint32_t color{};

  static constexpr VkVertexInputBindingDescription GetBindingDescription() noexcept {
    VkVertexInputBindingDescription binding_description{};
    binding_description.binding = 0;
    binding_description.stride = sizeof(Vertex);
    binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return binding_description;
  }

  // VK_FORMAT_R8G8B8A8_UNORM 颜色自动归一化，不需要手动 unpack，即 cpu 是 0~255，gpu shader 里是 0.0~1.0
  static constexpr std::array<VkVertexInputAttributeDescription, 2> GetAttributeDescriptions() noexcept {
    std::array<VkVertexInputAttributeDescription, 2> attribute_descriptions{};
    attribute_descriptions[0].binding = 0;
    attribute_descriptions[0].location = 0;
    attribute_descriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attribute_descriptions[0].offset = offsetof(Vertex, position);
    attribute_descriptions[1].binding = 0;
    attribute_descriptions[1].location = 1;
    attribute_descriptions[1].format = VK_FORMAT_R8G8B8A8_UNORM;
    attribute_descriptions[1].offset = offsetof(Vertex, color);
    return attribute_descriptions;
  }
};

static_assert(sizeof(Vertex) == 16);

class BoundingBox {
public:
  [[nodiscard]] glm::dvec3 Min() const noexcept { return m_min; }
  [[nodiscard]] glm::dvec3 Max() const noexcept { return m_max; }

  [[nodiscard]] glm::dvec3 GetBoundsCenter() const noexcept { return (m_min + m_max) * 0.5; }
  [[nodiscard]] double GetBoundsRadius() const noexcept { return std::max(glm::length((m_max - m_min) * 0.5), 0.001); }

  [[nodiscard]] static BoundingBox ComputeBoundingBox(const std::vector<Vertex>& vertices) {
    if (vertices.empty()) {
      throw std::runtime_error("point cloud contains no vertices");
    }

    BoundingBox bounds{};
    bounds.m_min = glm::dvec3{vertices.front().position};
    bounds.m_max = bounds.m_min;

    for (const Vertex& vertex : vertices) {
      const glm::dvec3 position{vertex.position};
      bounds.m_min = glm::min(bounds.m_min, position);
      bounds.m_max = glm::max(bounds.m_max, position);
    }

    return bounds;
  }

private:
  glm::dvec3 m_min{-1.0};
  glm::dvec3 m_max{1.0};
};

class PlyVertexLoader final {
public:
  [[nodiscard]] static std::vector<Vertex> Load(std::string_view file_path) {
    std::ifstream file{std::string{file_path}, std::ios::binary};
    if (!file) {
      throw std::runtime_error("failed to open PLY file");
    }

    tinyply::PlyFile ply_file{};
    ply_file.parse_header(file);
    const std::vector<tinyply::PlyElement> elements{ply_file.get_elements()};

    std::shared_ptr<tinyply::PlyData> positions{
      ply_file.request_properties_from_element("vertex", std::vector<std::string>{"x", "y", "z"})};

    std::shared_ptr<tinyply::PlyData> colors{};
    if (HasVertexProperties(elements, std::vector<std::string>{"red", "green", "blue"})) {
      colors = ply_file.request_properties_from_element("vertex", std::vector<std::string>{"red", "green", "blue"});
    } else if (HasVertexProperties(elements, std::vector<std::string>{"r", "g", "b"})) {
      colors = ply_file.request_properties_from_element("vertex", std::vector<std::string>{"r", "g", "b"});
    }

    ply_file.read(file);

    std::vector<Vertex> vertices{};
    FillPositions(*positions, vertices);
    if (colors) {
      FillColors(*colors, vertices);
    } else {
      FillDefaultColors(vertices);
    }

    if (vertices.empty()) {
      throw std::runtime_error("PLY file contains no vertices");
    }

    return vertices;
  }

private:
  [[nodiscard]] static constexpr uint32_t PackRgba8(uint8_t red, uint8_t green, uint8_t blue,
                                                    uint8_t alpha = 255) noexcept {
    return static_cast<uint32_t>(red) | (static_cast<uint32_t>(green) << 8U) | (static_cast<uint32_t>(blue) << 16U) |
           (static_cast<uint32_t>(alpha) << 24U);
  }

  [[nodiscard]] static bool HasVertexProperties(const std::vector<tinyply::PlyElement>& elements,
                                                const std::vector<std::string>& property_names) {
    const auto vertex_element{
      std::ranges::find_if(elements, [](const tinyply::PlyElement& element) { return "vertex" == element.name; })};
    if (vertex_element == elements.end()) {
      return false;
    }

    return std::ranges::all_of(property_names, [&vertex_element](const std::string& property_name) {
      return vertex_element->properties.end() !=
             std::ranges::find_if(vertex_element->properties, [&property_name](const tinyply::PlyProperty& property) {
               return property_name == property.name;
             });
    });
  }

  [[nodiscard]] static uint8_t ConvertColorComponent(float value) noexcept {
    const float scaled_value{value <= 1.0F ? value * 255.0F : value};
    return static_cast<uint8_t>(std::clamp(scaled_value, 0.0F, 255.0F));
  }

  [[nodiscard]] static uint8_t ConvertColorComponent(double value) noexcept {
    const double scaled_value{value <= 1.0 ? value * 255.0 : value};
    return static_cast<uint8_t>(std::clamp(scaled_value, 0.0, 255.0));
  }

  [[nodiscard]] static uint8_t ConvertColorComponent(uint16_t value) noexcept {
    if (value <= uint16_t{255}) {
      return static_cast<uint8_t>(value);
    }
    return static_cast<uint8_t>(static_cast<uint32_t>(value) * 255U / 65535U);
  }

  template <typename T>
  [[nodiscard]] static uint8_t ConvertColorComponent(T value) noexcept {
    return static_cast<uint8_t>(
      std::clamp(static_cast<uint32_t>(value), uint32_t{}, static_cast<uint32_t>(uint8_t{255})));
  }

  template <typename PositionType>
  static void FillPositions(const tinyply::PlyData& positions, std::vector<Vertex>& vertices) {
    if (0U == positions.count || positions.buffer.size_bytes() != positions.count * sizeof(PositionType) * 3U) {
      throw std::runtime_error("PLY vertex positions must contain x, y and z per vertex");
    }

    const size_t vertex_count{positions.count};
    vertices.resize(vertex_count);
    const PositionType* position_values{reinterpret_cast<const PositionType*>(positions.buffer.get_const())};
    for (size_t i{}; i < vertex_count; ++i) {
      vertices.at(i).position = {
        static_cast<float>(position_values[(i * 3U) + 0U]),
        static_cast<float>(position_values[(i * 3U) + 1U]),
        static_cast<float>(position_values[(i * 3U) + 2U]),
      };
    }
  }

  template <typename ColorType>
  static void FillColors(const tinyply::PlyData& colors, std::vector<Vertex>& vertices) {
    if (colors.count != vertices.size() || colors.buffer.size_bytes() != vertices.size() * sizeof(ColorType) * 3U) {
      throw std::runtime_error("PLY vertex colors must contain red, green and blue per vertex");
    }

    const ColorType* color_values{reinterpret_cast<const ColorType*>(colors.buffer.get_const())};
    for (size_t i{}; i < vertices.size(); ++i) {
      vertices.at(i).color = PackRgba8(ConvertColorComponent(color_values[(i * 3U) + 0U]),
                                       ConvertColorComponent(color_values[(i * 3U) + 1U]),
                                       ConvertColorComponent(color_values[(i * 3U) + 2U]));
    }
  }

  static void FillDefaultColors(std::vector<Vertex>& vertices) {
    for (Vertex& vertex : vertices) {
      vertex.color = PackRgba8(255, 255, 255);
    }
  }

  static void FillPositions(const tinyply::PlyData& positions, std::vector<Vertex>& vertices) {
    switch (positions.t) {
      case tinyply::Type::FLOAT32:
        FillPositions<float>(positions, vertices);
        return;
      case tinyply::Type::FLOAT64:
        FillPositions<double>(positions, vertices);
        return;
      default:
        throw std::runtime_error("PLY vertex positions must use float or double properties");
    }
  }

  static void FillColors(const tinyply::PlyData& colors, std::vector<Vertex>& vertices) {
    switch (colors.t) {
      case tinyply::Type::UINT8:
        FillColors<uint8_t>(colors, vertices);
        return;
      case tinyply::Type::UINT16:
        FillColors<uint16_t>(colors, vertices);
        return;
      case tinyply::Type::FLOAT32:
        FillColors<float>(colors, vertices);
        return;
      case tinyply::Type::FLOAT64:
        FillColors<double>(colors, vertices);
        return;
      default:
        throw std::runtime_error("PLY vertex colors must use uint8, uint16, float or double properties");
    }
  }
};

class PointCloudSample final : public lvk::VulkanSample {
  struct UniformBufferObject {
    glm::mat4 model{1.0F};
    glm::mat4 view{1.0F};
    glm::mat4 projection{1.0F};
  };

  struct BufferResource {
    VkBuffer buffer{VK_NULL_HANDLE};
    VkDeviceMemory memory{VK_NULL_HANDLE};
    void* mapped_memory{nullptr};
  };

public:
  PointCloudSample(std::vector<Vertex> vertices, BoundingBox bounds)
      : m_vertices(std::move(vertices)),
        m_vertex_count(static_cast<uint32_t>(m_vertices.size())),
        m_point_cloud_bounds(bounds) {}

protected:
  void Configure(lvk::ApplicationDesc& desc) override {
    desc.title = "Point Cloud";
    desc.width = 800;
    desc.height = 600;
  }

  void OnCreate() override {
    InitializeCamera();
    CreateVertexBuffer();
    CreateDescriptorSetLayout();
    CreateUniformBuffers();
    CreateDescriptorPool();
    CreateDescriptorSets();
    CreatePipeline();
    InitializeImGui();
  }

  void OnDestroy() noexcept override {
    m_imgui_layer.Shutdown();
    DestroyPipeline();
    DestroyDescriptorResources();
    DestroyBuffer(m_vertex_buffer);
  }

  void OnUpdate(float delta_seconds, const lvk::InputState& input_state) override {
    const lvk::camera::CameraControllerInput camera_input{
      MapCameraControllerInput(input_state, m_imgui_layer.WantsMouse())};

    UpdateFrameStats(delta_seconds);
    UpdateOrbitViewportSize();
    m_yaw_pitch_orbit_camera_controller.Update(delta_seconds, camera_input);
  }

  void OnRender(lvk::FrameContext& frame_context) override {
    UpdateUniformBuffer(frame_context.frame_index);

    BeginDefaultRenderPass(frame_context, m_clear_color);

    const VkExtent2D extent{GetSwapChain().GetExtent()};
    VkViewport viewport{};
    viewport.x = 0.0F;
    viewport.y = 0.0F;
    viewport.width = static_cast<float>(extent.width);
    viewport.height = static_cast<float>(extent.height);
    viewport.minDepth = 0.0F;
    viewport.maxDepth = 1.0F;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = extent;

    const std::array<VkBuffer, 1> vertex_buffers{m_vertex_buffer.buffer};
    const std::array<VkDeviceSize, 1> offsets{0};

    vkCmdBindPipeline(frame_context.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphics_pipeline);
    vkCmdSetViewport(frame_context.command_buffer, 0, 1, &viewport);
    vkCmdSetScissor(frame_context.command_buffer, 0, 1, &scissor);
    vkCmdBindVertexBuffers(frame_context.command_buffer, 0, static_cast<uint32_t>(vertex_buffers.size()),
                           vertex_buffers.data(), offsets.data());
    vkCmdBindDescriptorSets(frame_context.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline_layout, 0, 1,
                            &m_descriptor_sets.at(frame_context.frame_index), 0, nullptr);
    vkCmdDraw(frame_context.command_buffer, m_vertex_count, 1, 0, 0);

    m_imgui_layer.BeginFrame();
    DrawUi();
    m_imgui_layer.Render(frame_context.command_buffer);
    EndDefaultRenderPass(frame_context);
  }

  void OnSwapChainCleanup() noexcept override {
    m_imgui_layer.Shutdown();
    DestroyPipeline();
  }

  void OnSwapChainRecreated() override {
    UpdateCameraAspectRatio();
    CreatePipeline();
    InitializeImGui();
  }

private:
  [[nodiscard]] static lvk::camera::CameraControllerInput MapCameraControllerInput(const lvk::InputState& input_state,
                                                                                   bool wants_mouse) noexcept {
    lvk::camera::CameraControllerInput input{};
    input.cursor_x = input_state.mouse_x;
    input.cursor_y = input_state.mouse_y;

    if (!wants_mouse) {
      input.cursor_delta_x = input_state.mouse_delta_x;
      input.cursor_delta_y = input_state.mouse_delta_y;
      input.scroll_delta_y = input_state.scroll_delta_y;
      input.rotate = input_state.left_mouse_down;
      input.pan = input_state.middle_mouse_down || input_state.right_mouse_down;
      input.look = input_state.right_mouse_down;
    }

    return input;
  }

  void InitializeCamera() {
    m_yaw_pitch_orbit_camera_controller.Attach(&m_camera);
    lvk::camera::OrbitCameraControllerDesc desc{};
    desc.target = m_point_cloud_bounds.GetBoundsCenter();
    desc.distance = GetCameraFitDistance();
    desc.min_distance = std::max(m_point_cloud_bounds.GetBoundsRadius() * 0.001, 0.001);
    desc.max_distance = std::max(desc.distance * 10.0, m_point_cloud_bounds.GetBoundsRadius() * 20.0);
    ApplyOrbitDesc(desc);
    ResetCameraPose();
  }

  void ApplyCameraProjection() {
    const VkExtent2D extent{GetSwapChain().GetExtent()};
    const double aspect_ratio{static_cast<double>(extent.width) / static_cast<double>(extent.height)};
    const double bounds_radius{m_point_cloud_bounds.GetBoundsRadius()};
    const double near_plane{std::max(bounds_radius * 0.0001, 0.001)};
    const double far_plane{std::max(bounds_radius * 20.0, 100.0)};
    m_camera.SetPerspective(glm::radians(static_cast<double>(m_perspective_fov_deg)), aspect_ratio, near_plane,
                            far_plane);
  }

  [[nodiscard]] double GetCameraFitDistance() const noexcept {
    const double half_fov_rad{glm::radians(static_cast<double>(m_perspective_fov_deg)) * 0.5};
    return m_point_cloud_bounds.GetBoundsRadius() / std::sin(half_fov_rad);
  }

  void UpdateCameraAspectRatio() {
    ApplyCameraProjection();
    UpdateOrbitViewportSize();
  }

  void UpdateOrbitViewportSize() {
    const VkExtent2D extent{GetSwapChain().GetExtent()};
    const double width{std::max(static_cast<double>(extent.width), 1.0)};
    const double height{std::max(static_cast<double>(extent.height), 1.0)};
    m_yaw_pitch_orbit_camera_controller.SetViewportSize(width, height);
  }

  void ApplyOrbitDesc(const lvk::camera::OrbitCameraControllerDesc& desc) {
    m_yaw_pitch_orbit_camera_controller.SetDesc(desc);
    UpdateOrbitViewportSize();
  }

  void ResetCameraPose() {
    const glm::dvec3 target{m_point_cloud_bounds.GetBoundsCenter()};
    const double distance{GetCameraFitDistance()};
    const glm::dvec3 view_offset{glm::normalize(glm::dvec3{0.0, 0.35, 1.0}) * distance};
    ApplyCameraProjection();
    m_camera.LookAt(target + view_offset, target);
    m_yaw_pitch_orbit_camera_controller.Focus(target, distance);
  }

  void InitializeImGui() { m_imgui_layer.Initialize({&GetWindow(), &GetContext(), &GetSwapChain()}); }

  void UpdateFrameStats(float delta_seconds) noexcept {
    m_fps_elapsed_seconds += delta_seconds;
    ++m_fps_frame_count;

    if (m_fps_elapsed_seconds >= 1.0F) {
      m_display_fps = static_cast<float>(m_fps_frame_count) / m_fps_elapsed_seconds;
      m_frame_time_ms = 1000.0F / std::max(m_display_fps, 0.001F);
      m_fps_elapsed_seconds = 0.0F;
      m_fps_frame_count = 0;
    }
  }

  void DrawUi() {
    ImGui::SetNextWindowPos(ImVec2{12.0F, 12.0F}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2{160.0F, 0.0F}, ImGuiCond_FirstUseEver);
    ImGui::Begin("Point Cloud");
    lvk::DrawImGuiText(std::format("FPS: {:.1f}", m_display_fps));
    lvk::DrawImGuiText(std::format("Frame: {:.2f} ms", m_frame_time_ms));
    if (ImGui::Button("Reset camera")) {
      ResetCameraPose();
    }
    ImGui::End();
  }

  void CreatePipeline() {
    VkDevice device{GetContext().GetDevice()};
    const std::vector<char> vertex_shader_code{lvk::ReadBinaryFile(PROJECT_SHADER_DIR "10_02_base_vert.spv")};
    const std::vector<char> fragment_shader_code{lvk::ReadBinaryFile(PROJECT_SHADER_DIR "10_02_base_frag.spv")};
    VkShaderModule vertex_shader_module{lvk::CreateShaderModule(device, vertex_shader_code)};
    VkShaderModule fragment_shader_module{lvk::CreateShaderModule(device, fragment_shader_code)};

    try {
      CreatePipelineLayout(device);
      CreateGraphicsPipeline(device, vertex_shader_module, fragment_shader_module);
    } catch (...) {
      DestroyPipeline();
      vkDestroyShaderModule(device, fragment_shader_module, nullptr);
      vkDestroyShaderModule(device, vertex_shader_module, nullptr);
      throw;
    }

    vkDestroyShaderModule(device, fragment_shader_module, nullptr);
    vkDestroyShaderModule(device, vertex_shader_module, nullptr);
  }

  void CreatePipelineLayout(VkDevice device) {
    VkPipelineLayoutCreateInfo pipeline_layout_info{};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = 1;
    pipeline_layout_info.pSetLayouts = &m_descriptor_set_layout;
    lvk::CheckVkResult(vkCreatePipelineLayout(device, &pipeline_layout_info, nullptr, &m_pipeline_layout),
                       "failed to create pipeline layout");
  }

  void CreateGraphicsPipeline(VkDevice device, VkShaderModule vertex_shader_module,
                              VkShaderModule fragment_shader_module) {
    VkPipelineShaderStageCreateInfo vertex_shader_stage_info{};
    vertex_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertex_shader_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertex_shader_stage_info.module = vertex_shader_module;
    vertex_shader_stage_info.pName = "main";

    VkPipelineShaderStageCreateInfo fragment_shader_stage_info{};
    fragment_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragment_shader_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragment_shader_stage_info.module = fragment_shader_module;
    fragment_shader_stage_info.pName = "main";

    const std::array<VkPipelineShaderStageCreateInfo, 2> shader_stages{vertex_shader_stage_info,
                                                                       fragment_shader_stage_info};
    const VkVertexInputBindingDescription binding_description{Vertex::GetBindingDescription()};
    const std::array<VkVertexInputAttributeDescription, 2> attribute_descriptions{Vertex::GetAttributeDescriptions()};

    VkPipelineVertexInputStateCreateInfo vertex_input_info{};
    vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_info.vertexBindingDescriptionCount = 1;
    vertex_input_info.pVertexBindingDescriptions = &binding_description;
    vertex_input_info.vertexAttributeDescriptionCount = static_cast<uint32_t>(attribute_descriptions.size());
    vertex_input_info.pVertexAttributeDescriptions = attribute_descriptions.data();

    VkPipelineInputAssemblyStateCreateInfo input_assembly{};
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;

    VkPipelineViewportStateCreateInfo viewport_state{};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.lineWidth = 1.0F;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depth_stencil{};
    depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil.depthTestEnable = VK_TRUE;
    depth_stencil.depthWriteEnable = VK_TRUE;
    depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;

    VkPipelineColorBlendAttachmentState color_blend_attachment{};
    color_blend_attachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo color_blending{};
    color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blending.attachmentCount = 1;
    color_blending.pAttachments = &color_blend_attachment;

    const std::array<VkDynamicState, 2> dynamic_states{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic_state{};
    dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.dynamicStateCount = static_cast<uint32_t>(dynamic_states.size());
    dynamic_state.pDynamicStates = dynamic_states.data();

    VkGraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = static_cast<uint32_t>(shader_stages.size());
    pipeline_info.pStages = shader_stages.data();
    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pDepthStencilState = &depth_stencil;
    pipeline_info.pColorBlendState = &color_blending;
    pipeline_info.pDynamicState = &dynamic_state;
    pipeline_info.layout = m_pipeline_layout;
    pipeline_info.renderPass = GetSwapChain().GetRenderPass();

    lvk::CheckVkResult(
      vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &m_graphics_pipeline),
      "failed to create graphics pipeline");
  }

  void CreateVertexBuffer() {
    const VkDeviceSize buffer_size{sizeof(m_vertices.front()) * m_vertices.size()};

    BufferResource staging_buffer{};
    try {
      CreateBuffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging_buffer);
      lvk::CheckVkResult(
        vkMapMemory(GetContext().GetDevice(), staging_buffer.memory, 0, buffer_size, 0, &staging_buffer.mapped_memory),
        "failed to map staging buffer memory");
      std::memcpy(staging_buffer.mapped_memory, m_vertices.data(), static_cast<size_t>(buffer_size));

      CreateBuffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_vertex_buffer);
      CopyBuffer(staging_buffer.buffer, m_vertex_buffer.buffer, buffer_size);
    } catch (...) {
      DestroyBuffer(staging_buffer);
      DestroyBuffer(m_vertex_buffer);
      throw;
    }

    DestroyBuffer(staging_buffer);
  }

  void CreateUniformBuffers() {
    m_uniform_buffers.resize(k_max_frames_in_flight);
    for (BufferResource& uniform_buffer : m_uniform_buffers) {
      CreateHostVisibleBuffer(sizeof(UniformBufferObject), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, uniform_buffer);
    }
  }

  void CreateHostVisibleBuffer(VkDeviceSize size, VkBufferUsageFlags usage, BufferResource& resource) {
    CreateBuffer(size, usage, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, resource);
    lvk::CheckVkResult(vkMapMemory(GetContext().GetDevice(), resource.memory, 0, size, 0, &resource.mapped_memory),
                       "failed to map buffer memory");
  }

  void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                    BufferResource& resource) {
    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = size;
    buffer_info.usage = usage;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    lvk::CheckVkResult(vkCreateBuffer(GetContext().GetDevice(), &buffer_info, nullptr, &resource.buffer),
                       "failed to create buffer");

    VkMemoryRequirements memory_requirements{};
    vkGetBufferMemoryRequirements(GetContext().GetDevice(), resource.buffer, &memory_requirements);

    VkMemoryAllocateInfo allocate_info{};
    allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocate_info.allocationSize = memory_requirements.size;
    allocate_info.memoryTypeIndex =
      lvk::FindMemoryType(GetContext().GetPhysicalDevice(), memory_requirements.memoryTypeBits, properties);
    lvk::CheckVkResult(vkAllocateMemory(GetContext().GetDevice(), &allocate_info, nullptr, &resource.memory),
                       "failed to allocate buffer memory");
    lvk::CheckVkResult(vkBindBufferMemory(GetContext().GetDevice(), resource.buffer, resource.memory, 0),
                       "failed to bind buffer memory");
  }

  void CopyBuffer(VkBuffer source_buffer, VkBuffer target_buffer, VkDeviceSize size) {
    VkCommandPool command_pool{VK_NULL_HANDLE};
    VkCommandBuffer command_buffer{VK_NULL_HANDLE};

    try {
      command_pool = CreateTransferCommandPool();
      command_buffer = AllocateTransferCommandBuffer(command_pool);

      VkCommandBufferBeginInfo begin_info{};
      begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
      begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
      lvk::CheckVkResult(vkBeginCommandBuffer(command_buffer, &begin_info), "failed to begin transfer command buffer");

      VkBufferCopy copy_region{};
      copy_region.size = size;
      vkCmdCopyBuffer(command_buffer, source_buffer, target_buffer, 1, &copy_region);

      lvk::CheckVkResult(vkEndCommandBuffer(command_buffer), "failed to end transfer command buffer");

      VkSubmitInfo submit_info{};
      submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
      submit_info.commandBufferCount = 1;
      submit_info.pCommandBuffers = &command_buffer;

      lvk::CheckVkResult(vkQueueSubmit(GetContext().GetGraphicsQueue(), 1, &submit_info, VK_NULL_HANDLE),
                         "failed to submit transfer command buffer");
      lvk::CheckVkResult(vkQueueWaitIdle(GetContext().GetGraphicsQueue()), "failed to wait for transfer queue");
    } catch (...) {
      DestroyTransferResources(command_pool, command_buffer);
      throw;
    }

    DestroyTransferResources(command_pool, command_buffer);
  }

  [[nodiscard]] VkCommandPool CreateTransferCommandPool() {
    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    pool_info.queueFamilyIndex = GetContext().GetGraphicsQueueFamily();
    VkCommandPool command_pool{VK_NULL_HANDLE};
    lvk::CheckVkResult(vkCreateCommandPool(GetContext().GetDevice(), &pool_info, nullptr, &command_pool),
                       "failed to create transfer command pool");
    return command_pool;
  }

  [[nodiscard]] VkCommandBuffer AllocateTransferCommandBuffer(VkCommandPool command_pool) {
    VkCommandBufferAllocateInfo allocate_info{};
    allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocate_info.commandPool = command_pool;
    allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocate_info.commandBufferCount = 1;
    VkCommandBuffer command_buffer{VK_NULL_HANDLE};
    lvk::CheckVkResult(vkAllocateCommandBuffers(GetContext().GetDevice(), &allocate_info, &command_buffer),
                       "failed to allocate transfer command buffer");
    return command_buffer;
  }

  void DestroyTransferResources(VkCommandPool command_pool, VkCommandBuffer command_buffer) noexcept {
    VkDevice device{GetContext().GetDevice()};
    if (VK_NULL_HANDLE != command_buffer) {
      vkFreeCommandBuffers(device, command_pool, 1, &command_buffer);
    }
    if (VK_NULL_HANDLE != command_pool) {
      vkDestroyCommandPool(device, command_pool, nullptr);
    }
  }

  void CreateDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding uniform_binding{};
    uniform_binding.binding = 0;
    uniform_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uniform_binding.descriptorCount = 1;
    uniform_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo layout_info{};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = 1;
    layout_info.pBindings = &uniform_binding;
    lvk::CheckVkResult(
      vkCreateDescriptorSetLayout(GetContext().GetDevice(), &layout_info, nullptr, &m_descriptor_set_layout),
      "failed to create descriptor set layout");
  }

  void CreateDescriptorPool() {
    VkDescriptorPoolSize pool_size{};
    pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    pool_size.descriptorCount = k_max_frames_in_flight;

    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.poolSizeCount = 1;
    pool_info.pPoolSizes = &pool_size;
    pool_info.maxSets = k_max_frames_in_flight;
    lvk::CheckVkResult(vkCreateDescriptorPool(GetContext().GetDevice(), &pool_info, nullptr, &m_descriptor_pool),
                       "failed to create descriptor pool");
  }

  void CreateDescriptorSets() {
    std::array<VkDescriptorSetLayout, k_max_frames_in_flight> layouts{};
    layouts.fill(m_descriptor_set_layout);

    VkDescriptorSetAllocateInfo allocate_info{};
    allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocate_info.descriptorPool = m_descriptor_pool;
    allocate_info.descriptorSetCount = static_cast<uint32_t>(layouts.size());
    allocate_info.pSetLayouts = layouts.data();

    m_descriptor_sets.resize(k_max_frames_in_flight);
    lvk::CheckVkResult(vkAllocateDescriptorSets(GetContext().GetDevice(), &allocate_info, m_descriptor_sets.data()),
                       "failed to allocate descriptor sets");

    for (size_t i{}; i < m_descriptor_sets.size(); ++i) {
      VkDescriptorBufferInfo buffer_info{};
      buffer_info.buffer = m_uniform_buffers.at(i).buffer;
      buffer_info.range = sizeof(UniformBufferObject);

      VkWriteDescriptorSet descriptor_write{};
      descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      descriptor_write.dstSet = m_descriptor_sets.at(i);
      descriptor_write.dstBinding = 0;
      descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
      descriptor_write.descriptorCount = 1;
      descriptor_write.pBufferInfo = &buffer_info;
      vkUpdateDescriptorSets(GetContext().GetDevice(), 1, &descriptor_write, 0, nullptr);
    }
  }

  void UpdateUniformBuffer(uint32_t frame_index) {
    const lvk::camera::GpuCameraMatrices camera_matrices{
      m_camera.GetGpuMatrices(lvk::camera::projection_conventions::k_vulkan)};

    UniformBufferObject ubo{};
    ubo.view = camera_matrices.view;
    ubo.projection = camera_matrices.projection;
    std::memcpy(m_uniform_buffers.at(frame_index).mapped_memory, &ubo, sizeof(ubo));
  }

  void DestroyPipeline() noexcept {
    VkDevice device{GetContext().GetDevice()};
    if (VK_NULL_HANDLE != m_graphics_pipeline) {
      vkDestroyPipeline(device, m_graphics_pipeline, nullptr);
      m_graphics_pipeline = VK_NULL_HANDLE;
    }
    if (VK_NULL_HANDLE != m_pipeline_layout) {
      vkDestroyPipelineLayout(device, m_pipeline_layout, nullptr);
      m_pipeline_layout = VK_NULL_HANDLE;
    }
  }

  void DestroyDescriptorResources() noexcept {
    VkDevice device{GetContext().GetDevice()};
    if (VK_NULL_HANDLE != m_descriptor_pool) {
      vkDestroyDescriptorPool(device, m_descriptor_pool, nullptr);
      m_descriptor_pool = VK_NULL_HANDLE;
    }
    for (BufferResource& uniform_buffer : m_uniform_buffers) {
      DestroyBuffer(uniform_buffer);
    }
    m_uniform_buffers.clear();
    if (VK_NULL_HANDLE != m_descriptor_set_layout) {
      vkDestroyDescriptorSetLayout(device, m_descriptor_set_layout, nullptr);
      m_descriptor_set_layout = VK_NULL_HANDLE;
    }
  }

  void DestroyBuffer(BufferResource& resource) noexcept {
    VkDevice device{GetContext().GetDevice()};
    if (nullptr != resource.mapped_memory) {
      vkUnmapMemory(device, resource.memory);
      resource.mapped_memory = nullptr;
    }
    if (VK_NULL_HANDLE != resource.buffer) {
      vkDestroyBuffer(device, resource.buffer, nullptr);
      resource.buffer = VK_NULL_HANDLE;
    }
    if (VK_NULL_HANDLE != resource.memory) {
      vkFreeMemory(device, resource.memory, nullptr);
      resource.memory = VK_NULL_HANDLE;
    }
  }

private:
  static constexpr uint32_t k_max_frames_in_flight{2};

private:
  lvk::camera::Camera m_camera{};
  lvk::camera::YawPitchOrbitCameraController m_yaw_pitch_orbit_camera_controller{&m_camera};
  lvk::ImGuiLayer m_imgui_layer{};

  std::vector<BufferResource> m_uniform_buffers{};
  VkDescriptorSetLayout m_descriptor_set_layout{VK_NULL_HANDLE};
  VkDescriptorPool m_descriptor_pool{VK_NULL_HANDLE};
  std::vector<VkDescriptorSet> m_descriptor_sets{};
  VkPipelineLayout m_pipeline_layout{VK_NULL_HANDLE};
  VkPipeline m_graphics_pipeline{VK_NULL_HANDLE};

  BufferResource m_vertex_buffer{};
  std::vector<Vertex> m_vertices{};
  uint32_t m_vertex_count{};
  BoundingBox m_point_cloud_bounds{};

  std::array<float, 4> m_clear_color{0.1F, 0.2F, 0.3F, 1.0F};
  float m_perspective_fov_deg{60.0F};
  float m_fps_elapsed_seconds{};
  float m_display_fps{};
  float m_frame_time_ms{};
  uint32_t m_fps_frame_count{};
};
}  // namespace

int main(int argc, char** argv) {
  try {
    if (argc < 2) {
      std::cerr << "Usage: 10_02_point_cloud <file.ply>\n";
      return EXIT_FAILURE;
    }

    std::vector<Vertex> vertices{PlyVertexLoader::Load(argv[1])};
    BoundingBox bounds{BoundingBox::ComputeBoundingBox(vertices)};

    std::clog << "Loaded " << vertices.size() << " vertices from " << argv[1] << '\n';

    PointCloudSample sample(std::move(vertices), bounds);
    return sample.Run();
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return EXIT_FAILURE;
  } catch (...) {
    std::cerr << "Unknown error occurred\n";
    return EXIT_FAILURE;
  }
}
