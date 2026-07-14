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

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <format>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

#include "bounding_box.h"
#include "point_cloud_vertex.h"
#include "potree_lod_selector.h"
#include "potree_point_cloud.h"
#include "potree_vertex_decoder.h"

namespace {
using BoundingBox = lvk::point_cloud::BoundingBox;
using Vertex = lvk::point_cloud::PointCloudVertex;
using PotreeNodeIndex = lvk::point_cloud::PotreeNodeIndex;
using PotreePointCloud = lvk::point_cloud::PotreePointCloud;

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

  struct PointDecodePushConstant {
    glm::vec4 offset{};
    glm::vec4 scale{1.0F};
  };
  static_assert(sizeof(PointDecodePushConstant) == sizeof(float) * 8U);

  struct CameraClipPlanes {
    double near_plane{};
    double far_plane{};
  };

  struct PointCloudDrawable {
    BufferResource vertex_buffer{};
    uint32_t vertex_count{};
    bool is_resident{false};
  };

public:
  explicit PointCloudSample(PotreePointCloud point_cloud)
      : m_point_cloud{std::move(point_cloud)}, m_point_cloud_bounds{m_point_cloud.GetMetadata().bounds} {
    const lvk::point_cloud::PotreeMetadataInfo& metadata{m_point_cloud.GetMetadata()};
    m_point_decode_push_constant.offset = {
      static_cast<float>(metadata.offset.x),
      static_cast<float>(metadata.offset.y),
      static_cast<float>(metadata.offset.z),
      0.0F,
    };
    m_point_decode_push_constant.scale = {
      static_cast<float>(metadata.scale.x),
      static_cast<float>(metadata.scale.y),
      static_cast<float>(metadata.scale.z),
      1.0F,
    };
    m_drawables.resize(m_point_cloud.GetNodes().size());
  }

protected:
  void Configure(lvk::ApplicationDesc& desc) override {
    desc.title = "Point Cloud";
    desc.width = 800;
    desc.height = 600;
  }

  void OnCreate() override {
    InitializeCamera();
    const PotreeNodeIndex root_node_index{m_point_cloud.GetRootNodeIndex()};
    CreateNodeDrawable(root_node_index);
    m_rendered_node_indices.push_back(root_node_index);
    UpdateRenderedStats();
    UpdateLodSelection();
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
    DestroyVertexBuffers();
  }

  void OnUpdate(float delta_seconds, const lvk::InputState& input_state) override {
    const lvk::camera::CameraControllerInput camera_input{
      MapCameraControllerInput(input_state, m_imgui_layer.WantsMouse())};

    UpdateFrameStats(delta_seconds);
    UpdateOrbitViewportSize();
    m_yaw_pitch_orbit_camera_controller.Update(delta_seconds, camera_input);
    ApplyCameraProjection();
    UpdateLodSelection();
    LoadSelectedNodeDrawables();
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

    const std::array<VkDeviceSize, 1> offsets{0};

    vkCmdBindPipeline(frame_context.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphics_pipeline);
    vkCmdSetViewport(frame_context.command_buffer, 0, 1, &viewport);
    vkCmdSetScissor(frame_context.command_buffer, 0, 1, &scissor);
    vkCmdBindDescriptorSets(frame_context.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline_layout, 0, 1,
                            &m_descriptor_sets.at(frame_context.frame_index), 0, nullptr);

    vkCmdPushConstants(frame_context.command_buffer, m_pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                       sizeof(PointDecodePushConstant), &m_point_decode_push_constant);

    for (const PotreeNodeIndex node_index : m_rendered_node_indices) {
      const PointCloudDrawable& drawable{m_drawables.at(node_index)};
      if (!drawable.is_resident || drawable.vertex_count == 0) {
        continue;
      }
      const std::array<VkBuffer, 1> vertex_buffers{drawable.vertex_buffer.buffer};
      vkCmdBindVertexBuffers(frame_context.command_buffer, 0, static_cast<uint32_t>(vertex_buffers.size()),
                             vertex_buffers.data(), offsets.data());
      vkCmdDraw(frame_context.command_buffer, drawable.vertex_count, 1, 0, 0);
    }

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
    const double width{std::max(static_cast<double>(extent.width), 1.0)};
    const double height{std::max(static_cast<double>(extent.height), 1.0)};
    const CameraClipPlanes clip_planes{ComputeCameraClipPlanes()};
    m_camera.SetPerspective(glm::radians(static_cast<double>(m_perspective_fov_deg)), width / height,
                            clip_planes.near_plane, clip_planes.far_plane);
  }

  [[nodiscard]] CameraClipPlanes ComputeCameraClipPlanes() const noexcept {
    const glm::dvec3 bounds_min{m_point_cloud_bounds.GetMin()};
    const glm::dvec3 bounds_max{m_point_cloud_bounds.GetMax()};
    const glm::dvec3 camera_position{m_camera.GetPosition()};
    const glm::dvec3 camera_forward{m_camera.GetForward()};

    double nearest_depth{std::numeric_limits<double>::max()};
    double farthest_depth{std::numeric_limits<double>::lowest()};
    for (std::size_t x{}; x < 2; ++x) {
      for (std::size_t y{}; y < 2; ++y) {
        for (std::size_t z{}; z < 2; ++z) {
          const glm::dvec3 corner{
            x == 0 ? bounds_min.x : bounds_max.x,
            y == 0 ? bounds_min.y : bounds_max.y,
            z == 0 ? bounds_min.z : bounds_max.z,
          };
          const double depth{glm::dot(corner - camera_position, camera_forward)};
          nearest_depth = std::min(nearest_depth, depth);
          farthest_depth = std::max(farthest_depth, depth);
        }
      }
    }

    const double bounds_radius{m_point_cloud_bounds.GetBoundsRadius()};
    const double minimum_near_plane{std::max(bounds_radius * 0.0001, 0.001)};
    const double safety_margin{std::max(bounds_radius * 0.05, minimum_near_plane)};
    const double near_plane{std::max(nearest_depth - safety_margin, minimum_near_plane)};
    const double far_plane{std::max(farthest_depth + safety_margin, near_plane + minimum_near_plane)};
    return {near_plane, far_plane};
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
    m_camera.LookAt(target + view_offset, target);
    m_yaw_pitch_orbit_camera_controller.Focus(target, distance);
    ApplyCameraProjection();
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
    ImGui::SetNextWindowSize(ImVec2{240.0F, 0.0F}, ImGuiCond_FirstUseEver);
    ImGui::Begin("Point Cloud");
    lvk::DrawImGuiText(std::format("FPS: {:.1f}", m_display_fps));
    lvk::DrawImGuiText(std::format("Frame: {:.2f} ms", m_frame_time_ms));
    lvk::DrawImGuiText(
      std::format("Selected: {} nodes / {} points", m_selected_node_indices.size(), m_selected_point_count));
    lvk::DrawImGuiText(std::format("Rendered: {} nodes / {} points", m_rendered_node_count, m_rendered_point_count));
    lvk::DrawImGuiText(std::format("Resident: {} nodes / {} points", m_resident_node_count, m_resident_point_count));
    ImGui::SliderFloat("LOD spacing", &m_target_pixel_spacing, 0.1F, 4.0F, "%.1f px");
    ImGui::SliderInt("Point budget", &m_point_budget, 100'000, 10'000'000, "%d");
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
    VkPushConstantRange push_constant_range{};
    push_constant_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    push_constant_range.offset = 0;
    push_constant_range.size = sizeof(PointDecodePushConstant);

    VkPipelineLayoutCreateInfo pipeline_layout_info{};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = 1;
    pipeline_layout_info.pSetLayouts = &m_descriptor_set_layout;
    pipeline_layout_info.pushConstantRangeCount = 1;
    pipeline_layout_info.pPushConstantRanges = &push_constant_range;
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

  void UpdateLodSelection() {
    const VkExtent2D extent{GetSwapChain().GetExtent()};
    const double viewport_height_pixels{std::max(static_cast<double>(extent.height), 1.0)};
    const glm::dmat4 view{m_camera.GetViewMatrix()};
    const glm::dmat4 projection{m_camera.GetProjectionMatrix(lvk::camera::projection_conventions::k_vulkan)};

    lvk::point_cloud::PotreeLodSelectionParams params{};
    params.view_projection = projection * view;
    params.camera_position = m_camera.GetPosition();
    params.focal_length_pixels = std::abs(projection[1][1]) * viewport_height_pixels * 0.5;
    params.target_pixel_spacing = static_cast<double>(m_target_pixel_spacing);
    params.point_budget = static_cast<std::uint64_t>(m_point_budget);

    lvk::point_cloud::PotreeLodSelectionInfo selection{lvk::point_cloud::SelectPotreeLod(m_point_cloud, params)};
    m_selected_node_indices = std::move(selection.node_indices);
    m_selected_point_count = selection.point_count;
    CommitLodSelectionIfResident();
  }

  void LoadSelectedNodeDrawables() {
    std::size_t loaded_node_count{};
    std::uint64_t uploaded_byte_count{};

    for (const PotreeNodeIndex node_index : m_selected_node_indices) {
      if (m_drawables.at(node_index).is_resident) {
        continue;
      }

      const lvk::point_cloud::PotreeNodeInfo& node{m_point_cloud.GetNode(node_index)};
      const std::uint64_t node_byte_count{static_cast<std::uint64_t>(node.point_count) * sizeof(Vertex)};
      if (loaded_node_count >= k_max_node_loads_per_frame) {
        break;
      }
      const bool has_upload_budget = uploaded_byte_count < k_max_upload_bytes_per_frame &&
                                     node_byte_count <= k_max_upload_bytes_per_frame - uploaded_byte_count;
      if (loaded_node_count > 0 && !has_upload_budget) {
        break;
      }

      CreateNodeDrawable(node_index);
      ++loaded_node_count;
      uploaded_byte_count += node_byte_count;
    }

    CommitLodSelectionIfResident();
  }

  void CreateNodeDrawable(PotreeNodeIndex node_index) {
    PointCloudDrawable& drawable{m_drawables.at(node_index)};
    if (drawable.is_resident) {
      return;
    }

    BufferResource staging_buffer{};
    try {
      const std::vector<Vertex> vertices{lvk::point_cloud::LoadPotreeNodeVertices(m_point_cloud, node_index)};
      if (vertices.size() > std::numeric_limits<uint32_t>::max()) {
        throw std::runtime_error{"point cloud node contains too many vertices"};
      }

      drawable.vertex_count = static_cast<uint32_t>(vertices.size());
      if (!vertices.empty()) {
        const VkDeviceSize buffer_size{sizeof(Vertex) * vertices.size()};
        CreateBuffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging_buffer);
        lvk::CheckVkResult(vkMapMemory(GetContext().GetDevice(), staging_buffer.memory, 0, buffer_size, 0,
                                       &staging_buffer.mapped_memory),
                           "failed to map staging buffer memory");
        std::memcpy(staging_buffer.mapped_memory, vertices.data(), static_cast<std::size_t>(buffer_size));

        CreateBuffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, drawable.vertex_buffer);
        CopyBuffer(staging_buffer.buffer, drawable.vertex_buffer.buffer, buffer_size);
      }

      drawable.is_resident = true;
    } catch (...) {
      DestroyBuffer(staging_buffer);
      DestroyBuffer(drawable.vertex_buffer);
      drawable.vertex_count = 0;
      m_point_cloud.UnloadNodeData(node_index);
      throw;
    }

    DestroyBuffer(staging_buffer);
    m_point_cloud.UnloadNodeData(node_index);
    ++m_resident_node_count;
    m_resident_point_count += drawable.vertex_count;
  }

  void CommitLodSelectionIfResident() {
    const bool is_selection_resident =
      std::all_of(m_selected_node_indices.begin(), m_selected_node_indices.end(),
                  [this](PotreeNodeIndex node_index) { return m_drawables[node_index].is_resident; });
    if (!is_selection_resident) {
      return;
    }

    m_rendered_node_indices = m_selected_node_indices;
    UpdateRenderedStats();
  }

  void UpdateRenderedStats() noexcept {
    m_rendered_node_count = 0;
    m_rendered_point_count = 0;
    for (const PotreeNodeIndex node_index : m_rendered_node_indices) {
      const PointCloudDrawable& drawable{m_drawables[node_index]};
      if (!drawable.is_resident) {
        continue;
      }
      ++m_rendered_node_count;
      m_rendered_point_count += drawable.vertex_count;
    }
  }

  void DestroyVertexBuffers() noexcept {
    for (PointCloudDrawable& drawable : m_drawables) {
      DestroyBuffer(drawable.vertex_buffer);
    }
    m_drawables.clear();
    m_selected_node_indices.clear();
    m_rendered_node_indices.clear();
    m_resident_node_count = 0;
    m_resident_point_count = 0;
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
  static constexpr std::uint32_t k_max_frames_in_flight{2};
  static constexpr std::size_t k_max_node_loads_per_frame{2};
  static constexpr std::uint64_t k_max_upload_bytes_per_frame{32ULL * 1024ULL * 1024ULL};

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

  PotreePointCloud m_point_cloud;
  std::vector<PointCloudDrawable> m_drawables{};
  std::vector<PotreeNodeIndex> m_selected_node_indices{};
  std::vector<PotreeNodeIndex> m_rendered_node_indices{};
  PointDecodePushConstant m_point_decode_push_constant{};
  BoundingBox m_point_cloud_bounds{};

  std::array<float, 4> m_clear_color{0.1F, 0.2F, 0.3F, 1.0F};
  float m_perspective_fov_deg{60.0F};
  float m_target_pixel_spacing{1.0F};
  float m_fps_elapsed_seconds{};
  float m_display_fps{};
  float m_frame_time_ms{};
  int m_point_budget{2'000'000};
  std::uint64_t m_selected_point_count{};
  std::uint64_t m_rendered_point_count{};
  std::uint64_t m_resident_point_count{};
  std::size_t m_rendered_node_count{};
  std::size_t m_resident_node_count{};
  std::uint32_t m_fps_frame_count{};
};
}  // namespace

int main(int argc, char* argv[]) {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " <potree_point_cloud_directory>\n";
    return EXIT_FAILURE;
  }

  const std::filesystem::path point_cloud_directory{argv[1]};

  try {
    PotreePointCloud point_cloud{point_cloud_directory};
    std::clog << "Opened Potree point cloud with " << point_cloud.GetNodes().size() << " nodes\n";

    PointCloudSample sample{std::move(point_cloud)};
    return sample.Run();
  } catch (const std::exception& exception) {
    std::cerr << exception.what() << '\n';
    return EXIT_FAILURE;
  } catch (...) {
    std::cerr << "Unknown error occurred\n";
    return EXIT_FAILURE;
  }
}
