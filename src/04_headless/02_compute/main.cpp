
#include <vulkan/vulkan.h>

// clang-format off
#define GLM_FORCE_RADIANS            // GLM 函数的参数使用弧度
#define GLM_FORCE_DEPTH_ZERO_TO_ONE  // 透视矩阵深度值范围 [-1, 1] => [0, 1]
#define GLM_ENABLE_EXPERIMENTAL      // 允许使用 gtx 目录下的实验性扩展头文件
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
// clang-format on

#include <algorithm>
#include <array>
#include <bit>
#include <fstream>
#include <iostream>
#include <numbers>
#include <optional>
#include <set>
#include <stdexcept>
#include <vector>

namespace lvk_tidy {
template <typename T>
[[nodiscard]] T& GetRequiredValue(std::optional<T>& value) {
  if (!value.has_value()) {
    throw std::bad_optional_access{};
  }

  return *value;
}

template <typename T>
[[nodiscard]] const T& GetRequiredValue(const std::optional<T>& value) {
  if (!value.has_value()) {
    throw std::bad_optional_access{};
  }

  return *value;
}

template <typename T>
[[nodiscard]] T LoadInstanceProcAddress(VkInstance instance, const char* name) noexcept {
  return std::bit_cast<T>(vkGetInstanceProcAddr(instance, name));
}

template <typename T>
[[nodiscard]] T LoadDeviceProcAddress(VkDevice device, const char* name) noexcept {
  return std::bit_cast<T>(vkGetDeviceProcAddr(device, name));
}
}  // namespace lvk_tidy

constexpr size_t k_max_frames_in_flight{2};

// 需要开启的校验层的名称
const std::vector<const char*> k_validation_layers = {"VK_LAYER_KHRONOS_validation"};

// 是否启用校验层
#ifdef NDEBUG
const bool k_enable_validation_layers = false;
#else
const bool k_enable_validation_layers = true;
#endif  // NDEBUG

struct UBOCompute {
  uint32_t a{};
  uint32_t b{};
  uint32_t c{};
  uint32_t d{};
};

struct BUFCompute {
  uint32_t result{};
};

/// @brief 支持图形、计算、传输、呈现的队列族
struct QueueFamilyIndices {
  std::optional<uint32_t> graphicsFamily{};
  std::optional<uint32_t> presentFamily{};
  std::optional<uint32_t> computeFamily{};
  std::optional<uint32_t> transferFamily{};

  constexpr bool IsComplete() const noexcept { return computeFamily.has_value() && transferFamily.has_value(); }
};

class HelloTriangleApplication {
public:
  void Run() {
    InitVulkan();
    MainLoop();
    Cleanup();
  }

private:
  void InitVulkan() {
    CreateInstance();
    SetupDebugCallback();
    PickPhysicalDevice();
    CreateLogicalDevice();
    CreateComputeDescriptorSetLayout();
    CreateComputePipeline();
    CreateCommandPool();
    CreateComputeUniformBuffers();
    CreateDescriptorPool();
    CreateComputeDescriptorSets();
    CreateComputeCommandBuffers();
    CreateComputeSyncObjects();
  }

  void MainLoop() {
    static int s_count = 10;
    while (s_count-- > 0) {
      DrawFrame();
    }

    // 等待逻辑设备的操作结束执行
    // DrawFrame 函数中的操作是异步执行的，关闭窗口跳出while循环时，绘制操作和呈现操作可能仍在执行，不能进行清除操作
    vkDeviceWaitIdle(m_device);
  }

  void Cleanup() noexcept {
    vkDestroyPipeline(m_device, m_compute_pipeline, nullptr);
    vkDestroyPipelineLayout(m_device, m_compute_pipeline_layout, nullptr);

    for (size_t i = 0; i < k_max_frames_in_flight; ++i) {
      vkDestroyBuffer(m_device, m_compute_ubo_buffers.at(i), nullptr);
      vkFreeMemory(m_device, m_compute_ubo_buffers_memory.at(i), nullptr);

      vkDestroyBuffer(m_device, m_compute_result_buffers.at(i), nullptr);
      vkFreeMemory(m_device, m_compute_result_buffers_memory.at(i), nullptr);
    }

    vkDestroyDescriptorPool(m_device, m_descriptor_pool, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_compute_descriptor_set_layout, nullptr);

    for (size_t i = 0; i < k_max_frames_in_flight; ++i) {
      vkDestroyFence(m_device, m_compute_in_flight_fences.at(i), nullptr);
    }

    vkDestroyCommandPool(m_device, m_compute_command_pool, nullptr);
    if (m_queue_family_indices.computeFamily != m_queue_family_indices.transferFamily) {
      vkDestroyCommandPool(m_device, m_transfer_command_pool, nullptr);
    }

    vkDestroyDevice(m_device, nullptr);

    if (k_enable_validation_layers) {
      DestroyDebugUtilsMessengerEXT(m_instance, m_debug_messenger, nullptr);
    }

    vkDestroyInstance(m_instance, nullptr);
  }

private:
  void CreateComputeCommandBuffers() {
    m_compute_command_buffers.resize(k_max_frames_in_flight);

    VkCommandBufferAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = m_compute_command_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;  // 指定是主要还是辅助指令缓冲对象
    alloc_info.commandBufferCount = static_cast<uint32_t>(m_compute_command_buffers.size());

    if (VK_SUCCESS != vkAllocateCommandBuffers(m_device, &alloc_info, m_compute_command_buffers.data())) {
      throw std::runtime_error("failed to allocate command buffers");
    }
  }

  void CreateComputeUniformBuffers() {
    // 输入
    VkDeviceSize compute_ubo_buffer_size = sizeof(UBOCompute);

    m_compute_ubo_buffers.resize(k_max_frames_in_flight);
    m_compute_ubo_buffers_memory.resize(k_max_frames_in_flight);
    m_compute_ubo_buffers_mapped.resize(k_max_frames_in_flight);

    for (size_t i = 0; i < k_max_frames_in_flight; ++i) {
      CreateBuffer(compute_ubo_buffer_size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                   m_compute_ubo_buffers.at(i), m_compute_ubo_buffers_memory.at(i));
      vkMapMemory(m_device, m_compute_ubo_buffers_memory.at(i), 0, compute_ubo_buffer_size, 0,
                  &m_compute_ubo_buffers_mapped.at(i));
    }

    // 输出
    VkDeviceSize indirect_draw_buffer_size = sizeof(BUFCompute);

    m_compute_result_buffers.resize(k_max_frames_in_flight);
    m_compute_result_buffers_memory.resize(k_max_frames_in_flight);
    m_compute_result_buffers_mapped.resize(k_max_frames_in_flight);

    for (size_t i = 0; i < k_max_frames_in_flight; ++i) {
      CreateBuffer(indirect_draw_buffer_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                   m_compute_result_buffers.at(i), m_compute_result_buffers_memory.at(i));
      vkMapMemory(m_device, m_compute_result_buffers_memory.at(i), 0, indirect_draw_buffer_size, 0,
                  &m_compute_result_buffers_mapped.at(i));
    }
  }

  void UpdateComputeUniformBuffer(size_t current_image) {
    static uint32_t s_n{0};
    s_n++;

    UBOCompute ubo{s_n, s_n, s_n, s_n};
    std::memcpy(m_compute_ubo_buffers_mapped[current_image], &ubo, sizeof(ubo));
  }

  void CreateComputeDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding ubo = {};
    ubo.binding = 0;                                         // 绑定点
    ubo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;  // 描述符类型
    ubo.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;            // 指定在哪一个着色器阶段使用
    ubo.descriptorCount = 1;
    ubo.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutBinding buf = {};
    buf.binding = 1;
    buf.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    buf.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    buf.descriptorCount = 1;
    buf.pImmutableSamplers = nullptr;

    std::array bindings = {ubo, buf};

    VkDescriptorSetLayoutCreateInfo layout_info = {};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = static_cast<uint32_t>(bindings.size());
    layout_info.pBindings = bindings.data();

    if (VK_SUCCESS != vkCreateDescriptorSetLayout(m_device, &layout_info, nullptr, &m_compute_descriptor_set_layout)) {
      throw std::runtime_error("failed to create descriptor set layout");
    }
  }

  void CreateComputeDescriptorSets() {
    // 描述符布局对象的个数要匹配描述符集对象的个数
    std::vector<VkDescriptorSetLayout> layouts(k_max_frames_in_flight, m_compute_descriptor_set_layout);

    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = m_descriptor_pool;  // 指定分配描述符集对象的描述符池
    alloc_info.descriptorSetCount = static_cast<uint32_t>(layouts.size());
    alloc_info.pSetLayouts = layouts.data();

    // 描述符集对象会在描述符池对象清除时自动被清除
    // 在这里给每一个交换链图像使用相同的描述符布局创建对应的描述符集
    m_compute_descriptor_sets.resize(k_max_frames_in_flight);
    if (VK_SUCCESS != vkAllocateDescriptorSets(m_device, &alloc_info, m_compute_descriptor_sets.data())) {
      throw std::runtime_error("failed to allocate compute descriptor sets");
    }

    for (size_t i = 0; i < k_max_frames_in_flight; ++i) {
      VkDescriptorBufferInfo compute_ubo_buffer_info{};
      compute_ubo_buffer_info.buffer = m_compute_ubo_buffers.at(i);
      compute_ubo_buffer_info.offset = 0;
      compute_ubo_buffer_info.range = sizeof(UBOCompute);

      VkDescriptorBufferInfo indirect_draw_buffer_info{};
      indirect_draw_buffer_info.buffer = m_compute_result_buffers.at(i);
      indirect_draw_buffer_info.offset = 0;
      indirect_draw_buffer_info.range = sizeof(BUFCompute);

      std::array<VkWriteDescriptorSet, 2> descriptor_writes{};

      // 计算着色器中的 Uniform
      descriptor_writes.at(0).sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      descriptor_writes.at(0).dstSet = m_compute_descriptor_sets.at(i);
      descriptor_writes.at(0).dstBinding = 0;  // 绑定点
      descriptor_writes.at(0).dstArrayElement = 0;
      descriptor_writes.at(0).descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;  // 对应着色器中的 Uniform
      descriptor_writes.at(0).descriptorCount = 1;
      descriptor_writes.at(0).pBufferInfo = &compute_ubo_buffer_info;  // 指定描述符引用的缓冲数据
      descriptor_writes.at(0).pImageInfo = nullptr;                    // 指定描述符引用的图像数据
      descriptor_writes.at(0).pTexelBufferView = nullptr;              // 指定描述符引用的缓冲视图

      // 计算着色器中的 Buffer
      descriptor_writes.at(1).sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      descriptor_writes.at(1).dstSet = m_compute_descriptor_sets.at(i);
      descriptor_writes.at(1).dstBinding = 1;  // 绑定点
      descriptor_writes.at(1).dstArrayElement = 0;
      descriptor_writes.at(1).descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;  // 对应着色器中的 buffer
      descriptor_writes.at(1).descriptorCount = 1;
      descriptor_writes.at(1).pBufferInfo = &indirect_draw_buffer_info;  // 指定描述符引用的缓冲数据
      descriptor_writes.at(1).pImageInfo = nullptr;                      // 指定描述符引用的图像数据
      descriptor_writes.at(1).pTexelBufferView = nullptr;                // 指定描述符引用的缓冲视图

      // 更新描述符的配置
      vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(descriptor_writes.size()), descriptor_writes.data(), 0,
                             nullptr);
    }
  }

  void CreateComputePipeline() {
    auto compute_shader_code = ReadFile(PROJECT_ASSETS_DIR "shaders/04_02_base_comp.spv");

    VkShaderModule compute_shader_module = CreateShaderModule(compute_shader_code);

    VkPipelineShaderStageCreateInfo compute_shader_stage_info{};
    compute_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    compute_shader_stage_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    compute_shader_stage_info.module = compute_shader_module;
    compute_shader_stage_info.pName = "main";

    // 计算着色器中的 uniform、buffer
    VkPipelineLayoutCreateInfo pipeline_layout_info{};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = 1;
    pipeline_layout_info.pSetLayouts = &m_compute_descriptor_set_layout;

    if (vkCreatePipelineLayout(m_device, &pipeline_layout_info, nullptr, &m_compute_pipeline_layout) != VK_SUCCESS) {
      throw std::runtime_error("failed to create compute pipeline layout!");
    }

    VkComputePipelineCreateInfo pipeline_info{};
    pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipeline_info.layout = m_compute_pipeline_layout;
    pipeline_info.stage = compute_shader_stage_info;

    if (vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &m_compute_pipeline) !=
        VK_SUCCESS) {
      throw std::runtime_error("failed to create compute pipeline!");
    }

    vkDestroyShaderModule(m_device, compute_shader_module, nullptr);
  }

  void CreateComputeSyncObjects() {
    m_compute_in_flight_fences.resize(k_max_frames_in_flight);

    VkSemaphoreCreateInfo semaphore_info = {};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fence_info = {};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;  // 初始状态设置为已发出信号，避免 vkWaitForFences 一直等待

    for (size_t i = 0; i < k_max_frames_in_flight; ++i) {
      if (VK_SUCCESS != vkCreateFence(m_device, &fence_info, nullptr, &m_compute_in_flight_fences.at(i))) {
        throw std::runtime_error("failed to create compute synchronization objects for a frame");
      }
    }
  }

  void RecordComputeCommandBuffer(VkCommandBuffer command_buffer) {
    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (VK_SUCCESS != vkBeginCommandBuffer(command_buffer, &begin_info)) {
      throw std::runtime_error("failed to begin recording compute command buffer");
    }

    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_compute_pipeline);
    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_compute_pipeline_layout, 0, 1,
                            &m_compute_descriptor_sets.at(m_current_frame), 0, nullptr);
    vkCmdDispatch(command_buffer, 1, 1, 1);

    vkEndCommandBuffer(command_buffer);
  }

  /// @brief 创建 Vulkan 实例
  void CreateInstance() {
    if (k_enable_validation_layers && !CheckValidationLayerSupport()) {
      throw std::runtime_error("validation layers requested, but not available");
    }

    // 应用程序的信息，这些信息可能会作为驱动程序的优化依据，让驱动做一些特殊的优化
    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pNext = nullptr;
    app_info.pApplicationName = "Hello Triangle";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "No Engine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_0;

    // 指定驱动程序需要使用的全局扩展和校验层，全局是指对整个应用程序都有效，而不仅仅是某一个设备
    VkInstanceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;

    // 指定需要的全局扩展
    auto required_extensions = GetRequiredExtensions();
    create_info.enabledExtensionCount = static_cast<uint32_t>(required_extensions.size());
    create_info.ppEnabledExtensionNames = required_extensions.data();

    // 指定全局校验层
    if (k_enable_validation_layers) {
      create_info.enabledLayerCount = static_cast<uint32_t>(k_validation_layers.size());
      create_info.ppEnabledLayerNames = k_validation_layers.data();

      VkDebugUtilsMessengerCreateInfoEXT debug_create_info = {};
      PopulateDebugMessengerCreateInfo(debug_create_info);

      create_info.pNext = &debug_create_info;
    } else {
      create_info.enabledLayerCount = 0;
      create_info.pNext = nullptr;
    }

    // 创建 Vulkan 实例，用来初始化 Vulkan 库
    // 1.包含创建信息的结构体指针
    // 2.自定义的分配器回调函数
    // 3.指向实例句柄存储位置的指针
    if (VK_SUCCESS != vkCreateInstance(&create_info, nullptr, &m_instance)) {
      throw std::runtime_error("failed to create instance");
    }
  }

  /// @brief 检查需要开启的校验层是否被支持
  /// @return
  bool CheckValidationLayerSupport() const noexcept {
    // 获取所有可用的校验层列表
    uint32_t layer_count = 0;
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
    std::vector<VkLayerProperties> available_layers(layer_count);
    vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());
    std::cout << "-------------------------------------------\n"
              << "All available layers:\n";
    for (const auto& layer : available_layers) {
      std::cout << static_cast<const char*>(layer.layerName) << '\n';
    }

    // 检查需要开启的校验层是否可以在所有可用的校验层列表中找到
    for (const char* layer_name : k_validation_layers) {
      bool layer_found{false};

      for (const auto& layer_properties : available_layers) {
        if (0 == std::strcmp(layer_name, static_cast<const char*>(layer_properties.layerName))) {
          layer_found = true;
          break;
        }
      }

      if (!layer_found) {
        return false;
      }
    }

    return true;
  }

  /// @brief 获取所有需要开启的扩展
  /// @return
  std::vector<const char*> GetRequiredExtensions() const noexcept {
    // 获取 Vulkan 支持的所有扩展
    uint32_t extension_count = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);
    std::vector<VkExtensionProperties> extensions(extension_count);
    vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, extensions.data());
    std::cout << "-------------------------------------------\n"
              << "All supported extensions:\n";
    for (const auto& e : extensions) {
      std::cout << static_cast<const char*>(e.extensionName) << '\n';
    }

    // 将需要开启的所有扩展添加到列表并返回
    std::vector<const char*> required_extensions;
    if (k_enable_validation_layers) {
      // 根据需要开启调试报告相关的扩展
      required_extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return required_extensions;
  }

  /// @brief 设置回调函数来接受调试信息
  void SetupDebugCallback() {
    if (!k_enable_validation_layers) {
      return;
    }

    VkDebugUtilsMessengerCreateInfoEXT create_info = {};
    PopulateDebugMessengerCreateInfo(create_info);

    if (VK_SUCCESS != CreateDebugUtilsMessengerEXT(m_instance, &create_info, nullptr, &m_debug_messenger)) {
      throw std::runtime_error("failed to set up debug callback");
    }
  }

  /// @brief 选择一个满足需求的物理设备（显卡）
  /// @details 可以选择任意数量的显卡并同时使用它们
  void PickPhysicalDevice() {
    // 获取支持 Vulkan 的显卡数量
    uint32_t device_count{0};
    vkEnumeratePhysicalDevices(m_instance, &device_count, nullptr);

    if (0 == device_count) {
      throw std::runtime_error("failed to find GPUs with Vulkan support");
    }

    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(m_instance, &device_count, devices.data());

    for (const auto& device : devices) {
      if (IsDeviceSuitable(device)) {
        m_physical_device = device;
        break;
      }
    }

    if (nullptr == m_physical_device) {
      throw std::runtime_error("failed to find a suitable GPU");
    }
  }

  /// @brief 检查显卡是否满足需求
  /// @param device
  /// @return
  bool IsDeviceSuitable(VkPhysicalDevice device) noexcept {
    // 获取基本的设置属性，name、type以及Vulkan版本等等
    // VkPhysicalDeviceProperties deviceProperties;
    // vkGetPhysicalDeviceProperties(device, &deviceProperties);

    // 获取对纹理的压缩、64位浮点数和多视图渲染等可选功能的支持
    // VkPhysicalDeviceFeatures deviceFeatures;
    // vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

    m_queue_family_indices = FindQueueFamilies(device);
    auto extensions_supported = CheckDeviceExtensionSupported(device);

    return m_queue_family_indices.IsComplete() && extensions_supported;
  }

  /// @brief 查找满足需求的队列族
  /// @details 不同的队列族支持不同的类型的指令，例如计算、内存传输、绘图等指令
  /// @param device
  /// @return
  QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device) const noexcept {
    // 获取物理设备支持的队列族列表
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);
    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());

    // 查找专用队列
    auto get_dedicated_queue = [&queue_families](VkQueueFlagBits queue_flag_bits) -> std::optional<uint32_t> {
      for (size_t i = 0; i < queue_families.size(); ++i) {
        if (queue_flag_bits == queue_families.at(i).queueFlags) {
          return static_cast<uint32_t>(i);
        }
      }

      return std::nullopt;
    };

    QueueFamilyIndices indices{};
    indices.graphicsFamily = get_dedicated_queue(VK_QUEUE_GRAPHICS_BIT);
    indices.computeFamily = get_dedicated_queue(VK_QUEUE_COMPUTE_BIT);
    indices.transferFamily = get_dedicated_queue(VK_QUEUE_TRANSFER_BIT);

    auto get_support_queue = [&queue_families](VkQueueFlagBits queue_flag_bits,
                                               size_t index) -> std::optional<uint32_t> {
      if (0 != (queue_flag_bits & queue_families.at(index).queueFlags)) {
        return static_cast<uint32_t>(index);
      }
      return std::nullopt;
    };

    for (size_t i = 0; i < queue_families.size(); ++i) {
      // 如果没有专用队列，则使用第一个支持指定类型的队列
      if (!indices.graphicsFamily.has_value()) {
        indices.graphicsFamily = get_support_queue(VK_QUEUE_GRAPHICS_BIT, i);
      }
      if (!indices.computeFamily.has_value()) {
        indices.computeFamily = get_support_queue(VK_QUEUE_COMPUTE_BIT, i);
      }
      if (!indices.transferFamily.has_value()) {
        indices.transferFamily = get_support_queue(VK_QUEUE_TRANSFER_BIT, i);
      }

      if (indices.IsComplete()) {
        break;
      }
    }

    return indices;
  }

  /// @brief 创建逻辑设备作为和物理设备交互的接口
  void CreateLogicalDevice() {
    std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
    std::set<uint32_t> unique_queue_families{lvk_tidy::GetRequiredValue(m_queue_family_indices.computeFamily),
                                             lvk_tidy::GetRequiredValue(m_queue_family_indices.transferFamily)};

    // 控制指令缓存执行顺序的优先级，即使只有一个队列也要显示指定优先级，范围：[0.0, 1.0]
    float queue_priority{1.F};
    for (auto queue_family : unique_queue_families) {
      // 描述队列簇中预要申请使用的队列数量
      // 当前可用的驱动程序所提供的队列簇只允许创建少量的队列，并且很多时候没有必要创建多个队列
      // 因为可以在多个线程上创建所有命令缓冲区，然后在主线程一次性的以较低开销的调用提交队列
      VkDeviceQueueCreateInfo queue_create_info{};
      queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
      queue_create_info.queueFamilyIndex = queue_family;
      queue_create_info.queueCount = 1;
      queue_create_info.pQueuePriorities = &queue_priority;
      queue_create_infos.push_back(queue_create_info);
    }

    // 指定应用程序使用的设备特性（例如几何着色器）
    VkPhysicalDeviceFeatures device_features = {};

    VkDeviceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
    create_info.pQueueCreateInfos = queue_create_infos.data();
    create_info.pEnabledFeatures = &device_features;

    // 根据需要对设备和 Vulkan 实例使用相同的校验层
    if (k_enable_validation_layers) {
      create_info.enabledLayerCount = static_cast<uint32_t>(k_validation_layers.size());
      create_info.ppEnabledLayerNames = k_validation_layers.data();
    } else {
      create_info.enabledLayerCount = 0;
    }

    // 创建逻辑设备
    if (VK_SUCCESS != vkCreateDevice(m_physical_device, &create_info, nullptr, &m_device)) {
      throw std::runtime_error("failed to create logical device");
    }

    vkGetDeviceQueue(m_device, lvk_tidy::GetRequiredValue(m_queue_family_indices.computeFamily), 0, &m_compute_queue);
    vkGetDeviceQueue(m_device, lvk_tidy::GetRequiredValue(m_queue_family_indices.transferFamily), 0, &m_transfer_queue);
  }

  /// @brief 检测所需的扩展是否支持
  /// @param device
  /// @return
  bool CheckDeviceExtensionSupported(VkPhysicalDevice device) const noexcept {
    uint32_t extension_count{0};
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr);
    std::vector<VkExtensionProperties> available_extensions(extension_count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, available_extensions.data());

    std::set<std::string> required_extensions;  // 暂时不需要任何扩展
    for (const auto& extension : available_extensions) {
      required_extensions.erase(static_cast<const char*>(extension.extensionName));
    }

    // 如果为空，则支持
    return required_extensions.empty();
  }

  /// @brief 使用着色器字节码数组创建 VkShaderModule 对象
  /// @param code
  /// @return
  VkShaderModule CreateShaderModule(const std::vector<char>& code) const {
    VkShaderModuleCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = code.size();
    create_info.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shader_module = nullptr;
    if (VK_SUCCESS != vkCreateShaderModule(m_device, &create_info, nullptr, &shader_module)) {
      throw std::runtime_error("failed to create shader module");
    }

    return shader_module;
  }

  /// @brief 创建指令池，用于管理指令缓冲对象使用的内存，并负责指令缓冲对象的分配
  void CreateCommandPool() {
    // VK_COMMAND_POOL_CREATE_TRANSIENT_BIT
    // 指定从Pool中分配的CommandBuffer将是短暂的，意味着它们将在相对较短的时间内被重置或释放
    // VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT 允许从Pool中分配的任何CommandBuffer被单独重置到inital状态
    // 没有设置这个flag则不能使用 vkResetCommandBuffer
    // VK_COMMAND_POOL_CREATE_PROTECTED_BIT 指定从Pool中分配的CommandBuffer是受保护的CommandBuffer
    VkCommandPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_info.queueFamilyIndex = lvk_tidy::GetRequiredValue(m_queue_family_indices.computeFamily);

    if (VK_SUCCESS != vkCreateCommandPool(m_device, &pool_info, nullptr, &m_compute_command_pool)) {
      throw std::runtime_error("failed to create command pool");
    }

    if (m_queue_family_indices.computeFamily != m_queue_family_indices.transferFamily) {
      pool_info.queueFamilyIndex = lvk_tidy::GetRequiredValue(m_queue_family_indices.transferFamily);
      if (VK_SUCCESS != vkCreateCommandPool(m_device, &pool_info, nullptr, &m_transfer_command_pool)) {
        throw std::runtime_error("failed to create command pool");
      }
    } else {
      m_transfer_command_pool = m_compute_command_pool;
    }
  }

  void DrawFrame() {
    UpdateComputeUniformBuffer(m_current_frame);
    vkWaitForFences(m_device, 1, &m_compute_in_flight_fences[m_current_frame], VK_TRUE, UINT64_MAX);
    vkResetFences(m_device, 1, &m_compute_in_flight_fences[m_current_frame]);

    // 回读计算结果
    auto result = reinterpret_cast<BUFCompute*>(m_compute_result_buffers_mapped.at(m_current_frame));
    std::cout << result->result << '\n';

    vkResetCommandBuffer(m_compute_command_buffers[m_current_frame], /*VkCommandBufferResetFlagBits*/ 0);
    RecordComputeCommandBuffer(m_compute_command_buffers[m_current_frame]);

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &m_compute_command_buffers[m_current_frame];

    if (vkQueueSubmit(m_compute_queue, 1, &submit_info, m_compute_in_flight_fences[m_current_frame]) != VK_SUCCESS) {
      throw std::runtime_error("failed to submit compute command buffer!");
    };

    // 更新当前帧索引
    m_current_frame = (m_current_frame + 1) % k_max_frames_in_flight;
  }

  /// @brief 设置调试扩展信息
  /// @param createInfo
  void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& create_info) const noexcept {
    create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    // 设置回调函数处理的消息级别
    create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                  VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                  VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    // 设置回调函数处理的消息类型
    create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                              VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                              VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    // 设置回调函数
    create_info.pfnUserCallback = DebugCallback;
    // 设置用户自定义数据，是可选的
    create_info.pUserData = nullptr;
  }

  /// @brief 创建指定类型的缓冲
  /// @details Vulkan 的缓冲是可以存储任意数据的可以被显卡读取的内存，不仅可以存储顶点数据
  /// @param size
  /// @param usage
  /// @param properties
  /// @param buffer
  /// @param bufferMemory
  void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer,
                    VkDeviceMemory& buffer_memory) const {
    VkBufferCreateInfo buffer_info = {};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = size;                              // 缓冲的字节大小
    buffer_info.usage = usage;                            // 缓冲中的数据使用目的，可以使用位或来指定多个目的
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;  // 缓冲可以被特定的队列族所拥有，也可以在多个队列族共享
    buffer_info.flags = 0;                                // 配置缓冲的内存稀疏程度，0表示使用默认值

    if (VK_SUCCESS != vkCreateBuffer(m_device, &buffer_info, nullptr, &buffer)) {
      throw std::runtime_error("failed to create vertex buffer");
    }

    // 缓冲创建好之后还需要分配内存，首先获取缓冲的内存需求
    // size: 缓冲需要的内存的字节大小，可能和bufferInfo.size的值不同
    // alignment: 缓冲在实际被分配的内存中的开始位置，依赖于bufferInfo的usage和flags
    // memoryTypeBits: 指示适合该缓冲使用的内存类型的位域
    VkMemoryRequirements mem_requirements{};
    vkGetBufferMemoryRequirements(m_device, buffer, &mem_requirements);

    VkMemoryAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_requirements.size;
    alloc_info.memoryTypeIndex = FindMemoryType(mem_requirements.memoryTypeBits, properties);

    if (VK_SUCCESS != vkAllocateMemory(m_device, &alloc_info, nullptr, &buffer_memory)) {
      throw std::runtime_error("failed to allocate buffer memory");
    }

    // 4. 偏移值，需要满足能够被 memRequirements.alighment 整除
    vkBindBufferMemory(m_device, buffer, buffer_memory, 0);
  }

  /// @brief 在缓冲之间复制数据
  /// @param srcBuffer
  /// @param dstBuffer
  /// @param size
  void CopyBuffer(VkBuffer src_buffer, VkBuffer dst_buffer, VkDeviceSize size) const noexcept {
    VkCommandBufferAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandPool = m_transfer_command_pool;
    alloc_info.commandBufferCount = 1;

    VkCommandBuffer command_buffer{};
    vkAllocateCommandBuffers(m_device, &alloc_info, &command_buffer);

    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;  // 指定如何使用这个指令缓冲

    vkBeginCommandBuffer(command_buffer, &begin_info);
    VkBufferCopy copy_region = {};
    copy_region.srcOffset = 0;
    copy_region.dstOffset = 0;
    copy_region.size = size;
    vkCmdCopyBuffer(command_buffer, src_buffer, dst_buffer, 1, &copy_region);
    vkEndCommandBuffer(command_buffer);

    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;

    // 提交到内存传输指令队列执行内存传输
    vkQueueSubmit(m_transfer_queue, 1, &submit_info, nullptr);
    // 等待传输操作完成，也可以使用栅栏，栅栏可以同步多个不同的内存传输操作，给驱动程序的优化空间也更大
    vkQueueWaitIdle(m_transfer_queue);

    vkFreeCommandBuffers(m_device, m_transfer_command_pool, 1, &command_buffer);
  }

  /// @brief 查找最合适的内存类型
  /// @details 不同类型的内存所允许进行的操作以及操作的效率有所不同
  /// @param typeFilter 指定需要的内存类型的位域
  /// @param properties
  /// @return
  uint32_t FindMemoryType(uint32_t type_filter, VkMemoryPropertyFlags properties) const {
    // 查找物理设备可用的内存类型
    // memoryHeaps 内存来源，比如显存以及显存用尽后的位与主存中的交换空间
    VkPhysicalDeviceMemoryProperties mem_properties{};
    vkGetPhysicalDeviceMemoryProperties(m_physical_device, &mem_properties);

    for (uint32_t i = 0; i < mem_properties.memoryTypeCount; ++i) {
      if (type_filter & (1 << i) && (mem_properties.memoryTypes[i].propertyFlags & properties) == properties) {
        return i;
      }
    }

    throw std::runtime_error("failed to find suitable memory type");
  }

  /// @brief 创建描述符池，描述符集需要通过描述符池来创建
  void CreateDescriptorPool() {
    std::array<VkDescriptorPoolSize, 2> pool_sizes{};

    pool_sizes.at(0).type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    pool_sizes.at(0).descriptorCount = static_cast<uint32_t>(k_max_frames_in_flight);
    pool_sizes.at(1).type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    pool_sizes.at(1).descriptorCount = static_cast<uint32_t>(k_max_frames_in_flight);

    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
    pool_info.pPoolSizes = pool_sizes.data();
    pool_info.maxSets = static_cast<uint32_t>(k_max_frames_in_flight);
    pool_info.flags = 0;  // 可以用来设置独立的描述符集是否可以被清除掉，此处使用默认值

    if (VK_SUCCESS != vkCreateDescriptorPool(m_device, &pool_info, nullptr, &m_descriptor_pool)) {
      throw std::runtime_error("failed to create descriptor pool");
    }
  }

private:
  /// @brief 接受调试信息的回调函数
  /// @param message_severity 消息的级别：诊断、资源创建、警告、不合法或可能造成崩溃的操作
  /// @param message_type 发生了与规范和性能无关的事件、出现了违反规范的错误、进行了可能影响 Vulkan 性能的行为
  /// @param callback_data 包含了调试信息的字符串、存储有和消息相关的 Vulkan 对象句柄的数组、数组中的对象个数
  /// @param user_data 指向了设置回调函数时，传递的数据指针
  /// @return 引发校验层处理的 Vulkan API 调用是否中断，通常只在测试校验层本身时会返回true，其余都应该返回 VK_FALSE
  static VKAPI_ATTR VkBool32 VKAPI_CALL
  DebugCallback([[maybe_unused]] VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
                [[maybe_unused]] VkDebugUtilsMessageTypeFlagsEXT message_type,
                const VkDebugUtilsMessengerCallbackDataEXT* callback_data, [[maybe_unused]] void* user_data) noexcept {
    std::clog << "===========================================\n"
              << "Debug::validation layer: " << callback_data->pMessage << '\n';

    return VK_FALSE;
  }

  /// @brief 代理函数，用来加载 Vulkan 扩展函数 vkCreateDebugUtilsMessengerEXT
  /// @param instance
  /// @param pCreateInfo
  /// @param pAllocator
  /// @param pCallback
  /// @return
  static VkResult CreateDebugUtilsMessengerEXT(VkInstance instance,
                                               const VkDebugUtilsMessengerCreateInfoEXT* p_create_info,
                                               const VkAllocationCallbacks* p_allocator,
                                               VkDebugUtilsMessengerEXT* p_callback) noexcept {
    // vkCreateDebugUtilsMessengerEXT是一个扩展函数，不会被 Vulkan 库自动加载，所以需要手动加载
    auto func =
      lvk_tidy::LoadInstanceProcAddress<PFN_vkCreateDebugUtilsMessengerEXT>(instance, "vkCreateDebugUtilsMessengerEXT");

    if (nullptr != func) {
      return func(instance, p_create_info, p_allocator, p_callback);
    }

    return VK_ERROR_EXTENSION_NOT_PRESENT;
  }

  /// @brief 代理函数，用来加载 Vulkan 扩展函数 vkDestroyDebugUtilsMessengerEXT
  /// @param instance
  /// @param callback
  /// @param pAllocator
  static void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT callback,
                                            const VkAllocationCallbacks* p_allocator) noexcept {
    auto func = lvk_tidy::LoadInstanceProcAddress<PFN_vkDestroyDebugUtilsMessengerEXT>(
      instance, "vkDestroyDebugUtilsMessengerEXT");

    if (nullptr != func) {
      func(instance, callback, p_allocator);
    }
  }

  /// @brief 读取二进制着色器文件
  /// @param fileName
  /// @return
  static std::vector<char> ReadFile(const std::string& file_name) {
    std::ifstream file(file_name, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
      throw std::runtime_error("failed to open file: " + file_name);
    }

    size_t file_size = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(file_size);
    file.seekg(0);
    file.read(buffer.data(), static_cast<std::streamsize>(file_size));
    file.close();
    return buffer;
  }

private:
  VkInstance m_instance{nullptr};
  VkDebugUtilsMessengerEXT m_debug_messenger{nullptr};
  VkPhysicalDevice m_physical_device{nullptr};
  VkDevice m_device{nullptr};

  size_t m_current_frame{0};
  VkDescriptorPool m_descriptor_pool{nullptr};
  QueueFamilyIndices m_queue_family_indices{};
  VkCommandPool m_compute_command_pool{nullptr};
  VkCommandPool m_transfer_command_pool{nullptr};
  VkQueue m_transfer_queue{nullptr};  // 传输队列
  VkQueue m_compute_queue{nullptr};   // 计算队列

  VkPipeline m_compute_pipeline{nullptr};
  VkPipelineLayout m_compute_pipeline_layout{nullptr};
  std::vector<VkCommandBuffer> m_compute_command_buffers{};
  VkDescriptorSetLayout m_compute_descriptor_set_layout{nullptr};
  std::vector<VkDescriptorSet> m_compute_descriptor_sets{};
  std::vector<VkFence> m_compute_in_flight_fences{};

  std::vector<VkBuffer> m_compute_ubo_buffers{};
  std::vector<VkDeviceMemory> m_compute_ubo_buffers_memory{};
  std::vector<void*> m_compute_ubo_buffers_mapped{};
  std::vector<VkBuffer> m_compute_result_buffers{};
  std::vector<VkDeviceMemory> m_compute_result_buffers_memory{};
  std::vector<void*> m_compute_result_buffers_mapped{};
};

int main() {
  HelloTriangleApplication app;

  try {
    app.Run();
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
