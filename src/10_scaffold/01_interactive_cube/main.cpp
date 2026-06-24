// clang-format off
#include <imgui.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vulkan/vulkan.h>
// clang-format on

#include <lvk/camera/camera.h>
#include <lvk/camera/fps_camera_controller.h>
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
#include <cstring>
#include <format>
#include <string_view>
#include <vector>

namespace {
enum class CameraControlType { YawPitchOrbit, ArcballOrbit, Fps };

[[nodiscard]] const char* GetCameraControlTypeName(CameraControlType camera_control_type) noexcept {
  switch (camera_control_type) {
    case CameraControlType::YawPitchOrbit:
      return "YawPitch Orbit";
    case CameraControlType::ArcballOrbit:
      return "Arcball Orbit";
    case CameraControlType::Fps:
      return "FPS";
  }
  return "Unknown";
}

[[nodiscard]] glm::dvec3 GetCameraTarget() noexcept { return {0.0, 0.0, 0.0}; }

void DrawImGuiText(std::string_view text) { ImGui::TextUnformatted(text.data(), text.data() + text.size()); }

[[nodiscard]] lvk::camera::CameraControllerInput MapCameraControllerInput(const lvk::InputState& input_state,
                                                                          bool wants_mouse,
                                                                          bool wants_keyboard) noexcept {
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

  if (!wants_keyboard) {
    input.move_forward = input_state.key_w || input_state.key_up;
    input.move_backward = input_state.key_s || input_state.key_down;
    input.move_left = input_state.key_a || input_state.key_left;
    input.move_right = input_state.key_d || input_state.key_right;
    input.move_up = input_state.key_e || input_state.space_down;
    input.move_down = input_state.key_q;
    input.fast = input_state.shift_down;
  }

  return input;
}

struct Vertex {
  glm::vec3 position{};
  glm::vec3 color{};

  static constexpr VkVertexInputBindingDescription GetBindingDescription() noexcept {
    VkVertexInputBindingDescription binding_description{};
    binding_description.binding = 0;
    binding_description.stride = sizeof(Vertex);
    binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return binding_description;
  }

  static constexpr std::array<VkVertexInputAttributeDescription, 2> GetAttributeDescriptions() noexcept {
    std::array<VkVertexInputAttributeDescription, 2> attribute_descriptions{};
    attribute_descriptions[0].binding = 0;
    attribute_descriptions[0].location = 0;
    attribute_descriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attribute_descriptions[0].offset = offsetof(Vertex, position);
    attribute_descriptions[1].binding = 0;
    attribute_descriptions[1].location = 1;
    attribute_descriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attribute_descriptions[1].offset = offsetof(Vertex, color);
    return attribute_descriptions;
  }
};

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

const std::array<Vertex, 8> k_vertices{{
  {{-0.5F, -0.5F, -0.5F}, {0.95F, 0.25F, 0.18F}},
  {{0.5F, -0.5F, -0.5F}, {0.95F, 0.72F, 0.22F}},
  {{0.5F, 0.5F, -0.5F}, {0.28F, 0.82F, 0.45F}},
  {{-0.5F, 0.5F, -0.5F}, {0.15F, 0.58F, 0.95F}},
  {{-0.5F, -0.5F, 0.5F}, {0.62F, 0.35F, 0.95F}},
  {{0.5F, -0.5F, 0.5F}, {0.95F, 0.42F, 0.72F}},
  {{0.5F, 0.5F, 0.5F}, {0.55F, 0.90F, 0.92F}},
  {{-0.5F, 0.5F, 0.5F}, {0.92F, 0.92F, 0.95F}},
}};

constexpr std::array<uint16_t, 36> k_indices{{
  0, 1, 2, 2, 3, 0, 4, 6, 5, 6, 4, 7, 0, 4, 5, 5, 1, 0, 3, 2, 6, 6, 7, 3, 1, 5, 6, 6, 2, 1, 0, 3, 7, 7, 4, 0,
}};

class InteractiveCubeSample final : public lvk::VulkanSample {
protected:
  void Configure(lvk::ApplicationDesc& desc) override {
    desc.title = "Interactive Cube";
    desc.width = 1280;
    desc.height = 720;
  }

  void OnCreate() override {
    InitializeCamera();
    CreateVertexBuffer();
    CreateIndexBuffer();
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
    DestroyBuffer(m_index_buffer);
    DestroyBuffer(m_vertex_buffer);
  }

  void OnUpdate(float delta_seconds, const lvk::InputState& input_state) override {
    lvk::camera::CameraControllerInput camera_input{
      MapCameraControllerInput(input_state, m_imgui_layer.WantsMouse(), m_imgui_layer.WantsKeyboard())};

    if (lvk::camera::ProjectionType::Orthographic == m_projection_type && 0.0 != camera_input.scroll_delta_y) {
      ApplyOrthographicScrollZoom(camera_input.scroll_delta_y);
      camera_input.scroll_delta_y = 0.0;
    }

    UpdateOrbitViewportSize();
    if (CameraControlType::YawPitchOrbit == m_camera_control_type) {
      m_yaw_pitch_orbit_camera_controller.Update(delta_seconds, camera_input);
    } else if (CameraControlType::ArcballOrbit == m_camera_control_type) {
      m_arcball_orbit_camera_controller.Update(delta_seconds, camera_input);
    } else {
      m_fps_camera_controller.Update(delta_seconds, camera_input);
    }

    if (m_enable_rotation) {
      m_rotation_rad += delta_seconds * m_rotation_speed;
    }
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
    vkCmdBindIndexBuffer(frame_context.command_buffer, m_index_buffer.buffer, 0, VK_INDEX_TYPE_UINT16);
    vkCmdBindDescriptorSets(frame_context.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline_layout, 0, 1,
                            &m_descriptor_sets.at(frame_context.frame_index), 0, nullptr);
    vkCmdDrawIndexed(frame_context.command_buffer, static_cast<uint32_t>(k_indices.size()), 1, 0, 0, 0);

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
  void InitializeCamera() {
    ResetCameraPose();

    m_yaw_pitch_orbit_camera_controller.Attach(&m_camera);
    m_arcball_orbit_camera_controller.Attach(&m_camera);
    lvk::camera::OrbitCameraControllerDesc desc{};
    desc.distance = 4.5;
    desc.min_distance = 1.0;
    desc.max_distance = 50.0;
    ApplyOrbitDesc(desc);
    m_yaw_pitch_orbit_camera_controller.Focus(GetCameraTarget(), 4.5);
    m_arcball_orbit_camera_controller.SyncFromCameraAndTarget();

    m_fps_camera_controller.Attach(&m_camera);
    lvk::camera::FpsCameraControllerDesc fps_desc{};
    fps_desc.move_speed = 3.0;
    fps_desc.fast_move_multiplier = 4.0;
    fps_desc.look_sensitivity = 0.005;
    m_fps_camera_controller.SetDesc(fps_desc);
    m_fps_camera_controller.SyncFromCamera();
  }

  void ApplyCameraProjection() {
    const VkExtent2D extent{GetSwapChain().GetExtent()};
    const double aspect_ratio{static_cast<double>(extent.width) / static_cast<double>(extent.height)};
    if (lvk::camera::ProjectionType::Perspective == m_projection_type) {
      m_camera.SetPerspective(glm::radians(static_cast<double>(m_perspective_fov_deg)), aspect_ratio, 0.01, 100.0);
    } else {
      m_camera.SetOrthographic(static_cast<double>(m_orthographic_height), aspect_ratio, 0.01, 100.0);
    }
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
    m_arcball_orbit_camera_controller.SetViewportSize(width, height);
  }

  void ApplyOrbitDesc(const lvk::camera::OrbitCameraControllerDesc& desc) {
    m_yaw_pitch_orbit_camera_controller.SetDesc(desc);
    m_arcball_orbit_camera_controller.SetDesc(desc);
    UpdateOrbitViewportSize();
  }

  void ResetCameraPose() {
    ApplyCameraProjection();
    m_camera.LookAt({0.0, 1.5, 4.0}, GetCameraTarget());
    m_yaw_pitch_orbit_camera_controller.Focus(GetCameraTarget(), 4.5);
    m_arcball_orbit_camera_controller.SyncFromCameraAndTarget();
    m_fps_camera_controller.SyncFromCamera();
  }

  void ToggleProjectionType() {
    if (lvk::camera::ProjectionType::Perspective == m_projection_type) {
      MatchOrthographicHeightToPerspective();
      m_projection_type = lvk::camera::ProjectionType::Orthographic;
    } else {
      m_projection_type = lvk::camera::ProjectionType::Perspective;
    }
    ApplyCameraProjection();
  }

  void MatchOrthographicHeightToPerspective() {
    const double distance{GetCameraTargetDistance()};
    const double vertical_fov_rad{glm::radians(static_cast<double>(m_perspective_fov_deg))};
    m_orthographic_height =
      static_cast<float>(std::clamp(2.0 * distance * std::tan(vertical_fov_rad * 0.5), 1.0, 20.0));
  }

  [[nodiscard]] double GetCameraTargetDistance() const {
    const double distance{glm::length(m_camera.GetPosition() - GetCameraTarget())};
    return std::max(distance, 0.01);
  }

  void ApplyOrthographicScrollZoom(double scroll_delta_y) {
    const lvk::camera::OrbitCameraControllerDesc& desc{m_yaw_pitch_orbit_camera_controller.GetDesc()};
    const double factor{std::pow(1.0 - desc.dolly_sensitivity, scroll_delta_y)};
    m_orthographic_height =
      static_cast<float>(std::clamp(static_cast<double>(m_orthographic_height) * factor, 1.0, 20.0));
    ApplyCameraProjection();
  }

  void ToggleCameraControlType() {
    if (CameraControlType::YawPitchOrbit == m_camera_control_type) {
      m_arcball_orbit_camera_controller.SetDesc(m_yaw_pitch_orbit_camera_controller.GetDesc());
      m_arcball_orbit_camera_controller.SyncFromCameraAndTarget();
      m_camera_control_type = CameraControlType::ArcballOrbit;
    } else if (CameraControlType::ArcballOrbit == m_camera_control_type) {
      m_camera_control_type = CameraControlType::Fps;
      m_fps_camera_controller.SyncFromCamera();
    } else {
      m_yaw_pitch_orbit_camera_controller.SetDesc(m_arcball_orbit_camera_controller.GetDesc());
      m_camera_control_type = CameraControlType::YawPitchOrbit;
      m_yaw_pitch_orbit_camera_controller.SyncFromCameraAndTarget();
    }
  }

  void InitializeImGui() { m_imgui_layer.Initialize({&GetWindow(), &GetContext(), &GetSwapChain()}); }

  void DrawUi() {
    ImGui::SetNextWindowPos(ImVec2{12.0F, 12.0F}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2{320.0F, 0.0F}, ImGuiCond_FirstUseEver);
    ImGui::Begin("Interactive Cube");
    ImGui::Checkbox("Rotate", &m_enable_rotation);
    ImGui::SliderFloat("Rotation speed", &m_rotation_speed, 0.0F, 4.0F);
    ImGui::ColorEdit3("Clear color", m_clear_color.data());
    if (lvk::camera::ProjectionType::Perspective == m_projection_type) {
      if (ImGui::Button("Switch to orthographic")) {
        ToggleProjectionType();
      }
      if (ImGui::SliderFloat("Vertical FOV", &m_perspective_fov_deg, 20.0F, 100.0F)) {
        ApplyCameraProjection();
      }
    } else {
      if (ImGui::Button("Switch to perspective")) {
        ToggleProjectionType();
      }
      if (ImGui::SliderFloat("Orthographic height", &m_orthographic_height, 1.0F, 20.0F)) {
        ApplyCameraProjection();
      }
    }
    if (CameraControlType::YawPitchOrbit == m_camera_control_type) {
      if (ImGui::Button("Switch to arcball orbit controls")) {
        ToggleCameraControlType();
      }
    } else if (CameraControlType::ArcballOrbit == m_camera_control_type) {
      if (ImGui::Button("Switch to FPS controls")) {
        ToggleCameraControlType();
      }
    } else if (ImGui::Button("Switch to yaw-pitch orbit controls")) {
      ToggleCameraControlType();
    }
    if (ImGui::Button("Reset camera")) {
      ResetCameraPose();
    }
    DrawCameraDebugUi();
    ImGui::Separator();
    if (CameraControlType::Fps != m_camera_control_type) {
      ImGui::TextUnformatted("Left drag: orbit");
      ImGui::TextUnformatted("Middle/right drag: pan");
      if (lvk::camera::ProjectionType::Orthographic == m_projection_type) {
        ImGui::TextUnformatted("Wheel: zoom");
      } else {
        ImGui::TextUnformatted("Wheel: dolly");
      }
    } else {
      ImGui::TextUnformatted("Right drag: look");
      ImGui::TextUnformatted("WASD/arrows: move");
      ImGui::TextUnformatted("Q/E or Space: vertical");
      ImGui::TextUnformatted("Shift: fast move");
    }
    ImGui::End();
  }

  void DrawCameraDebugUi() {
    const glm::dvec3 position{m_camera.GetPosition()};
    const glm::dvec3 forward{m_camera.GetForward()};
    const glm::dvec3 right{m_camera.GetRight()};
    const glm::dvec3 up{m_camera.GetUp()};
    const lvk::camera::OrbitCameraControllerDesc& orbit_desc{GetActiveOrbitDesc()};

    ImGui::Separator();
    DrawImGuiText(std::format("Camera mode: {}", GetCameraControlTypeName(m_camera_control_type)));
    DrawImGuiText(std::format("Projection: {}", lvk::camera::ProjectionType::Perspective == m_projection_type
                                                  ? "Perspective"
                                                  : "Orthographic"));
    DrawImGuiText(std::format("Position: {:.3f}, {:.3f}, {:.3f}", position.x, position.y, position.z));
    DrawImGuiText(std::format("Forward: {:.3f}, {:.3f}, {:.3f}", forward.x, forward.y, forward.z));
    DrawImGuiText(std::format("Right: {:.3f}, {:.3f}, {:.3f}", right.x, right.y, right.z));
    DrawImGuiText(std::format("Up: {:.3f}, {:.3f}, {:.3f}", up.x, up.y, up.z));
    DrawImGuiText(std::format("Orbit target: {:.3f}, {:.3f}, {:.3f}", orbit_desc.target.x, orbit_desc.target.y,
                              orbit_desc.target.z));
    DrawImGuiText(std::format("Orbit distance: {:.3f}", orbit_desc.distance));
    DrawImGuiText(std::format("Vertical FOV: {:.1f} deg", m_perspective_fov_deg));
  }

  [[nodiscard]] const lvk::camera::OrbitCameraControllerDesc& GetActiveOrbitDesc() const {
    if (CameraControlType::ArcballOrbit == m_camera_control_type) {
      return m_arcball_orbit_camera_controller.GetDesc();
    }
    return m_yaw_pitch_orbit_camera_controller.GetDesc();
  }

  void CreatePipeline() {
    VkDevice device{GetContext().GetDevice()};
    const std::vector<char> vertex_shader_code{lvk::ReadBinaryFile(PROJECT_SHADER_DIR "10_01_base_vert.spv")};
    const std::vector<char> fragment_shader_code{lvk::ReadBinaryFile(PROJECT_SHADER_DIR "10_01_base_frag.spv")};
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
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

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
    const VkDeviceSize buffer_size{sizeof(k_vertices.front()) * k_vertices.size()};
    CreateHostVisibleBuffer(buffer_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, m_vertex_buffer);
    std::memcpy(m_vertex_buffer.mapped_memory, k_vertices.data(), static_cast<size_t>(buffer_size));
  }

  void CreateIndexBuffer() {
    const VkDeviceSize buffer_size{sizeof(k_indices.front()) * k_indices.size()};
    CreateHostVisibleBuffer(buffer_size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, m_index_buffer);
    std::memcpy(m_index_buffer.mapped_memory, k_indices.data(), static_cast<size_t>(buffer_size));
  }

  void CreateUniformBuffers() {
    m_uniform_buffers.resize(k_max_frames_in_flight);
    for (BufferResource& uniform_buffer : m_uniform_buffers) {
      CreateHostVisibleBuffer(sizeof(UniformBufferObject), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, uniform_buffer);
    }
  }

  void CreateHostVisibleBuffer(VkDeviceSize size, VkBufferUsageFlags usage, BufferResource& resource) {
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
      lvk::FindMemoryType(GetContext().GetPhysicalDevice(), memory_requirements.memoryTypeBits,
                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    lvk::CheckVkResult(vkAllocateMemory(GetContext().GetDevice(), &allocate_info, nullptr, &resource.memory),
                       "failed to allocate buffer memory");
    lvk::CheckVkResult(vkBindBufferMemory(GetContext().GetDevice(), resource.buffer, resource.memory, 0),
                       "failed to bind buffer memory");
    lvk::CheckVkResult(vkMapMemory(GetContext().GetDevice(), resource.memory, 0, size, 0, &resource.mapped_memory),
                       "failed to map buffer memory");
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
    ubo.model = glm::rotate(glm::mat4{1.0F}, m_rotation_rad, glm::vec3{0.0F, 1.0F, 0.0F});
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
  lvk::camera::ArcballOrbitCameraController m_arcball_orbit_camera_controller{&m_camera};
  lvk::camera::FpsCameraController m_fps_camera_controller{&m_camera};
  lvk::ImGuiLayer m_imgui_layer{};

  BufferResource m_vertex_buffer{};
  BufferResource m_index_buffer{};
  std::vector<BufferResource> m_uniform_buffers{};
  VkDescriptorSetLayout m_descriptor_set_layout{VK_NULL_HANDLE};
  VkDescriptorPool m_descriptor_pool{VK_NULL_HANDLE};
  std::vector<VkDescriptorSet> m_descriptor_sets{};
  VkPipelineLayout m_pipeline_layout{VK_NULL_HANDLE};
  VkPipeline m_graphics_pipeline{VK_NULL_HANDLE};

  std::array<float, 4> m_clear_color{0.08F, 0.10F, 0.13F, 1.0F};
  float m_rotation_rad{};
  float m_rotation_speed{0.8F};
  float m_perspective_fov_deg{60.0F};
  float m_orthographic_height{5.0F};
  lvk::camera::ProjectionType m_projection_type{lvk::camera::ProjectionType::Perspective};
  CameraControlType m_camera_control_type{CameraControlType::YawPitchOrbit};
  bool m_enable_rotation{true};
};
}  // namespace

int main() {
  InteractiveCubeSample sample{};
  return sample.Run();
}
