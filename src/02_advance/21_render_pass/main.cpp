/**
 * 1. 两个完全一样的 RenderPass，只会显示第二次的 RenderPass 绘制结果
 * 2. 前面的 RenderPass 会被后面的 RenderPass 覆盖，即使 renderArea 不重叠 // XXX 待继续验证
 * 3. 清除深度缓冲区，指定图元始终在最上层
 * 4. 将 RenderPass 封装为类
 */

#define TEST4

#ifdef TEST1

#define GLFW_INCLUDE_VULKAN  // 定义这个宏之后 glfw3.h 文件就会包含 Vulkan 的头文件
#include <GLFW/glfw3.h>

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

// 窗口默认大小
constexpr uint32_t k_width = 800;
constexpr uint32_t k_height = 600;

// 同时并行处理的帧数
constexpr int k_max_frames_in_flight = 2;

// 需要开启的校验层的名称
const std::vector<const char*> k_validation_layers = {"VK_LAYER_KHRONOS_validation"};
// 交换链扩展
const std::vector<const char*> k_device_extensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

// 是否启用校验层
#ifdef NDEBUG
const bool k_enable_validation_layers = false;
#else
const bool k_enable_validation_layers = true;
#endif  // NDEBUG

struct Vertex {
  glm::vec3 pos{0.F, 0.F, 0.F};
  glm::vec3 color{0.F, 0.F, 0.F};

  static constexpr VkVertexInputBindingDescription GetBindingDescription() noexcept {
    VkVertexInputBindingDescription binding_description{};

    binding_description.binding = 0;
    binding_description.stride = sizeof(Vertex);
    binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    return binding_description;
  }

  static constexpr std::array<VkVertexInputAttributeDescription, 2> GetAttributeDescriptions() noexcept {
    static_assert(std::is_standard_layout_v<Vertex>, "Vertex must be standard layout for offsetof");
    std::array<VkVertexInputAttributeDescription, 2> attribute_descriptions{};

    attribute_descriptions[0].binding = 0;
    attribute_descriptions[0].location = 0;
    attribute_descriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attribute_descriptions[0].offset = offsetof(Vertex, pos);

    attribute_descriptions[1].binding = 0;
    attribute_descriptions[1].location = 1;
    attribute_descriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attribute_descriptions[1].offset = offsetof(Vertex, color);

    return attribute_descriptions;
  }
};

struct UniformBufferObject {
  glm::mat4 model{glm::mat4(1.F)};
  glm::mat4 view{glm::mat4(1.F)};
  glm::mat4 proj{glm::mat4(1.F)};
};

/// @brief 支持图形和呈现的队列族
struct QueueFamilyIndices {
  std::optional<uint32_t> graphics_family{};
  std::optional<uint32_t> present_family{};

  constexpr bool IsComplete() const noexcept { return graphics_family.has_value() && present_family.has_value(); }
};

/// @brief 交换链属性信息
struct SwapChainSupportDetails {
  VkSurfaceCapabilitiesKHR capabilities{};
  std::vector<VkSurfaceFormatKHR> formats;
  std::vector<VkPresentModeKHR> present_modes;
};

struct Drawable {
  VkBuffer vertex_buffer{nullptr};
  VkDeviceMemory vertex_buffer_memory{nullptr};
  VkBuffer index_buffer{nullptr};
  VkDeviceMemory index_buffer_memory{nullptr};
  uint32_t index_count{0};
};

// clang-format off
const std::vector<Vertex> k_vertices_cube {
    {{-0.5F,  0.5F, -0.5F}, {1.F, 0.F, 0.F}},
    {{ 0.5F,  0.5F, -0.5F}, {1.F, 1.F, 0.F}},
    {{ 0.5F, -0.5F, -0.5F}, {0.F, 1.F, 0.F}},
    {{-0.5F, -0.5F, -0.5F}, {0.F, 1.F, 1.F}},

    {{-0.5F,  0.5F,  0.5F}, {0.F, 0.F, 1.F}},
    {{ 0.5F,  0.5F,  0.5F}, {1.F, 0.F, 1.F}},
    {{ 0.5F, -0.5F,  0.5F}, {0.F, 0.F, 0.F}},
    {{-0.5F, -0.5F,  0.5F}, {1.F, 1.F, 1.F}},
};

const std::vector<uint16_t> k_indices_cube {
    0, 1, 2, 0, 2, 3, // 前
    1, 5, 6, 1, 6, 2, // 右
    5, 4, 7, 5, 7, 6, // 后
    4, 0, 3, 4, 3, 7, // 左
    3, 2, 6, 3, 6, 7, // 上
    4, 5, 1, 4, 1, 0, // 下
};

const std::vector<Vertex> k_vertices_plane {
    { {-0.7F, -0.7F,  0.0F }, { 1.F, 1.F, 1.F } },
    { {-0.7F,  0.7F,  0.0F }, { 1.F, 1.F, 1.F } },
    { { 0.7F,  0.7F,  0.0F }, { 1.F, 1.F, 1.F } },
    { { 0.7F, -0.7F,  0.0F }, { 1.F, 1.F, 1.F } },
};

const std::vector<uint16_t> k_indices_plane {
    0, 1, 2, 0, 2, 3
};

// clang-format on

class HelloTriangleApplication {
public:
  void Run() {
    InitWindow();
    InitVulkan();
    MainLoop();
    Cleanup();
  }

private:
  void InitWindow() {
    if (GLFW_FALSE == glfwInit()) {
      throw std::runtime_error("failed to init GLFW");
    }

    // 禁止 glfw 创建 OpenGL 上下文
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    m_window = glfwCreateWindow(k_width, k_height, "Vulkan", nullptr, nullptr);

    glfwSetWindowUserPointer(m_window, this);
    glfwSetFramebufferSizeCallback(m_window, FramebufferResizeCallback);
    glfwSetKeyCallback(m_window, KeyCallback);
    glfwSetCursorPosCallback(m_window, CursorPosCallback);
    glfwSetScrollCallback(m_window, ScrollCallback);
    glfwSetMouseButtonCallback(m_window, MouseButtonCallback);
  }

  void InitVulkan() {
    CreateInstance();
    SetupDebugCallback();
    CreateSurface();
    PickPhysicalDevice();
    CreateLogicalDevice();
    CreateSwapChain();
    CreateImageViews();
    CreateRenderPass();
    CreateDescriptorSetLayout();
    CreateGraphicsPipeline();
    CreateCommandPool();
    CreateDepthResources();
    CreateFramebuffers();
    CreateVertexBuffer(k_vertices_cube, m_drawable_cube);
    CreateIndexBuffer(k_indices_cube, m_drawable_cube);
    CreateVertexBuffer(k_vertices_plane, m_drawable_plane);
    CreateIndexBuffer(k_indices_plane, m_drawable_plane);
    CreateUniformBuffers();
    CreateDescriptorPool();
    CreateDescriptorSets();
    CreateCommandBuffers();
    CreateSyncObjects();
  }

  void MainLoop() {
    while (!glfwWindowShouldClose(m_window)) {
      glfwPollEvents();
      DrawFrame();
    }

    // 等待逻辑设备的操作结束执行
    // DrawFrame 函数中的操作是异步执行的，关闭窗口跳出while循环时，绘制操作和呈现操作可能仍在执行，不能进行清除操作
    vkDeviceWaitIdle(m_device);
  }

  void Cleanup() noexcept {
    CleanupSwapChain();

    DestroyDrawable(m_drawable_cube);
    DestroyDrawable(m_drawable_plane);

    vkDestroyPipeline(m_device, m_graphics_pipeline, nullptr);
    vkDestroyPipelineLayout(m_device, m_pipeline_layout, nullptr);
    vkDestroyRenderPass(m_device, m_render_pass, nullptr);

    for (size_t i = 0; i < k_max_frames_in_flight; ++i) {
      vkDestroyBuffer(m_device, m_uniform_buffers.at(i), nullptr);
      vkFreeMemory(m_device, m_uniform_buffers_memory.at(i), nullptr);
    }

    vkDestroyDescriptorPool(m_device, m_descriptor_pool, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_descriptor_set_layout, nullptr);

    for (size_t i = 0; i < k_max_frames_in_flight; ++i) {
      vkDestroySemaphore(m_device, m_image_available_semaphores.at(i), nullptr);
      vkDestroyFence(m_device, m_in_flight_fences.at(i), nullptr);
    }
    for (auto semaphore : m_render_finished_semaphores) {
      vkDestroySemaphore(m_device, semaphore, nullptr);
    }

    vkDestroyCommandPool(m_device, m_command_pool, nullptr);
    vkDestroyDevice(m_device, nullptr);

    if (k_enable_validation_layers) {
      DestroyDebugUtilsMessengerEXT(m_instance, m_debug_messenger, nullptr);
    }

    vkDestroySurfaceKHR(m_instance, m_surface, nullptr);  // 清理表面对象，必须在 Vulkan 实例清理之前
    vkDestroyInstance(m_instance, nullptr);

    glfwDestroyWindow(m_window);
    glfwTerminate();
  }

private:
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

    // Vulkan 是一个与平台无关的 API ，所以需要一个和窗口系统交互的扩展
    // 使用 glfw 获取这个扩展
    uint32_t glfw_extension_count = 0;
    const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);
    std::cout << "-------------------------------------------\n"
              << "GLFW extensions:\n";
    for (size_t i = 0; i < glfw_extension_count; ++i) {
      std::cout << glfw_extensions[i] << '\n';
    }

    // 将需要开启的所有扩展添加到列表并返回
    std::vector<const char*> required_extensions(glfw_extensions, glfw_extensions + glfw_extension_count);
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
  bool IsDeviceSuitable(VkPhysicalDevice device) const noexcept {
    // 获取基本的设置属性，name、type以及Vulkan版本等等
    // VkPhysicalDeviceProperties deviceProperties;
    // vkGetPhysicalDeviceProperties(device, &deviceProperties);

    // 获取对纹理的压缩、64位浮点数和多视图渲染等可选功能的支持
    // VkPhysicalDeviceFeatures device_features;
    // vkGetPhysicalDeviceFeatures(device, &device_features);

    auto indices = FindQueueFamilies(device);
    auto extensions_supported = CheckDeviceExtensionSupported(device);

    bool swap_chain_adequate{false};
    if (extensions_supported) {
      auto swap_chain_support = QuerySwapChainSupport(device);
      swap_chain_adequate = !swap_chain_support.formats.empty() & !swap_chain_support.present_modes.empty();
    }

    return indices.IsComplete() && extensions_supported && swap_chain_adequate;
  }

  /// @brief 查找满足需求的队列族
  /// @details 不同的队列族支持不同的类型的指令，例如计算、内存传输、绘图等指令
  /// @param device
  /// @return
  QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device) const noexcept {
    QueueFamilyIndices indices;

    // 获取物理设备支持的队列族列表
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);
    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());

    for (uint32_t i = 0; i < queue_families.size(); ++i) {
      // 图形队列族
      if (queue_families.at(i).queueFlags & VK_QUEUE_GRAPHICS_BIT) {
        indices.graphics_family = i;
      }

      // 呈现队列族
      VkBool32 present_support = false;
      vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &present_support);

      if (present_support) {
        indices.present_family = i;
      }

      if (indices.IsComplete()) {
        break;
      }
    }

    return indices;
  }

  /// @brief 创建逻辑设备作为和物理设备交互的接口
  void CreateLogicalDevice() {
    auto indices = FindQueueFamilies(m_physical_device);

    std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
    std::set<uint32_t> unique_queue_families{lvk_tidy::GetRequiredValue(indices.graphics_family),
                                             lvk_tidy::GetRequiredValue(indices.present_family)};

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
    create_info.enabledExtensionCount = static_cast<uint32_t>(k_device_extensions.size());
    create_info.ppEnabledExtensionNames = k_device_extensions.data();

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

    // 获取指定队列族的队列句柄，设备队列在逻辑设备被销毁时隐式清理
    // 此处的队列簇可能相同，也就是以相同的参数调用了两次这个函数
    // 1.逻辑设备对象
    // 2.队列族索引
    // 3.队列索引，因为只创建了一个队列，所以此处使用索引0
    // 4.用来存储返回的队列句柄的内存地址
    vkGetDeviceQueue(m_device, lvk_tidy::GetRequiredValue(indices.graphics_family), 0, &m_graphics_queue);
    vkGetDeviceQueue(m_device, lvk_tidy::GetRequiredValue(indices.present_family), 0, &m_present_queue);
  }

  /// @brief 创建表面，需要在程序退出前清理
  /// @details 不同平台创建表面的方式不一样，这里使用 GLFW 统一创建
  void CreateSurface() {
    // Vulkan 不能直接与窗口系统进行交互（是一个与平台特性无关的API集合），surface是 Vulkan 与窗体系统的连接桥梁
    // 需要在instance创建之后立即创建窗体surface，因为它会影响物理设备的选择
    // 窗体surface本身对于 Vulkan 也是非强制的，不需要同 OpenGL 一样必须要创建窗体surface
    if (VK_SUCCESS != glfwCreateWindowSurface(m_instance, m_window, nullptr, &m_surface)) {
      throw std::runtime_error("failed to create window surface");
    }
  }

  /// @brief 检测所需的扩展是否支持
  /// @param device
  /// @return
  bool CheckDeviceExtensionSupported(VkPhysicalDevice device) const noexcept {
    uint32_t extension_count{0};
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr);
    std::vector<VkExtensionProperties> available_extensions(extension_count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, available_extensions.data());

    std::set<std::string> required_extensions(std::begin(k_device_extensions), std::end(k_device_extensions));

    for (const auto& extension : available_extensions) {
      required_extensions.erase(static_cast<const char*>(extension.extensionName));
    }

    // 如果为空，则支持
    return required_extensions.empty();
  }

  /// @brief 查询交换链支持的细节信息
  /// @param device
  /// @return
  SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device) const noexcept {
    SwapChainSupportDetails details;

    // 查询基础表面特性
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_surface, &details.capabilities);

    // 查询表面支持的格式，确保集合对于所有有效的格式可扩充
    uint32_t format_count{0};
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &format_count, nullptr);
    if (0 != format_count) {
      details.formats.resize(format_count);
      vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &format_count, details.formats.data());
    }

    // 查询表面支持的呈现模式
    uint32_t present_mode_count{0};
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &present_mode_count, nullptr);
    if (0 != present_mode_count) {
      details.present_modes.resize(present_mode_count);
      vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &present_mode_count, details.present_modes.data());
    }

    return details;
  }

  /// @brief 选择合适的表面格式
  /// @param available_formats
  /// @return
  VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& available_formats) const noexcept {
    // VkSurfaceFormatKHR 结构都包含一个 format 和一个 colorSpace 成员
    // format 成员变量指定色彩通道和类型
    // colorSpace 成员描述 SRGB 颜色空间是否通过 VK_COLOR_SPACE_SRGB_NONLINEAR_KHR 标志支持
    // 在较早版本的规范中，这个标志名为 VK_COLORSPACE_SRGB_NONLINEAR_KHR

    // 表面没有自己的首选格式，直接返回指定的格式
    if (1 == available_formats.size() && available_formats.front().format == VK_FORMAT_UNDEFINED) {
      return {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    }

    // 遍历格式列表，如果想要的格式存在则直接返回
    for (const auto& available_format : available_formats) {
      if (VK_FORMAT_B8G8R8A8_UNORM == available_format.format &&
          VK_COLOR_SPACE_SRGB_NONLINEAR_KHR == available_format.colorSpace) {
        return available_format;
      }
    }

    // 如果以上两种方式都失效，通过“优良”进行打分排序，大多数情况下会选择第一个格式作为理想的选择
    return available_formats.front();
  }

  /// @brief 查找最佳的可用呈现模式，模式是非常重要的，因为它代表了在屏幕呈现图像的条件
  /// @param available_present_modes
  /// @return
  VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& available_present_modes) const noexcept {
    // 以下4种模式可以使用
    // 1.VK_PRESENT_MODE_IMMEDIATE_KHR: 应用程序提交的图像被立即传输到屏幕呈现，这种模式可能会造成撕裂效果。
    // 2.VK_PRESENT_MODE_FIFO_KHR:
    // 交换链被看作一个队列，当显示内容需要刷新的时候，显示设备从队列的前面获取图像，并且程序将渲染完成的图像插入队列的后面。
    // 如果队列是满的程序会等待。这种规模与视频游戏的垂直同步很类似。显示设备的刷新时刻被成为“垂直中断”。
    // 3.VK_PRESENT_MODE_FIFO_RELAXED_KHR:
    // 该模式与上一个模式略有不同的地方为，如果应用程序存在延迟，即接受最后一个垂直同步信号时队列空了，
    // 将不会等待下一个垂直同步信号，而是将图像直接传送。这样做可能导致可见的撕裂效果。
    // 4.VK_PRESENT_MODE_MAILBOX_KHR:
    // 这是第二种模式的变种。当交换链队列满的时候，选择新的替换旧的图像，从而替代阻塞应用程序的情形。
    // 这种模式通常用来实现三重缓冲区，与标准的垂直同步双缓冲相比，它可以有效避免延迟带来的撕裂效果。

    VkPresentModeKHR best_mode = VK_PRESENT_MODE_FIFO_KHR;

    for (const auto& available_present_mode : available_present_modes) {
      if (VK_PRESENT_MODE_MAILBOX_KHR == available_present_mode) {
        return available_present_mode;
      }
      if (VK_PRESENT_MODE_IMMEDIATE_KHR == available_present_mode) {
        best_mode = available_present_mode;
      }
    }

    return best_mode;
  }

  /// @brief 设置交换范围，交换范围是交换链中图像的分辨率
  /// @param capabilities
  /// @return
  VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) const noexcept {
    // 如果是最大值则表示允许我们自己选择对于窗口最合适的交换范围
    if (std::numeric_limits<uint32_t>::max() != capabilities.currentExtent.width) {
      return capabilities.currentExtent;
    }
    int width{0};
    int height{0};
    glfwGetFramebufferSize(m_window, &width, &height);

    // 使用GLFW窗口的大小来设置交换链中图像的分辨率
    VkExtent2D actual_extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};

    actual_extent.width =
      std::clamp(actual_extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    actual_extent.height =
      std::clamp(actual_extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

    return actual_extent;
  }

  /// @brief 创建交换链
  void CreateSwapChain() {
    SwapChainSupportDetails swap_chain_support = QuerySwapChainSupport(m_physical_device);
    VkSurfaceFormatKHR surface_format = ChooseSwapSurfaceFormat(swap_chain_support.formats);
    VkPresentModeKHR present_mode = ChooseSwapPresentMode(swap_chain_support.present_modes);
    VkExtent2D extent = ChooseSwapExtent(swap_chain_support.capabilities);

    // 交换链中的图像数量，可以理解为队列的长度，指定运行时图像的最小数量
    // maxImageCount数值为0代表除了内存之外没有限制
    uint32_t image_count = swap_chain_support.capabilities.minImageCount + 1;
    if (swap_chain_support.capabilities.maxImageCount > 0 &&
        image_count > swap_chain_support.capabilities.maxImageCount) {
      image_count = swap_chain_support.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = m_surface;
    create_info.minImageCount = image_count;
    create_info.imageFormat = surface_format.format;
    create_info.imageColorSpace = surface_format.colorSpace;
    create_info.imageExtent = extent;
    create_info.imageArrayLayers = 1;  // 图像包含的层次，通常为1，3d图像大于1
    create_info.imageUsage =
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;  // 指定在图像上进行怎样的操作，比如显示（颜色附件）、后处理等

    auto indices = FindQueueFamilies(m_physical_device);
    const std::array<uint32_t, 2> queue_family_indices{
      static_cast<uint32_t>(lvk_tidy::GetRequiredValue(indices.graphics_family)),
      static_cast<uint32_t>(lvk_tidy::GetRequiredValue(indices.present_family))};

    // 在多个队列族使用交换链图像的方式
    if (indices.graphics_family != indices.present_family) {
      // 图形队列和呈现队列不是同一个队列
      // VK_SHARING_MODE_CONCURRENT 表示图像可以在多个队列族间使用，不需要显式地改变图像所有权
      create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
      create_info.queueFamilyIndexCount = 2;
      create_info.pQueueFamilyIndices = queue_family_indices.data();
    } else {
      // VK_SHARING_MODE_EXCLUSIVE 表示一张图像同一时间只能被一个队列族所拥有，这种模式性能最佳
      create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
      create_info.queueFamilyIndexCount = 0;
      create_info.pQueueFamilyIndices = nullptr;
    }

    // 指定一个固定的变换操作（需要交换链具有supportedTransforms特性），此处不进行任何变换
    create_info.preTransform = swap_chain_support.capabilities.currentTransform;
    // 指定 alpha 通道是否被用来和窗口系统中的其他窗口进行混合操作
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    // 设置呈现模式
    create_info.presentMode = present_mode;
    // 设置为true，不关心被遮蔽的像素数据，比如由于其他的窗体置于前方时或者渲染的部分内容存在于可视区域之外，
    // 除非真的需要读取这些像素获数据进行处理（设置为true回读窗口像素可能会有问题），否则可以开启裁剪获得最佳性能。
    create_info.clipped = VK_TRUE;
    // 交换链重建
    // Vulkan运行时，交换链可能在某些条件下被替换，比如窗口调整大小或者交换链需要重新分配更大的图像队列。
    // 在这种情况下，交换链实际上需要重新分配创建，并且必须在此字段中指定对旧的引用，用以回收资源
    create_info.oldSwapchain = VK_NULL_HANDLE;

    if (VK_SUCCESS != vkCreateSwapchainKHR(m_device, &create_info, nullptr, &m_swap_chain)) {
      throw std::runtime_error("failed to create swap chain");
    }

    // 获取交换链中图像，图像会在交换链销毁的同时自动清理
    // 之前给VkSwapchainCreateInfoKHR 设置了期望的图像数量，但是实际运行允许创建更多的图像数量，因此需要重新获取数量
    vkGetSwapchainImagesKHR(m_device, m_swap_chain, &image_count, nullptr);
    m_swap_chain_images.resize(image_count);
    vkGetSwapchainImagesKHR(m_device, m_swap_chain, &image_count, m_swap_chain_images.data());

    m_swap_chain_image_format = surface_format.format;
    m_swap_chain_extent = extent;
  }

  /// @brief 创建图像视图
  /// @details 任何 VkImage 对象都需要通过 VkImageView 来绑定访问，包括处于交换链中的、处于渲染管线中的
  void CreateImageViews() {
    m_swap_chain_image_views.resize(m_swap_chain_images.size());

    for (size_t i = 0; i < m_swap_chain_images.size(); ++i) {
      VkImageViewCreateInfo create_info = {};
      create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      create_info.image = m_swap_chain_images.at(i);
      create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;  // 1d 2d 3d cube纹理
      create_info.format = m_swap_chain_image_format;
      create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;  // 图像颜色通过的映射
      create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
      create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
      create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
      create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;  // 指定图像的用途，图像的哪一部分可以被访问
      create_info.subresourceRange.baseMipLevel = 0;
      create_info.subresourceRange.levelCount = 1;
      create_info.subresourceRange.baseArrayLayer = 0;
      create_info.subresourceRange.layerCount = 1;

      if (VK_SUCCESS != vkCreateImageView(m_device, &create_info, nullptr, &m_swap_chain_image_views.at(i))) {
        throw std::runtime_error("failed to create image views");
      }
    }
  }

  /// @brief 创建图形管线
  /// @details 在 Vulkan 中几乎不允许对图形管线进行动态设置，也就意味着每一种状态都需要提前创建一个图形管线
  void CreateGraphicsPipeline() {
    auto vert_shader_code = ReadFile(PROJECT_SHADER_DIR "02_21_base_vert.spv");
    auto frag_shader_code = ReadFile(PROJECT_SHADER_DIR "02_21_base_frag.spv");

    VkShaderModule vert_shader_module = CreateShaderModule(vert_shader_code);
    VkShaderModule frag_shader_module = CreateShaderModule(frag_shader_code);

    VkPipelineShaderStageCreateInfo vert_shader_stage_info = {};
    vert_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vert_shader_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vert_shader_stage_info.module = vert_shader_module;
    vert_shader_stage_info.pName = "main";                 // 指定调用的着色器函数，同一份代码可以实现多个着色器
    vert_shader_stage_info.pSpecializationInfo = nullptr;  // 设置着色器常量

    VkPipelineShaderStageCreateInfo frag_shader_stage_info = {};
    frag_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    frag_shader_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    frag_shader_stage_info.module = frag_shader_module;
    frag_shader_stage_info.pName = "main";
    frag_shader_stage_info.pSpecializationInfo = nullptr;

    auto binding_description = Vertex::GetBindingDescription();
    auto attribute_descriptions = Vertex::GetAttributeDescriptions();

    // 顶点信息
    VkPipelineVertexInputStateCreateInfo vertex_input_info = {};
    vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_info.vertexBindingDescriptionCount = 1;
    vertex_input_info.pVertexBindingDescriptions = &binding_description;
    vertex_input_info.vertexAttributeDescriptionCount = static_cast<uint32_t>(attribute_descriptions.size());
    vertex_input_info.pVertexAttributeDescriptions = attribute_descriptions.data();

    // 拓扑信息
    VkPipelineInputAssemblyStateCreateInfo input_assembly = {};
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;  // 指定绘制的图元类型：点、线、三角形
    input_assembly.primitiveRestartEnable = VK_FALSE;

    // 视口
    VkViewport viewport = {};
    viewport.x = 0.F;
    viewport.y = 0.F;
    viewport.width = static_cast<float>(m_swap_chain_extent.width);
    viewport.height = static_cast<float>(m_swap_chain_extent.height);
    viewport.minDepth = 0.F;  // 深度值范围，必须在 [0.F, 1.F]之间
    viewport.maxDepth = 1.F;

    // 裁剪
    VkRect2D scissor = {};
    scissor.offset = {0, 0};
    scissor.extent = m_swap_chain_extent;

    VkPipelineViewportStateCreateInfo viewport_state = {};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.pViewports = &viewport;
    viewport_state.scissorCount = 1;
    viewport_state.pScissors = &scissor;

    // 光栅化
    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;  // 设置为true会禁止一切片段输出到帧缓冲
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.F;
    rasterizer.cullMode = VK_CULL_MODE_NONE;                 // 表面剔除类型，正面、背面、双面剔除
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;  // 指定顺时针的顶点序是正面还是反面
    rasterizer.depthBiasEnable = VK_FALSE;
    rasterizer.depthBiasConstantFactor = 1.F;
    rasterizer.depthBiasClamp = 0.F;
    rasterizer.depthBiasSlopeFactor = 0.F;

    // 多重采样
    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.F;
    multisampling.pSampleMask = nullptr;
    multisampling.alphaToCoverageEnable = VK_FALSE;
    multisampling.alphaToOneEnable = VK_FALSE;

    // 深度和模板测试
    VkPipelineDepthStencilStateCreateInfo depth_stencil = {};
    depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil.depthTestEnable = VK_TRUE;            // 是否启用深度测试
    depth_stencil.depthWriteEnable = VK_TRUE;           // 深度测试通过后是否写入深度缓冲
    depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;  // 深度值比较方式
    depth_stencil.depthBoundsTestEnable = VK_FALSE;     // 指定可选的深度范围测试
    depth_stencil.minDepthBounds = 0.F;
    depth_stencil.maxDepthBounds = 1.F;
    depth_stencil.stencilTestEnable = VK_FALSE;  // 模板测试
    depth_stencil.front = {};
    depth_stencil.back = {};

    // 颜色混合，可以对指定帧缓冲单独设置，也可以设置全局颜色混合方式
    VkPipelineColorBlendAttachmentState color_blend_attachment{};
    color_blend_attachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo color_blending{};
    color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blending.logicOpEnable = VK_FALSE;
    color_blending.logicOp = VK_LOGIC_OP_COPY;
    color_blending.attachmentCount = 1;
    color_blending.pAttachments = &color_blend_attachment;
    color_blending.blendConstants[0] = 0.0F;
    color_blending.blendConstants[1] = 0.0F;
    color_blending.blendConstants[2] = 0.0F;
    color_blending.blendConstants[3] = 0.0F;

    // 动态状态，视口大小、线宽、混合常量等
    std::vector<VkDynamicState> dynamic_states = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic_state{};
    dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.dynamicStateCount = static_cast<uint32_t>(dynamic_states.size());
    dynamic_state.pDynamicStates = dynamic_states.data();

    // 管线布局，在着色器中使用 uniform 变量
    VkPipelineLayoutCreateInfo pipeline_layout_info{};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = 1;
    pipeline_layout_info.pSetLayouts = &m_descriptor_set_layout;
    pipeline_layout_info.pushConstantRangeCount = 0;

    if (VK_SUCCESS != vkCreatePipelineLayout(m_device, &pipeline_layout_info, nullptr, &m_pipeline_layout)) {
      throw std::runtime_error("failed to create pipeline layout");
    }

    const std::array<VkPipelineShaderStageCreateInfo, 2> shader_stages{vert_shader_stage_info, frag_shader_stage_info};

    // 完整的图形管线包括：着色器阶段、固定功能状态、管线布局、渲染流程
    VkGraphicsPipelineCreateInfo pipeline_info = {};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = 2;
    pipeline_info.pStages = shader_stages.data();
    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pDepthStencilState = nullptr;
    pipeline_info.pColorBlendState = &color_blending;
    pipeline_info.pDynamicState = &dynamic_state;
    pipeline_info.layout = m_pipeline_layout;
    pipeline_info.renderPass = m_render_pass;
    pipeline_info.subpass = 0;                   // 子流程在子流程数组中的索引
    pipeline_info.basePipelineHandle = nullptr;  // 以一个创建好的图形管线为基础创建一个新的图形管线
    pipeline_info.basePipelineIndex = -1;  // 只有该结构体的成员 flags 被设置为 VK_PIPELINE_CREATE_DERIVATIVE_BIT 才有效
    pipeline_info.pDepthStencilState = &depth_stencil;

    if (VK_SUCCESS !=
        vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &m_graphics_pipeline)) {
      throw std::runtime_error("failed to create graphics pipeline");
    }

    vkDestroyShaderModule(m_device, frag_shader_module, nullptr);
    vkDestroyShaderModule(m_device, vert_shader_module, nullptr);
  }

  /// @brief 使用着色器字节码数组创建 VkShaderModule 对象
  /// @param code
  /// @return
  VkShaderModule CreateShaderModule(const std::vector<char>& code) const {
    VkShaderModuleCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = code.size();
    create_info.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shader_module = VK_NULL_HANDLE;
    if (VK_SUCCESS != vkCreateShaderModule(m_device, &create_info, nullptr, &shader_module)) {
      throw std::runtime_error("failed to create shader module");
    }

    return shader_module;
  }

  /// @brief 创建渲染流程对象，用于渲染的帧缓冲附着，指定颜色和深度缓冲以及采样数
  void CreateRenderPass() {
    // 附着描述
    VkAttachmentDescription color_attachment = {};
    color_attachment.format = m_swap_chain_image_format;                 // 颜色缓冲附着的格式
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;                    // 采样数
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;               // 渲染之前对附着中的数据（颜色和深度）进行操作
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;             // 渲染之后对附着中的数据（颜色和深度）进行操作
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;    // 渲染之前对模板缓冲的操作
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;  // 渲染之后对模板缓冲的操作
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;          // 渲染流程开始前的图像布局方式
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;      // 渲染流程结束后的图像布局方式

    VkAttachmentDescription depth_attachment = {};
    depth_attachment.format = FindDepthFormat();
    depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;  // 绘制结束后不需要从深度缓冲复制深度数据
    depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;  // 不需要读取之前深度图像数据
    depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // 子流程引用的附着
    VkAttachmentReference color_attachment_ref = {};
    color_attachment_ref.attachment = 0;  // 索引，对应于片段着色器中的 layout(location = 0) out
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depth_attachment_ref = {};
    depth_attachment_ref.attachment = 1;
    depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // 子流程
    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;  // 图形渲染子流程
    subpass.colorAttachmentCount = 1;                             // 颜色附着个数
    subpass.pColorAttachments = &color_attachment_ref;            // 指定颜色附着
    subpass.pDepthStencilAttachment =
      &depth_attachment_ref;  // 指定深度模板附着，深度模板附着只能是一个，所以不用设置数量

    // 渲染流程使用的依赖信息
    VkSubpassDependency dependency = {};
    dependency.srcSubpass =
      VK_SUBPASS_EXTERNAL;      // 渲染流程开始前的子流程，为了避免出现循环依赖，dst的值必须大于src的值
    dependency.dstSubpass = 0;  // 渲染流程结束后的子流程
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                              VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;  // 指定需要等待的管线阶段
    dependency.srcAccessMask = 0;                                          // 指定子流程将进行的操作类型
    dependency.dstStageMask =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 2> attachments{color_attachment, depth_attachment};

    VkRenderPassCreateInfo render_pass_info = {};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = static_cast<uint32_t>(attachments.size());
    render_pass_info.pAttachments = attachments.data();
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;
    render_pass_info.dependencyCount = 1;
    render_pass_info.pDependencies = &dependency;

    if (VK_SUCCESS != vkCreateRenderPass(m_device, &render_pass_info, nullptr, &m_render_pass)) {
      throw std::runtime_error("failed to create render pass");
    }
  }

  /// @brief 创建帧缓冲对象，附着需要绑定到帧缓冲对象上使用
  void CreateFramebuffers() {
    m_swap_chain_framebuffers.resize(m_swap_chain_image_views.size());

    for (size_t i = 0; i < m_swap_chain_image_views.size(); ++i) {
      std::array<VkImageView, 2> attachments{m_swap_chain_image_views.at(i), m_depth_image_view};

      VkFramebufferCreateInfo framebuffer_info = {};
      framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
      framebuffer_info.renderPass = m_render_pass;
      framebuffer_info.attachmentCount = static_cast<uint32_t>(attachments.size());
      framebuffer_info.pAttachments = attachments.data();
      framebuffer_info.width = m_swap_chain_extent.width;
      framebuffer_info.height = m_swap_chain_extent.height;
      framebuffer_info.layers = 1;

      if (VK_SUCCESS != vkCreateFramebuffer(m_device, &framebuffer_info, nullptr, &m_swap_chain_framebuffers.at(i))) {
        throw std::runtime_error("failed to create framebuffer");
      }
    }
  }

  /// @brief 创建指令池，用于管理指令缓冲对象使用的内存，并负责指令缓冲对象的分配
  void CreateCommandPool() {
    auto indices = FindQueueFamilies(m_physical_device);

    // VK_COMMAND_POOL_CREATE_TRANSIENT_BIT
    // 指定从Pool中分配的CommandBuffer将是短暂的，意味着它们将在相对较短的时间内被重置或释放
    // VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT 允许从Pool中分配的任何CommandBuffer被单独重置到inital状态
    // 没有设置这个flag则不能使用 vkResetCommandBuffer
    // VK_COMMAND_POOL_CREATE_PROTECTED_BIT 指定从Pool中分配的CommandBuffer是受保护的CommandBuffer
    VkCommandPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_info.queueFamilyIndex = lvk_tidy::GetRequiredValue(indices.graphics_family);

    if (VK_SUCCESS != vkCreateCommandPool(m_device, &pool_info, nullptr, &m_command_pool)) {
      throw std::runtime_error("failed to create command pool");
    }
  }

  /// @brief 为交换链中的每一个图像创建指令缓冲对象，使用它记录绘制指令
  void CreateCommandBuffers() {
    m_command_buffers.resize(m_swap_chain_framebuffers.size());

    VkCommandBufferAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = m_command_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;  // 指定是主要还是辅助指令缓冲对象
    alloc_info.commandBufferCount = static_cast<uint32_t>(m_command_buffers.size());

    if (VK_SUCCESS != vkAllocateCommandBuffers(m_device, &alloc_info, m_command_buffers.data())) {
      throw std::runtime_error("failed to allocate command buffers");
    }
  }

  /// @brief 记录指令到指令缓冲
  void RecordCommandBuffer(VkCommandBuffer command_buffer, VkFramebuffer framebuffer) const {
    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;  // 指定怎样使用指令缓冲
    begin_info.pInheritanceInfo = nullptr;  // 只用于辅助指令缓冲，指定从调用它的主要指令缓冲继承的状态

    if (VK_SUCCESS != vkBeginCommandBuffer(command_buffer, &begin_info)) {
      throw std::runtime_error("failed to begin recording command buffer");
    }

    // 清除色，相当于背景色
    std::array<VkClearValue, 2> clear_values{};
    clear_values.at(0).color = {{.1F, .2F, .3F, 1.F}};
    clear_values.at(1).depthStencil = {1.F, 0};

    VkRenderPassBeginInfo render_pass_info = {};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_info.renderPass = m_render_pass;  // 指定使用的渲染流程对象
    render_pass_info.framebuffer = framebuffer;   // 指定使用的帧缓冲对象
    render_pass_info.renderArea.offset = {0, 0};  // 指定用于渲染的区域
    render_pass_info.renderArea.extent = m_swap_chain_extent;
    render_pass_info.clearValueCount =
      static_cast<uint32_t>(clear_values.size());  // 指定使用 VK_ATTACHMENT_LOAD_OP_CLEAR 标记后使用的清除值
    render_pass_info.pClearValues = clear_values.data();

    VkViewport viewport = {};
    viewport.x = 0.F;
    viewport.y = 0.F;
    viewport.width = static_cast<float>(m_swap_chain_extent.width);
    viewport.height = static_cast<float>(m_swap_chain_extent.height);
    viewport.minDepth = 0.F;
    viewport.maxDepth = 1.F;

    VkRect2D scissor = {};
    scissor.offset = {0, 0};
    scissor.extent = m_swap_chain_extent;
    const std::array<VkDeviceSize, 1> offsets{0};

    //--------------------------------------------------------------------
    // 第一次 RenderPass
    vkCmdBeginRenderPass(command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphics_pipeline);
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);
    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline_layout, 0, 1,
                            &m_descriptor_sets.at(m_current_frame), 0, nullptr);

    // 绘制立方体
    const std::array<VkBuffer, 1> vertex_buffers_cube{m_drawable_cube.vertex_buffer};
    vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers_cube.data(), offsets.data());
    vkCmdBindIndexBuffer(command_buffer, m_drawable_cube.index_buffer, 0, VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed(command_buffer, m_drawable_cube.index_count, 1, 0, 0, 0);

    vkCmdEndRenderPass(command_buffer);

    //--------------------------------------------------------------------
    // 第二次 RenderPass
    vkCmdBeginRenderPass(command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphics_pipeline);
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);
    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline_layout, 0, 1,
                            &m_descriptor_sets.at(m_current_frame), 0, nullptr);

    // 绘制平面
    const std::array<VkBuffer, 1> vertex_buffers_plane{m_drawable_plane.vertex_buffer};
    vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers_plane.data(), offsets.data());
    vkCmdBindIndexBuffer(command_buffer, m_drawable_plane.index_buffer, 0, VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed(command_buffer, m_drawable_plane.index_count, 1, 0, 0, 0);

    vkCmdEndRenderPass(command_buffer);

    // 结束记录指令到指令缓冲
    if (VK_SUCCESS != vkEndCommandBuffer(command_buffer)) {
      throw std::runtime_error("failed to record command buffer");
    }
  }

  /// @brief 绘制每一帧
  /// @details 流程：从交换链获取一张图像、对帧缓冲附着执行指令缓冲中的渲染指令、返回渲染后的图像到交换链进行呈现操作
  void DrawFrame() {
    // 等待一组栅栏中的一个或全部栅栏发出信号，即上一次提交的指令结束执行
    // 使用栅栏可以进行CPU与GPU之间的同步，防止超过 k_max_frames_in_flight 帧的指令同时被提交执行
    vkWaitForFences(m_device, 1, &m_in_flight_fences.at(m_current_frame), VK_TRUE,
                    std::numeric_limits<uint64_t>::max());

    // 从交换链获取一张图像
    uint32_t image_index{0};
    // 3.获取图像的超时时间，此处禁用图像获取超时
    // 4.通知的同步对象
    // 5.输出可用的交换链图像的索引
    VkResult result =
      vkAcquireNextImageKHR(m_device, m_swap_chain, std::numeric_limits<uint64_t>::max(),
                            m_image_available_semaphores.at(m_current_frame), VK_NULL_HANDLE, &image_index);

    // VK_ERROR_OUT_OF_DATE_KHR 交换链不能继续使用，通常发生在窗口大小改变后
    // VK_SUBOPTIMAL_KHR 交换链仍然可以使用，但表面属性已经不能准确匹配
    if (VK_ERROR_OUT_OF_DATE_KHR == result) {
      RecreateSwapChain();
      return;
    }
    if (VK_SUCCESS != result && VK_SUBOPTIMAL_KHR != result) {
      throw std::runtime_error("failed to acquire swap chain image");
    }

    // 每一帧都更新uniform
    UpdateUniformBuffer(static_cast<uint32_t>(m_current_frame));

    // 手动将栅栏重置为未发出信号的状态（必须手动设置）
    vkResetFences(m_device, 1, &m_in_flight_fences.at(m_current_frame));
    vkResetCommandBuffer(m_command_buffers.at(image_index), 0);
    RecordCommandBuffer(m_command_buffers.at(image_index), m_swap_chain_framebuffers.at(image_index));

    const std::array<VkPipelineStageFlags, 1> wait_stages{VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores =
      &m_image_available_semaphores.at(m_current_frame);  // 指定队列开始执行前需要等待的信号量
    submit_info.pWaitDstStageMask = wait_stages.data();   // 指定需要等待的管线阶段
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &m_command_buffers[image_index];  // 指定实际被提交执行的指令缓冲对象
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores =
      &m_render_finished_semaphores.at(image_index);  // 指定在指令缓冲执行结束后发出信号的信号量对象

    // 提交指令缓冲给图形指令队列
    // 如果不等待上一次提交的指令结束执行，可能会导致内存泄漏
    // 1.vkQueueWaitIdle 可以等待上一次的指令结束执行，2.也可以同时渲染多帧解决该问题（使用栅栏）
    // vkQueueSubmit 的最后一个参数用来指定在指令缓冲执行结束后需要发起信号的栅栏对象
    if (VK_SUCCESS != vkQueueSubmit(m_graphics_queue, 1, &submit_info, m_in_flight_fences.at(m_current_frame))) {
      throw std::runtime_error("failed to submit draw command buffer");
    }

    VkPresentInfoKHR present_info = {};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &m_render_finished_semaphores.at(image_index);  // 指定开始呈现操作需要等待的信号量
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &m_swap_chain;   // 指定用于呈现图像的交换链
    present_info.pImageIndices = &image_index;  // 指定需要呈现的图像在交换链中的索引
    present_info.pResults = nullptr;            // 可以通过该变量获取每个交换链的呈现操作是否成功的信息

    // 请求交换链进行图像呈现操作
    result = vkQueuePresentKHR(m_present_queue, &present_info);

    if (VK_ERROR_OUT_OF_DATE_KHR == result || VK_SUBOPTIMAL_KHR == result || m_framebuffer_resized) {
      m_framebuffer_resized = false;
      // 交换链不完全匹配时也重建交换链
      RecreateSwapChain();
    } else if (VK_SUCCESS != result) {
      throw std::runtime_error("failed to presend swap chain image");
    }

    // 更新当前帧索引
    m_current_frame = (m_current_frame + 1) % k_max_frames_in_flight;
  }

  /// @brief 创建同步对象，用于发出图像已经被获取可以开始渲染和渲染已经结束可以开始呈现的信号
  void CreateSyncObjects() {
    m_image_available_semaphores.resize(k_max_frames_in_flight);
    m_render_finished_semaphores.resize(m_swap_chain_images.size());
    m_in_flight_fences.resize(k_max_frames_in_flight);

    VkSemaphoreCreateInfo semaphore_info = {};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fence_info = {};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;  // 初始状态设置为已发出信号，避免 vkWaitForFences 一直等待

    for (size_t i = 0; i < k_max_frames_in_flight; ++i) {
      if (VK_SUCCESS != vkCreateSemaphore(m_device, &semaphore_info, nullptr, &m_image_available_semaphores.at(i)) ||
          vkCreateFence(m_device, &fence_info, nullptr, &m_in_flight_fences.at(i))) {
        throw std::runtime_error("failed to create synchronization objects for a frame");
      }
    }
    for (auto& semaphore : m_render_finished_semaphores) {
      if (VK_SUCCESS != vkCreateSemaphore(m_device, &semaphore_info, nullptr, &semaphore)) {
        throw std::runtime_error("failed to create render finished semaphore");
      }
    }
  }

  /// @brief 重建交换链
  void RecreateSwapChain() {
    int width{0};
    int height{0};
    glfwGetFramebufferSize(m_window, &width, &height);
    while (0 == width || 0 == height) {
      glfwGetFramebufferSize(m_window, &width, &height);
      glfwWaitEvents();
    }

    // 等待设备处于空闲状态，避免在对象的使用过程中将其清除重建
    vkDeviceWaitIdle(m_device);

    // 在重建前清理之前使用的对象
    CleanupSwapChain();

    // 可以通过动态状态来设置视口和裁剪矩形来避免重建管线
    CreateSwapChain();
    CreateImageViews();
    CreateDepthResources();
    CreateFramebuffers();
  }

  void DestroyDrawable(Drawable& drawable) noexcept {
    vkDestroyBuffer(m_device, drawable.vertex_buffer, nullptr);
    vkFreeMemory(m_device, drawable.vertex_buffer_memory, nullptr);
    vkDestroyBuffer(m_device, drawable.index_buffer, nullptr);
    vkFreeMemory(m_device, drawable.index_buffer_memory, nullptr);
  }

  /// @brief 清除交换链相关对象
  void CleanupSwapChain() noexcept {
    vkDestroyImageView(m_device, m_depth_image_view, nullptr);
    vkDestroyImage(m_device, m_depth_image, nullptr);
    vkFreeMemory(m_device, m_depth_image_memory, nullptr);

    for (auto framebuffer : m_swap_chain_framebuffers) {
      vkDestroyFramebuffer(m_device, framebuffer, nullptr);
    }

    for (auto image_view : m_swap_chain_image_views) {
      vkDestroyImageView(m_device, image_view, nullptr);
    }

    vkDestroySwapchainKHR(m_device, m_swap_chain, nullptr);
  }

  /// @brief 设置调试扩展信息
  /// @param create_info
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

  /// @brief 创建顶点缓冲
  void CreateVertexBuffer(const std::vector<Vertex>& vertices, Drawable& drawable) {
    VkDeviceSize buffer_size = sizeof(Vertex) * vertices.size();

    // 为了提升性能，使用一个临时（暂存）缓冲，先将顶点数据加载到临时缓冲，再复制到顶点缓冲
    VkBuffer staging_buffer{};
    VkDeviceMemory staging_buffer_memory{};

    // 创建一个CPU可见的缓冲作为临时缓冲
    // VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT 用于从CPU写入数据
    // VK_MEMORY_PROPERTY_HOST_COHERENT_BIT 可以保证数据被立即复制到缓冲关联的内存
    CreateBuffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging_buffer,
                 staging_buffer_memory);

    void* data{nullptr};
    // 将缓冲关联的内存映射到CPU可以访问的内存
    vkMapMemory(m_device, staging_buffer_memory, 0, buffer_size, 0, &data);
    // 将顶点数据复制到映射后的内存
    std::memcpy(data, vertices.data(), static_cast<size_t>(buffer_size));
    // 结束内存映射
    vkUnmapMemory(m_device, staging_buffer_memory);

    // 创建一个显卡读取较快的缓冲作为真正的顶点缓冲
    // 具有 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT 标记的内存最适合显卡读取，CPU通常不能访问这种类型的内存
    CreateBuffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, drawable.vertex_buffer, drawable.vertex_buffer_memory);

    // m_vertexBuffer 现在关联的内存是设备所有的（显卡），不能使用 vkMapMemory 函数对它关联的内存进行映射
    // 从临时（暂存）缓冲复制数据到显卡读取较快的缓冲中
    CopyBuffer(staging_buffer, drawable.vertex_buffer, buffer_size);

    vkDestroyBuffer(m_device, staging_buffer, nullptr);
    vkFreeMemory(m_device, staging_buffer_memory, nullptr);

    // 驱动程序可能并不会立即复制数据到缓冲关联的内存中去，因为处理器都有缓存，写入内存的数据并不一定在多个核心同时可见
    // 保证数据被立即复制到缓冲关联的内存可以使用以下函数，或者使用 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT 属性的内存类型
    // 使用函数的方法性能更好
    // 写入数据到映射的内存后，调用 vkFlushMappedMemoryRanges
    // 读取映射的内存数据前，调用 vkInvalidateMappedMemoryRanges
  }

  /// @brief 创建索引缓冲
  void CreateIndexBuffer(const std::vector<uint16_t>& indices, Drawable& drawable) {
    drawable.index_count = static_cast<uint32_t>(indices.size());
    VkDeviceSize buffer_size = sizeof(indices.front()) * indices.size();

    VkBuffer staging_buffer{};
    VkDeviceMemory staging_buffer_memory{};

    CreateBuffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging_buffer,
                 staging_buffer_memory);

    void* data{nullptr};
    vkMapMemory(m_device, staging_buffer_memory, 0, buffer_size, 0, &data);
    std::memcpy(data, indices.data(), static_cast<size_t>(buffer_size));
    vkUnmapMemory(m_device, staging_buffer_memory);

    CreateBuffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, drawable.index_buffer, drawable.index_buffer_memory);

    CopyBuffer(staging_buffer, drawable.index_buffer, buffer_size);

    vkDestroyBuffer(m_device, staging_buffer, nullptr);
    vkFreeMemory(m_device, staging_buffer_memory, nullptr);
  }

  /// @brief 创建指定类型的缓冲
  /// @details Vulkan 的缓冲是可以存储任意数据的可以被显卡读取的内存，不仅可以存储顶点数据
  /// @param size
  /// @param usage
  /// @param properties
  /// @param buffer
  /// @param buffer_memory
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

    // 4. 偏移值，需要满足能够被 mem_requirements.alighment 整除
    vkBindBufferMemory(m_device, buffer, buffer_memory, 0);
  }

  /// @brief 在缓冲之间复制数据
  /// @param src_buffer
  /// @param dst_buffer
  /// @param size
  void CopyBuffer(VkBuffer src_buffer, VkBuffer dst_buffer, VkDeviceSize size) const noexcept {
    VkCommandBufferAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandPool = m_command_pool;
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
    vkQueueSubmit(m_graphics_queue, 1, &submit_info, nullptr);
    // 等待传输操作完成，也可以使用栅栏，栅栏可以同步多个不同的内存传输操作，给驱动程序的优化空间也更大
    vkQueueWaitIdle(m_graphics_queue);

    vkFreeCommandBuffers(m_device, m_command_pool, 1, &command_buffer);
  }

  VkImageView CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspect_flags) {
    VkImageViewCreateInfo view_info{};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = image;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = format;
    view_info.subresourceRange.aspectMask = aspect_flags;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;

    VkImageView image_view{};
    if (VK_SUCCESS != vkCreateImageView(m_device, &view_info, nullptr, &image_view)) {
      throw std::runtime_error("failed to create texture image view");
    }

    return image_view;
  }

  /// @brief 查找最合适的内存类型
  /// @details 不同类型的内存所允许进行的操作以及操作的效率有所不同
  /// @param type_filter 指定需要的内存类型的位域
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

  /// @brief 创建着色器使用的每一个描述符绑定信息
  void CreateDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding ubo_layout_binding = {};
    ubo_layout_binding.binding = 0;
    ubo_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ubo_layout_binding.descriptorCount = 1;                      // uniform 缓冲对象数组的大小
    ubo_layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;  // 指定在哪一个着色器阶段使用
    ubo_layout_binding.pImmutableSamplers = nullptr;             // 指定图像采样相关的属性

    VkDescriptorSetLayoutCreateInfo layout_info = {};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = 1;
    layout_info.pBindings = &ubo_layout_binding;

    if (VK_SUCCESS != vkCreateDescriptorSetLayout(m_device, &layout_info, nullptr, &m_descriptor_set_layout)) {
      throw std::runtime_error("failed to create descriptor set layout");
    }
  }

  /// @brief 创建Uniform Buffer
  /// @details 由于缓冲需要频繁更新，所以此处使用暂存缓冲并不会带来性能提升
  void CreateUniformBuffers() {
    VkDeviceSize buffer_size = sizeof(UniformBufferObject);

    m_uniform_buffers.resize(k_max_frames_in_flight);
    m_uniform_buffers_memory.resize(k_max_frames_in_flight);
    m_uniform_buffers_mapped.resize(k_max_frames_in_flight);

    for (size_t i = 0; i < k_max_frames_in_flight; ++i) {
      CreateBuffer(buffer_size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m_uniform_buffers.at(i),
                   m_uniform_buffers_memory.at(i));
      vkMapMemory(m_device, m_uniform_buffers_memory.at(i), 0, buffer_size, 0, &m_uniform_buffers_mapped.at(i));
    }
  }

  /// @brief 在绘制每一帧时更新uniform
  /// @param current_image
  void UpdateUniformBuffer(uint32_t current_image) {
    auto aspect = static_cast<float>(m_swap_chain_extent.width) / static_cast<float>(m_swap_chain_extent.height);

    UniformBufferObject ubo{};
    ubo.model = glm::mat4(1.F);
    ubo.view = glm::lookAt(m_eye_pos, m_look_at, m_view_up);
    ubo.proj = glm::perspective(glm::radians(45.F), aspect, 0.1F, 100.F);

    // glm的裁剪坐标的Y轴和 Vulkan 是相反的
    ubo.proj[1][1] *= -1;

    // 将变换矩阵的数据复制到uniform缓冲
    std::memcpy(m_uniform_buffers_mapped[current_image], &ubo, sizeof(ubo));
  }

  /// @brief 创建描述符池，描述符集需要通过描述符池来创建
  void CreateDescriptorPool() {
    VkDescriptorPoolSize pool_size{};
    pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    pool_size.descriptorCount = static_cast<uint32_t>(k_max_frames_in_flight);

    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.poolSizeCount = 1;
    pool_info.pPoolSizes = &pool_size;
    pool_info.maxSets = static_cast<uint32_t>(k_max_frames_in_flight);  // 指定可以分配的最大描述符集个数
    pool_info.flags = 0;  // 可以用来设置独立的描述符集是否可以被清除掉，此处使用默认值

    if (VK_SUCCESS != vkCreateDescriptorPool(m_device, &pool_info, nullptr, &m_descriptor_pool)) {
      throw std::runtime_error("failed to create descriptor pool");
    }
  }

  /// @brief 创建描述符集
  void CreateDescriptorSets() {
    // 描述符布局对象的个数要匹配描述符集对象的个数
    std::vector<VkDescriptorSetLayout> layouts(k_max_frames_in_flight, m_descriptor_set_layout);

    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = m_descriptor_pool;  // 指定分配描述符集对象的描述符池
    alloc_info.descriptorSetCount = static_cast<uint32_t>(k_max_frames_in_flight);
    alloc_info.pSetLayouts = layouts.data();

    // 描述符集对象会在描述符池对象清除时自动被清除
    m_descriptor_sets.resize(k_max_frames_in_flight);
    if (VK_SUCCESS != vkAllocateDescriptorSets(m_device, &alloc_info, m_descriptor_sets.data())) {
      throw std::runtime_error("failed to allocate descriptor sets");
    }

    for (size_t i = 0; i < k_max_frames_in_flight; ++i) {
      VkDescriptorBufferInfo buffer_info{};
      buffer_info.buffer = m_uniform_buffers.at(i);
      buffer_info.offset = 0;
      buffer_info.range = sizeof(UniformBufferObject);

      VkWriteDescriptorSet descriptor_write{};
      descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      descriptor_write.dstSet = m_descriptor_sets.at(i);  // 指定要更新的描述符集对象
      descriptor_write.dstBinding = 0;                    // 指定缓冲绑定
      descriptor_write.dstArrayElement = 0;               // 描述符数组的第一个元素的索引（没有数组就使用0）
      descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
      descriptor_write.descriptorCount = 1;
      descriptor_write.pBufferInfo = &buffer_info;  // 指定描述符引用的缓冲数据
      descriptor_write.pImageInfo = nullptr;        // 指定描述符引用的图像数据
      descriptor_write.pTexelBufferView = nullptr;  // 指定描述符引用的缓冲视图

      // 更新描述符的配置
      vkUpdateDescriptorSets(m_device, 1, &descriptor_write, 0, nullptr);
    }
  }

  /// @brief 创建指定格式的图像对象
  /// @param width
  /// @param height
  /// @param format
  /// @param tiling
  /// @param usage
  /// @param properties
  /// @param image
  /// @param image_memory
  void CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage,
                   VkMemoryPropertyFlags /*properties*/, VkImage& image, VkDeviceMemory& image_memory)

  {
    // tiling成员变量可以是 VK_IMAGE_TILING_LINEAR 纹素以行主序的方式排列，可以直接访问图像
    //                     VK_IMAGE_TILING_OPTIMAL 纹素以一种对访问优化的方式排列
    // initialLayout成员变量可以是 VK_IMAGE_LAYOUT_UNDEFINED GPU不可用，纹素在第一次变换会被丢弃
    //                            VK_IMAGE_LAYOUT_PREINITIALIZED GPU不可用，纹素在第一次变换会被保留
    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.extent.width = static_cast<uint32_t>(width);
    image_info.extent.height = static_cast<uint32_t>(height);
    image_info.extent.depth = 1;
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.format = format;
    image_info.tiling = tiling;                            // 设置之后不可修改
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;  // 图像数据是接收方，不需要保留第一次变换时的纹理数据
    image_info.usage = usage;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;  // 只被一个队列族使用（支持传输操作的队列族），所以使用独占模式
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;          // 设置多重采样，只对用作附着的图像对象有效
    image_info.flags = 0;  // 可以用来设置稀疏图像的优化，比如体素地形没必要为“空气”部分分配内存

    if (VK_SUCCESS != vkCreateImage(m_device, &image_info, nullptr, &image)) {
      throw std::runtime_error("failed to create image");
    }

    VkMemoryRequirements mem_requirements{};
    vkGetImageMemoryRequirements(m_device, image, &mem_requirements);

    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_requirements.size;
    alloc_info.memoryTypeIndex = FindMemoryType(mem_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (VK_SUCCESS != vkAllocateMemory(m_device, &alloc_info, nullptr, &image_memory)) {
      throw std::runtime_error("failed to allocate image memory");
    }
    vkBindImageMemory(m_device, image, image_memory, 0);
  }

  /// @brief 配置深度图像需要的资源
  void CreateDepthResources() {
    auto depth_format = FindDepthFormat();

    CreateImage(m_swap_chain_extent.width, m_swap_chain_extent.height, depth_format, VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_depth_image,
                m_depth_image_memory);

    m_depth_image_view = CreateImageView(m_depth_image, depth_format, VK_IMAGE_ASPECT_DEPTH_BIT);
  }

  /// @brief 查找一个符合要求又被设备支持的图像数据格式
  /// @param candidates
  /// @param tiling
  /// @param features
  /// @return
  VkFormat FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling,
                               VkFormatFeatureFlags features) const {
    for (auto format : candidates) {
      // 查找可用的深度图像格式
      VkFormatProperties props{};
      vkGetPhysicalDeviceFormatProperties(m_physical_device, format, &props);
      // props.bufferFeatures        表示数据格式支持缓冲
      // props.linearTilingFeatures  表示数据格式支持线性tiling模式
      // props.optimalTilingFeatures 表示数据格式支持优化tiling模式

      if ((VK_IMAGE_TILING_LINEAR == tiling && (props.linearTilingFeatures & features) == features) ||
          (VK_IMAGE_TILING_OPTIMAL == tiling && (props.optimalTilingFeatures & features) == features)) {
        return format;
      }
    }

    throw std::runtime_error("failed to find supported format");
  }

  /// @brief 查找适合作为深度附着的图像数据格式
  /// @return
  VkFormat FindDepthFormat() const {
    return FindSupportedFormat({VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
                               VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
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
  /// @param p_create_info
  /// @param p_allocator
  /// @param p_callback
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
  /// @param p_allocator
  static void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT callback,
                                            const VkAllocationCallbacks* p_allocator) noexcept {
    auto func = lvk_tidy::LoadInstanceProcAddress<PFN_vkDestroyDebugUtilsMessengerEXT>(
      instance, "vkDestroyDebugUtilsMessengerEXT");

    if (nullptr != func) {
      func(instance, callback, p_allocator);
    }
  }

  /// @brief 读取二进制着色器文件
  /// @param file_name
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

  static void FramebufferResizeCallback(GLFWwindow* window, int /*width*/, int /*height*/) noexcept {
    if (auto app = reinterpret_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(window))) {
      app->m_framebuffer_resized = true;
    }
  }

  /// @brief 光标位置回调
  /// @param window
  /// @param xpos
  /// @param ypos
  static void CursorPosCallback(GLFWwindow* window, double xpos, double ypos) noexcept {
    if (auto app = reinterpret_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(window))) {
      // 计算鼠标移动的偏移量
      double x_offset = xpos - app->m_mouse_last_x;
      double y_offset = ypos - app->m_mouse_last_y;

      if (app->m_is_rotating) {
        // 设置灵敏度，可以根据需要调整
        constexpr auto sensitivity = 0.5 / 180.0 * std::numbers::pi_v<double>;
        x_offset *= sensitivity;
        y_offset *= sensitivity;

        // 构造旋转四元数
        glm::vec3 x_axis = glm::normalize(glm::cross(app->m_view_up, app->m_eye_pos));
        glm::vec3 y_axis = glm::normalize(app->m_view_up);
        glm::quat x_rotation = glm::angleAxis(static_cast<float>(-y_offset), x_axis);
        glm::quat y_rotation = glm::angleAxis(static_cast<float>(-x_offset), y_axis);
        glm::quat rotation_quat = x_rotation * y_rotation;

        app->m_eye_pos = glm::vec3(glm::mat4(rotation_quat) * glm::vec4(app->m_eye_pos, 1.F));
        app->m_view_up = glm::vec3(glm::mat4(rotation_quat) * glm::vec4(app->m_view_up, 1.F));
        // app->m_view_up = glm::normalize(glm::cross(x_axis, -app->m_eye_pos));
      }

      if (app->m_is_translating) {
        x_offset /= (app->m_swap_chain_extent.width / 2);
        y_offset /= (app->m_swap_chain_extent.height / 2);

        glm::vec3 x_axis = glm::normalize(glm::cross(app->m_view_up, app->m_eye_pos));
        glm::vec3 y_axis = glm::normalize(app->m_view_up);
        glm::mat4 x_translation = glm::translate(glm::mat4(1.F), x_axis * static_cast<float>(-x_offset));
        glm::mat4 y_translation = glm::translate(glm::mat4(1.F), y_axis * static_cast<float>(y_offset));
        glm::mat4 translation = x_translation * y_translation;

        app->m_eye_pos = glm::vec3(translation * glm::vec4(app->m_eye_pos, 1.F));
        app->m_look_at = glm::vec3(translation * glm::vec4(app->m_look_at, 1.F));
      }

      // 更新上一次的鼠标位置
      app->m_mouse_last_x = xpos;
      app->m_mouse_last_y = ypos;
    }
  }

  /// @brief 鼠标滚轮滚动回调
  /// @param window
  /// @param xoffset
  /// @param yoffset 小于0向前滚动，大于0向后滚动
  static void ScrollCallback(GLFWwindow* window, double /*xoffset*/, double yoffset) noexcept {
    if (auto app = reinterpret_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(window))) {
      glm::vec3 direction = glm::normalize(app->m_eye_pos - app->m_look_at);
      float distance = glm::distance(app->m_eye_pos, app->m_look_at);
      glm::vec3 translate = direction * distance * 0.1F;

      if (yoffset < 0.0) {
        glm::mat4 mat = glm::translate(glm::mat4(1.F), translate);
        app->m_eye_pos = glm::vec3(mat * glm::vec4(app->m_eye_pos, 1.F));
      } else if (yoffset > 0.0) {
        glm::mat4 mat = glm::translate(glm::mat4(1.F), translate * -1.F);
        app->m_eye_pos = glm::vec3(mat * glm::vec4(app->m_eye_pos, 1.F));
      }
    }
  }

  /// @brief 鼠标按钮操作回调
  /// @param window
  /// @param button
  /// @param action
  /// @param mods
  static void MouseButtonCallback(GLFWwindow* window, int button, int action, int /*mods*/) noexcept {
    if (auto app = reinterpret_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(window))) {
      if (GLFW_PRESS == action && GLFW_MOUSE_BUTTON_RIGHT == button) {
        app->m_is_rotating = true;
        glfwGetCursorPos(window, &app->m_mouse_last_x, &app->m_mouse_last_y);
      } else if (GLFW_RELEASE == action && GLFW_MOUSE_BUTTON_RIGHT == button) {
        app->m_is_rotating = false;
      }

      if (GLFW_PRESS == action && GLFW_MOUSE_BUTTON_LEFT == button) {
        app->m_is_translating = true;
        glfwGetCursorPos(window, &app->m_mouse_last_x, &app->m_mouse_last_y);
      } else if (GLFW_RELEASE == action && GLFW_MOUSE_BUTTON_LEFT == button) {
        app->m_is_translating = false;
      }
    }
  }

  /// @brief 键盘操作回调
  /// @param window
  /// @param key
  /// @param scancode
  /// @param action
  /// @param mods
  static void KeyCallback(GLFWwindow* window, int key, int /*scancode*/, int action, int /*mods*/) noexcept {
    if (auto app = reinterpret_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(window))) {
      // 按下'r'/'R'恢复视角
      if (GLFW_PRESS == action && GLFW_KEY_R == key) {
        app->m_view_up = glm::vec3{0.F, 1.F, 0.F};
        app->m_eye_pos = glm::vec3{0.F, 0.F, -3.F};
        app->m_look_at = glm::vec3{0.F};
      }
    }
  }

private:
  GLFWwindow* m_window{nullptr};
  VkInstance m_instance{nullptr};
  VkDebugUtilsMessengerEXT m_debug_messenger{nullptr};
  VkPhysicalDevice m_physical_device{nullptr};
  VkDevice m_device{nullptr};
  VkQueue m_graphics_queue{nullptr};  // 图形队列
  VkSurfaceKHR m_surface{nullptr};
  VkQueue m_present_queue{nullptr};  // 呈现队列
  VkSwapchainKHR m_swap_chain{nullptr};
  std::vector<VkImage> m_swap_chain_images{};
  VkFormat m_swap_chain_image_format{};
  VkExtent2D m_swap_chain_extent{};
  std::vector<VkImageView> m_swap_chain_image_views{};
  VkRenderPass m_render_pass{nullptr};
  VkPipelineLayout m_pipeline_layout{nullptr};
  VkPipeline m_graphics_pipeline{nullptr};
  std::vector<VkFramebuffer> m_swap_chain_framebuffers{};
  VkCommandPool m_command_pool{nullptr};
  std::vector<VkCommandBuffer> m_command_buffers{};

  std::vector<VkSemaphore> m_image_available_semaphores{};
  std::vector<VkSemaphore> m_render_finished_semaphores{};
  std::vector<VkFence> m_in_flight_fences{};
  size_t m_current_frame{0};
  bool m_framebuffer_resized{false};

  Drawable m_drawable_cube{};
  Drawable m_drawable_plane{};

  VkDescriptorSetLayout m_descriptor_set_layout{nullptr};
  std::vector<VkBuffer> m_uniform_buffers{};
  std::vector<VkDeviceMemory> m_uniform_buffers_memory{};
  std::vector<void*> m_uniform_buffers_mapped{};
  VkDescriptorPool m_descriptor_pool{nullptr};
  std::vector<VkDescriptorSet> m_descriptor_sets{};

  VkImage m_depth_image{nullptr};
  VkDeviceMemory m_depth_image_memory{nullptr};
  VkImageView m_depth_image_view{nullptr};

  glm::vec3 m_view_up{0.F, 1.F, 0.F};
  glm::vec3 m_eye_pos{0.F, 0.F, -3.F};
  glm::vec3 m_look_at{0.F};
  double m_mouse_last_x{0.0};
  double m_mouse_last_y{0.0};
  bool m_is_rotating{false};
  bool m_is_translating{false};
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

#endif  // TEST1

#ifdef TEST2

#define GLFW_INCLUDE_VULKAN  // 定义这个宏之后 glfw3.h 文件就会包含 Vulkan 的头文件
#include <GLFW/glfw3.h>

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

// 窗口默认大小
constexpr uint32_t k_width = 800;
constexpr uint32_t k_height = 600;

// 同时并行处理的帧数
constexpr int k_max_frames_in_flight = 2;

// 需要开启的校验层的名称
const std::vector<const char*> k_validation_layers = {"VK_LAYER_KHRONOS_validation"};
// 交换链扩展
const std::vector<const char*> k_device_extensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

// 是否启用校验层
#ifdef NDEBUG
const bool k_enable_validation_layers = false;
#else
const bool k_enable_validation_layers = true;
#endif  // NDEBUG

struct Vertex {
  glm::vec3 pos{0.F, 0.F, 0.F};
  glm::vec3 color{0.F, 0.F, 0.F};

  static constexpr VkVertexInputBindingDescription GetBindingDescription() noexcept {
    VkVertexInputBindingDescription binding_description{};

    binding_description.binding = 0;
    binding_description.stride = sizeof(Vertex);
    binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    return binding_description;
  }

  static constexpr std::array<VkVertexInputAttributeDescription, 2> GetAttributeDescriptions() noexcept {
    static_assert(std::is_standard_layout_v<Vertex>, "Vertex must be standard layout for offsetof");
    std::array<VkVertexInputAttributeDescription, 2> attribute_descriptions{};

    attribute_descriptions[0].binding = 0;
    attribute_descriptions[0].location = 0;
    attribute_descriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attribute_descriptions[0].offset = offsetof(Vertex, pos);

    attribute_descriptions[1].binding = 0;
    attribute_descriptions[1].location = 1;
    attribute_descriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attribute_descriptions[1].offset = offsetof(Vertex, color);

    return attribute_descriptions;
  }
};

struct UniformBufferObject {
  glm::mat4 model{glm::mat4(1.F)};
  glm::mat4 view{glm::mat4(1.F)};
  glm::mat4 proj{glm::mat4(1.F)};
};

/// @brief 支持图形和呈现的队列族
struct QueueFamilyIndices {
  std::optional<uint32_t> graphics_family{};
  std::optional<uint32_t> present_family{};

  constexpr bool IsComplete() const noexcept { return graphics_family.has_value() && present_family.has_value(); }
};

/// @brief 交换链属性信息
struct SwapChainSupportDetails {
  VkSurfaceCapabilitiesKHR capabilities{};
  std::vector<VkSurfaceFormatKHR> formats;
  std::vector<VkPresentModeKHR> present_modes;
};

struct Drawable {
  VkBuffer vertex_buffer{nullptr};
  VkDeviceMemory vertex_buffer_memory{nullptr};
  VkBuffer index_buffer{nullptr};
  VkDeviceMemory index_buffer_memory{nullptr};
  uint32_t index_count{0};
};

// clang-format off
const std::vector<Vertex> k_vertices_cube {
    {{-0.5F,  0.5F, -0.5F}, {1.F, 0.F, 0.F}},
    {{ 0.5F,  0.5F, -0.5F}, {1.F, 1.F, 0.F}},
    {{ 0.5F, -0.5F, -0.5F}, {0.F, 1.F, 0.F}},
    {{-0.5F, -0.5F, -0.5F}, {0.F, 1.F, 1.F}},

    {{-0.5F,  0.5F,  0.5F}, {0.F, 0.F, 1.F}},
    {{ 0.5F,  0.5F,  0.5F}, {1.F, 0.F, 1.F}},
    {{ 0.5F, -0.5F,  0.5F}, {0.F, 0.F, 0.F}},
    {{-0.5F, -0.5F,  0.5F}, {1.F, 1.F, 1.F}},
};

const std::vector<uint16_t> k_indices_cube {
    0, 1, 2, 0, 2, 3, // 前
    1, 5, 6, 1, 6, 2, // 右
    5, 4, 7, 5, 7, 6, // 后
    4, 0, 3, 4, 3, 7, // 左
    3, 2, 6, 3, 6, 7, // 上
    4, 5, 1, 4, 1, 0, // 下
};

const std::vector<Vertex> k_vertices_plane {
    { {-0.7F, -0.7F,  0.0F }, { 1.F, 1.F, 1.F } },
    { {-0.7F,  0.7F,  0.0F }, { 1.F, 1.F, 1.F } },
    { { 0.7F,  0.7F,  0.0F }, { 1.F, 1.F, 1.F } },
    { { 0.7F, -0.7F,  0.0F }, { 1.F, 1.F, 1.F } },
};

const std::vector<uint16_t> k_indices_plane {
    0, 1, 2, 0, 2, 3
};

// clang-format on

class HelloTriangleApplication {
public:
  void Run() {
    InitWindow();
    InitVulkan();
    MainLoop();
    Cleanup();
  }

private:
  void InitWindow() {
    if (GLFW_FALSE == glfwInit()) {
      throw std::runtime_error("failed to init GLFW");
    }

    // 禁止 glfw 创建 OpenGL 上下文
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    m_window = glfwCreateWindow(k_width, k_height, "Vulkan", nullptr, nullptr);

    glfwSetWindowUserPointer(m_window, this);
    glfwSetFramebufferSizeCallback(m_window, FramebufferResizeCallback);
    glfwSetKeyCallback(m_window, KeyCallback);
    glfwSetCursorPosCallback(m_window, CursorPosCallback);
    glfwSetScrollCallback(m_window, ScrollCallback);
    glfwSetMouseButtonCallback(m_window, MouseButtonCallback);
  }

  void InitVulkan() {
    CreateInstance();
    SetupDebugCallback();
    CreateSurface();
    PickPhysicalDevice();
    CreateLogicalDevice();
    CreateSwapChain();
    CreateImageViews();
    CreateRenderPass();
    CreateDescriptorSetLayout();
    CreateGraphicsPipeline();
    CreateCommandPool();
    CreateDepthResources();
    CreateFramebuffers();
    CreateVertexBuffer(k_vertices_cube, m_drawable_cube);
    CreateIndexBuffer(k_indices_cube, m_drawable_cube);
    CreateVertexBuffer(k_vertices_plane, m_drawable_plane);
    CreateIndexBuffer(k_indices_plane, m_drawable_plane);
    CreateUniformBuffers();
    CreateDescriptorPool();
    CreateDescriptorSets();
    CreateCommandBuffers();
    CreateSyncObjects();
  }

  void MainLoop() {
    while (!glfwWindowShouldClose(m_window)) {
      glfwPollEvents();
      DrawFrame();
    }

    // 等待逻辑设备的操作结束执行
    // DrawFrame 函数中的操作是异步执行的，关闭窗口跳出while循环时，绘制操作和呈现操作可能仍在执行，不能进行清除操作
    vkDeviceWaitIdle(m_device);
  }

  void Cleanup() noexcept {
    CleanupSwapChain();

    DestroyDrawable(m_drawable_cube);
    DestroyDrawable(m_drawable_plane);

    vkDestroyPipeline(m_device, m_graphics_pipeline, nullptr);
    vkDestroyPipelineLayout(m_device, m_pipeline_layout, nullptr);
    vkDestroyRenderPass(m_device, m_render_pass, nullptr);

    for (size_t i = 0; i < k_max_frames_in_flight; ++i) {
      vkDestroyBuffer(m_device, m_uniform_buffers.at(i), nullptr);
      vkFreeMemory(m_device, m_uniform_buffers_memory.at(i), nullptr);
    }

    vkDestroyDescriptorPool(m_device, m_descriptor_pool, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_descriptor_set_layout, nullptr);

    for (size_t i = 0; i < k_max_frames_in_flight; ++i) {
      vkDestroySemaphore(m_device, m_image_available_semaphores.at(i), nullptr);
      vkDestroyFence(m_device, m_in_flight_fences.at(i), nullptr);
    }
    for (auto semaphore : m_render_finished_semaphores) {
      vkDestroySemaphore(m_device, semaphore, nullptr);
    }

    vkDestroyCommandPool(m_device, m_command_pool, nullptr);
    vkDestroyDevice(m_device, nullptr);

    if (k_enable_validation_layers) {
      DestroyDebugUtilsMessengerEXT(m_instance, m_debug_messenger, nullptr);
    }

    vkDestroySurfaceKHR(m_instance, m_surface, nullptr);  // 清理表面对象，必须在 Vulkan 实例清理之前
    vkDestroyInstance(m_instance, nullptr);

    glfwDestroyWindow(m_window);
    glfwTerminate();
  }

private:
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

    // Vulkan 是一个与平台无关的 API ，所以需要一个和窗口系统交互的扩展
    // 使用 glfw 获取这个扩展
    uint32_t glfw_extension_count = 0;
    const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);
    std::cout << "-------------------------------------------\n"
              << "GLFW extensions:\n";
    for (size_t i = 0; i < glfw_extension_count; ++i) {
      std::cout << glfw_extensions[i] << '\n';
    }

    // 将需要开启的所有扩展添加到列表并返回
    std::vector<const char*> required_extensions(glfw_extensions, glfw_extensions + glfw_extension_count);
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
  bool IsDeviceSuitable(VkPhysicalDevice device) const noexcept {
    // 获取基本的设置属性，name、type以及Vulkan版本等等
    // VkPhysicalDeviceProperties deviceProperties;
    // vkGetPhysicalDeviceProperties(device, &deviceProperties);

    // 获取对纹理的压缩、64位浮点数和多视图渲染等可选功能的支持
    // VkPhysicalDeviceFeatures device_features;
    // vkGetPhysicalDeviceFeatures(device, &device_features);

    auto indices = FindQueueFamilies(device);
    auto extensions_supported = CheckDeviceExtensionSupported(device);

    bool swap_chain_adequate{false};
    if (extensions_supported) {
      auto swap_chain_support = QuerySwapChainSupport(device);
      swap_chain_adequate = !swap_chain_support.formats.empty() & !swap_chain_support.present_modes.empty();
    }

    return indices.IsComplete() && extensions_supported && swap_chain_adequate;
  }

  /// @brief 查找满足需求的队列族
  /// @details 不同的队列族支持不同的类型的指令，例如计算、内存传输、绘图等指令
  /// @param device
  /// @return
  QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device) const noexcept {
    QueueFamilyIndices indices;

    // 获取物理设备支持的队列族列表
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);
    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());

    for (uint32_t i = 0; i < queue_families.size(); ++i) {
      // 图形队列族
      if (queue_families.at(i).queueFlags & VK_QUEUE_GRAPHICS_BIT) {
        indices.graphics_family = i;
      }

      // 呈现队列族
      VkBool32 present_support = false;
      vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &present_support);

      if (present_support) {
        indices.present_family = i;
      }

      if (indices.IsComplete()) {
        break;
      }
    }

    return indices;
  }

  /// @brief 创建逻辑设备作为和物理设备交互的接口
  void CreateLogicalDevice() {
    auto indices = FindQueueFamilies(m_physical_device);

    std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
    std::set<uint32_t> unique_queue_families{lvk_tidy::GetRequiredValue(indices.graphics_family),
                                             lvk_tidy::GetRequiredValue(indices.present_family)};

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
    create_info.enabledExtensionCount = static_cast<uint32_t>(k_device_extensions.size());
    create_info.ppEnabledExtensionNames = k_device_extensions.data();

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

    // 获取指定队列族的队列句柄，设备队列在逻辑设备被销毁时隐式清理
    // 此处的队列簇可能相同，也就是以相同的参数调用了两次这个函数
    // 1.逻辑设备对象
    // 2.队列族索引
    // 3.队列索引，因为只创建了一个队列，所以此处使用索引0
    // 4.用来存储返回的队列句柄的内存地址
    vkGetDeviceQueue(m_device, lvk_tidy::GetRequiredValue(indices.graphics_family), 0, &m_graphics_queue);
    vkGetDeviceQueue(m_device, lvk_tidy::GetRequiredValue(indices.present_family), 0, &m_present_queue);
  }

  /// @brief 创建表面，需要在程序退出前清理
  /// @details 不同平台创建表面的方式不一样，这里使用 GLFW 统一创建
  void CreateSurface() {
    // Vulkan 不能直接与窗口系统进行交互（是一个与平台特性无关的API集合），surface是 Vulkan 与窗体系统的连接桥梁
    // 需要在instance创建之后立即创建窗体surface，因为它会影响物理设备的选择
    // 窗体surface本身对于 Vulkan 也是非强制的，不需要同 OpenGL 一样必须要创建窗体surface
    if (VK_SUCCESS != glfwCreateWindowSurface(m_instance, m_window, nullptr, &m_surface)) {
      throw std::runtime_error("failed to create window surface");
    }
  }

  /// @brief 检测所需的扩展是否支持
  /// @param device
  /// @return
  bool CheckDeviceExtensionSupported(VkPhysicalDevice device) const noexcept {
    uint32_t extension_count{0};
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr);
    std::vector<VkExtensionProperties> available_extensions(extension_count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, available_extensions.data());

    std::set<std::string> required_extensions(std::begin(k_device_extensions), std::end(k_device_extensions));

    for (const auto& extension : available_extensions) {
      required_extensions.erase(static_cast<const char*>(extension.extensionName));
    }

    // 如果为空，则支持
    return required_extensions.empty();
  }

  /// @brief 查询交换链支持的细节信息
  /// @param device
  /// @return
  SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device) const noexcept {
    SwapChainSupportDetails details;

    // 查询基础表面特性
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_surface, &details.capabilities);

    // 查询表面支持的格式，确保集合对于所有有效的格式可扩充
    uint32_t format_count{0};
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &format_count, nullptr);
    if (0 != format_count) {
      details.formats.resize(format_count);
      vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &format_count, details.formats.data());
    }

    // 查询表面支持的呈现模式
    uint32_t present_mode_count{0};
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &present_mode_count, nullptr);
    if (0 != present_mode_count) {
      details.present_modes.resize(present_mode_count);
      vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &present_mode_count, details.present_modes.data());
    }

    return details;
  }

  /// @brief 选择合适的表面格式
  /// @param available_formats
  /// @return
  VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& available_formats) const noexcept {
    // VkSurfaceFormatKHR 结构都包含一个 format 和一个 colorSpace 成员
    // format 成员变量指定色彩通道和类型
    // colorSpace 成员描述 SRGB 颜色空间是否通过 VK_COLOR_SPACE_SRGB_NONLINEAR_KHR 标志支持
    // 在较早版本的规范中，这个标志名为 VK_COLORSPACE_SRGB_NONLINEAR_KHR

    // 表面没有自己的首选格式，直接返回指定的格式
    if (1 == available_formats.size() && available_formats.front().format == VK_FORMAT_UNDEFINED) {
      return {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    }

    // 遍历格式列表，如果想要的格式存在则直接返回
    for (const auto& available_format : available_formats) {
      if (VK_FORMAT_B8G8R8A8_UNORM == available_format.format &&
          VK_COLOR_SPACE_SRGB_NONLINEAR_KHR == available_format.colorSpace) {
        return available_format;
      }
    }

    // 如果以上两种方式都失效，通过“优良”进行打分排序，大多数情况下会选择第一个格式作为理想的选择
    return available_formats.front();
  }

  /// @brief 查找最佳的可用呈现模式，模式是非常重要的，因为它代表了在屏幕呈现图像的条件
  /// @param available_present_modes
  /// @return
  VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& available_present_modes) const noexcept {
    // 以下4种模式可以使用
    // 1.VK_PRESENT_MODE_IMMEDIATE_KHR: 应用程序提交的图像被立即传输到屏幕呈现，这种模式可能会造成撕裂效果。
    // 2.VK_PRESENT_MODE_FIFO_KHR:
    // 交换链被看作一个队列，当显示内容需要刷新的时候，显示设备从队列的前面获取图像，并且程序将渲染完成的图像插入队列的后面。
    // 如果队列是满的程序会等待。这种规模与视频游戏的垂直同步很类似。显示设备的刷新时刻被成为“垂直中断”。
    // 3.VK_PRESENT_MODE_FIFO_RELAXED_KHR:
    // 该模式与上一个模式略有不同的地方为，如果应用程序存在延迟，即接受最后一个垂直同步信号时队列空了，
    // 将不会等待下一个垂直同步信号，而是将图像直接传送。这样做可能导致可见的撕裂效果。
    // 4.VK_PRESENT_MODE_MAILBOX_KHR:
    // 这是第二种模式的变种。当交换链队列满的时候，选择新的替换旧的图像，从而替代阻塞应用程序的情形。
    // 这种模式通常用来实现三重缓冲区，与标准的垂直同步双缓冲相比，它可以有效避免延迟带来的撕裂效果。

    VkPresentModeKHR best_mode = VK_PRESENT_MODE_FIFO_KHR;

    for (const auto& available_present_mode : available_present_modes) {
      if (VK_PRESENT_MODE_MAILBOX_KHR == available_present_mode) {
        return available_present_mode;
      }
      if (VK_PRESENT_MODE_IMMEDIATE_KHR == available_present_mode) {
        best_mode = available_present_mode;
      }
    }

    return best_mode;
  }

  /// @brief 设置交换范围，交换范围是交换链中图像的分辨率
  /// @param capabilities
  /// @return
  VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) const noexcept {
    // 如果是最大值则表示允许我们自己选择对于窗口最合适的交换范围
    if (std::numeric_limits<uint32_t>::max() != capabilities.currentExtent.width) {
      return capabilities.currentExtent;
    }
    int width{0};
    int height{0};
    glfwGetFramebufferSize(m_window, &width, &height);

    // 使用GLFW窗口的大小来设置交换链中图像的分辨率
    VkExtent2D actual_extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};

    actual_extent.width =
      std::clamp(actual_extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    actual_extent.height =
      std::clamp(actual_extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

    return actual_extent;
  }

  /// @brief 创建交换链
  void CreateSwapChain() {
    SwapChainSupportDetails swap_chain_support = QuerySwapChainSupport(m_physical_device);
    VkSurfaceFormatKHR surface_format = ChooseSwapSurfaceFormat(swap_chain_support.formats);
    VkPresentModeKHR present_mode = ChooseSwapPresentMode(swap_chain_support.present_modes);
    VkExtent2D extent = ChooseSwapExtent(swap_chain_support.capabilities);

    // 交换链中的图像数量，可以理解为队列的长度，指定运行时图像的最小数量
    // maxImageCount数值为0代表除了内存之外没有限制
    uint32_t image_count = swap_chain_support.capabilities.minImageCount + 1;
    if (swap_chain_support.capabilities.maxImageCount > 0 &&
        image_count > swap_chain_support.capabilities.maxImageCount) {
      image_count = swap_chain_support.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = m_surface;
    create_info.minImageCount = image_count;
    create_info.imageFormat = surface_format.format;
    create_info.imageColorSpace = surface_format.colorSpace;
    create_info.imageExtent = extent;
    create_info.imageArrayLayers = 1;  // 图像包含的层次，通常为1，3d图像大于1
    create_info.imageUsage =
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;  // 指定在图像上进行怎样的操作，比如显示（颜色附件）、后处理等

    auto indices = FindQueueFamilies(m_physical_device);
    const std::array<uint32_t, 2> queue_family_indices{
      static_cast<uint32_t>(lvk_tidy::GetRequiredValue(indices.graphics_family)),
      static_cast<uint32_t>(lvk_tidy::GetRequiredValue(indices.present_family))};

    // 在多个队列族使用交换链图像的方式
    if (indices.graphics_family != indices.present_family) {
      // 图形队列和呈现队列不是同一个队列
      // VK_SHARING_MODE_CONCURRENT 表示图像可以在多个队列族间使用，不需要显式地改变图像所有权
      create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
      create_info.queueFamilyIndexCount = 2;
      create_info.pQueueFamilyIndices = queue_family_indices.data();
    } else {
      // VK_SHARING_MODE_EXCLUSIVE 表示一张图像同一时间只能被一个队列族所拥有，这种模式性能最佳
      create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
      create_info.queueFamilyIndexCount = 0;
      create_info.pQueueFamilyIndices = nullptr;
    }

    // 指定一个固定的变换操作（需要交换链具有supportedTransforms特性），此处不进行任何变换
    create_info.preTransform = swap_chain_support.capabilities.currentTransform;
    // 指定 alpha 通道是否被用来和窗口系统中的其他窗口进行混合操作
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    // 设置呈现模式
    create_info.presentMode = present_mode;
    // 设置为true，不关心被遮蔽的像素数据，比如由于其他的窗体置于前方时或者渲染的部分内容存在于可视区域之外，
    // 除非真的需要读取这些像素获数据进行处理（设置为true回读窗口像素可能会有问题），否则可以开启裁剪获得最佳性能。
    create_info.clipped = VK_TRUE;
    // 交换链重建
    // Vulkan运行时，交换链可能在某些条件下被替换，比如窗口调整大小或者交换链需要重新分配更大的图像队列。
    // 在这种情况下，交换链实际上需要重新分配创建，并且必须在此字段中指定对旧的引用，用以回收资源
    create_info.oldSwapchain = VK_NULL_HANDLE;

    if (VK_SUCCESS != vkCreateSwapchainKHR(m_device, &create_info, nullptr, &m_swap_chain)) {
      throw std::runtime_error("failed to create swap chain");
    }

    // 获取交换链中图像，图像会在交换链销毁的同时自动清理
    // 之前给VkSwapchainCreateInfoKHR 设置了期望的图像数量，但是实际运行允许创建更多的图像数量，因此需要重新获取数量
    vkGetSwapchainImagesKHR(m_device, m_swap_chain, &image_count, nullptr);
    m_swap_chain_images.resize(image_count);
    vkGetSwapchainImagesKHR(m_device, m_swap_chain, &image_count, m_swap_chain_images.data());

    m_swap_chain_image_format = surface_format.format;
    m_swap_chain_extent = extent;
  }

  /// @brief 创建图像视图
  /// @details 任何 VkImage 对象都需要通过 VkImageView 来绑定访问，包括处于交换链中的、处于渲染管线中的
  void CreateImageViews() {
    m_swap_chain_image_views.resize(m_swap_chain_images.size());

    for (size_t i = 0; i < m_swap_chain_images.size(); ++i) {
      VkImageViewCreateInfo create_info = {};
      create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      create_info.image = m_swap_chain_images.at(i);
      create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;  // 1d 2d 3d cube纹理
      create_info.format = m_swap_chain_image_format;
      create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;  // 图像颜色通过的映射
      create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
      create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
      create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
      create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;  // 指定图像的用途，图像的哪一部分可以被访问
      create_info.subresourceRange.baseMipLevel = 0;
      create_info.subresourceRange.levelCount = 1;
      create_info.subresourceRange.baseArrayLayer = 0;
      create_info.subresourceRange.layerCount = 1;

      if (VK_SUCCESS != vkCreateImageView(m_device, &create_info, nullptr, &m_swap_chain_image_views.at(i))) {
        throw std::runtime_error("failed to create image views");
      }
    }
  }

  /// @brief 创建图形管线
  /// @details 在 Vulkan 中几乎不允许对图形管线进行动态设置，也就意味着每一种状态都需要提前创建一个图形管线
  void CreateGraphicsPipeline() {
    auto vert_shader_code = ReadFile(PROJECT_SHADER_DIR "02_21_base_vert.spv");
    auto frag_shader_code = ReadFile(PROJECT_SHADER_DIR "02_21_base_frag.spv");

    VkShaderModule vert_shader_module = CreateShaderModule(vert_shader_code);
    VkShaderModule frag_shader_module = CreateShaderModule(frag_shader_code);

    VkPipelineShaderStageCreateInfo vert_shader_stage_info = {};
    vert_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vert_shader_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vert_shader_stage_info.module = vert_shader_module;
    vert_shader_stage_info.pName = "main";                 // 指定调用的着色器函数，同一份代码可以实现多个着色器
    vert_shader_stage_info.pSpecializationInfo = nullptr;  // 设置着色器常量

    VkPipelineShaderStageCreateInfo frag_shader_stage_info = {};
    frag_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    frag_shader_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    frag_shader_stage_info.module = frag_shader_module;
    frag_shader_stage_info.pName = "main";
    frag_shader_stage_info.pSpecializationInfo = nullptr;

    auto binding_description = Vertex::GetBindingDescription();
    auto attribute_descriptions = Vertex::GetAttributeDescriptions();

    // 顶点信息
    VkPipelineVertexInputStateCreateInfo vertex_input_info = {};
    vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_info.vertexBindingDescriptionCount = 1;
    vertex_input_info.pVertexBindingDescriptions = &binding_description;
    vertex_input_info.vertexAttributeDescriptionCount = static_cast<uint32_t>(attribute_descriptions.size());
    vertex_input_info.pVertexAttributeDescriptions = attribute_descriptions.data();

    // 拓扑信息
    VkPipelineInputAssemblyStateCreateInfo input_assembly = {};
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;  // 指定绘制的图元类型：点、线、三角形
    input_assembly.primitiveRestartEnable = VK_FALSE;

    // 视口
    VkViewport viewport = {};
    viewport.x = 0.F;
    viewport.y = 0.F;
    viewport.width = static_cast<float>(m_swap_chain_extent.width);
    viewport.height = static_cast<float>(m_swap_chain_extent.height);
    viewport.minDepth = 0.F;  // 深度值范围，必须在 [0.F, 1.F]之间
    viewport.maxDepth = 1.F;

    // 裁剪
    VkRect2D scissor = {};
    scissor.offset = {0, 0};
    scissor.extent = m_swap_chain_extent;

    VkPipelineViewportStateCreateInfo viewport_state = {};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.pViewports = &viewport;
    viewport_state.scissorCount = 1;
    viewport_state.pScissors = &scissor;

    // 光栅化
    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;  // 设置为true会禁止一切片段输出到帧缓冲
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.F;
    rasterizer.cullMode = VK_CULL_MODE_NONE;                 // 表面剔除类型，正面、背面、双面剔除
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;  // 指定顺时针的顶点序是正面还是反面
    rasterizer.depthBiasEnable = VK_FALSE;
    rasterizer.depthBiasConstantFactor = 1.F;
    rasterizer.depthBiasClamp = 0.F;
    rasterizer.depthBiasSlopeFactor = 0.F;

    // 多重采样
    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.F;
    multisampling.pSampleMask = nullptr;
    multisampling.alphaToCoverageEnable = VK_FALSE;
    multisampling.alphaToOneEnable = VK_FALSE;

    // 深度和模板测试
    VkPipelineDepthStencilStateCreateInfo depth_stencil = {};
    depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil.depthTestEnable = VK_TRUE;            // 是否启用深度测试
    depth_stencil.depthWriteEnable = VK_TRUE;           // 深度测试通过后是否写入深度缓冲
    depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;  // 深度值比较方式
    depth_stencil.depthBoundsTestEnable = VK_FALSE;     // 指定可选的深度范围测试
    depth_stencil.minDepthBounds = 0.F;
    depth_stencil.maxDepthBounds = 1.F;
    depth_stencil.stencilTestEnable = VK_FALSE;  // 模板测试
    depth_stencil.front = {};
    depth_stencil.back = {};

    // 颜色混合，可以对指定帧缓冲单独设置，也可以设置全局颜色混合方式
    VkPipelineColorBlendAttachmentState color_blend_attachment{};
    color_blend_attachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo color_blending{};
    color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blending.logicOpEnable = VK_FALSE;
    color_blending.logicOp = VK_LOGIC_OP_COPY;
    color_blending.attachmentCount = 1;
    color_blending.pAttachments = &color_blend_attachment;
    color_blending.blendConstants[0] = 0.0F;
    color_blending.blendConstants[1] = 0.0F;
    color_blending.blendConstants[2] = 0.0F;
    color_blending.blendConstants[3] = 0.0F;

    // 动态状态，视口大小、线宽、混合常量等
    std::vector<VkDynamicState> dynamic_states = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic_state{};
    dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.dynamicStateCount = static_cast<uint32_t>(dynamic_states.size());
    dynamic_state.pDynamicStates = dynamic_states.data();

    // 管线布局，在着色器中使用 uniform 变量
    VkPipelineLayoutCreateInfo pipeline_layout_info{};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = 1;
    pipeline_layout_info.pSetLayouts = &m_descriptor_set_layout;
    pipeline_layout_info.pushConstantRangeCount = 0;

    if (VK_SUCCESS != vkCreatePipelineLayout(m_device, &pipeline_layout_info, nullptr, &m_pipeline_layout)) {
      throw std::runtime_error("failed to create pipeline layout");
    }

    const std::array<VkPipelineShaderStageCreateInfo, 2> shader_stages{vert_shader_stage_info, frag_shader_stage_info};

    // 完整的图形管线包括：着色器阶段、固定功能状态、管线布局、渲染流程
    VkGraphicsPipelineCreateInfo pipeline_info = {};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = 2;
    pipeline_info.pStages = shader_stages.data();
    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pDepthStencilState = nullptr;
    pipeline_info.pColorBlendState = &color_blending;
    pipeline_info.pDynamicState = &dynamic_state;
    pipeline_info.layout = m_pipeline_layout;
    pipeline_info.renderPass = m_render_pass;
    pipeline_info.subpass = 0;                   // 子流程在子流程数组中的索引
    pipeline_info.basePipelineHandle = nullptr;  // 以一个创建好的图形管线为基础创建一个新的图形管线
    pipeline_info.basePipelineIndex = -1;  // 只有该结构体的成员 flags 被设置为 VK_PIPELINE_CREATE_DERIVATIVE_BIT 才有效
    pipeline_info.pDepthStencilState = &depth_stencil;

    if (VK_SUCCESS !=
        vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &m_graphics_pipeline)) {
      throw std::runtime_error("failed to create graphics pipeline");
    }

    vkDestroyShaderModule(m_device, frag_shader_module, nullptr);
    vkDestroyShaderModule(m_device, vert_shader_module, nullptr);
  }

  /// @brief 使用着色器字节码数组创建 VkShaderModule 对象
  /// @param code
  /// @return
  VkShaderModule CreateShaderModule(const std::vector<char>& code) const {
    VkShaderModuleCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = code.size();
    create_info.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shader_module = VK_NULL_HANDLE;
    if (VK_SUCCESS != vkCreateShaderModule(m_device, &create_info, nullptr, &shader_module)) {
      throw std::runtime_error("failed to create shader module");
    }

    return shader_module;
  }

  /// @brief 创建渲染流程对象，用于渲染的帧缓冲附着，指定颜色和深度缓冲以及采样数
  void CreateRenderPass() {
    // 附着描述
    VkAttachmentDescription color_attachment = {};
    color_attachment.format = m_swap_chain_image_format;                 // 颜色缓冲附着的格式
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;                    // 采样数
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;               // 渲染之前对附着中的数据（颜色和深度）进行操作
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;             // 渲染之后对附着中的数据（颜色和深度）进行操作
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;    // 渲染之前对模板缓冲的操作
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;  // 渲染之后对模板缓冲的操作
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;          // 渲染流程开始前的图像布局方式
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;      // 渲染流程结束后的图像布局方式

    VkAttachmentDescription depth_attachment = {};
    depth_attachment.format = FindDepthFormat();
    depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;  // 绘制结束后不需要从深度缓冲复制深度数据
    depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;  // 不需要读取之前深度图像数据
    depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // 子流程引用的附着
    VkAttachmentReference color_attachment_ref = {};
    color_attachment_ref.attachment = 0;  // 索引，对应于片段着色器中的 layout(location = 0) out
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depth_attachment_ref = {};
    depth_attachment_ref.attachment = 1;
    depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // 子流程
    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;  // 图形渲染子流程
    subpass.colorAttachmentCount = 1;                             // 颜色附着个数
    subpass.pColorAttachments = &color_attachment_ref;            // 指定颜色附着
    subpass.pDepthStencilAttachment =
      &depth_attachment_ref;  // 指定深度模板附着，深度模板附着只能是一个，所以不用设置数量

    // 渲染流程使用的依赖信息
    VkSubpassDependency dependency = {};
    dependency.srcSubpass =
      VK_SUBPASS_EXTERNAL;      // 渲染流程开始前的子流程，为了避免出现循环依赖，dst的值必须大于src的值
    dependency.dstSubpass = 0;  // 渲染流程结束后的子流程
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                              VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;  // 指定需要等待的管线阶段
    dependency.srcAccessMask = 0;                                          // 指定子流程将进行的操作类型
    dependency.dstStageMask =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 2> attachments{color_attachment, depth_attachment};

    VkRenderPassCreateInfo render_pass_info = {};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = static_cast<uint32_t>(attachments.size());
    render_pass_info.pAttachments = attachments.data();
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;
    render_pass_info.dependencyCount = 1;
    render_pass_info.pDependencies = &dependency;

    if (VK_SUCCESS != vkCreateRenderPass(m_device, &render_pass_info, nullptr, &m_render_pass)) {
      throw std::runtime_error("failed to create render pass");
    }
  }

  /// @brief 创建帧缓冲对象，附着需要绑定到帧缓冲对象上使用
  void CreateFramebuffers() {
    m_swap_chain_framebuffers.resize(m_swap_chain_image_views.size());

    for (size_t i = 0; i < m_swap_chain_image_views.size(); ++i) {
      std::array<VkImageView, 2> attachments{m_swap_chain_image_views.at(i), m_depth_image_view};

      VkFramebufferCreateInfo framebuffer_info = {};
      framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
      framebuffer_info.renderPass = m_render_pass;
      framebuffer_info.attachmentCount = static_cast<uint32_t>(attachments.size());
      framebuffer_info.pAttachments = attachments.data();
      framebuffer_info.width = m_swap_chain_extent.width;
      framebuffer_info.height = m_swap_chain_extent.height;
      framebuffer_info.layers = 1;

      if (VK_SUCCESS != vkCreateFramebuffer(m_device, &framebuffer_info, nullptr, &m_swap_chain_framebuffers.at(i))) {
        throw std::runtime_error("failed to create framebuffer");
      }
    }
  }

  /// @brief 创建指令池，用于管理指令缓冲对象使用的内存，并负责指令缓冲对象的分配
  void CreateCommandPool() {
    auto indices = FindQueueFamilies(m_physical_device);

    // VK_COMMAND_POOL_CREATE_TRANSIENT_BIT
    // 指定从Pool中分配的CommandBuffer将是短暂的，意味着它们将在相对较短的时间内被重置或释放
    // VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT 允许从Pool中分配的任何CommandBuffer被单独重置到inital状态
    // 没有设置这个flag则不能使用 vkResetCommandBuffer
    // VK_COMMAND_POOL_CREATE_PROTECTED_BIT 指定从Pool中分配的CommandBuffer是受保护的CommandBuffer
    VkCommandPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_info.queueFamilyIndex = lvk_tidy::GetRequiredValue(indices.graphics_family);

    if (VK_SUCCESS != vkCreateCommandPool(m_device, &pool_info, nullptr, &m_command_pool)) {
      throw std::runtime_error("failed to create command pool");
    }
  }

  /// @brief 为交换链中的每一个图像创建指令缓冲对象，使用它记录绘制指令
  void CreateCommandBuffers() {
    m_command_buffers.resize(m_swap_chain_framebuffers.size());

    VkCommandBufferAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = m_command_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;  // 指定是主要还是辅助指令缓冲对象
    alloc_info.commandBufferCount = static_cast<uint32_t>(m_command_buffers.size());

    if (VK_SUCCESS != vkAllocateCommandBuffers(m_device, &alloc_info, m_command_buffers.data())) {
      throw std::runtime_error("failed to allocate command buffers");
    }
  }

  /// @brief 记录指令到指令缓冲
  void RecordCommandBuffer(VkCommandBuffer command_buffer, VkFramebuffer framebuffer) const {
    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;  // 指定怎样使用指令缓冲
    begin_info.pInheritanceInfo = nullptr;  // 只用于辅助指令缓冲，指定从调用它的主要指令缓冲继承的状态

    if (VK_SUCCESS != vkBeginCommandBuffer(command_buffer, &begin_info)) {
      throw std::runtime_error("failed to begin recording command buffer");
    }

    // 清除色，相当于背景色
    std::array<VkClearValue, 2> clear_values{};
    clear_values.at(0).color = {{.1F, .2F, .3F, 1.F}};
    clear_values.at(1).depthStencil = {1.F, 0};

    uint32_t fb_width = m_swap_chain_extent.width;    // framebuffer 的宽度
    uint32_t fb_height = m_swap_chain_extent.height;  // farmebuffer 的高度

    VkRenderPassBeginInfo render_pass_info = {};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_info.renderPass = m_render_pass;
    render_pass_info.framebuffer = framebuffer;
    render_pass_info.renderArea.offset = {50, 50};
    render_pass_info.renderArea.extent = {fb_width / 2 - 100, fb_height / 2 - 100};
    render_pass_info.clearValueCount = static_cast<uint32_t>(clear_values.size());
    render_pass_info.pClearValues = clear_values.data();

    VkViewport viewport = {};
    viewport.x = 50.F;
    viewport.y = 50.F;
    viewport.width = static_cast<float>(fb_width / 2 - 100);
    viewport.height = static_cast<float>(fb_height / 2 - 100);
    viewport.minDepth = 0.F;
    viewport.maxDepth = 1.F;

    VkRect2D scissor = {};
    scissor.offset = {50, 50};
    scissor.extent = {fb_width / 2 - 100, fb_height / 2 - 100};

    const std::array<VkDeviceSize, 1> offsets{0};

    //--------------------------------------------------------------------
    // 第一次 RenderPass
    vkCmdBeginRenderPass(command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphics_pipeline);
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);
    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline_layout, 0, 1,
                            &m_descriptor_sets.at(m_current_frame), 0, nullptr);

    // 绘制立方体
    const std::array<VkBuffer, 1> vertex_buffers_cube{m_drawable_cube.vertex_buffer};
    vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers_cube.data(), offsets.data());
    vkCmdBindIndexBuffer(command_buffer, m_drawable_cube.index_buffer, 0, VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed(command_buffer, m_drawable_cube.index_count, 1, 0, 0, 0);

    vkCmdEndRenderPass(command_buffer);

    //--------------------------------------------------------------------
    // 第二次 RenderPass
    render_pass_info.renderArea.offset = {static_cast<int32_t>(fb_width) / 2 + 50,
                                          static_cast<int32_t>(fb_height) / 2 + 50};
    render_pass_info.renderArea.extent = {fb_width / 2 - 100, fb_height / 2 - 100};
    viewport.x = static_cast<float>(fb_width / 2 + 50);
    viewport.y = static_cast<float>(fb_height / 2 + 50);
    viewport.width = static_cast<float>(fb_width / 2 - 100);
    viewport.height = static_cast<float>(fb_height / 2 - 100);
    scissor.offset = {static_cast<int32_t>(fb_width) / 2 + 50, static_cast<int32_t>(fb_height) / 2 + 50};
    scissor.extent = {fb_width / 2 - 100, fb_height / 2 - 100};

    vkCmdBeginRenderPass(command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphics_pipeline);
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);
    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline_layout, 0, 1,
                            &m_descriptor_sets.at(m_current_frame), 0, nullptr);

    // 绘制平面
    const std::array<VkBuffer, 1> vertex_buffers_plane{m_drawable_plane.vertex_buffer};
    vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers_plane.data(), offsets.data());
    vkCmdBindIndexBuffer(command_buffer, m_drawable_plane.index_buffer, 0, VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed(command_buffer, m_drawable_plane.index_count, 1, 0, 0, 0);

    vkCmdEndRenderPass(command_buffer);

    // 结束记录指令到指令缓冲
    if (VK_SUCCESS != vkEndCommandBuffer(command_buffer)) {
      throw std::runtime_error("failed to record command buffer");
    }
  }

  /// @brief 绘制每一帧
  /// @details 流程：从交换链获取一张图像、对帧缓冲附着执行指令缓冲中的渲染指令、返回渲染后的图像到交换链进行呈现操作
  void DrawFrame() {
    // 等待一组栅栏中的一个或全部栅栏发出信号，即上一次提交的指令结束执行
    // 使用栅栏可以进行CPU与GPU之间的同步，防止超过 k_max_frames_in_flight 帧的指令同时被提交执行
    vkWaitForFences(m_device, 1, &m_in_flight_fences.at(m_current_frame), VK_TRUE,
                    std::numeric_limits<uint64_t>::max());

    // 从交换链获取一张图像
    uint32_t image_index{0};
    // 3.获取图像的超时时间，此处禁用图像获取超时
    // 4.通知的同步对象
    // 5.输出可用的交换链图像的索引
    VkResult result =
      vkAcquireNextImageKHR(m_device, m_swap_chain, std::numeric_limits<uint64_t>::max(),
                            m_image_available_semaphores.at(m_current_frame), VK_NULL_HANDLE, &image_index);

    // VK_ERROR_OUT_OF_DATE_KHR 交换链不能继续使用，通常发生在窗口大小改变后
    // VK_SUBOPTIMAL_KHR 交换链仍然可以使用，但表面属性已经不能准确匹配
    if (VK_ERROR_OUT_OF_DATE_KHR == result) {
      RecreateSwapChain();
      return;
    }
    if (VK_SUCCESS != result && VK_SUBOPTIMAL_KHR != result) {
      throw std::runtime_error("failed to acquire swap chain image");
    }

    // 每一帧都更新uniform
    UpdateUniformBuffer(static_cast<uint32_t>(m_current_frame));

    // 手动将栅栏重置为未发出信号的状态（必须手动设置）
    vkResetFences(m_device, 1, &m_in_flight_fences.at(m_current_frame));
    vkResetCommandBuffer(m_command_buffers.at(image_index), 0);
    RecordCommandBuffer(m_command_buffers.at(image_index), m_swap_chain_framebuffers.at(image_index));

    const std::array<VkPipelineStageFlags, 1> wait_stages{VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores =
      &m_image_available_semaphores.at(m_current_frame);  // 指定队列开始执行前需要等待的信号量
    submit_info.pWaitDstStageMask = wait_stages.data();   // 指定需要等待的管线阶段
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &m_command_buffers[image_index];  // 指定实际被提交执行的指令缓冲对象
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores =
      &m_render_finished_semaphores.at(image_index);  // 指定在指令缓冲执行结束后发出信号的信号量对象

    // 提交指令缓冲给图形指令队列
    // 如果不等待上一次提交的指令结束执行，可能会导致内存泄漏
    // 1.vkQueueWaitIdle 可以等待上一次的指令结束执行，2.也可以同时渲染多帧解决该问题（使用栅栏）
    // vkQueueSubmit 的最后一个参数用来指定在指令缓冲执行结束后需要发起信号的栅栏对象
    if (VK_SUCCESS != vkQueueSubmit(m_graphics_queue, 1, &submit_info, m_in_flight_fences.at(m_current_frame))) {
      throw std::runtime_error("failed to submit draw command buffer");
    }

    VkPresentInfoKHR present_info = {};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &m_render_finished_semaphores.at(image_index);  // 指定开始呈现操作需要等待的信号量
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &m_swap_chain;   // 指定用于呈现图像的交换链
    present_info.pImageIndices = &image_index;  // 指定需要呈现的图像在交换链中的索引
    present_info.pResults = nullptr;            // 可以通过该变量获取每个交换链的呈现操作是否成功的信息

    // 请求交换链进行图像呈现操作
    result = vkQueuePresentKHR(m_present_queue, &present_info);

    if (VK_ERROR_OUT_OF_DATE_KHR == result || VK_SUBOPTIMAL_KHR == result || m_framebuffer_resized) {
      m_framebuffer_resized = false;
      // 交换链不完全匹配时也重建交换链
      RecreateSwapChain();
    } else if (VK_SUCCESS != result) {
      throw std::runtime_error("failed to presend swap chain image");
    }

    // 更新当前帧索引
    m_current_frame = (m_current_frame + 1) % k_max_frames_in_flight;
  }

  /// @brief 创建同步对象，用于发出图像已经被获取可以开始渲染和渲染已经结束可以开始呈现的信号
  void CreateSyncObjects() {
    m_image_available_semaphores.resize(k_max_frames_in_flight);
    m_render_finished_semaphores.resize(m_swap_chain_images.size());
    m_in_flight_fences.resize(k_max_frames_in_flight);

    VkSemaphoreCreateInfo semaphore_info = {};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fence_info = {};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;  // 初始状态设置为已发出信号，避免 vkWaitForFences 一直等待

    for (size_t i = 0; i < k_max_frames_in_flight; ++i) {
      if (VK_SUCCESS != vkCreateSemaphore(m_device, &semaphore_info, nullptr, &m_image_available_semaphores.at(i)) ||
          vkCreateFence(m_device, &fence_info, nullptr, &m_in_flight_fences.at(i))) {
        throw std::runtime_error("failed to create synchronization objects for a frame");
      }
    }
    for (auto& semaphore : m_render_finished_semaphores) {
      if (VK_SUCCESS != vkCreateSemaphore(m_device, &semaphore_info, nullptr, &semaphore)) {
        throw std::runtime_error("failed to create render finished semaphore");
      }
    }
  }

  /// @brief 重建交换链
  void RecreateSwapChain() {
    int width{0};
    int height{0};
    glfwGetFramebufferSize(m_window, &width, &height);
    while (0 == width || 0 == height) {
      glfwGetFramebufferSize(m_window, &width, &height);
      glfwWaitEvents();
    }

    // 等待设备处于空闲状态，避免在对象的使用过程中将其清除重建
    vkDeviceWaitIdle(m_device);

    // 在重建前清理之前使用的对象
    CleanupSwapChain();

    // 可以通过动态状态来设置视口和裁剪矩形来避免重建管线
    CreateSwapChain();
    CreateImageViews();
    CreateDepthResources();
    CreateFramebuffers();
  }

  void DestroyDrawable(Drawable& drawable) noexcept {
    vkDestroyBuffer(m_device, drawable.vertex_buffer, nullptr);
    vkFreeMemory(m_device, drawable.vertex_buffer_memory, nullptr);
    vkDestroyBuffer(m_device, drawable.index_buffer, nullptr);
    vkFreeMemory(m_device, drawable.index_buffer_memory, nullptr);
  }

  /// @brief 清除交换链相关对象
  void CleanupSwapChain() noexcept {
    vkDestroyImageView(m_device, m_depth_image_view, nullptr);
    vkDestroyImage(m_device, m_depth_image, nullptr);
    vkFreeMemory(m_device, m_depth_image_memory, nullptr);

    for (auto framebuffer : m_swap_chain_framebuffers) {
      vkDestroyFramebuffer(m_device, framebuffer, nullptr);
    }

    for (auto image_view : m_swap_chain_image_views) {
      vkDestroyImageView(m_device, image_view, nullptr);
    }

    vkDestroySwapchainKHR(m_device, m_swap_chain, nullptr);
  }

  /// @brief 设置调试扩展信息
  /// @param create_info
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

  /// @brief 创建顶点缓冲
  void CreateVertexBuffer(const std::vector<Vertex>& vertices, Drawable& drawable) {
    VkDeviceSize buffer_size = sizeof(Vertex) * vertices.size();

    // 为了提升性能，使用一个临时（暂存）缓冲，先将顶点数据加载到临时缓冲，再复制到顶点缓冲
    VkBuffer staging_buffer{};
    VkDeviceMemory staging_buffer_memory{};

    // 创建一个CPU可见的缓冲作为临时缓冲
    // VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT 用于从CPU写入数据
    // VK_MEMORY_PROPERTY_HOST_COHERENT_BIT 可以保证数据被立即复制到缓冲关联的内存
    CreateBuffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging_buffer,
                 staging_buffer_memory);

    void* data{nullptr};
    // 将缓冲关联的内存映射到CPU可以访问的内存
    vkMapMemory(m_device, staging_buffer_memory, 0, buffer_size, 0, &data);
    // 将顶点数据复制到映射后的内存
    std::memcpy(data, vertices.data(), static_cast<size_t>(buffer_size));
    // 结束内存映射
    vkUnmapMemory(m_device, staging_buffer_memory);

    // 创建一个显卡读取较快的缓冲作为真正的顶点缓冲
    // 具有 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT 标记的内存最适合显卡读取，CPU通常不能访问这种类型的内存
    CreateBuffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, drawable.vertex_buffer, drawable.vertex_buffer_memory);

    // m_vertexBuffer 现在关联的内存是设备所有的（显卡），不能使用 vkMapMemory 函数对它关联的内存进行映射
    // 从临时（暂存）缓冲复制数据到显卡读取较快的缓冲中
    CopyBuffer(staging_buffer, drawable.vertex_buffer, buffer_size);

    vkDestroyBuffer(m_device, staging_buffer, nullptr);
    vkFreeMemory(m_device, staging_buffer_memory, nullptr);

    // 驱动程序可能并不会立即复制数据到缓冲关联的内存中去，因为处理器都有缓存，写入内存的数据并不一定在多个核心同时可见
    // 保证数据被立即复制到缓冲关联的内存可以使用以下函数，或者使用 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT 属性的内存类型
    // 使用函数的方法性能更好
    // 写入数据到映射的内存后，调用 vkFlushMappedMemoryRanges
    // 读取映射的内存数据前，调用 vkInvalidateMappedMemoryRanges
  }

  /// @brief 创建索引缓冲
  void CreateIndexBuffer(const std::vector<uint16_t>& indices, Drawable& drawable) {
    drawable.index_count = static_cast<uint32_t>(indices.size());
    VkDeviceSize buffer_size = sizeof(indices.front()) * indices.size();

    VkBuffer staging_buffer{};
    VkDeviceMemory staging_buffer_memory{};

    CreateBuffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging_buffer,
                 staging_buffer_memory);

    void* data{nullptr};
    vkMapMemory(m_device, staging_buffer_memory, 0, buffer_size, 0, &data);
    std::memcpy(data, indices.data(), static_cast<size_t>(buffer_size));
    vkUnmapMemory(m_device, staging_buffer_memory);

    CreateBuffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, drawable.index_buffer, drawable.index_buffer_memory);

    CopyBuffer(staging_buffer, drawable.index_buffer, buffer_size);

    vkDestroyBuffer(m_device, staging_buffer, nullptr);
    vkFreeMemory(m_device, staging_buffer_memory, nullptr);
  }

  /// @brief 创建指定类型的缓冲
  /// @details Vulkan 的缓冲是可以存储任意数据的可以被显卡读取的内存，不仅可以存储顶点数据
  /// @param size
  /// @param usage
  /// @param properties
  /// @param buffer
  /// @param buffer_memory
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

    // 4. 偏移值，需要满足能够被 mem_requirements.alighment 整除
    vkBindBufferMemory(m_device, buffer, buffer_memory, 0);
  }

  /// @brief 在缓冲之间复制数据
  /// @param src_buffer
  /// @param dst_buffer
  /// @param size
  void CopyBuffer(VkBuffer src_buffer, VkBuffer dst_buffer, VkDeviceSize size) const noexcept {
    VkCommandBufferAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandPool = m_command_pool;
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
    vkQueueSubmit(m_graphics_queue, 1, &submit_info, nullptr);
    // 等待传输操作完成，也可以使用栅栏，栅栏可以同步多个不同的内存传输操作，给驱动程序的优化空间也更大
    vkQueueWaitIdle(m_graphics_queue);

    vkFreeCommandBuffers(m_device, m_command_pool, 1, &command_buffer);
  }

  VkImageView CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspect_flags) {
    VkImageViewCreateInfo view_info{};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = image;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = format;
    view_info.subresourceRange.aspectMask = aspect_flags;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;

    VkImageView image_view{};
    if (VK_SUCCESS != vkCreateImageView(m_device, &view_info, nullptr, &image_view)) {
      throw std::runtime_error("failed to create texture image view");
    }

    return image_view;
  }

  /// @brief 查找最合适的内存类型
  /// @details 不同类型的内存所允许进行的操作以及操作的效率有所不同
  /// @param type_filter 指定需要的内存类型的位域
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

  /// @brief 创建着色器使用的每一个描述符绑定信息
  void CreateDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding ubo_layout_binding = {};
    ubo_layout_binding.binding = 0;
    ubo_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ubo_layout_binding.descriptorCount = 1;                      // uniform 缓冲对象数组的大小
    ubo_layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;  // 指定在哪一个着色器阶段使用
    ubo_layout_binding.pImmutableSamplers = nullptr;             // 指定图像采样相关的属性

    VkDescriptorSetLayoutCreateInfo layout_info = {};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = 1;
    layout_info.pBindings = &ubo_layout_binding;

    if (VK_SUCCESS != vkCreateDescriptorSetLayout(m_device, &layout_info, nullptr, &m_descriptor_set_layout)) {
      throw std::runtime_error("failed to create descriptor set layout");
    }
  }

  /// @brief 创建Uniform Buffer
  /// @details 由于缓冲需要频繁更新，所以此处使用暂存缓冲并不会带来性能提升
  void CreateUniformBuffers() {
    VkDeviceSize buffer_size = sizeof(UniformBufferObject);

    m_uniform_buffers.resize(k_max_frames_in_flight);
    m_uniform_buffers_memory.resize(k_max_frames_in_flight);
    m_uniform_buffers_mapped.resize(k_max_frames_in_flight);

    for (size_t i = 0; i < k_max_frames_in_flight; ++i) {
      CreateBuffer(buffer_size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m_uniform_buffers.at(i),
                   m_uniform_buffers_memory.at(i));
      vkMapMemory(m_device, m_uniform_buffers_memory.at(i), 0, buffer_size, 0, &m_uniform_buffers_mapped.at(i));
    }
  }

  /// @brief 在绘制每一帧时更新uniform
  /// @param current_image
  void UpdateUniformBuffer(uint32_t current_image) {
    auto aspect = static_cast<float>(m_swap_chain_extent.width) / static_cast<float>(m_swap_chain_extent.height);

    UniformBufferObject ubo{};
    ubo.model = glm::mat4(1.F);
    ubo.view = glm::lookAt(m_eye_pos, m_look_at, m_view_up);
    ubo.proj = glm::perspective(glm::radians(45.F), aspect, 0.1F, 100.F);

    // glm的裁剪坐标的Y轴和 Vulkan 是相反的
    ubo.proj[1][1] *= -1;

    // 将变换矩阵的数据复制到uniform缓冲
    std::memcpy(m_uniform_buffers_mapped[current_image], &ubo, sizeof(ubo));
  }

  /// @brief 创建描述符池，描述符集需要通过描述符池来创建
  void CreateDescriptorPool() {
    VkDescriptorPoolSize pool_size{};
    pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    pool_size.descriptorCount = static_cast<uint32_t>(k_max_frames_in_flight);

    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.poolSizeCount = 1;
    pool_info.pPoolSizes = &pool_size;
    pool_info.maxSets = static_cast<uint32_t>(k_max_frames_in_flight);  // 指定可以分配的最大描述符集个数
    pool_info.flags = 0;  // 可以用来设置独立的描述符集是否可以被清除掉，此处使用默认值

    if (VK_SUCCESS != vkCreateDescriptorPool(m_device, &pool_info, nullptr, &m_descriptor_pool)) {
      throw std::runtime_error("failed to create descriptor pool");
    }
  }

  /// @brief 创建描述符集
  void CreateDescriptorSets() {
    // 描述符布局对象的个数要匹配描述符集对象的个数
    std::vector<VkDescriptorSetLayout> layouts(k_max_frames_in_flight, m_descriptor_set_layout);

    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = m_descriptor_pool;  // 指定分配描述符集对象的描述符池
    alloc_info.descriptorSetCount = static_cast<uint32_t>(k_max_frames_in_flight);
    alloc_info.pSetLayouts = layouts.data();

    // 描述符集对象会在描述符池对象清除时自动被清除
    m_descriptor_sets.resize(k_max_frames_in_flight);
    if (VK_SUCCESS != vkAllocateDescriptorSets(m_device, &alloc_info, m_descriptor_sets.data())) {
      throw std::runtime_error("failed to allocate descriptor sets");
    }

    for (size_t i = 0; i < k_max_frames_in_flight; ++i) {
      VkDescriptorBufferInfo buffer_info{};
      buffer_info.buffer = m_uniform_buffers.at(i);
      buffer_info.offset = 0;
      buffer_info.range = sizeof(UniformBufferObject);

      VkWriteDescriptorSet descriptor_write{};
      descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      descriptor_write.dstSet = m_descriptor_sets.at(i);  // 指定要更新的描述符集对象
      descriptor_write.dstBinding = 0;                    // 指定缓冲绑定
      descriptor_write.dstArrayElement = 0;               // 描述符数组的第一个元素的索引（没有数组就使用0）
      descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
      descriptor_write.descriptorCount = 1;
      descriptor_write.pBufferInfo = &buffer_info;  // 指定描述符引用的缓冲数据
      descriptor_write.pImageInfo = nullptr;        // 指定描述符引用的图像数据
      descriptor_write.pTexelBufferView = nullptr;  // 指定描述符引用的缓冲视图

      // 更新描述符的配置
      vkUpdateDescriptorSets(m_device, 1, &descriptor_write, 0, nullptr);
    }
  }

  /// @brief 创建指定格式的图像对象
  /// @param width
  /// @param height
  /// @param format
  /// @param tiling
  /// @param usage
  /// @param properties
  /// @param image
  /// @param image_memory
  void CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage,
                   VkMemoryPropertyFlags /*properties*/, VkImage& image, VkDeviceMemory& image_memory)

  {
    // tiling成员变量可以是 VK_IMAGE_TILING_LINEAR 纹素以行主序的方式排列，可以直接访问图像
    //                     VK_IMAGE_TILING_OPTIMAL 纹素以一种对访问优化的方式排列
    // initialLayout成员变量可以是 VK_IMAGE_LAYOUT_UNDEFINED GPU不可用，纹素在第一次变换会被丢弃
    //                            VK_IMAGE_LAYOUT_PREINITIALIZED GPU不可用，纹素在第一次变换会被保留
    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.extent.width = static_cast<uint32_t>(width);
    image_info.extent.height = static_cast<uint32_t>(height);
    image_info.extent.depth = 1;
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.format = format;
    image_info.tiling = tiling;                            // 设置之后不可修改
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;  // 图像数据是接收方，不需要保留第一次变换时的纹理数据
    image_info.usage = usage;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;  // 只被一个队列族使用（支持传输操作的队列族），所以使用独占模式
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;          // 设置多重采样，只对用作附着的图像对象有效
    image_info.flags = 0;  // 可以用来设置稀疏图像的优化，比如体素地形没必要为“空气”部分分配内存

    if (VK_SUCCESS != vkCreateImage(m_device, &image_info, nullptr, &image)) {
      throw std::runtime_error("failed to create image");
    }

    VkMemoryRequirements mem_requirements{};
    vkGetImageMemoryRequirements(m_device, image, &mem_requirements);

    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_requirements.size;
    alloc_info.memoryTypeIndex = FindMemoryType(mem_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (VK_SUCCESS != vkAllocateMemory(m_device, &alloc_info, nullptr, &image_memory)) {
      throw std::runtime_error("failed to allocate image memory");
    }
    vkBindImageMemory(m_device, image, image_memory, 0);
  }

  /// @brief 配置深度图像需要的资源
  void CreateDepthResources() {
    auto depth_format = FindDepthFormat();

    CreateImage(m_swap_chain_extent.width, m_swap_chain_extent.height, depth_format, VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_depth_image,
                m_depth_image_memory);

    m_depth_image_view = CreateImageView(m_depth_image, depth_format, VK_IMAGE_ASPECT_DEPTH_BIT);
  }

  /// @brief 查找一个符合要求又被设备支持的图像数据格式
  /// @param candidates
  /// @param tiling
  /// @param features
  /// @return
  VkFormat FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling,
                               VkFormatFeatureFlags features) const {
    for (auto format : candidates) {
      // 查找可用的深度图像格式
      VkFormatProperties props{};
      vkGetPhysicalDeviceFormatProperties(m_physical_device, format, &props);
      // props.bufferFeatures        表示数据格式支持缓冲
      // props.linearTilingFeatures  表示数据格式支持线性tiling模式
      // props.optimalTilingFeatures 表示数据格式支持优化tiling模式

      if ((VK_IMAGE_TILING_LINEAR == tiling && (props.linearTilingFeatures & features) == features) ||
          (VK_IMAGE_TILING_OPTIMAL == tiling && (props.optimalTilingFeatures & features) == features)) {
        return format;
      }
    }

    throw std::runtime_error("failed to find supported format");
  }

  /// @brief 查找适合作为深度附着的图像数据格式
  /// @return
  VkFormat FindDepthFormat() const {
    return FindSupportedFormat({VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
                               VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
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
  /// @param p_create_info
  /// @param p_allocator
  /// @param p_callback
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
  /// @param p_allocator
  static void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT callback,
                                            const VkAllocationCallbacks* p_allocator) noexcept {
    auto func = lvk_tidy::LoadInstanceProcAddress<PFN_vkDestroyDebugUtilsMessengerEXT>(
      instance, "vkDestroyDebugUtilsMessengerEXT");

    if (nullptr != func) {
      func(instance, callback, p_allocator);
    }
  }

  /// @brief 读取二进制着色器文件
  /// @param file_name
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

  static void FramebufferResizeCallback(GLFWwindow* window, int /*width*/, int /*height*/) noexcept {
    if (auto app = reinterpret_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(window))) {
      app->m_framebuffer_resized = true;
    }
  }

  /// @brief 光标位置回调
  /// @param window
  /// @param xpos
  /// @param ypos
  static void CursorPosCallback(GLFWwindow* window, double xpos, double ypos) noexcept {
    if (auto app = reinterpret_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(window))) {
      // 计算鼠标移动的偏移量
      double x_offset = xpos - app->m_mouse_last_x;
      double y_offset = ypos - app->m_mouse_last_y;

      if (app->m_is_rotating) {
        // 设置灵敏度，可以根据需要调整
        constexpr auto sensitivity = 0.5 / 180.0 * std::numbers::pi_v<double>;
        x_offset *= sensitivity;
        y_offset *= sensitivity;

        // 构造旋转四元数
        glm::vec3 x_axis = glm::normalize(glm::cross(app->m_view_up, app->m_eye_pos));
        glm::vec3 y_axis = glm::normalize(app->m_view_up);
        glm::quat x_rotation = glm::angleAxis(static_cast<float>(-y_offset), x_axis);
        glm::quat y_rotation = glm::angleAxis(static_cast<float>(-x_offset), y_axis);
        glm::quat rotation_quat = x_rotation * y_rotation;

        app->m_eye_pos = glm::vec3(glm::mat4(rotation_quat) * glm::vec4(app->m_eye_pos, 1.F));
        app->m_view_up = glm::vec3(glm::mat4(rotation_quat) * glm::vec4(app->m_view_up, 1.F));
        // app->m_view_up = glm::normalize(glm::cross(x_axis, -app->m_eye_pos));
      }

      if (app->m_is_translating) {
        x_offset /= (app->m_swap_chain_extent.width / 2);
        y_offset /= (app->m_swap_chain_extent.height / 2);

        glm::vec3 x_axis = glm::normalize(glm::cross(app->m_view_up, app->m_eye_pos));
        glm::vec3 y_axis = glm::normalize(app->m_view_up);
        glm::mat4 x_translation = glm::translate(glm::mat4(1.F), x_axis * static_cast<float>(-x_offset));
        glm::mat4 y_translation = glm::translate(glm::mat4(1.F), y_axis * static_cast<float>(y_offset));
        glm::mat4 translation = x_translation * y_translation;

        app->m_eye_pos = glm::vec3(translation * glm::vec4(app->m_eye_pos, 1.F));
        app->m_look_at = glm::vec3(translation * glm::vec4(app->m_look_at, 1.F));
      }

      // 更新上一次的鼠标位置
      app->m_mouse_last_x = xpos;
      app->m_mouse_last_y = ypos;
    }
  }

  /// @brief 鼠标滚轮滚动回调
  /// @param window
  /// @param xoffset
  /// @param yoffset 小于0向前滚动，大于0向后滚动
  static void ScrollCallback(GLFWwindow* window, double /*xoffset*/, double yoffset) noexcept {
    if (auto app = reinterpret_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(window))) {
      glm::vec3 direction = glm::normalize(app->m_eye_pos - app->m_look_at);
      float distance = glm::distance(app->m_eye_pos, app->m_look_at);
      glm::vec3 translate = direction * distance * 0.1F;

      if (yoffset < 0.0) {
        glm::mat4 mat = glm::translate(glm::mat4(1.F), translate);
        app->m_eye_pos = glm::vec3(mat * glm::vec4(app->m_eye_pos, 1.F));
      } else if (yoffset > 0.0) {
        glm::mat4 mat = glm::translate(glm::mat4(1.F), translate * -1.F);
        app->m_eye_pos = glm::vec3(mat * glm::vec4(app->m_eye_pos, 1.F));
      }
    }
  }

  /// @brief 鼠标按钮操作回调
  /// @param window
  /// @param button
  /// @param action
  /// @param mods
  static void MouseButtonCallback(GLFWwindow* window, int button, int action, int /*mods*/) noexcept {
    if (auto app = reinterpret_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(window))) {
      if (GLFW_PRESS == action && GLFW_MOUSE_BUTTON_RIGHT == button) {
        app->m_is_rotating = true;
        glfwGetCursorPos(window, &app->m_mouse_last_x, &app->m_mouse_last_y);
      } else if (GLFW_RELEASE == action && GLFW_MOUSE_BUTTON_RIGHT == button) {
        app->m_is_rotating = false;
      }

      if (GLFW_PRESS == action && GLFW_MOUSE_BUTTON_LEFT == button) {
        app->m_is_translating = true;
        glfwGetCursorPos(window, &app->m_mouse_last_x, &app->m_mouse_last_y);
      } else if (GLFW_RELEASE == action && GLFW_MOUSE_BUTTON_LEFT == button) {
        app->m_is_translating = false;
      }
    }
  }

  /// @brief 键盘操作回调
  /// @param window
  /// @param key
  /// @param scancode
  /// @param action
  /// @param mods
  static void KeyCallback(GLFWwindow* window, int key, int /*scancode*/, int action, int /*mods*/) noexcept {
    if (auto app = reinterpret_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(window))) {
      // 按下'r'/'R'恢复视角
      if (GLFW_PRESS == action && GLFW_KEY_R == key) {
        app->m_view_up = glm::vec3{0.F, 1.F, 0.F};
        app->m_eye_pos = glm::vec3{0.F, 0.F, -3.F};
        app->m_look_at = glm::vec3{0.F};
      }
    }
  }

private:
  GLFWwindow* m_window{nullptr};
  VkInstance m_instance{nullptr};
  VkDebugUtilsMessengerEXT m_debug_messenger{nullptr};
  VkPhysicalDevice m_physical_device{nullptr};
  VkDevice m_device{nullptr};
  VkQueue m_graphics_queue{nullptr};  // 图形队列
  VkSurfaceKHR m_surface{nullptr};
  VkQueue m_present_queue{nullptr};  // 呈现队列
  VkSwapchainKHR m_swap_chain{nullptr};
  std::vector<VkImage> m_swap_chain_images{};
  VkFormat m_swap_chain_image_format{};
  VkExtent2D m_swap_chain_extent{};
  std::vector<VkImageView> m_swap_chain_image_views{};
  VkRenderPass m_render_pass{nullptr};
  VkPipelineLayout m_pipeline_layout{nullptr};
  VkPipeline m_graphics_pipeline{nullptr};
  std::vector<VkFramebuffer> m_swap_chain_framebuffers{};
  VkCommandPool m_command_pool{nullptr};
  std::vector<VkCommandBuffer> m_command_buffers{};

  std::vector<VkSemaphore> m_image_available_semaphores{};
  std::vector<VkSemaphore> m_render_finished_semaphores{};
  std::vector<VkFence> m_in_flight_fences{};
  size_t m_current_frame{0};
  bool m_framebuffer_resized{false};

  Drawable m_drawable_cube{};
  Drawable m_drawable_plane{};

  VkDescriptorSetLayout m_descriptor_set_layout{nullptr};
  std::vector<VkBuffer> m_uniform_buffers{};
  std::vector<VkDeviceMemory> m_uniform_buffers_memory{};
  std::vector<void*> m_uniform_buffers_mapped{};
  VkDescriptorPool m_descriptor_pool{nullptr};
  std::vector<VkDescriptorSet> m_descriptor_sets{};

  VkImage m_depth_image{nullptr};
  VkDeviceMemory m_depth_image_memory{nullptr};
  VkImageView m_depth_image_view{nullptr};

  glm::vec3 m_view_up{0.F, 1.F, 0.F};
  glm::vec3 m_eye_pos{0.F, 0.F, -3.F};
  glm::vec3 m_look_at{0.F};
  double m_mouse_last_x{0.0};
  double m_mouse_last_y{0.0};
  bool m_is_rotating{false};
  bool m_is_translating{false};
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

#endif  // TEST2

#ifdef TEST3

#define GLFW_INCLUDE_VULKAN  // 定义这个宏之后 glfw3.h 文件就会包含 Vulkan 的头文件
#include <GLFW/glfw3.h>

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

// 窗口默认大小
constexpr uint32_t k_width = 800;
constexpr uint32_t k_height = 600;

// 同时并行处理的帧数
constexpr int k_max_frames_in_flight = 2;

// 需要开启的校验层的名称
const std::vector<const char*> k_validation_layers = {"VK_LAYER_KHRONOS_validation"};
// 交换链扩展
const std::vector<const char*> k_device_extensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

// 是否启用校验层
#ifdef NDEBUG
const bool k_enable_validation_layers = false;
#else
const bool k_enable_validation_layers = true;
#endif  // NDEBUG

struct Vertex {
  glm::vec3 pos{0.F, 0.F, 0.F};
  glm::vec3 color{0.F, 0.F, 0.F};

  static constexpr VkVertexInputBindingDescription GetBindingDescription() noexcept {
    VkVertexInputBindingDescription binding_description{};

    binding_description.binding = 0;
    binding_description.stride = sizeof(Vertex);
    binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    return binding_description;
  }

  static constexpr std::array<VkVertexInputAttributeDescription, 2> GetAttributeDescriptions() noexcept {
    static_assert(std::is_standard_layout_v<Vertex>, "Vertex must be standard layout for offsetof");
    std::array<VkVertexInputAttributeDescription, 2> attribute_descriptions{};

    attribute_descriptions[0].binding = 0;
    attribute_descriptions[0].location = 0;
    attribute_descriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attribute_descriptions[0].offset = offsetof(Vertex, pos);

    attribute_descriptions[1].binding = 0;
    attribute_descriptions[1].location = 1;
    attribute_descriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attribute_descriptions[1].offset = offsetof(Vertex, color);

    return attribute_descriptions;
  }
};

struct UniformBufferObject {
  glm::mat4 model{glm::mat4(1.F)};
  glm::mat4 view{glm::mat4(1.F)};
  glm::mat4 proj{glm::mat4(1.F)};
};

/// @brief 支持图形和呈现的队列族
struct QueueFamilyIndices {
  std::optional<uint32_t> graphics_family{};
  std::optional<uint32_t> present_family{};

  constexpr bool IsComplete() const noexcept { return graphics_family.has_value() && present_family.has_value(); }
};

/// @brief 交换链属性信息
struct SwapChainSupportDetails {
  VkSurfaceCapabilitiesKHR capabilities{};
  std::vector<VkSurfaceFormatKHR> formats;
  std::vector<VkPresentModeKHR> present_modes;
};

struct Drawable {
  VkBuffer vertex_buffer{nullptr};
  VkDeviceMemory vertex_buffer_memory{nullptr};
  VkBuffer index_buffer{nullptr};
  VkDeviceMemory index_buffer_memory{nullptr};
  uint32_t index_count{0};
};

// clang-format off
const std::vector<Vertex> k_vertices_cube {
    {{-0.5F,  0.5F, -0.5F}, {1.F, 0.F, 0.F}},
    {{ 0.5F,  0.5F, -0.5F}, {1.F, 1.F, 0.F}},
    {{ 0.5F, -0.5F, -0.5F}, {0.F, 1.F, 0.F}},
    {{-0.5F, -0.5F, -0.5F}, {0.F, 1.F, 1.F}},

    {{-0.5F,  0.5F,  0.5F}, {0.F, 0.F, 1.F}},
    {{ 0.5F,  0.5F,  0.5F}, {1.F, 0.F, 1.F}},
    {{ 0.5F, -0.5F,  0.5F}, {0.F, 0.F, 0.F}},
    {{-0.5F, -0.5F,  0.5F}, {1.F, 1.F, 1.F}},
};

const std::vector<uint16_t> k_indices_cube {
    0, 1, 2, 0, 2, 3, // 前
    1, 5, 6, 1, 6, 2, // 右
    5, 4, 7, 5, 7, 6, // 后
    4, 0, 3, 4, 3, 7, // 左
    3, 2, 6, 3, 6, 7, // 上
    4, 5, 1, 4, 1, 0, // 下
};

const std::vector<Vertex> k_vertices_plane {
    { {-0.7F, -0.7F,  0.0F }, { 1.F, 1.F, 1.F } },
    { {-0.7F,  0.7F,  0.0F }, { 1.F, 1.F, 1.F } },
    { { 0.7F,  0.7F,  0.0F }, { 1.F, 1.F, 1.F } },
    { { 0.7F, -0.7F,  0.0F }, { 1.F, 1.F, 1.F } },
};

const std::vector<uint16_t> k_indices_plane {
    0, 1, 2, 0, 2, 3
};

// clang-format on

class HelloTriangleApplication {
public:
  void Run() {
    InitWindow();
    InitVulkan();
    MainLoop();
    Cleanup();
  }

private:
  void InitWindow() {
    if (GLFW_FALSE == glfwInit()) {
      throw std::runtime_error("failed to init GLFW");
    }

    // 禁止 glfw 创建 OpenGL 上下文
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    m_window = glfwCreateWindow(k_width, k_height, "Vulkan", nullptr, nullptr);

    glfwSetWindowUserPointer(m_window, this);
    glfwSetFramebufferSizeCallback(m_window, FramebufferResizeCallback);
    glfwSetKeyCallback(m_window, KeyCallback);
    glfwSetCursorPosCallback(m_window, CursorPosCallback);
    glfwSetScrollCallback(m_window, ScrollCallback);
    glfwSetMouseButtonCallback(m_window, MouseButtonCallback);
  }

  void InitVulkan() {
    CreateInstance();
    SetupDebugCallback();
    CreateSurface();
    PickPhysicalDevice();
    CreateLogicalDevice();
    CreateSwapChain();
    CreateImageViews();
    CreateRenderPass();
    CreateDescriptorSetLayout();
    CreateGraphicsPipeline();
    CreateCommandPool();
    CreateDepthResources();
    CreateFramebuffers();
    CreateVertexBuffer(k_vertices_cube, m_drawable_cube);
    CreateIndexBuffer(k_indices_cube, m_drawable_cube);
    CreateVertexBuffer(k_vertices_plane, m_drawable_plane);
    CreateIndexBuffer(k_indices_plane, m_drawable_plane);
    CreateUniformBuffers();
    CreateDescriptorPool();
    CreateDescriptorSets();
    CreateCommandBuffers();
    CreateSyncObjects();
  }

  void MainLoop() {
    while (!glfwWindowShouldClose(m_window)) {
      glfwPollEvents();
      DrawFrame();
    }

    // 等待逻辑设备的操作结束执行
    // DrawFrame 函数中的操作是异步执行的，关闭窗口跳出while循环时，绘制操作和呈现操作可能仍在执行，不能进行清除操作
    vkDeviceWaitIdle(m_device);
  }

  void Cleanup() noexcept {
    CleanupSwapChain();

    DestroyDrawable(m_drawable_cube);
    DestroyDrawable(m_drawable_plane);

    vkDestroyPipeline(m_device, m_graphics_pipeline, nullptr);
    vkDestroyPipelineLayout(m_device, m_pipeline_layout, nullptr);
    vkDestroyRenderPass(m_device, m_render_pass, nullptr);

    for (size_t i = 0; i < k_max_frames_in_flight; ++i) {
      vkDestroyBuffer(m_device, m_uniform_buffers.at(i), nullptr);
      vkFreeMemory(m_device, m_uniform_buffers_memory.at(i), nullptr);
    }

    vkDestroyDescriptorPool(m_device, m_descriptor_pool, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_descriptor_set_layout, nullptr);

    for (size_t i = 0; i < k_max_frames_in_flight; ++i) {
      vkDestroySemaphore(m_device, m_image_available_semaphores.at(i), nullptr);
      vkDestroyFence(m_device, m_in_flight_fences.at(i), nullptr);
    }
    for (auto semaphore : m_render_finished_semaphores) {
      vkDestroySemaphore(m_device, semaphore, nullptr);
    }

    vkDestroyCommandPool(m_device, m_command_pool, nullptr);
    vkDestroyDevice(m_device, nullptr);

    if (k_enable_validation_layers) {
      DestroyDebugUtilsMessengerEXT(m_instance, m_debug_messenger, nullptr);
    }

    vkDestroySurfaceKHR(m_instance, m_surface, nullptr);  // 清理表面对象，必须在 Vulkan 实例清理之前
    vkDestroyInstance(m_instance, nullptr);

    glfwDestroyWindow(m_window);
    glfwTerminate();
  }

private:
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

    // Vulkan 是一个与平台无关的 API ，所以需要一个和窗口系统交互的扩展
    // 使用 glfw 获取这个扩展
    uint32_t glfw_extension_count = 0;
    const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);
    std::cout << "-------------------------------------------\n"
              << "GLFW extensions:\n";
    for (size_t i = 0; i < glfw_extension_count; ++i) {
      std::cout << glfw_extensions[i] << '\n';
    }

    // 将需要开启的所有扩展添加到列表并返回
    std::vector<const char*> required_extensions(glfw_extensions, glfw_extensions + glfw_extension_count);
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
  bool IsDeviceSuitable(VkPhysicalDevice device) const noexcept {
    // 获取基本的设置属性，name、type以及Vulkan版本等等
    // VkPhysicalDeviceProperties deviceProperties;
    // vkGetPhysicalDeviceProperties(device, &deviceProperties);

    // 获取对纹理的压缩、64位浮点数和多视图渲染等可选功能的支持
    // VkPhysicalDeviceFeatures device_features;
    // vkGetPhysicalDeviceFeatures(device, &device_features);

    auto indices = FindQueueFamilies(device);
    auto extensions_supported = CheckDeviceExtensionSupported(device);

    bool swap_chain_adequate{false};
    if (extensions_supported) {
      auto swap_chain_support = QuerySwapChainSupport(device);
      swap_chain_adequate = !swap_chain_support.formats.empty() & !swap_chain_support.present_modes.empty();
    }

    return indices.IsComplete() && extensions_supported && swap_chain_adequate;
  }

  /// @brief 查找满足需求的队列族
  /// @details 不同的队列族支持不同的类型的指令，例如计算、内存传输、绘图等指令
  /// @param device
  /// @return
  QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device) const noexcept {
    QueueFamilyIndices indices;

    // 获取物理设备支持的队列族列表
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);
    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());

    for (uint32_t i = 0; i < queue_families.size(); ++i) {
      // 图形队列族
      if (queue_families.at(i).queueFlags & VK_QUEUE_GRAPHICS_BIT) {
        indices.graphics_family = i;
      }

      // 呈现队列族
      VkBool32 present_support = false;
      vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &present_support);

      if (present_support) {
        indices.present_family = i;
      }

      if (indices.IsComplete()) {
        break;
      }
    }

    return indices;
  }

  /// @brief 创建逻辑设备作为和物理设备交互的接口
  void CreateLogicalDevice() {
    auto indices = FindQueueFamilies(m_physical_device);

    std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
    std::set<uint32_t> unique_queue_families{lvk_tidy::GetRequiredValue(indices.graphics_family),
                                             lvk_tidy::GetRequiredValue(indices.present_family)};

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
    create_info.enabledExtensionCount = static_cast<uint32_t>(k_device_extensions.size());
    create_info.ppEnabledExtensionNames = k_device_extensions.data();

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

    // 获取指定队列族的队列句柄，设备队列在逻辑设备被销毁时隐式清理
    // 此处的队列簇可能相同，也就是以相同的参数调用了两次这个函数
    // 1.逻辑设备对象
    // 2.队列族索引
    // 3.队列索引，因为只创建了一个队列，所以此处使用索引0
    // 4.用来存储返回的队列句柄的内存地址
    vkGetDeviceQueue(m_device, lvk_tidy::GetRequiredValue(indices.graphics_family), 0, &m_graphics_queue);
    vkGetDeviceQueue(m_device, lvk_tidy::GetRequiredValue(indices.present_family), 0, &m_present_queue);
  }

  /// @brief 创建表面，需要在程序退出前清理
  /// @details 不同平台创建表面的方式不一样，这里使用 GLFW 统一创建
  void CreateSurface() {
    // Vulkan 不能直接与窗口系统进行交互（是一个与平台特性无关的API集合），surface是 Vulkan 与窗体系统的连接桥梁
    // 需要在instance创建之后立即创建窗体surface，因为它会影响物理设备的选择
    // 窗体surface本身对于 Vulkan 也是非强制的，不需要同 OpenGL 一样必须要创建窗体surface
    if (VK_SUCCESS != glfwCreateWindowSurface(m_instance, m_window, nullptr, &m_surface)) {
      throw std::runtime_error("failed to create window surface");
    }
  }

  /// @brief 检测所需的扩展是否支持
  /// @param device
  /// @return
  bool CheckDeviceExtensionSupported(VkPhysicalDevice device) const noexcept {
    uint32_t extension_count{0};
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr);
    std::vector<VkExtensionProperties> available_extensions(extension_count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, available_extensions.data());

    std::set<std::string> required_extensions(std::begin(k_device_extensions), std::end(k_device_extensions));

    for (const auto& extension : available_extensions) {
      required_extensions.erase(static_cast<const char*>(extension.extensionName));
    }

    // 如果为空，则支持
    return required_extensions.empty();
  }

  /// @brief 查询交换链支持的细节信息
  /// @param device
  /// @return
  SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device) const noexcept {
    SwapChainSupportDetails details;

    // 查询基础表面特性
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_surface, &details.capabilities);

    // 查询表面支持的格式，确保集合对于所有有效的格式可扩充
    uint32_t format_count{0};
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &format_count, nullptr);
    if (0 != format_count) {
      details.formats.resize(format_count);
      vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &format_count, details.formats.data());
    }

    // 查询表面支持的呈现模式
    uint32_t present_mode_count{0};
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &present_mode_count, nullptr);
    if (0 != present_mode_count) {
      details.present_modes.resize(present_mode_count);
      vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &present_mode_count, details.present_modes.data());
    }

    return details;
  }

  /// @brief 选择合适的表面格式
  /// @param available_formats
  /// @return
  VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& available_formats) const noexcept {
    // VkSurfaceFormatKHR 结构都包含一个 format 和一个 colorSpace 成员
    // format 成员变量指定色彩通道和类型
    // colorSpace 成员描述 SRGB 颜色空间是否通过 VK_COLOR_SPACE_SRGB_NONLINEAR_KHR 标志支持
    // 在较早版本的规范中，这个标志名为 VK_COLORSPACE_SRGB_NONLINEAR_KHR

    // 表面没有自己的首选格式，直接返回指定的格式
    if (1 == available_formats.size() && available_formats.front().format == VK_FORMAT_UNDEFINED) {
      return {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    }

    // 遍历格式列表，如果想要的格式存在则直接返回
    for (const auto& available_format : available_formats) {
      if (VK_FORMAT_B8G8R8A8_UNORM == available_format.format &&
          VK_COLOR_SPACE_SRGB_NONLINEAR_KHR == available_format.colorSpace) {
        return available_format;
      }
    }

    // 如果以上两种方式都失效，通过“优良”进行打分排序，大多数情况下会选择第一个格式作为理想的选择
    return available_formats.front();
  }

  /// @brief 查找最佳的可用呈现模式，模式是非常重要的，因为它代表了在屏幕呈现图像的条件
  /// @param available_present_modes
  /// @return
  VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& available_present_modes) const noexcept {
    // 以下4种模式可以使用
    // 1.VK_PRESENT_MODE_IMMEDIATE_KHR: 应用程序提交的图像被立即传输到屏幕呈现，这种模式可能会造成撕裂效果。
    // 2.VK_PRESENT_MODE_FIFO_KHR:
    // 交换链被看作一个队列，当显示内容需要刷新的时候，显示设备从队列的前面获取图像，并且程序将渲染完成的图像插入队列的后面。
    // 如果队列是满的程序会等待。这种规模与视频游戏的垂直同步很类似。显示设备的刷新时刻被成为“垂直中断”。
    // 3.VK_PRESENT_MODE_FIFO_RELAXED_KHR:
    // 该模式与上一个模式略有不同的地方为，如果应用程序存在延迟，即接受最后一个垂直同步信号时队列空了，
    // 将不会等待下一个垂直同步信号，而是将图像直接传送。这样做可能导致可见的撕裂效果。
    // 4.VK_PRESENT_MODE_MAILBOX_KHR:
    // 这是第二种模式的变种。当交换链队列满的时候，选择新的替换旧的图像，从而替代阻塞应用程序的情形。
    // 这种模式通常用来实现三重缓冲区，与标准的垂直同步双缓冲相比，它可以有效避免延迟带来的撕裂效果。

    VkPresentModeKHR best_mode = VK_PRESENT_MODE_FIFO_KHR;

    for (const auto& available_present_mode : available_present_modes) {
      if (VK_PRESENT_MODE_MAILBOX_KHR == available_present_mode) {
        return available_present_mode;
      }
      if (VK_PRESENT_MODE_IMMEDIATE_KHR == available_present_mode) {
        best_mode = available_present_mode;
      }
    }

    return best_mode;
  }

  /// @brief 设置交换范围，交换范围是交换链中图像的分辨率
  /// @param capabilities
  /// @return
  VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) const noexcept {
    // 如果是最大值则表示允许我们自己选择对于窗口最合适的交换范围
    if (std::numeric_limits<uint32_t>::max() != capabilities.currentExtent.width) {
      return capabilities.currentExtent;
    }
    int width{0};
    int height{0};
    glfwGetFramebufferSize(m_window, &width, &height);

    // 使用GLFW窗口的大小来设置交换链中图像的分辨率
    VkExtent2D actual_extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};

    actual_extent.width =
      std::clamp(actual_extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    actual_extent.height =
      std::clamp(actual_extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

    return actual_extent;
  }

  /// @brief 创建交换链
  void CreateSwapChain() {
    SwapChainSupportDetails swap_chain_support = QuerySwapChainSupport(m_physical_device);
    VkSurfaceFormatKHR surface_format = ChooseSwapSurfaceFormat(swap_chain_support.formats);
    VkPresentModeKHR present_mode = ChooseSwapPresentMode(swap_chain_support.present_modes);
    VkExtent2D extent = ChooseSwapExtent(swap_chain_support.capabilities);

    // 交换链中的图像数量，可以理解为队列的长度，指定运行时图像的最小数量
    // maxImageCount数值为0代表除了内存之外没有限制
    uint32_t image_count = swap_chain_support.capabilities.minImageCount + 1;
    if (swap_chain_support.capabilities.maxImageCount > 0 &&
        image_count > swap_chain_support.capabilities.maxImageCount) {
      image_count = swap_chain_support.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = m_surface;
    create_info.minImageCount = image_count;
    create_info.imageFormat = surface_format.format;
    create_info.imageColorSpace = surface_format.colorSpace;
    create_info.imageExtent = extent;
    create_info.imageArrayLayers = 1;  // 图像包含的层次，通常为1，3d图像大于1
    create_info.imageUsage =
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;  // 指定在图像上进行怎样的操作，比如显示（颜色附件）、后处理等

    auto indices = FindQueueFamilies(m_physical_device);
    const std::array<uint32_t, 2> queue_family_indices{
      static_cast<uint32_t>(lvk_tidy::GetRequiredValue(indices.graphics_family)),
      static_cast<uint32_t>(lvk_tidy::GetRequiredValue(indices.present_family))};

    // 在多个队列族使用交换链图像的方式
    if (indices.graphics_family != indices.present_family) {
      // 图形队列和呈现队列不是同一个队列
      // VK_SHARING_MODE_CONCURRENT 表示图像可以在多个队列族间使用，不需要显式地改变图像所有权
      create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
      create_info.queueFamilyIndexCount = 2;
      create_info.pQueueFamilyIndices = queue_family_indices.data();
    } else {
      // VK_SHARING_MODE_EXCLUSIVE 表示一张图像同一时间只能被一个队列族所拥有，这种模式性能最佳
      create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
      create_info.queueFamilyIndexCount = 0;
      create_info.pQueueFamilyIndices = nullptr;
    }

    // 指定一个固定的变换操作（需要交换链具有supportedTransforms特性），此处不进行任何变换
    create_info.preTransform = swap_chain_support.capabilities.currentTransform;
    // 指定 alpha 通道是否被用来和窗口系统中的其他窗口进行混合操作
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    // 设置呈现模式
    create_info.presentMode = present_mode;
    // 设置为true，不关心被遮蔽的像素数据，比如由于其他的窗体置于前方时或者渲染的部分内容存在于可视区域之外，
    // 除非真的需要读取这些像素获数据进行处理（设置为true回读窗口像素可能会有问题），否则可以开启裁剪获得最佳性能。
    create_info.clipped = VK_TRUE;
    // 交换链重建
    // Vulkan运行时，交换链可能在某些条件下被替换，比如窗口调整大小或者交换链需要重新分配更大的图像队列。
    // 在这种情况下，交换链实际上需要重新分配创建，并且必须在此字段中指定对旧的引用，用以回收资源
    create_info.oldSwapchain = VK_NULL_HANDLE;

    if (VK_SUCCESS != vkCreateSwapchainKHR(m_device, &create_info, nullptr, &m_swap_chain)) {
      throw std::runtime_error("failed to create swap chain");
    }

    // 获取交换链中图像，图像会在交换链销毁的同时自动清理
    // 之前给VkSwapchainCreateInfoKHR 设置了期望的图像数量，但是实际运行允许创建更多的图像数量，因此需要重新获取数量
    vkGetSwapchainImagesKHR(m_device, m_swap_chain, &image_count, nullptr);
    m_swap_chain_images.resize(image_count);
    vkGetSwapchainImagesKHR(m_device, m_swap_chain, &image_count, m_swap_chain_images.data());

    m_swap_chain_image_format = surface_format.format;
    m_swap_chain_extent = extent;
  }

  /// @brief 创建图像视图
  /// @details 任何 VkImage 对象都需要通过 VkImageView 来绑定访问，包括处于交换链中的、处于渲染管线中的
  void CreateImageViews() {
    m_swap_chain_image_views.resize(m_swap_chain_images.size());

    for (size_t i = 0; i < m_swap_chain_images.size(); ++i) {
      VkImageViewCreateInfo create_info = {};
      create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      create_info.image = m_swap_chain_images.at(i);
      create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;  // 1d 2d 3d cube纹理
      create_info.format = m_swap_chain_image_format;
      create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;  // 图像颜色通过的映射
      create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
      create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
      create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
      create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;  // 指定图像的用途，图像的哪一部分可以被访问
      create_info.subresourceRange.baseMipLevel = 0;
      create_info.subresourceRange.levelCount = 1;
      create_info.subresourceRange.baseArrayLayer = 0;
      create_info.subresourceRange.layerCount = 1;

      if (VK_SUCCESS != vkCreateImageView(m_device, &create_info, nullptr, &m_swap_chain_image_views.at(i))) {
        throw std::runtime_error("failed to create image views");
      }
    }
  }

  /// @brief 创建图形管线
  /// @details 在 Vulkan 中几乎不允许对图形管线进行动态设置，也就意味着每一种状态都需要提前创建一个图形管线
  void CreateGraphicsPipeline() {
    auto vert_shader_code = ReadFile(PROJECT_SHADER_DIR "02_21_base_vert.spv");
    auto frag_shader_code = ReadFile(PROJECT_SHADER_DIR "02_21_base_frag.spv");

    VkShaderModule vert_shader_module = CreateShaderModule(vert_shader_code);
    VkShaderModule frag_shader_module = CreateShaderModule(frag_shader_code);

    VkPipelineShaderStageCreateInfo vert_shader_stage_info = {};
    vert_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vert_shader_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vert_shader_stage_info.module = vert_shader_module;
    vert_shader_stage_info.pName = "main";                 // 指定调用的着色器函数，同一份代码可以实现多个着色器
    vert_shader_stage_info.pSpecializationInfo = nullptr;  // 设置着色器常量

    VkPipelineShaderStageCreateInfo frag_shader_stage_info = {};
    frag_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    frag_shader_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    frag_shader_stage_info.module = frag_shader_module;
    frag_shader_stage_info.pName = "main";
    frag_shader_stage_info.pSpecializationInfo = nullptr;

    auto binding_description = Vertex::GetBindingDescription();
    auto attribute_descriptions = Vertex::GetAttributeDescriptions();

    // 顶点信息
    VkPipelineVertexInputStateCreateInfo vertex_input_info = {};
    vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_info.vertexBindingDescriptionCount = 1;
    vertex_input_info.pVertexBindingDescriptions = &binding_description;
    vertex_input_info.vertexAttributeDescriptionCount = static_cast<uint32_t>(attribute_descriptions.size());
    vertex_input_info.pVertexAttributeDescriptions = attribute_descriptions.data();

    // 拓扑信息
    VkPipelineInputAssemblyStateCreateInfo input_assembly = {};
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;  // 指定绘制的图元类型：点、线、三角形
    input_assembly.primitiveRestartEnable = VK_FALSE;

    // 视口
    VkViewport viewport = {};
    viewport.x = 0.F;
    viewport.y = 0.F;
    viewport.width = static_cast<float>(m_swap_chain_extent.width);
    viewport.height = static_cast<float>(m_swap_chain_extent.height);
    viewport.minDepth = 0.F;  // 深度值范围，必须在 [0.F, 1.F]之间
    viewport.maxDepth = 1.F;

    // 裁剪
    VkRect2D scissor = {};
    scissor.offset = {0, 0};
    scissor.extent = m_swap_chain_extent;

    VkPipelineViewportStateCreateInfo viewport_state = {};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.pViewports = &viewport;
    viewport_state.scissorCount = 1;
    viewport_state.pScissors = &scissor;

    // 光栅化
    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;  // 设置为true会禁止一切片段输出到帧缓冲
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.F;
    rasterizer.cullMode = VK_CULL_MODE_NONE;                 // 表面剔除类型，正面、背面、双面剔除
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;  // 指定顺时针的顶点序是正面还是反面
    rasterizer.depthBiasEnable = VK_FALSE;
    rasterizer.depthBiasConstantFactor = 1.F;
    rasterizer.depthBiasClamp = 0.F;
    rasterizer.depthBiasSlopeFactor = 0.F;

    // 多重采样
    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.F;
    multisampling.pSampleMask = nullptr;
    multisampling.alphaToCoverageEnable = VK_FALSE;
    multisampling.alphaToOneEnable = VK_FALSE;

    // 深度和模板测试
    VkPipelineDepthStencilStateCreateInfo depth_stencil = {};
    depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil.depthTestEnable = VK_TRUE;            // 是否启用深度测试
    depth_stencil.depthWriteEnable = VK_TRUE;           // 深度测试通过后是否写入深度缓冲
    depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;  // 深度值比较方式
    depth_stencil.depthBoundsTestEnable = VK_FALSE;     // 指定可选的深度范围测试
    depth_stencil.minDepthBounds = 0.F;
    depth_stencil.maxDepthBounds = 1.F;
    depth_stencil.stencilTestEnable = VK_FALSE;  // 模板测试
    depth_stencil.front = {};
    depth_stencil.back = {};

    // 颜色混合，可以对指定帧缓冲单独设置，也可以设置全局颜色混合方式
    VkPipelineColorBlendAttachmentState color_blend_attachment{};
    color_blend_attachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo color_blending{};
    color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blending.logicOpEnable = VK_FALSE;
    color_blending.logicOp = VK_LOGIC_OP_COPY;
    color_blending.attachmentCount = 1;
    color_blending.pAttachments = &color_blend_attachment;
    color_blending.blendConstants[0] = 0.0F;
    color_blending.blendConstants[1] = 0.0F;
    color_blending.blendConstants[2] = 0.0F;
    color_blending.blendConstants[3] = 0.0F;

    // 动态状态，视口大小、线宽、混合常量等
    std::vector<VkDynamicState> dynamic_states = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic_state{};
    dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.dynamicStateCount = static_cast<uint32_t>(dynamic_states.size());
    dynamic_state.pDynamicStates = dynamic_states.data();

    // 管线布局，在着色器中使用 uniform 变量
    VkPipelineLayoutCreateInfo pipeline_layout_info{};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = 1;
    pipeline_layout_info.pSetLayouts = &m_descriptor_set_layout;
    pipeline_layout_info.pushConstantRangeCount = 0;

    if (VK_SUCCESS != vkCreatePipelineLayout(m_device, &pipeline_layout_info, nullptr, &m_pipeline_layout)) {
      throw std::runtime_error("failed to create pipeline layout");
    }

    const std::array<VkPipelineShaderStageCreateInfo, 2> shader_stages{vert_shader_stage_info, frag_shader_stage_info};

    // 完整的图形管线包括：着色器阶段、固定功能状态、管线布局、渲染流程
    VkGraphicsPipelineCreateInfo pipeline_info = {};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = 2;
    pipeline_info.pStages = shader_stages.data();
    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pDepthStencilState = nullptr;
    pipeline_info.pColorBlendState = &color_blending;
    pipeline_info.pDynamicState = &dynamic_state;
    pipeline_info.layout = m_pipeline_layout;
    pipeline_info.renderPass = m_render_pass;
    pipeline_info.subpass = 0;                   // 子流程在子流程数组中的索引
    pipeline_info.basePipelineHandle = nullptr;  // 以一个创建好的图形管线为基础创建一个新的图形管线
    pipeline_info.basePipelineIndex = -1;  // 只有该结构体的成员 flags 被设置为 VK_PIPELINE_CREATE_DERIVATIVE_BIT 才有效
    pipeline_info.pDepthStencilState = &depth_stencil;

    if (VK_SUCCESS !=
        vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &m_graphics_pipeline)) {
      throw std::runtime_error("failed to create graphics pipeline");
    }

    vkDestroyShaderModule(m_device, frag_shader_module, nullptr);
    vkDestroyShaderModule(m_device, vert_shader_module, nullptr);
  }

  /// @brief 使用着色器字节码数组创建 VkShaderModule 对象
  /// @param code
  /// @return
  VkShaderModule CreateShaderModule(const std::vector<char>& code) const {
    VkShaderModuleCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = code.size();
    create_info.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shader_module = VK_NULL_HANDLE;
    if (VK_SUCCESS != vkCreateShaderModule(m_device, &create_info, nullptr, &shader_module)) {
      throw std::runtime_error("failed to create shader module");
    }

    return shader_module;
  }

  /// @brief 创建渲染流程对象，用于渲染的帧缓冲附着，指定颜色和深度缓冲以及采样数
  void CreateRenderPass() {
    // 附着描述
    VkAttachmentDescription color_attachment = {};
    color_attachment.format = m_swap_chain_image_format;                 // 颜色缓冲附着的格式
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;                    // 采样数
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;               // 渲染之前对附着中的数据（颜色和深度）进行操作
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;             // 渲染之后对附着中的数据（颜色和深度）进行操作
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;    // 渲染之前对模板缓冲的操作
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;  // 渲染之后对模板缓冲的操作
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;          // 渲染流程开始前的图像布局方式
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;      // 渲染流程结束后的图像布局方式

    VkAttachmentDescription depth_attachment = {};
    depth_attachment.format = FindDepthFormat();
    depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;  // 绘制结束后不需要从深度缓冲复制深度数据
    depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;  // 不需要读取之前深度图像数据
    depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // 子流程引用的附着
    VkAttachmentReference color_attachment_ref = {};
    color_attachment_ref.attachment = 0;  // 索引，对应于片段着色器中的 layout(location = 0) out
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depth_attachment_ref = {};
    depth_attachment_ref.attachment = 1;
    depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // 子流程
    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;  // 图形渲染子流程
    subpass.colorAttachmentCount = 1;                             // 颜色附着个数
    subpass.pColorAttachments = &color_attachment_ref;            // 指定颜色附着
    subpass.pDepthStencilAttachment =
      &depth_attachment_ref;  // 指定深度模板附着，深度模板附着只能是一个，所以不用设置数量

    // 渲染流程使用的依赖信息
    VkSubpassDependency dependency = {};
    dependency.srcSubpass =
      VK_SUBPASS_EXTERNAL;      // 渲染流程开始前的子流程，为了避免出现循环依赖，dst的值必须大于src的值
    dependency.dstSubpass = 0;  // 渲染流程结束后的子流程
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                              VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;  // 指定需要等待的管线阶段
    dependency.srcAccessMask = 0;                                          // 指定子流程将进行的操作类型
    dependency.dstStageMask =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 2> attachments{color_attachment, depth_attachment};

    VkRenderPassCreateInfo render_pass_info = {};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = static_cast<uint32_t>(attachments.size());
    render_pass_info.pAttachments = attachments.data();
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;
    render_pass_info.dependencyCount = 1;
    render_pass_info.pDependencies = &dependency;

    if (VK_SUCCESS != vkCreateRenderPass(m_device, &render_pass_info, nullptr, &m_render_pass)) {
      throw std::runtime_error("failed to create render pass");
    }
  }

  /// @brief 创建帧缓冲对象，附着需要绑定到帧缓冲对象上使用
  void CreateFramebuffers() {
    m_swap_chain_framebuffers.resize(m_swap_chain_image_views.size());

    for (size_t i = 0; i < m_swap_chain_image_views.size(); ++i) {
      std::array<VkImageView, 2> attachments{m_swap_chain_image_views.at(i), m_depth_image_view};

      VkFramebufferCreateInfo framebuffer_info = {};
      framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
      framebuffer_info.renderPass = m_render_pass;
      framebuffer_info.attachmentCount = static_cast<uint32_t>(attachments.size());
      framebuffer_info.pAttachments = attachments.data();
      framebuffer_info.width = m_swap_chain_extent.width;
      framebuffer_info.height = m_swap_chain_extent.height;
      framebuffer_info.layers = 1;

      if (VK_SUCCESS != vkCreateFramebuffer(m_device, &framebuffer_info, nullptr, &m_swap_chain_framebuffers.at(i))) {
        throw std::runtime_error("failed to create framebuffer");
      }
    }
  }

  /// @brief 创建指令池，用于管理指令缓冲对象使用的内存，并负责指令缓冲对象的分配
  void CreateCommandPool() {
    auto indices = FindQueueFamilies(m_physical_device);

    // VK_COMMAND_POOL_CREATE_TRANSIENT_BIT
    // 指定从Pool中分配的CommandBuffer将是短暂的，意味着它们将在相对较短的时间内被重置或释放
    // VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT 允许从Pool中分配的任何CommandBuffer被单独重置到inital状态
    // 没有设置这个flag则不能使用 vkResetCommandBuffer
    // VK_COMMAND_POOL_CREATE_PROTECTED_BIT 指定从Pool中分配的CommandBuffer是受保护的CommandBuffer
    VkCommandPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_info.queueFamilyIndex = lvk_tidy::GetRequiredValue(indices.graphics_family);

    if (VK_SUCCESS != vkCreateCommandPool(m_device, &pool_info, nullptr, &m_command_pool)) {
      throw std::runtime_error("failed to create command pool");
    }
  }

  /// @brief 为交换链中的每一个图像创建指令缓冲对象，使用它记录绘制指令
  void CreateCommandBuffers() {
    m_command_buffers.resize(m_swap_chain_framebuffers.size());

    VkCommandBufferAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = m_command_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;  // 指定是主要还是辅助指令缓冲对象
    alloc_info.commandBufferCount = static_cast<uint32_t>(m_command_buffers.size());

    if (VK_SUCCESS != vkAllocateCommandBuffers(m_device, &alloc_info, m_command_buffers.data())) {
      throw std::runtime_error("failed to allocate command buffers");
    }
  }

  /// @brief 记录指令到指令缓冲
  void RecordCommandBuffer(VkCommandBuffer command_buffer, VkFramebuffer framebuffer) const {
    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;  // 指定怎样使用指令缓冲
    begin_info.pInheritanceInfo = nullptr;  // 只用于辅助指令缓冲，指定从调用它的主要指令缓冲继承的状态

    if (VK_SUCCESS != vkBeginCommandBuffer(command_buffer, &begin_info)) {
      throw std::runtime_error("failed to begin recording command buffer");
    }

    // 清除色，相当于背景色
    std::array<VkClearValue, 2> clear_values{};
    clear_values.at(0).color = {{.1F, .2F, .3F, 1.F}};
    clear_values.at(1).depthStencil = {1.F, 0};

    uint32_t fb_width = m_swap_chain_extent.width;    // framebuffer 的宽度
    uint32_t fb_height = m_swap_chain_extent.height;  // farmebuffer 的高度

    VkRenderPassBeginInfo render_pass_info = {};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_info.renderPass = m_render_pass;
    render_pass_info.framebuffer = framebuffer;
    render_pass_info.renderArea.offset = {0, 0};
    render_pass_info.renderArea.extent = {fb_width, fb_height};
    render_pass_info.clearValueCount = static_cast<uint32_t>(clear_values.size());
    render_pass_info.pClearValues = clear_values.data();

    VkViewport viewport = {};
    viewport.x = 0.F;
    viewport.y = 0.F;
    viewport.width = static_cast<float>(fb_width);
    viewport.height = static_cast<float>(fb_height);
    viewport.minDepth = 0.F;
    viewport.maxDepth = 1.F;

    VkRect2D scissor = {};
    scissor.offset = {0, 0};
    scissor.extent = {fb_width, fb_height};

    const std::array<VkDeviceSize, 1> offsets{0};

    //--------------------------------------------------------------------
    vkCmdBeginRenderPass(command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphics_pipeline);
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);
    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline_layout, 0, 1,
                            &m_descriptor_sets.at(m_current_frame), 0, nullptr);

    // 绘制立方体
    const std::array<VkBuffer, 1> vertex_buffers_cube{m_drawable_cube.vertex_buffer};
    vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers_cube.data(), offsets.data());
    vkCmdBindIndexBuffer(command_buffer, m_drawable_cube.index_buffer, 0, VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed(command_buffer, m_drawable_cube.index_count, 1, 0, 0, 0);

    // 清除深度缓冲区
    VkClearValue clear_value{};
    clear_value.depthStencil = {1.F, 0};

    VkClearAttachment clear_attachments{VK_IMAGE_ASPECT_DEPTH_BIT, 1, clear_value};
    VkClearRect clear_rects{{VkOffset2D{0, 0}, VkExtent2D{fb_width, fb_height}}, 0, 1};
    vkCmdClearAttachments(command_buffer, 1, &clear_attachments, 1, &clear_rects);

    // 绘制平面
    const std::array<VkBuffer, 1> vertex_buffers_plane{m_drawable_plane.vertex_buffer};
    vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers_plane.data(), offsets.data());
    vkCmdBindIndexBuffer(command_buffer, m_drawable_plane.index_buffer, 0, VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed(command_buffer, m_drawable_plane.index_count, 1, 0, 0, 0);

    vkCmdEndRenderPass(command_buffer);

    // 结束记录指令到指令缓冲
    if (VK_SUCCESS != vkEndCommandBuffer(command_buffer)) {
      throw std::runtime_error("failed to record command buffer");
    }
  }

  /// @brief 绘制每一帧
  /// @details 流程：从交换链获取一张图像、对帧缓冲附着执行指令缓冲中的渲染指令、返回渲染后的图像到交换链进行呈现操作
  void DrawFrame() {
    // 等待一组栅栏中的一个或全部栅栏发出信号，即上一次提交的指令结束执行
    // 使用栅栏可以进行CPU与GPU之间的同步，防止超过 k_max_frames_in_flight 帧的指令同时被提交执行
    vkWaitForFences(m_device, 1, &m_in_flight_fences.at(m_current_frame), VK_TRUE,
                    std::numeric_limits<uint64_t>::max());

    // 从交换链获取一张图像
    uint32_t image_index{0};
    // 3.获取图像的超时时间，此处禁用图像获取超时
    // 4.通知的同步对象
    // 5.输出可用的交换链图像的索引
    VkResult result =
      vkAcquireNextImageKHR(m_device, m_swap_chain, std::numeric_limits<uint64_t>::max(),
                            m_image_available_semaphores.at(m_current_frame), VK_NULL_HANDLE, &image_index);

    // VK_ERROR_OUT_OF_DATE_KHR 交换链不能继续使用，通常发生在窗口大小改变后
    // VK_SUBOPTIMAL_KHR 交换链仍然可以使用，但表面属性已经不能准确匹配
    if (VK_ERROR_OUT_OF_DATE_KHR == result) {
      RecreateSwapChain();
      return;
    }
    if (VK_SUCCESS != result && VK_SUBOPTIMAL_KHR != result) {
      throw std::runtime_error("failed to acquire swap chain image");
    }

    // 每一帧都更新uniform
    UpdateUniformBuffer(static_cast<uint32_t>(m_current_frame));

    // 手动将栅栏重置为未发出信号的状态（必须手动设置）
    vkResetFences(m_device, 1, &m_in_flight_fences.at(m_current_frame));
    vkResetCommandBuffer(m_command_buffers.at(image_index), 0);
    RecordCommandBuffer(m_command_buffers.at(image_index), m_swap_chain_framebuffers.at(image_index));

    const std::array<VkPipelineStageFlags, 1> wait_stages{VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores =
      &m_image_available_semaphores.at(m_current_frame);  // 指定队列开始执行前需要等待的信号量
    submit_info.pWaitDstStageMask = wait_stages.data();   // 指定需要等待的管线阶段
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &m_command_buffers[image_index];  // 指定实际被提交执行的指令缓冲对象
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores =
      &m_render_finished_semaphores.at(image_index);  // 指定在指令缓冲执行结束后发出信号的信号量对象

    // 提交指令缓冲给图形指令队列
    // 如果不等待上一次提交的指令结束执行，可能会导致内存泄漏
    // 1.vkQueueWaitIdle 可以等待上一次的指令结束执行，2.也可以同时渲染多帧解决该问题（使用栅栏）
    // vkQueueSubmit 的最后一个参数用来指定在指令缓冲执行结束后需要发起信号的栅栏对象
    if (VK_SUCCESS != vkQueueSubmit(m_graphics_queue, 1, &submit_info, m_in_flight_fences.at(m_current_frame))) {
      throw std::runtime_error("failed to submit draw command buffer");
    }

    VkPresentInfoKHR present_info = {};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &m_render_finished_semaphores.at(image_index);  // 指定开始呈现操作需要等待的信号量
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &m_swap_chain;   // 指定用于呈现图像的交换链
    present_info.pImageIndices = &image_index;  // 指定需要呈现的图像在交换链中的索引
    present_info.pResults = nullptr;            // 可以通过该变量获取每个交换链的呈现操作是否成功的信息

    // 请求交换链进行图像呈现操作
    result = vkQueuePresentKHR(m_present_queue, &present_info);

    if (VK_ERROR_OUT_OF_DATE_KHR == result || VK_SUBOPTIMAL_KHR == result || m_framebuffer_resized) {
      m_framebuffer_resized = false;
      // 交换链不完全匹配时也重建交换链
      RecreateSwapChain();
    } else if (VK_SUCCESS != result) {
      throw std::runtime_error("failed to presend swap chain image");
    }

    // 更新当前帧索引
    m_current_frame = (m_current_frame + 1) % k_max_frames_in_flight;
  }

  /// @brief 创建同步对象，用于发出图像已经被获取可以开始渲染和渲染已经结束可以开始呈现的信号
  void CreateSyncObjects() {
    m_image_available_semaphores.resize(k_max_frames_in_flight);
    m_render_finished_semaphores.resize(m_swap_chain_images.size());
    m_in_flight_fences.resize(k_max_frames_in_flight);

    VkSemaphoreCreateInfo semaphore_info = {};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fence_info = {};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;  // 初始状态设置为已发出信号，避免 vkWaitForFences 一直等待

    for (size_t i = 0; i < k_max_frames_in_flight; ++i) {
      if (VK_SUCCESS != vkCreateSemaphore(m_device, &semaphore_info, nullptr, &m_image_available_semaphores.at(i)) ||
          vkCreateFence(m_device, &fence_info, nullptr, &m_in_flight_fences.at(i))) {
        throw std::runtime_error("failed to create synchronization objects for a frame");
      }
    }
    for (auto& semaphore : m_render_finished_semaphores) {
      if (VK_SUCCESS != vkCreateSemaphore(m_device, &semaphore_info, nullptr, &semaphore)) {
        throw std::runtime_error("failed to create render finished semaphore");
      }
    }
  }

  /// @brief 重建交换链
  void RecreateSwapChain() {
    int width{0};
    int height{0};
    glfwGetFramebufferSize(m_window, &width, &height);
    while (0 == width || 0 == height) {
      glfwGetFramebufferSize(m_window, &width, &height);
      glfwWaitEvents();
    }

    // 等待设备处于空闲状态，避免在对象的使用过程中将其清除重建
    vkDeviceWaitIdle(m_device);

    // 在重建前清理之前使用的对象
    CleanupSwapChain();

    // 可以通过动态状态来设置视口和裁剪矩形来避免重建管线
    CreateSwapChain();
    CreateImageViews();
    CreateDepthResources();
    CreateFramebuffers();
  }

  void DestroyDrawable(Drawable& drawable) noexcept {
    vkDestroyBuffer(m_device, drawable.vertex_buffer, nullptr);
    vkFreeMemory(m_device, drawable.vertex_buffer_memory, nullptr);
    vkDestroyBuffer(m_device, drawable.index_buffer, nullptr);
    vkFreeMemory(m_device, drawable.index_buffer_memory, nullptr);
  }

  /// @brief 清除交换链相关对象
  void CleanupSwapChain() noexcept {
    vkDestroyImageView(m_device, m_depth_image_view, nullptr);
    vkDestroyImage(m_device, m_depth_image, nullptr);
    vkFreeMemory(m_device, m_depth_image_memory, nullptr);

    for (auto framebuffer : m_swap_chain_framebuffers) {
      vkDestroyFramebuffer(m_device, framebuffer, nullptr);
    }

    for (auto image_view : m_swap_chain_image_views) {
      vkDestroyImageView(m_device, image_view, nullptr);
    }

    vkDestroySwapchainKHR(m_device, m_swap_chain, nullptr);
  }

  /// @brief 设置调试扩展信息
  /// @param create_info
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

  /// @brief 创建顶点缓冲
  void CreateVertexBuffer(const std::vector<Vertex>& vertices, Drawable& drawable) {
    VkDeviceSize buffer_size = sizeof(Vertex) * vertices.size();

    // 为了提升性能，使用一个临时（暂存）缓冲，先将顶点数据加载到临时缓冲，再复制到顶点缓冲
    VkBuffer staging_buffer{};
    VkDeviceMemory staging_buffer_memory{};

    // 创建一个CPU可见的缓冲作为临时缓冲
    // VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT 用于从CPU写入数据
    // VK_MEMORY_PROPERTY_HOST_COHERENT_BIT 可以保证数据被立即复制到缓冲关联的内存
    CreateBuffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging_buffer,
                 staging_buffer_memory);

    void* data{nullptr};
    // 将缓冲关联的内存映射到CPU可以访问的内存
    vkMapMemory(m_device, staging_buffer_memory, 0, buffer_size, 0, &data);
    // 将顶点数据复制到映射后的内存
    std::memcpy(data, vertices.data(), static_cast<size_t>(buffer_size));
    // 结束内存映射
    vkUnmapMemory(m_device, staging_buffer_memory);

    // 创建一个显卡读取较快的缓冲作为真正的顶点缓冲
    // 具有 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT 标记的内存最适合显卡读取，CPU通常不能访问这种类型的内存
    CreateBuffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, drawable.vertex_buffer, drawable.vertex_buffer_memory);

    // m_vertexBuffer 现在关联的内存是设备所有的（显卡），不能使用 vkMapMemory 函数对它关联的内存进行映射
    // 从临时（暂存）缓冲复制数据到显卡读取较快的缓冲中
    CopyBuffer(staging_buffer, drawable.vertex_buffer, buffer_size);

    vkDestroyBuffer(m_device, staging_buffer, nullptr);
    vkFreeMemory(m_device, staging_buffer_memory, nullptr);

    // 驱动程序可能并不会立即复制数据到缓冲关联的内存中去，因为处理器都有缓存，写入内存的数据并不一定在多个核心同时可见
    // 保证数据被立即复制到缓冲关联的内存可以使用以下函数，或者使用 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT 属性的内存类型
    // 使用函数的方法性能更好
    // 写入数据到映射的内存后，调用 vkFlushMappedMemoryRanges
    // 读取映射的内存数据前，调用 vkInvalidateMappedMemoryRanges
  }

  /// @brief 创建索引缓冲
  void CreateIndexBuffer(const std::vector<uint16_t>& indices, Drawable& drawable) {
    drawable.index_count = static_cast<uint32_t>(indices.size());
    VkDeviceSize buffer_size = sizeof(indices.front()) * indices.size();

    VkBuffer staging_buffer{};
    VkDeviceMemory staging_buffer_memory{};

    CreateBuffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging_buffer,
                 staging_buffer_memory);

    void* data{nullptr};
    vkMapMemory(m_device, staging_buffer_memory, 0, buffer_size, 0, &data);
    std::memcpy(data, indices.data(), static_cast<size_t>(buffer_size));
    vkUnmapMemory(m_device, staging_buffer_memory);

    CreateBuffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, drawable.index_buffer, drawable.index_buffer_memory);

    CopyBuffer(staging_buffer, drawable.index_buffer, buffer_size);

    vkDestroyBuffer(m_device, staging_buffer, nullptr);
    vkFreeMemory(m_device, staging_buffer_memory, nullptr);
  }

  /// @brief 创建指定类型的缓冲
  /// @details Vulkan 的缓冲是可以存储任意数据的可以被显卡读取的内存，不仅可以存储顶点数据
  /// @param size
  /// @param usage
  /// @param properties
  /// @param buffer
  /// @param buffer_memory
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

    // 4. 偏移值，需要满足能够被 mem_requirements.alighment 整除
    vkBindBufferMemory(m_device, buffer, buffer_memory, 0);
  }

  /// @brief 在缓冲之间复制数据
  /// @param src_buffer
  /// @param dst_buffer
  /// @param size
  void CopyBuffer(VkBuffer src_buffer, VkBuffer dst_buffer, VkDeviceSize size) const noexcept {
    VkCommandBufferAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandPool = m_command_pool;
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
    vkQueueSubmit(m_graphics_queue, 1, &submit_info, nullptr);
    // 等待传输操作完成，也可以使用栅栏，栅栏可以同步多个不同的内存传输操作，给驱动程序的优化空间也更大
    vkQueueWaitIdle(m_graphics_queue);

    vkFreeCommandBuffers(m_device, m_command_pool, 1, &command_buffer);
  }

  VkImageView CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspect_flags) {
    VkImageViewCreateInfo view_info{};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = image;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = format;
    view_info.subresourceRange.aspectMask = aspect_flags;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;

    VkImageView image_view{};
    if (VK_SUCCESS != vkCreateImageView(m_device, &view_info, nullptr, &image_view)) {
      throw std::runtime_error("failed to create texture image view");
    }

    return image_view;
  }

  /// @brief 查找最合适的内存类型
  /// @details 不同类型的内存所允许进行的操作以及操作的效率有所不同
  /// @param type_filter 指定需要的内存类型的位域
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

  /// @brief 创建着色器使用的每一个描述符绑定信息
  void CreateDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding ubo_layout_binding = {};
    ubo_layout_binding.binding = 0;
    ubo_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ubo_layout_binding.descriptorCount = 1;                      // uniform 缓冲对象数组的大小
    ubo_layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;  // 指定在哪一个着色器阶段使用
    ubo_layout_binding.pImmutableSamplers = nullptr;             // 指定图像采样相关的属性

    VkDescriptorSetLayoutCreateInfo layout_info = {};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = 1;
    layout_info.pBindings = &ubo_layout_binding;

    if (VK_SUCCESS != vkCreateDescriptorSetLayout(m_device, &layout_info, nullptr, &m_descriptor_set_layout)) {
      throw std::runtime_error("failed to create descriptor set layout");
    }
  }

  /// @brief 创建Uniform Buffer
  /// @details 由于缓冲需要频繁更新，所以此处使用暂存缓冲并不会带来性能提升
  void CreateUniformBuffers() {
    VkDeviceSize buffer_size = sizeof(UniformBufferObject);

    m_uniform_buffers.resize(k_max_frames_in_flight);
    m_uniform_buffers_memory.resize(k_max_frames_in_flight);
    m_uniform_buffers_mapped.resize(k_max_frames_in_flight);

    for (size_t i = 0; i < k_max_frames_in_flight; ++i) {
      CreateBuffer(buffer_size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m_uniform_buffers.at(i),
                   m_uniform_buffers_memory.at(i));
      vkMapMemory(m_device, m_uniform_buffers_memory.at(i), 0, buffer_size, 0, &m_uniform_buffers_mapped.at(i));
    }
  }

  /// @brief 在绘制每一帧时更新uniform
  /// @param current_image
  void UpdateUniformBuffer(uint32_t current_image) {
    auto aspect = static_cast<float>(m_swap_chain_extent.width) / static_cast<float>(m_swap_chain_extent.height);

    UniformBufferObject ubo{};
    ubo.model = glm::mat4(1.F);
    ubo.view = glm::lookAt(m_eye_pos, m_look_at, m_view_up);
    ubo.proj = glm::perspective(glm::radians(45.F), aspect, 0.1F, 100.F);

    // glm的裁剪坐标的Y轴和 Vulkan 是相反的
    ubo.proj[1][1] *= -1;

    // 将变换矩阵的数据复制到uniform缓冲
    std::memcpy(m_uniform_buffers_mapped[current_image], &ubo, sizeof(ubo));
  }

  /// @brief 创建描述符池，描述符集需要通过描述符池来创建
  void CreateDescriptorPool() {
    VkDescriptorPoolSize pool_size{};
    pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    pool_size.descriptorCount = static_cast<uint32_t>(k_max_frames_in_flight);

    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.poolSizeCount = 1;
    pool_info.pPoolSizes = &pool_size;
    pool_info.maxSets = static_cast<uint32_t>(k_max_frames_in_flight);  // 指定可以分配的最大描述符集个数
    pool_info.flags = 0;  // 可以用来设置独立的描述符集是否可以被清除掉，此处使用默认值

    if (VK_SUCCESS != vkCreateDescriptorPool(m_device, &pool_info, nullptr, &m_descriptor_pool)) {
      throw std::runtime_error("failed to create descriptor pool");
    }
  }

  /// @brief 创建描述符集
  void CreateDescriptorSets() {
    // 描述符布局对象的个数要匹配描述符集对象的个数
    std::vector<VkDescriptorSetLayout> layouts(k_max_frames_in_flight, m_descriptor_set_layout);

    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = m_descriptor_pool;  // 指定分配描述符集对象的描述符池
    alloc_info.descriptorSetCount = static_cast<uint32_t>(k_max_frames_in_flight);
    alloc_info.pSetLayouts = layouts.data();

    // 描述符集对象会在描述符池对象清除时自动被清除
    m_descriptor_sets.resize(k_max_frames_in_flight);
    if (VK_SUCCESS != vkAllocateDescriptorSets(m_device, &alloc_info, m_descriptor_sets.data())) {
      throw std::runtime_error("failed to allocate descriptor sets");
    }

    for (size_t i = 0; i < k_max_frames_in_flight; ++i) {
      VkDescriptorBufferInfo buffer_info{};
      buffer_info.buffer = m_uniform_buffers.at(i);
      buffer_info.offset = 0;
      buffer_info.range = sizeof(UniformBufferObject);

      VkWriteDescriptorSet descriptor_write{};
      descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      descriptor_write.dstSet = m_descriptor_sets.at(i);  // 指定要更新的描述符集对象
      descriptor_write.dstBinding = 0;                    // 指定缓冲绑定
      descriptor_write.dstArrayElement = 0;               // 描述符数组的第一个元素的索引（没有数组就使用0）
      descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
      descriptor_write.descriptorCount = 1;
      descriptor_write.pBufferInfo = &buffer_info;  // 指定描述符引用的缓冲数据
      descriptor_write.pImageInfo = nullptr;        // 指定描述符引用的图像数据
      descriptor_write.pTexelBufferView = nullptr;  // 指定描述符引用的缓冲视图

      // 更新描述符的配置
      vkUpdateDescriptorSets(m_device, 1, &descriptor_write, 0, nullptr);
    }
  }

  /// @brief 创建指定格式的图像对象
  /// @param width
  /// @param height
  /// @param format
  /// @param tiling
  /// @param usage
  /// @param properties
  /// @param image
  /// @param image_memory
  void CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage,
                   VkMemoryPropertyFlags /*properties*/, VkImage& image, VkDeviceMemory& image_memory)

  {
    // tiling成员变量可以是 VK_IMAGE_TILING_LINEAR 纹素以行主序的方式排列，可以直接访问图像
    //                     VK_IMAGE_TILING_OPTIMAL 纹素以一种对访问优化的方式排列
    // initialLayout成员变量可以是 VK_IMAGE_LAYOUT_UNDEFINED GPU不可用，纹素在第一次变换会被丢弃
    //                            VK_IMAGE_LAYOUT_PREINITIALIZED GPU不可用，纹素在第一次变换会被保留
    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.extent.width = static_cast<uint32_t>(width);
    image_info.extent.height = static_cast<uint32_t>(height);
    image_info.extent.depth = 1;
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.format = format;
    image_info.tiling = tiling;                            // 设置之后不可修改
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;  // 图像数据是接收方，不需要保留第一次变换时的纹理数据
    image_info.usage = usage;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;  // 只被一个队列族使用（支持传输操作的队列族），所以使用独占模式
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;          // 设置多重采样，只对用作附着的图像对象有效
    image_info.flags = 0;  // 可以用来设置稀疏图像的优化，比如体素地形没必要为“空气”部分分配内存

    if (VK_SUCCESS != vkCreateImage(m_device, &image_info, nullptr, &image)) {
      throw std::runtime_error("failed to create image");
    }

    VkMemoryRequirements mem_requirements{};
    vkGetImageMemoryRequirements(m_device, image, &mem_requirements);

    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_requirements.size;
    alloc_info.memoryTypeIndex = FindMemoryType(mem_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (VK_SUCCESS != vkAllocateMemory(m_device, &alloc_info, nullptr, &image_memory)) {
      throw std::runtime_error("failed to allocate image memory");
    }
    vkBindImageMemory(m_device, image, image_memory, 0);
  }

  /// @brief 配置深度图像需要的资源
  void CreateDepthResources() {
    auto depth_format = FindDepthFormat();

    CreateImage(m_swap_chain_extent.width, m_swap_chain_extent.height, depth_format, VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_depth_image,
                m_depth_image_memory);

    m_depth_image_view = CreateImageView(m_depth_image, depth_format, VK_IMAGE_ASPECT_DEPTH_BIT);
  }

  /// @brief 查找一个符合要求又被设备支持的图像数据格式
  /// @param candidates
  /// @param tiling
  /// @param features
  /// @return
  VkFormat FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling,
                               VkFormatFeatureFlags features) const {
    for (auto format : candidates) {
      // 查找可用的深度图像格式
      VkFormatProperties props{};
      vkGetPhysicalDeviceFormatProperties(m_physical_device, format, &props);
      // props.bufferFeatures        表示数据格式支持缓冲
      // props.linearTilingFeatures  表示数据格式支持线性tiling模式
      // props.optimalTilingFeatures 表示数据格式支持优化tiling模式

      if ((VK_IMAGE_TILING_LINEAR == tiling && (props.linearTilingFeatures & features) == features) ||
          (VK_IMAGE_TILING_OPTIMAL == tiling && (props.optimalTilingFeatures & features) == features)) {
        return format;
      }
    }

    throw std::runtime_error("failed to find supported format");
  }

  /// @brief 查找适合作为深度附着的图像数据格式
  /// @return
  VkFormat FindDepthFormat() const {
    return FindSupportedFormat({VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
                               VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
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
  /// @param p_create_info
  /// @param p_allocator
  /// @param p_callback
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
  /// @param p_allocator
  static void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT callback,
                                            const VkAllocationCallbacks* p_allocator) noexcept {
    auto func = lvk_tidy::LoadInstanceProcAddress<PFN_vkDestroyDebugUtilsMessengerEXT>(
      instance, "vkDestroyDebugUtilsMessengerEXT");

    if (nullptr != func) {
      func(instance, callback, p_allocator);
    }
  }

  /// @brief 读取二进制着色器文件
  /// @param file_name
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

  static void FramebufferResizeCallback(GLFWwindow* window, int /*width*/, int /*height*/) noexcept {
    if (auto app = reinterpret_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(window))) {
      app->m_framebuffer_resized = true;
    }
  }

  /// @brief 光标位置回调
  /// @param window
  /// @param xpos
  /// @param ypos
  static void CursorPosCallback(GLFWwindow* window, double xpos, double ypos) noexcept {
    if (auto app = reinterpret_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(window))) {
      // 计算鼠标移动的偏移量
      double x_offset = xpos - app->m_mouse_last_x;
      double y_offset = ypos - app->m_mouse_last_y;

      if (app->m_is_rotating) {
        // 设置灵敏度，可以根据需要调整
        constexpr auto sensitivity = 0.5 / 180.0 * std::numbers::pi_v<double>;
        x_offset *= sensitivity;
        y_offset *= sensitivity;

        // 构造旋转四元数
        glm::vec3 x_axis = glm::normalize(glm::cross(app->m_view_up, app->m_eye_pos));
        glm::vec3 y_axis = glm::normalize(app->m_view_up);
        glm::quat x_rotation = glm::angleAxis(static_cast<float>(-y_offset), x_axis);
        glm::quat y_rotation = glm::angleAxis(static_cast<float>(-x_offset), y_axis);
        glm::quat rotation_quat = x_rotation * y_rotation;

        app->m_eye_pos = glm::vec3(glm::mat4(rotation_quat) * glm::vec4(app->m_eye_pos, 1.F));
        app->m_view_up = glm::vec3(glm::mat4(rotation_quat) * glm::vec4(app->m_view_up, 1.F));
        // app->m_view_up = glm::normalize(glm::cross(x_axis, -app->m_eye_pos));
      }

      if (app->m_is_translating) {
        x_offset /= (app->m_swap_chain_extent.width / 2);
        y_offset /= (app->m_swap_chain_extent.height / 2);

        glm::vec3 x_axis = glm::normalize(glm::cross(app->m_view_up, app->m_eye_pos));
        glm::vec3 y_axis = glm::normalize(app->m_view_up);
        glm::mat4 x_translation = glm::translate(glm::mat4(1.F), x_axis * static_cast<float>(-x_offset));
        glm::mat4 y_translation = glm::translate(glm::mat4(1.F), y_axis * static_cast<float>(y_offset));
        glm::mat4 translation = x_translation * y_translation;

        app->m_eye_pos = glm::vec3(translation * glm::vec4(app->m_eye_pos, 1.F));
        app->m_look_at = glm::vec3(translation * glm::vec4(app->m_look_at, 1.F));
      }

      // 更新上一次的鼠标位置
      app->m_mouse_last_x = xpos;
      app->m_mouse_last_y = ypos;
    }
  }

  /// @brief 鼠标滚轮滚动回调
  /// @param window
  /// @param xoffset
  /// @param yoffset 小于0向前滚动，大于0向后滚动
  static void ScrollCallback(GLFWwindow* window, double /*xoffset*/, double yoffset) noexcept {
    if (auto app = reinterpret_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(window))) {
      glm::vec3 direction = glm::normalize(app->m_eye_pos - app->m_look_at);
      float distance = glm::distance(app->m_eye_pos, app->m_look_at);
      glm::vec3 translate = direction * distance * 0.1F;

      if (yoffset < 0.0) {
        glm::mat4 mat = glm::translate(glm::mat4(1.F), translate);
        app->m_eye_pos = glm::vec3(mat * glm::vec4(app->m_eye_pos, 1.F));
      } else if (yoffset > 0.0) {
        glm::mat4 mat = glm::translate(glm::mat4(1.F), translate * -1.F);
        app->m_eye_pos = glm::vec3(mat * glm::vec4(app->m_eye_pos, 1.F));
      }
    }
  }

  /// @brief 鼠标按钮操作回调
  /// @param window
  /// @param button
  /// @param action
  /// @param mods
  static void MouseButtonCallback(GLFWwindow* window, int button, int action, int /*mods*/) noexcept {
    if (auto app = reinterpret_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(window))) {
      if (GLFW_PRESS == action && GLFW_MOUSE_BUTTON_RIGHT == button) {
        app->m_is_rotating = true;
        glfwGetCursorPos(window, &app->m_mouse_last_x, &app->m_mouse_last_y);
      } else if (GLFW_RELEASE == action && GLFW_MOUSE_BUTTON_RIGHT == button) {
        app->m_is_rotating = false;
      }

      if (GLFW_PRESS == action && GLFW_MOUSE_BUTTON_LEFT == button) {
        app->m_is_translating = true;
        glfwGetCursorPos(window, &app->m_mouse_last_x, &app->m_mouse_last_y);
      } else if (GLFW_RELEASE == action && GLFW_MOUSE_BUTTON_LEFT == button) {
        app->m_is_translating = false;
      }
    }
  }

  /// @brief 键盘操作回调
  /// @param window
  /// @param key
  /// @param scancode
  /// @param action
  /// @param mods
  static void KeyCallback(GLFWwindow* window, int key, int /*scancode*/, int action, int /*mods*/) noexcept {
    if (auto app = reinterpret_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(window))) {
      // 按下'r'/'R'恢复视角
      if (GLFW_PRESS == action && GLFW_KEY_R == key) {
        app->m_view_up = glm::vec3{0.F, 1.F, 0.F};
        app->m_eye_pos = glm::vec3{0.F, 0.F, -3.F};
        app->m_look_at = glm::vec3{0.F};
      }
    }
  }

private:
  GLFWwindow* m_window{nullptr};
  VkInstance m_instance{nullptr};
  VkDebugUtilsMessengerEXT m_debug_messenger{nullptr};
  VkPhysicalDevice m_physical_device{nullptr};
  VkDevice m_device{nullptr};
  VkQueue m_graphics_queue{nullptr};  // 图形队列
  VkSurfaceKHR m_surface{nullptr};
  VkQueue m_present_queue{nullptr};  // 呈现队列
  VkSwapchainKHR m_swap_chain{nullptr};
  std::vector<VkImage> m_swap_chain_images{};
  VkFormat m_swap_chain_image_format{};
  VkExtent2D m_swap_chain_extent{};
  std::vector<VkImageView> m_swap_chain_image_views{};
  VkRenderPass m_render_pass{nullptr};
  VkPipelineLayout m_pipeline_layout{nullptr};
  VkPipeline m_graphics_pipeline{nullptr};
  std::vector<VkFramebuffer> m_swap_chain_framebuffers{};
  VkCommandPool m_command_pool{nullptr};
  std::vector<VkCommandBuffer> m_command_buffers{};

  std::vector<VkSemaphore> m_image_available_semaphores{};
  std::vector<VkSemaphore> m_render_finished_semaphores{};
  std::vector<VkFence> m_in_flight_fences{};
  size_t m_current_frame{0};
  bool m_framebuffer_resized{false};

  Drawable m_drawable_cube{};
  Drawable m_drawable_plane{};

  VkDescriptorSetLayout m_descriptor_set_layout{nullptr};
  std::vector<VkBuffer> m_uniform_buffers{};
  std::vector<VkDeviceMemory> m_uniform_buffers_memory{};
  std::vector<void*> m_uniform_buffers_mapped{};
  VkDescriptorPool m_descriptor_pool{nullptr};
  std::vector<VkDescriptorSet> m_descriptor_sets{};

  VkImage m_depth_image{nullptr};
  VkDeviceMemory m_depth_image_memory{nullptr};
  VkImageView m_depth_image_view{nullptr};

  glm::vec3 m_view_up{0.F, 1.F, 0.F};
  glm::vec3 m_eye_pos{0.F, 0.F, -3.F};
  glm::vec3 m_look_at{0.F};
  double m_mouse_last_x{0.0};
  double m_mouse_last_y{0.0};
  bool m_is_rotating{false};
  bool m_is_translating{false};
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

#endif  // TEST3

#ifdef TEST4

#define GLFW_INCLUDE_VULKAN  // 定义这个宏之后 glfw3.h 文件就会包含 Vulkan 的头文件
#include <GLFW/glfw3.h>

// clang-format off
#define GLM_FORCE_RADIANS            // GLM 函数的参数使用弧度
#define GLM_FORCE_DEPTH_ZERO_TO_ONE  // 透视矩阵深度值范围 [-1, 1] => [0, 1]
#define GLM_ENABLE_EXPERIMENTAL      // 允许使用 gtx 目录下的实验性扩展头文件
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
// clang-format on

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include <algorithm>
#include <array>
#include <bit>
#include <fstream>
#include <iostream>
#include <map>
#include <numbers>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
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

// 窗口默认大小
constexpr uint32_t k_width = 800;
constexpr uint32_t k_height = 600;

// 同时并行处理的帧数
constexpr int k_max_frames_in_flight = 2;

// 需要开启的校验层的名称
const std::vector<const char*> k_validation_layers = {"VK_LAYER_KHRONOS_validation"};
// 交换链扩展
const std::vector<const char*> k_device_extensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

// 是否启用校验层
#ifdef NDEBUG
const bool k_enable_validation_layers = false;
#else
const bool k_enable_validation_layers = true;
#endif  // NDEBUG

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

static uint32_t FindMemoryType(VkPhysicalDevice physical_device, uint32_t type_filter,
                               VkMemoryPropertyFlags properties) {
  // 查找物理设备可用的内存类型
  // memoryHeaps 内存来源，比如显存以及显存用尽后的位与主存中的交换空间
  VkPhysicalDeviceMemoryProperties mem_properties{};
  vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_properties);

  for (uint32_t i = 0; i < mem_properties.memoryTypeCount; ++i) {
    if (type_filter & (1 << i) && (mem_properties.memoryTypes[i].propertyFlags & properties) == properties) {
      return i;
    }
  }

  throw std::runtime_error("failed to find suitable memory type");
}

struct Vertex {
  glm::vec3 pos{0.F, 0.F, 0.F};
  glm::vec3 color{0.F, 0.F, 0.F};

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
    attribute_descriptions[0].offset = 0;

    attribute_descriptions[1].binding = 0;
    attribute_descriptions[1].location = 1;
    attribute_descriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attribute_descriptions[1].offset = sizeof(float) * 3;

    return attribute_descriptions;
  }
};

struct UniformBufferObject {
  glm::mat4 model{glm::mat4(1.F)};
  glm::mat4 view{glm::mat4(1.F)};
  glm::mat4 proj{glm::mat4(1.F)};
};

/// @brief 支持图形和呈现的队列族
struct QueueFamilyIndices {
  std::optional<uint32_t> graphicsFamily{};
  std::optional<uint32_t> presentFamily{};

  constexpr bool IsComplete() const noexcept { return graphicsFamily.has_value() && presentFamily.has_value(); }
};

/// @brief 交换链属性信息
struct SwapChainSupportDetails {
  VkSurfaceCapabilitiesKHR capabilities{};
  std::vector<VkSurfaceFormatKHR> foramts;
  std::vector<VkPresentModeKHR> presentModes;
};

const std::vector<Vertex> k_vertices_cube{
  {{-0.5F, 0.5F, -0.5F}, {1.F, 0.F, 0.F}}, {{0.5F, 0.5F, -0.5F}, {1.F, 1.F, 0.F}},
  {{0.5F, -0.5F, -0.5F}, {0.F, 1.F, 0.F}}, {{-0.5F, -0.5F, -0.5F}, {0.F, 1.F, 1.F}},

  {{-0.5F, 0.5F, 0.5F}, {0.F, 0.F, 1.F}},  {{0.5F, 0.5F, 0.5F}, {1.F, 0.F, 1.F}},
  {{0.5F, -0.5F, 0.5F}, {0.F, 0.F, 0.F}},  {{-0.5F, -0.5F, 0.5F}, {1.F, 1.F, 1.F}},
};

const std::vector<uint16_t> k_indices_cube{
  0, 1, 2, 0, 2, 3, 1, 5, 6, 1, 6, 2, 5, 4, 7, 5, 7, 6, 4, 0, 3, 4, 3, 7, 3, 2, 6, 3, 6, 7, 4, 5, 1, 4, 1, 0,
};

const std::vector<Vertex> k_vertices_plane{
  {{-1.F, -1.F, 1.F}, {1.F, 1.F, 1.F}},
  {{-1.F, -1.F, -1.F}, {1.F, 1.F, 1.F}},
  {{1.F, -1.F, -1.F}, {1.F, 1.F, 1.F}},
  {{1.F, -1.F, 1.F}, {1.F, 1.F, 1.F}},
};

const std::vector<uint16_t> k_indices_plane{0, 1, 2, 0, 2, 3};

struct Uniform {
  std::vector<VkBuffer> buffers{};
  std::vector<VkDeviceMemory> deviceMemorys{};
  std::vector<void*> mapped{};
};

struct Drawable {
  VkBuffer vertexBuffer{nullptr};
  VkDeviceMemory vertexBufferMemory{nullptr};
  VkBuffer indexBuffer{nullptr};
  VkDeviceMemory indexBufferMemory{nullptr};
  uint32_t indexCount{0};

  void Draw(VkCommandBuffer command_buffer) const noexcept {
    const std::array<VkBuffer, 1> vertex_buffers{vertexBuffer};
    const std::array<VkDeviceSize, 1> offsets{0};

    vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers.data(), offsets.data());
    vkCmdBindIndexBuffer(command_buffer, indexBuffer, 0, VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed(command_buffer, indexCount, 1, 0, 0, 0);
  }
};

struct ImageData {
  VkImage image{};
  VkImageView imageView{};
  VkDeviceMemory deviceMemory{};
};

class RenderPass {
public:
  void Cleanup() {
    vkDestroyPipeline(m_device, m_pipeline, nullptr);
    vkDestroyPipelineLayout(m_device, m_pipeline_layout, nullptr);
    vkDestroyRenderPass(m_device, m_render_pass, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_descriptor_set_layout, nullptr);

    for (auto i = 0; i < k_max_frames_in_flight; ++i) {
      vkDestroyBuffer(m_device, m_mvp.buffers[i], nullptr);
      vkFreeMemory(m_device, m_mvp.deviceMemorys[i], nullptr);
    }

    for (auto framebuffer : m_framebuffers) {
      vkDestroyFramebuffer(m_device, framebuffer, nullptr);
    }
  }

  void SetColorOutput(const std::vector<ImageData>& colors) { m_output_colors = colors; }

  void SetDepthOutput(const ImageData& depth) { m_output_depth = depth; }

  void AddDrawable(const Drawable& drawable) { m_drawables.emplace_back(drawable); }

  void SetDevice(VkPhysicalDevice physical_device, VkDevice device, VkDescriptorPool descriptor_pool) {
    m_descriptor_pool = descriptor_pool;
    m_physical_device = physical_device;
    m_device = device;
  }

  void SetExtent(const VkExtent2D extent) { m_extent = extent; }

  void SetColorFormat(const VkFormat format) { m_color_format = format; }

  void SetDepthFormat(const VkFormat format) { m_depth_format = format; }

  void Compile() {
    CreateRenderPass();
    CreateFramebuffers();

    CreateDescriptorSetLayout();
    CreateGraphicsPipeline();

    CreateUniformBuffers();
    CreateDescriptorSets();
  }

  void Execute(VkCommandBuffer command_buffer, const UniformBufferObject& ubo, const size_t frame_index,
               const uint32_t image_index) const {
    UpdateUniformBuffer(frame_index, ubo);
    RecordCommandBuffer(command_buffer, frame_index, image_index);
  }

  VkRenderPass GetRenderPass() const noexcept { return m_render_pass; }

  void CreateRenderPass() {
    VkAttachmentDescription color_attachment = {};
    color_attachment.format = m_color_format;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depth_attachment = {};
    depth_attachment.format = m_depth_format;
    depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;  // 绘制结束后不需要从深度缓冲复制深度数据
    depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;  // 不需要读取之前深度图像数据
    depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // 子流程引用的附着
    VkAttachmentReference color_attachment_ref = {};
    color_attachment_ref.attachment = 0;  // 索引，对应于片段着色器中的 layout(location = 0) out
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depth_attachment_ref = {};
    depth_attachment_ref.attachment = 1;
    depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // 子流程
    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;  // 图形渲染子流程
    subpass.colorAttachmentCount = 1;                             // 颜色附着个数
    subpass.pColorAttachments = &color_attachment_ref;            // 指定颜色附着
    subpass.pDepthStencilAttachment =
      &depth_attachment_ref;  // 指定深度模板附着，深度模板附着只能是一个，所以不用设置数量

    // 渲染流程使用的依赖信息
    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 2> attachments{color_attachment, depth_attachment};

    VkRenderPassCreateInfo render_pass_info = {};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = static_cast<uint32_t>(attachments.size());
    render_pass_info.pAttachments = attachments.data();
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;
    render_pass_info.dependencyCount = 1;
    render_pass_info.pDependencies = &dependency;

    if (VK_SUCCESS != vkCreateRenderPass(m_device, &render_pass_info, nullptr, &m_render_pass)) {
      throw std::runtime_error("failed to create render pass");
    }
  }

  void CreateFramebuffers() {
    m_framebuffers.resize(m_output_colors.size());

    for (size_t i = 0; i < m_output_colors.size(); ++i) {
      std::array<VkImageView, 2> attachments{m_output_colors[i].imageView, m_output_depth.imageView};

      VkFramebufferCreateInfo framebuffer_info = {};
      framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
      framebuffer_info.renderPass = m_render_pass;
      framebuffer_info.attachmentCount = static_cast<uint32_t>(attachments.size());
      framebuffer_info.pAttachments = attachments.data();
      framebuffer_info.width = m_extent.width;
      framebuffer_info.height = m_extent.height;
      framebuffer_info.layers = 1;

      if (VK_SUCCESS != vkCreateFramebuffer(m_device, &framebuffer_info, nullptr, &m_framebuffers[i])) {
        throw std::runtime_error("failed to create framebuffer");
      }
    }
  }

  void CreateDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding ubo_layout_binding = {};
    ubo_layout_binding.binding = 0;
    ubo_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ubo_layout_binding.descriptorCount = 1;
    ubo_layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    ubo_layout_binding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layout_info = {};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = 1;
    layout_info.pBindings = &ubo_layout_binding;

    if (VK_SUCCESS != vkCreateDescriptorSetLayout(m_device, &layout_info, nullptr, &m_descriptor_set_layout)) {
      throw std::runtime_error("failed to create descriptor set layout");
    }
  }

  void CreateGraphicsPipeline() {
    auto vert_shader_code = ReadFile(PROJECT_SHADER_DIR "02_21_base_vert.spv");
    auto frag_shader_code = ReadFile(PROJECT_SHADER_DIR "02_21_base_frag.spv");

    VkShaderModule vert_shader_module = CreateShaderModule(vert_shader_code);
    VkShaderModule frag_shader_module = CreateShaderModule(frag_shader_code);

    VkPipelineShaderStageCreateInfo vert_shader_stage_info = {};
    vert_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vert_shader_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vert_shader_stage_info.module = vert_shader_module;
    vert_shader_stage_info.pName = "main";                 // 指定调用的着色器函数，同一份代码可以实现多个着色器
    vert_shader_stage_info.pSpecializationInfo = nullptr;  // 设置着色器常量

    VkPipelineShaderStageCreateInfo frag_shader_stage_info = {};
    frag_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    frag_shader_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    frag_shader_stage_info.module = frag_shader_module;
    frag_shader_stage_info.pName = "main";
    frag_shader_stage_info.pSpecializationInfo = nullptr;

    auto binding_description = Vertex::GetBindingDescription();
    auto attribute_descriptions = Vertex::GetAttributeDescriptions();

    // 顶点信息
    VkPipelineVertexInputStateCreateInfo vertex_input_info = {};
    vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_info.vertexBindingDescriptionCount = 1;
    vertex_input_info.pVertexBindingDescriptions = &binding_description;
    vertex_input_info.vertexAttributeDescriptionCount = static_cast<uint32_t>(attribute_descriptions.size());
    vertex_input_info.pVertexAttributeDescriptions = attribute_descriptions.data();

    // 拓扑信息
    VkPipelineInputAssemblyStateCreateInfo input_assembly = {};
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;  // 指定绘制的图元类型：点、线、三角形
    input_assembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewport_state = {};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount = 1;

    // 光栅化
    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;  // 设置为true会禁止一切片段输出到帧缓冲
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.F;
    rasterizer.cullMode = VK_CULL_MODE_NONE;                 // 表面剔除类型，正面、背面、双面剔除
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;  // 指定顺时针的顶点序是正面还是反面
    rasterizer.depthBiasEnable = VK_FALSE;
    rasterizer.depthBiasConstantFactor = 1.F;
    rasterizer.depthBiasClamp = 0.F;
    rasterizer.depthBiasSlopeFactor = 0.F;

    // 多重采样
    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.F;
    multisampling.pSampleMask = nullptr;
    multisampling.alphaToCoverageEnable = VK_FALSE;
    multisampling.alphaToOneEnable = VK_FALSE;

    // 深度和模板测试
    VkPipelineDepthStencilStateCreateInfo depth_stencil = {};
    depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil.depthTestEnable = VK_TRUE;            // 是否启用深度测试
    depth_stencil.depthWriteEnable = VK_TRUE;           // 深度测试通过后是否写入深度缓冲
    depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;  // 深度值比较方式
    depth_stencil.depthBoundsTestEnable = VK_FALSE;     // 指定可选的深度范围测试
    depth_stencil.minDepthBounds = 0.F;
    depth_stencil.maxDepthBounds = 1.F;
    depth_stencil.stencilTestEnable = VK_FALSE;  // 模板测试
    depth_stencil.front = {};
    depth_stencil.back = {};

    // 颜色混合，可以对指定帧缓冲单独设置，也可以设置全局颜色混合方式
    VkPipelineColorBlendAttachmentState color_blend_attachment{};
    color_blend_attachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo color_blending{};
    color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blending.logicOpEnable = VK_FALSE;
    color_blending.logicOp = VK_LOGIC_OP_COPY;
    color_blending.attachmentCount = 1;
    color_blending.pAttachments = &color_blend_attachment;
    color_blending.blendConstants[0] = 0.0F;
    color_blending.blendConstants[1] = 0.0F;
    color_blending.blendConstants[2] = 0.0F;
    color_blending.blendConstants[3] = 0.0F;

    // 动态状态，视口大小、线宽、混合常量等
    std::vector<VkDynamicState> dynamic_states = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic_state{};
    dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.dynamicStateCount = static_cast<uint32_t>(dynamic_states.size());
    dynamic_state.pDynamicStates = dynamic_states.data();

    // 管线布局，在着色器中使用 uniform 变量
    VkPipelineLayoutCreateInfo pipeline_layout_info{};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = 1;
    pipeline_layout_info.pSetLayouts = &m_descriptor_set_layout;
    pipeline_layout_info.pushConstantRangeCount = 0;

    if (VK_SUCCESS != vkCreatePipelineLayout(m_device, &pipeline_layout_info, nullptr, &m_pipeline_layout)) {
      throw std::runtime_error("failed to create pipeline layout");
    }

    const std::array<VkPipelineShaderStageCreateInfo, 2> shader_stages{vert_shader_stage_info, frag_shader_stage_info};

    // 完整的图形管线包括：着色器阶段、固定功能状态、管线布局、渲染流程
    VkGraphicsPipelineCreateInfo pipeline_info = {};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = 2;
    pipeline_info.pStages = shader_stages.data();
    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pDepthStencilState = nullptr;
    pipeline_info.pColorBlendState = &color_blending;
    pipeline_info.pDynamicState = &dynamic_state;
    pipeline_info.layout = m_pipeline_layout;
    pipeline_info.renderPass = m_render_pass;
    pipeline_info.subpass = 0;
    pipeline_info.basePipelineHandle = nullptr;
    pipeline_info.basePipelineIndex = -1;
    pipeline_info.pDepthStencilState = &depth_stencil;

    if (VK_SUCCESS != vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &m_pipeline)) {
      throw std::runtime_error("failed to create graphics pipeline");
    }

    vkDestroyShaderModule(m_device, frag_shader_module, nullptr);
    vkDestroyShaderModule(m_device, vert_shader_module, nullptr);
  }

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

  void CreateUniformBuffers() {
    VkDeviceSize buffer_size = sizeof(UniformBufferObject);

    m_mvp.buffers.resize(k_max_frames_in_flight);
    m_mvp.deviceMemorys.resize(k_max_frames_in_flight);
    m_mvp.mapped.resize(k_max_frames_in_flight);

    for (size_t i = 0; i < k_max_frames_in_flight; ++i) {
      CreateBuffer(buffer_size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m_mvp.buffers[i],
                   m_mvp.deviceMemorys[i]);

      vkMapMemory(m_device, m_mvp.deviceMemorys[i], 0, buffer_size, 0, &m_mvp.mapped[i]);
    }
  }

  void CreateDescriptorSets() {
    std::vector<VkDescriptorSetLayout> layouts(k_max_frames_in_flight, m_descriptor_set_layout);

    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = m_descriptor_pool;
    alloc_info.descriptorSetCount = static_cast<uint32_t>(k_max_frames_in_flight);
    alloc_info.pSetLayouts = layouts.data();

    m_descriptor_sets.resize(k_max_frames_in_flight);
    if (VK_SUCCESS != vkAllocateDescriptorSets(m_device, &alloc_info, m_descriptor_sets.data())) {
      throw std::runtime_error("failed to allocate descriptor sets");
    }

    for (size_t i = 0; i < k_max_frames_in_flight; ++i) {
      VkDescriptorBufferInfo buffer_info{};
      buffer_info.buffer = m_mvp.buffers[i];
      buffer_info.offset = 0;
      buffer_info.range = sizeof(UniformBufferObject);

      VkWriteDescriptorSet descriptor_write{};
      descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      descriptor_write.dstSet = m_descriptor_sets.at(i);
      descriptor_write.dstBinding = 0;
      descriptor_write.dstArrayElement = 0;
      descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
      descriptor_write.descriptorCount = 1;
      descriptor_write.pBufferInfo = &buffer_info;
      descriptor_write.pImageInfo = nullptr;
      descriptor_write.pTexelBufferView = nullptr;

      vkUpdateDescriptorSets(m_device, 1, &descriptor_write, 0, nullptr);
    }
  }

  void RecordCommandBuffer(VkCommandBuffer command_buffer, const size_t frame_index, const uint32_t image_index) const {
    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
    begin_info.pInheritanceInfo = nullptr;

    if (VK_SUCCESS != vkBeginCommandBuffer(command_buffer, &begin_info)) {
      throw std::runtime_error("failed to begin recording command buffer");
    }

    // 清除色，相当于背景色
    std::array<VkClearValue, 2> clear_values{};
    clear_values.at(0).color = {{.1f, .2f, .3f, 1.F}};
    clear_values.at(1).depthStencil = {1.F, 0};

    VkRenderPassBeginInfo render_pass_info = {};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_info.renderPass = m_render_pass;
    render_pass_info.framebuffer = m_framebuffers[image_index];
    render_pass_info.renderArea.offset = {0, 0};
    render_pass_info.renderArea.extent = m_extent;
    render_pass_info.clearValueCount = static_cast<uint32_t>(clear_values.size());
    render_pass_info.pClearValues = clear_values.data();

    vkCmdBeginRenderPass(command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

    VkViewport viewport = {};
    viewport.x = 0.F;
    viewport.y = 0.F;
    viewport.width = static_cast<float>(m_extent.width);
    viewport.height = static_cast<float>(m_extent.height);
    viewport.minDepth = 0.F;
    viewport.maxDepth = 1.F;

    vkCmdSetViewport(command_buffer, 0, 1, &viewport);

    VkRect2D scissor = {};
    scissor.offset = {0, 0};
    scissor.extent = m_extent;

    vkCmdSetScissor(command_buffer, 0, 1, &scissor);

    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline_layout, 0, 1,
                            &m_descriptor_sets[frame_index], 0, nullptr);

    for (const auto& drawable : m_drawables) {
      drawable.Draw(command_buffer);
    }

    // 绘制ImGui
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), command_buffer);

    // 结束渲染流程
    vkCmdEndRenderPass(command_buffer);

    // 结束记录指令到指令缓冲
    if (VK_SUCCESS != vkEndCommandBuffer(command_buffer)) {
      throw std::runtime_error("failed to record command buffer");
    }
  }

  void UpdateUniformBuffer(const size_t frame_index, const UniformBufferObject& ubo) const noexcept {
    std::memcpy(m_mvp.mapped[frame_index], &ubo, sizeof(ubo));
  }

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

    VkMemoryRequirements mem_requirements{};
    vkGetBufferMemoryRequirements(m_device, buffer, &mem_requirements);

    VkMemoryAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_requirements.size;
    alloc_info.memoryTypeIndex = FindMemoryType(m_physical_device, mem_requirements.memoryTypeBits, properties);

    if (VK_SUCCESS != vkAllocateMemory(m_device, &alloc_info, nullptr, &buffer_memory)) {
      throw std::runtime_error("failed to allocate buffer memory");
    }

    vkBindBufferMemory(m_device, buffer, buffer_memory, 0);
  }

private:
  VkDescriptorPool m_descriptor_pool{};
  VkPhysicalDevice m_physical_device{};
  VkDevice m_device{};
  VkExtent2D m_extent{};

  VkPipeline m_pipeline{};
  VkPipelineLayout m_pipeline_layout{};
  VkDescriptorSetLayout m_descriptor_set_layout{};

  Uniform m_mvp{};
  std::vector<VkDescriptorSet> m_descriptor_sets{};

  VkRenderPass m_render_pass{};
  std::vector<VkFramebuffer> m_framebuffers{};

  std::vector<ImageData> m_output_colors{};
  ImageData m_output_depth{};
  VkFormat m_color_format{};
  VkFormat m_depth_format{};

  std::vector<Drawable> m_drawables{};
};

class RenderGraph {
public:
  RenderPass& AddPass(const std::string& name) {
    m_render_passes.try_emplace(name, RenderPass{});
    return m_render_passes.at(name);
  }

  void Compile() {
    for (auto& [_, renderPass] : m_render_passes) {
      renderPass.Compile();
    }
  }

  void Execute(VkCommandBuffer cmd, const UniformBufferObject& ubo, const size_t frame_index,
               const uint32_t image_index) {
    for (const auto& [_, renderPass] : m_render_passes) {
      renderPass.Execute(cmd, ubo, frame_index, image_index);
    }
  }

  void CleanUp() {
    for (auto& [_, renderPass] : m_render_passes) {
      renderPass.Cleanup();
    }
  }

  const RenderPass& GetRenderPass(const std::string& name) const noexcept { return m_render_passes.at(name); }

private:
  std::map<std::string, RenderPass> m_render_passes{};
};

class HelloTriangleApplication {
public:
  void Run() {
    InitWindow();
    InitVulkan();
    MainLoop();
    Cleanup();
  }

private:
  void InitWindow() {
    if (GLFW_FALSE == glfwInit()) {
      throw std::runtime_error("failed to init GLFW");
    }

    // 禁止 glfw 创建 OpenGL 上下文
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);

    m_window = glfwCreateWindow(k_width, k_height, "Vulkan", nullptr, nullptr);

    glfwSetWindowUserPointer(m_window, this);
    glfwSetFramebufferSizeCallback(m_window, FramebufferResizeCallback);
    glfwSetKeyCallback(m_window, KeyCallback);
    glfwSetCursorPosCallback(m_window, CursorPosCallback);
    glfwSetScrollCallback(m_window, ScrollCallback);
    glfwSetMouseButtonCallback(m_window, MouseButtonCallback);
  }

  void InitVulkan() {
    CreateInstance();
    SetupDebugCallback();
    CreateSurface();
    PickPhysicalDevice();
    CreateLogicalDevice();

    CreateCommandPool();
    CreateSwapChain();
    CreateImageViews();
    CreateDepthResources();
    CreateDescriptorPool();
    CreateImGuiDescriptorPool();
    CreateCommandBuffers();
    CreateSyncObjects();
    CreateDrawables();

    auto& present_pass = m_render_graph.AddPass("present");

    present_pass.AddDrawable(m_drawable_cube);
    present_pass.AddDrawable(m_drawable_plane);
    present_pass.SetDevice(m_physical_device, m_device, m_descriptor_pool);
    present_pass.SetExtent(m_swap_chain_extent);
    present_pass.SetColorFormat(m_swap_chain_image_format);
    present_pass.SetDepthFormat(m_depth_format);
    present_pass.SetColorOutput(m_colors);
    present_pass.SetDepthOutput(m_depth);

    m_render_graph.Compile();

    InitImGui();
  }

  void MainLoop() {
    while (!glfwWindowShouldClose(m_window)) {
      glfwPollEvents();
      PrepareImGui();
      DrawFrame();
    }

    // 等待逻辑设备的操作结束执行
    // DrawFrame 函数中的操作是异步执行的，关闭窗口跳出while循环时，绘制操作和呈现操作可能仍在执行，不能进行清除操作
    vkDeviceWaitIdle(m_device);
  }

  void Cleanup() noexcept {
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    CleanupSwapChain();
    m_render_graph.CleanUp();

    auto destroy_drawable = [this](Drawable& drawable) {
      vkDestroyBuffer(m_device, drawable.vertexBuffer, nullptr);
      vkFreeMemory(m_device, drawable.vertexBufferMemory, nullptr);
      vkDestroyBuffer(m_device, drawable.indexBuffer, nullptr);
      vkFreeMemory(m_device, drawable.indexBufferMemory, nullptr);
    };

    destroy_drawable(m_drawable_cube);
    destroy_drawable(m_drawable_plane);

    vkDestroyDescriptorPool(m_device, m_descriptor_pool, nullptr);
    vkDestroyDescriptorPool(m_device, m_imgui_descriptor_pool, nullptr);

    for (size_t i = 0; i < k_max_frames_in_flight; ++i) {
      vkDestroySemaphore(m_device, m_image_available_semaphores.at(i), nullptr);
      vkDestroyFence(m_device, m_in_flight_fences.at(i), nullptr);
    }
    for (auto semaphore : m_render_finished_semaphores) {
      vkDestroySemaphore(m_device, semaphore, nullptr);
    }

    vkDestroyCommandPool(m_device, m_command_pool, nullptr);
    vkDestroyDevice(m_device, nullptr);

    if (k_enable_validation_layers) {
      DestroyDebugUtilsMessengerEXT(m_instance, m_debug_messenger, nullptr);
    }

    vkDestroySurfaceKHR(m_instance, m_surface, nullptr);  // 清理表面对象，必须在 Vulkan 实例清理之前
    vkDestroyInstance(m_instance, nullptr);

    glfwDestroyWindow(m_window);
    glfwTerminate();
  }

private:
  void CreateDrawables() {
    CreateVertexBuffer(k_vertices_cube, m_drawable_cube);
    CreateIndexBuffer(k_indices_cube, m_drawable_cube);

    CreateVertexBuffer(k_vertices_plane, m_drawable_plane);
    CreateIndexBuffer(k_indices_plane, m_drawable_plane);
  }

  /// @brief 添加需要渲染的GUI控件
  void PrepareImGui() {
    ImGui::NewFrame();

    ImGui::SetNextWindowPos(ImVec2(10, 10));
    ImGui::Begin("Display Infomation", nullptr,
                 ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
    ImGui::TextUnformatted("test text");
    ImGui::End();

    ImGui::Render();
  }

  /// @brief 初始化ImGui
  void InitImGui() {
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.DisplaySize = ImVec2((float)(k_width), (float)(k_height));
    io.Fonts->Build();

    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForVulkan(m_window, true);

    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = m_instance;
    init_info.PhysicalDevice = m_physical_device;
    init_info.Device = m_device;
    init_info.Queue = m_present_queue;
    init_info.DescriptorPool = m_imgui_descriptor_pool;
    init_info.Subpass = 0;
    init_info.MinImageCount = m_min_image_count;
    init_info.ImageCount = m_min_image_count;
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&init_info, m_render_graph.GetRenderPass("present").GetRenderPass());

    vkResetCommandPool(m_device, m_command_pool, 0);

    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(m_command_buffers[m_current_frame], &begin_info);

    ImGui_ImplVulkan_CreateFontsTexture(m_command_buffers[m_current_frame]);

    VkSubmitInfo end_info = {};
    end_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    end_info.commandBufferCount = 1;
    end_info.pCommandBuffers = &m_command_buffers[m_current_frame];
    vkEndCommandBuffer(m_command_buffers[m_current_frame]);

    vkQueueSubmit(m_graphics_queue, 1, &end_info, VK_NULL_HANDLE);
    vkDeviceWaitIdle(m_device);

    ImGui_ImplVulkan_DestroyFontUploadObjects();
  }

  /// @brief 创建ImGui的描述符池
  void CreateImGuiDescriptorPool() {
    // clang-format off
        std::vector<VkDescriptorPoolSize> pool_sizes {
            { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
        };
    // clang-format on

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000 * static_cast<uint32_t>(pool_sizes.size());
    pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
    pool_info.pPoolSizes = pool_sizes.data();
    if (VK_SUCCESS != vkCreateDescriptorPool(m_device, &pool_info, nullptr, &m_imgui_descriptor_pool)) {
      throw std::runtime_error("failed to create descriptor pool");
    }
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

    // Vulkan 是一个与平台无关的 API ，所以需要一个和窗口系统交互的扩展
    // 使用 glfw 获取这个扩展
    uint32_t glfw_extension_count = 0;
    const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);
    std::cout << "-------------------------------------------\n"
              << "GLFW extensions:\n";
    for (size_t i = 0; i < glfw_extension_count; ++i) {
      std::cout << glfw_extensions[i] << '\n';
    }

    // 将需要开启的所有扩展添加到列表并返回
    std::vector<const char*> required_extensions(glfw_extensions, glfw_extensions + glfw_extension_count);
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
  bool IsDeviceSuitable(VkPhysicalDevice device) const noexcept {
    // 获取基本的设置属性，name、type以及Vulkan版本等等
    // VkPhysicalDeviceProperties deviceProperties;
    // vkGetPhysicalDeviceProperties(device, &deviceProperties);

    // 获取对纹理的压缩、64位浮点数和多视图渲染等可选功能的支持
    // VkPhysicalDeviceFeatures deviceFeatures;
    // vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

    auto indices = FindQueueFamilies(device);
    auto extensions_supported = CheckDeviceExtensionSupported(device);

    bool swap_chain_adequate{false};
    if (extensions_supported) {
      auto swap_chain_support = QuerySwapChainSupport(device);
      swap_chain_adequate = !swap_chain_support.foramts.empty() && !swap_chain_support.presentModes.empty();
    }

    return indices.IsComplete() && extensions_supported && swap_chain_adequate;
  }

  /// @brief 查找满足需求的队列族
  /// @details 不同的队列族支持不同的类型的指令，例如计算、内存传输、绘图等指令
  /// @param device
  /// @return
  QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device) const noexcept {
    QueueFamilyIndices indices;

    // 获取物理设备支持的队列族列表
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);
    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());

    for (uint32_t i = 0; i < queue_families.size(); ++i) {
      // 图形队列族
      if (queue_families.at(i).queueFlags & VK_QUEUE_GRAPHICS_BIT) {
        indices.graphicsFamily = i;
      }

      // 呈现队列族
      VkBool32 present_support = false;
      vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &present_support);

      if (present_support) {
        indices.presentFamily = i;
      }

      if (indices.IsComplete()) {
        break;
      }
    }

    return indices;
  }

  /// @brief 创建逻辑设备作为和物理设备交互的接口
  void CreateLogicalDevice() {
    auto indices = FindQueueFamilies(m_physical_device);

    std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
    std::set<uint32_t> unique_queue_families{lvk_tidy::GetRequiredValue(indices.graphicsFamily),
                                             lvk_tidy::GetRequiredValue(indices.presentFamily)};

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
    create_info.enabledExtensionCount = static_cast<uint32_t>(k_device_extensions.size());
    create_info.ppEnabledExtensionNames = k_device_extensions.data();

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

    // 获取指定队列族的队列句柄，设备队列在逻辑设备被销毁时隐式清理
    // 此处的队列簇可能相同，也就是以相同的参数调用了两次这个函数
    // 1.逻辑设备对象
    // 2.队列族索引
    // 3.队列索引，因为只创建了一个队列，所以此处使用索引0
    // 4.用来存储返回的队列句柄的内存地址
    vkGetDeviceQueue(m_device, lvk_tidy::GetRequiredValue(indices.graphicsFamily), 0, &m_graphics_queue);
    vkGetDeviceQueue(m_device, lvk_tidy::GetRequiredValue(indices.presentFamily), 0, &m_present_queue);
  }

  /// @brief 创建表面，需要在程序退出前清理
  /// @details 不同平台创建表面的方式不一样，这里使用 GLFW 统一创建
  void CreateSurface() {
    // Vulkan 不能直接与窗口系统进行交互（是一个与平台特性无关的API集合），surface是 Vulkan 与窗体系统的连接桥梁
    // 需要在instance创建之后立即创建窗体surface，因为它会影响物理设备的选择
    // 窗体surface本身对于 Vulkan 也是非强制的，不需要同 OpenGL 一样必须要创建窗体surface
    if (VK_SUCCESS != glfwCreateWindowSurface(m_instance, m_window, nullptr, &m_surface)) {
      throw std::runtime_error("failed to create window surface");
    }
  }

  /// @brief 检测所需的扩展是否支持
  /// @param device
  /// @return
  bool CheckDeviceExtensionSupported(VkPhysicalDevice device) const noexcept {
    uint32_t extension_count{0};
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr);
    std::vector<VkExtensionProperties> available_extensions(extension_count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, available_extensions.data());

    std::set<std::string> required_extensions(std::begin(k_device_extensions), std::end(k_device_extensions));

    for (const auto& extension : available_extensions) {
      required_extensions.erase(static_cast<const char*>(extension.extensionName));
    }

    // 如果为空，则支持
    return required_extensions.empty();
  }

  /// @brief 查询交换链支持的细节信息
  /// @param device
  /// @return
  SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device) const noexcept {
    SwapChainSupportDetails details;

    // 查询基础表面特性
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_surface, &details.capabilities);

    // 查询表面支持的格式，确保集合对于所有有效的格式可扩充
    uint32_t format_count{0};
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &format_count, nullptr);
    if (0 != format_count) {
      details.foramts.resize(format_count);
      vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &format_count, details.foramts.data());
    }

    // 查询表面支持的呈现模式
    uint32_t present_mode_count{0};
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &present_mode_count, nullptr);
    if (0 != present_mode_count) {
      details.presentModes.resize(present_mode_count);
      vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &present_mode_count, details.presentModes.data());
    }

    return details;
  }

  /// @brief 选择合适的表面格式
  /// @param available_formats
  /// @return
  VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& available_formats) const noexcept {
    // VkSurfaceFormatKHR 结构都包含一个 format 和一个 colorSpace 成员
    // format 成员变量指定色彩通道和类型
    // colorSpace 成员描述 SRGB 颜色空间是否通过 VK_COLOR_SPACE_SRGB_NONLINEAR_KHR 标志支持
    // 在较早版本的规范中，这个标志名为 VK_COLORSPACE_SRGB_NONLINEAR_KHR

    // 表面没有自己的首选格式，直接返回指定的格式
    if (1 == available_formats.size() && available_formats.front().format == VK_FORMAT_UNDEFINED) {
      return {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    }

    // 遍历格式列表，如果想要的格式存在则直接返回
    for (const auto& available_format : available_formats) {
      if (VK_FORMAT_B8G8R8A8_UNORM == available_format.format &&
          VK_COLOR_SPACE_SRGB_NONLINEAR_KHR == available_format.colorSpace) {
        return available_format;
      }
    }

    // 如果以上两种方式都失效，通过“优良”进行打分排序，大多数情况下会选择第一个格式作为理想的选择
    return available_formats.front();
  }

  /// @brief 查找最佳的可用呈现模式，模式是非常重要的，因为它代表了在屏幕呈现图像的条件
  /// @param available_present_modes
  /// @return
  VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& available_present_modes) const noexcept {
    // 以下4种模式可以使用
    // 1.VK_PRESENT_MODE_IMMEDIATE_KHR: 应用程序提交的图像被立即传输到屏幕呈现，这种模式可能会造成撕裂效果。
    // 2.VK_PRESENT_MODE_FIFO_KHR:
    // 交换链被看作一个队列，当显示内容需要刷新的时候，显示设备从队列的前面获取图像，并且程序将渲染完成的图像插入队列的后面。
    // 如果队列是满的程序会等待。这种规模与视频游戏的垂直同步很类似。显示设备的刷新时刻被成为“垂直中断”。
    // 3.VK_PRESENT_MODE_FIFO_RELAXED_KHR:
    // 该模式与上一个模式略有不同的地方为，如果应用程序存在延迟，即接受最后一个垂直同步信号时队列空了，
    // 将不会等待下一个垂直同步信号，而是将图像直接传送。这样做可能导致可见的撕裂效果。
    // 4.VK_PRESENT_MODE_MAILBOX_KHR:
    // 这是第二种模式的变种。当交换链队列满的时候，选择新的替换旧的图像，从而替代阻塞应用程序的情形。
    // 这种模式通常用来实现三重缓冲区，与标准的垂直同步双缓冲相比，它可以有效避免延迟带来的撕裂效果。

    VkPresentModeKHR best_mode = VK_PRESENT_MODE_FIFO_KHR;

    for (const auto& available_present_mode : available_present_modes) {
      if (VK_PRESENT_MODE_MAILBOX_KHR == available_present_mode) {
        return available_present_mode;
      }
      if (VK_PRESENT_MODE_IMMEDIATE_KHR == available_present_mode) {
        best_mode = available_present_mode;
      }
    }

    return best_mode;
  }

  /// @brief 设置交换范围，交换范围是交换链中图像的分辨率
  /// @param capabilities
  /// @return
  VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) const noexcept {
    // 如果是最大值则表示允许我们自己选择对于窗口最合适的交换范围
    if (std::numeric_limits<uint32_t>::max() != capabilities.currentExtent.width) {
      return capabilities.currentExtent;
    }

    int width{0};

    int height{0};
    glfwGetFramebufferSize(m_window, &width, &height);

    // 使用GLFW窗口的大小来设置交换链中图像的分辨率
    VkExtent2D actual_extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};

    actual_extent.width =
      std::clamp(actual_extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    actual_extent.height =
      std::clamp(actual_extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

    return actual_extent;
  }

  /// @brief 创建交换链
  void CreateSwapChain() {
    SwapChainSupportDetails swap_chain_support = QuerySwapChainSupport(m_physical_device);
    VkSurfaceFormatKHR surface_format = ChooseSwapSurfaceFormat(swap_chain_support.foramts);
    VkPresentModeKHR present_mode = ChooseSwapPresentMode(swap_chain_support.presentModes);
    VkExtent2D extent = ChooseSwapExtent(swap_chain_support.capabilities);

    // 交换链中的图像数量，可以理解为队列的长度，指定运行时图像的最小数量
    // maxImageCount数值为0代表除了内存之外没有限制
    m_min_image_count = swap_chain_support.capabilities.minImageCount + 1;
    if (swap_chain_support.capabilities.maxImageCount > 0 &&
        m_min_image_count > swap_chain_support.capabilities.maxImageCount) {
      m_min_image_count = swap_chain_support.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = m_surface;
    create_info.minImageCount = m_min_image_count;
    create_info.imageFormat = surface_format.format;
    create_info.imageColorSpace = surface_format.colorSpace;
    create_info.imageExtent = extent;
    create_info.imageArrayLayers = 1;  // 图像包含的层次，通常为1，3d图像大于1
    create_info.imageUsage =
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;  // 指定在图像上进行怎样的操作，比如显示（颜色附件）、后处理等

    auto indices = FindQueueFamilies(m_physical_device);
    const std::array<uint32_t, 2> queue_family_indices{
      static_cast<uint32_t>(lvk_tidy::GetRequiredValue(indices.graphicsFamily)),
      static_cast<uint32_t>(lvk_tidy::GetRequiredValue(indices.presentFamily))};

    // 在多个队列族使用交换链图像的方式
    if (indices.graphicsFamily != indices.presentFamily) {
      // 图形队列和呈现队列不是同一个队列
      // VK_SHARING_MODE_CONCURRENT 表示图像可以在多个队列族间使用，不需要显式地改变图像所有权
      create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
      create_info.queueFamilyIndexCount = 2;
      create_info.pQueueFamilyIndices = queue_family_indices.data();
    } else {
      // VK_SHARING_MODE_EXCLUSIVE 表示一张图像同一时间只能被一个队列族所拥有，这种模式性能最佳
      create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
      create_info.queueFamilyIndexCount = 0;
      create_info.pQueueFamilyIndices = nullptr;
    }

    // 指定一个固定的变换操作（需要交换链具有supportedTransforms特性），此处不进行任何变换
    create_info.preTransform = swap_chain_support.capabilities.currentTransform;
    // 指定 alpha 通道是否被用来和窗口系统中的其他窗口进行混合操作
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    // 设置呈现模式
    create_info.presentMode = present_mode;
    // 设置为true，不关心被遮蔽的像素数据，比如由于其他的窗体置于前方时或者渲染的部分内容存在于可视区域之外，
    // 除非真的需要读取这些像素获数据进行处理（设置为true回读窗口像素可能会有问题），否则可以开启裁剪获得最佳性能。
    create_info.clipped = VK_TRUE;
    // 交换链重建
    // Vulkan运行时，交换链可能在某些条件下被替换，比如窗口调整大小或者交换链需要重新分配更大的图像队列。
    // 在这种情况下，交换链实际上需要重新分配创建，并且必须在此字段中指定对旧的引用，用以回收资源
    create_info.oldSwapchain = VK_NULL_HANDLE;

    if (VK_SUCCESS != vkCreateSwapchainKHR(m_device, &create_info, nullptr, &m_swap_chain)) {
      throw std::runtime_error("failed to create swap chain");
    }

    // 获取交换链中图像，图像会在交换链销毁的同时自动清理
    // 之前给VkSwapchainCreateInfoKHR 设置了期望的图像数量，但是实际运行允许创建更多的图像数量，因此需要重新获取数量
    vkGetSwapchainImagesKHR(m_device, m_swap_chain, &m_min_image_count, nullptr);

    m_colors.resize(m_min_image_count);
    std::vector<VkImage> swap_chain_images;
    swap_chain_images.reserve(m_min_image_count);
    for (auto i = 0U; i < m_min_image_count; ++i) {
      swap_chain_images.emplace_back(m_colors[i].image);
    }
    vkGetSwapchainImagesKHR(m_device, m_swap_chain, &m_min_image_count, swap_chain_images.data());
    for (auto i = 0U; i < m_min_image_count; ++i) {
      m_colors[i].image = swap_chain_images[i];
    }

    m_swap_chain_image_format = surface_format.format;
    m_swap_chain_extent = extent;
  }

  /// @brief 创建图像视图
  /// @details 任何 VkImage 对象都需要通过 VkImageView 来绑定访问，包括处于交换链中的、处于渲染管线中的
  void CreateImageViews() {
    for (auto i = 0U; i < m_min_image_count; ++i) {
      VkImageViewCreateInfo create_info = {};
      create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      create_info.image = m_colors[i].image;
      create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
      create_info.format = m_swap_chain_image_format;
      create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
      create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
      create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
      create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
      create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      create_info.subresourceRange.baseMipLevel = 0;
      create_info.subresourceRange.levelCount = 1;
      create_info.subresourceRange.baseArrayLayer = 0;
      create_info.subresourceRange.layerCount = 1;

      if (VK_SUCCESS != vkCreateImageView(m_device, &create_info, nullptr, &m_colors[i].imageView)) {
        throw std::runtime_error("failed to create image views");
      }
    }
  }

  /// @brief 创建指令池，用于管理指令缓冲对象使用的内存，并负责指令缓冲对象的分配
  void CreateCommandPool() {
    auto indices = FindQueueFamilies(m_physical_device);

    // VK_COMMAND_POOL_CREATE_TRANSIENT_BIT
    // 指定从Pool中分配的CommandBuffer将是短暂的，意味着它们将在相对较短的时间内被重置或释放
    // VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT 允许从Pool中分配的任何CommandBuffer被单独重置到inital状态
    // 没有设置这个flag则不能使用 vkResetCommandBuffer
    // VK_COMMAND_POOL_CREATE_PROTECTED_BIT 指定从Pool中分配的CommandBuffer是受保护的CommandBuffer
    VkCommandPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_info.queueFamilyIndex = lvk_tidy::GetRequiredValue(indices.graphicsFamily);

    if (VK_SUCCESS != vkCreateCommandPool(m_device, &pool_info, nullptr, &m_command_pool)) {
      throw std::runtime_error("failed to create command pool");
    }
  }

  void CreateCommandBuffers() {
    m_command_buffers.resize(k_max_frames_in_flight);

    VkCommandBufferAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = m_command_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;  // 指定是主要还是辅助指令缓冲对象
    alloc_info.commandBufferCount = static_cast<uint32_t>(m_command_buffers.size());

    if (VK_SUCCESS != vkAllocateCommandBuffers(m_device, &alloc_info, m_command_buffers.data())) {
      throw std::runtime_error("failed to allocate command buffers");
    }
  }

  /// @brief 绘制每一帧
  /// @details 流程：从交换链获取一张图像、对帧缓冲附着执行指令缓冲中的渲染指令、返回渲染后的图像到交换链进行呈现操作
  void DrawFrame() {
    vkWaitForFences(m_device, 1, &m_in_flight_fences.at(m_current_frame), VK_TRUE,
                    std::numeric_limits<uint64_t>::max());

    uint32_t image_index{0};
    VkResult result =
      vkAcquireNextImageKHR(m_device, m_swap_chain, std::numeric_limits<uint64_t>::max(),
                            m_image_available_semaphores.at(m_current_frame), VK_NULL_HANDLE, &image_index);

    if (VK_ERROR_OUT_OF_DATE_KHR == result) {
      RecreateSwapChain();
      return;
    }
    if (VK_SUCCESS != result && VK_SUBOPTIMAL_KHR != result) {
      throw std::runtime_error("failed to acquire swap chain image");
    }

    auto aspect = static_cast<float>(m_swap_chain_extent.width) / static_cast<float>(m_swap_chain_extent.height);

    UniformBufferObject ubo{};
    ubo.model = glm::mat4(1.F);
    ubo.view = glm::lookAt(m_eye_pos, m_look_at, m_view_up);
    ubo.proj = glm::perspective(glm::radians(45.F), aspect, 0.1F, 100.F);

    ubo.proj[1][1] *= -1;  // glm的裁剪坐标的Y轴和 Vulkan 是相反的

    // 手动将栅栏重置为未发出信号的状态（必须手动设置）
    vkResetFences(m_device, 1, &m_in_flight_fences.at(m_current_frame));
    vkResetCommandBuffer(m_command_buffers.at(m_current_frame), 0);

    m_render_graph.Execute(m_command_buffers.at(m_current_frame), ubo, m_current_frame, image_index);

    const std::array<VkPipelineStageFlags, 1> wait_stages{VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &m_image_available_semaphores.at(m_current_frame);
    submit_info.pWaitDstStageMask = wait_stages.data();
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &m_command_buffers[m_current_frame];
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &m_render_finished_semaphores[image_index];

    // 提交指令缓冲给图形指令队列
    // 如果不等待上一次提交的指令结束执行，可能会导致内存泄漏
    // 1.vkQueueWaitIdle 可以等待上一次的指令结束执行，2.也可以同时渲染多帧解决该问题（使用栅栏）
    // vkQueueSubmit 的最后一个参数用来指定在指令缓冲执行结束后需要发起信号的栅栏对象
    if (VK_SUCCESS != vkQueueSubmit(m_graphics_queue, 1, &submit_info, m_in_flight_fences.at(m_current_frame))) {
      throw std::runtime_error("failed to submit draw command buffer");
    }

    VkPresentInfoKHR present_info = {};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &m_render_finished_semaphores.at(image_index);  // 指定开始呈现操作需要等待的信号量
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &m_swap_chain;   // 指定用于呈现图像的交换链
    present_info.pImageIndices = &image_index;  // 指定需要呈现的图像在交换链中的索引
    present_info.pResults = nullptr;            // 可以通过该变量获取每个交换链的呈现操作是否成功的信息

    // 请求交换链进行图像呈现操作
    result = vkQueuePresentKHR(m_present_queue, &present_info);

    if (VK_ERROR_OUT_OF_DATE_KHR == result || VK_SUBOPTIMAL_KHR == result || m_framebuffer_resized) {
      m_framebuffer_resized = false;
      // 交换链不完全匹配时也重建交换链
      RecreateSwapChain();
    } else if (VK_SUCCESS != result) {
      throw std::runtime_error("failed to presend swap chain image");
    }

    // 更新当前帧索引
    m_current_frame = (m_current_frame + 1) % k_max_frames_in_flight;
  }

  /// @brief 创建同步对象，用于发出图像已经被获取可以开始渲染和渲染已经结束可以开始呈现的信号
  void CreateSyncObjects() {
    m_image_available_semaphores.resize(k_max_frames_in_flight);
    m_render_finished_semaphores.resize(m_min_image_count);
    m_in_flight_fences.resize(k_max_frames_in_flight);

    VkSemaphoreCreateInfo semaphore_info = {};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fence_info = {};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;  // 初始状态设置为已发出信号，避免 vkWaitForFences 一直等待

    for (size_t i = 0; i < k_max_frames_in_flight; ++i) {
      if (VK_SUCCESS != vkCreateSemaphore(m_device, &semaphore_info, nullptr, &m_image_available_semaphores.at(i)) ||
          VK_SUCCESS != vkCreateFence(m_device, &fence_info, nullptr, &m_in_flight_fences.at(i))) {
        throw std::runtime_error("failed to create synchronization objects for a frame");
      }
    }
    for (auto& semaphore : m_render_finished_semaphores) {
      if (VK_SUCCESS != vkCreateSemaphore(m_device, &semaphore_info, nullptr, &semaphore)) {
        throw std::runtime_error("failed to create render finished semaphore");
      }
    }
  }

  /// @brief 重建交换链
  void RecreateSwapChain() {
    int width{0};
    int height{0};
    glfwGetFramebufferSize(m_window, &width, &height);
    while (0 == width || 0 == height) {
      glfwGetFramebufferSize(m_window, &width, &height);
      glfwWaitEvents();
    }

    // 等待设备处于空闲状态，避免在对象的使用过程中将其清除重建
    vkDeviceWaitIdle(m_device);

    // 在重建前清理之前使用的对象
    CleanupSwapChain();

    // 可以通过动态状态来设置视口和裁剪矩形来避免重建管线
    CreateSwapChain();
    CreateImageViews();
    CreateDepthResources();
  }

  /// @brief 清除交换链相关对象
  void CleanupSwapChain() noexcept {
    vkDestroyImageView(m_device, m_depth.imageView, nullptr);
    vkDestroyImage(m_device, m_depth.image, nullptr);
    vkFreeMemory(m_device, m_depth.deviceMemory, nullptr);

    for (auto color : m_colors) {
      vkDestroyImageView(m_device, color.imageView, nullptr);
    }

    vkDestroySwapchainKHR(m_device, m_swap_chain, nullptr);
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

  /// @brief 创建顶点缓冲
  void CreateVertexBuffer(const std::vector<Vertex>& vertices, Drawable& drawable) {
    VkDeviceSize buffer_size = sizeof(Vertex) * vertices.size();

    VkBuffer staging_buffer{};
    VkDeviceMemory staging_buffer_memory{};

    CreateBuffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging_buffer,
                 staging_buffer_memory);

    void* data{nullptr};
    vkMapMemory(m_device, staging_buffer_memory, 0, buffer_size, 0, &data);
    std::memcpy(data, vertices.data(), static_cast<size_t>(buffer_size));
    vkUnmapMemory(m_device, staging_buffer_memory);

    CreateBuffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, drawable.vertexBuffer, drawable.vertexBufferMemory);

    CopyBuffer(staging_buffer, drawable.vertexBuffer, buffer_size);

    vkDestroyBuffer(m_device, staging_buffer, nullptr);
    vkFreeMemory(m_device, staging_buffer_memory, nullptr);
  }

  /// @brief 创建索引缓冲
  void CreateIndexBuffer(const std::vector<uint16_t>& indices, Drawable& drawable) {
    drawable.indexCount = static_cast<uint32_t>(indices.size());

    VkDeviceSize buffer_size = sizeof(uint16_t) * indices.size();

    VkBuffer staging_buffer{};
    VkDeviceMemory staging_buffer_memory{};

    CreateBuffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging_buffer,
                 staging_buffer_memory);

    void* data{nullptr};
    vkMapMemory(m_device, staging_buffer_memory, 0, buffer_size, 0, &data);
    std::memcpy(data, indices.data(), static_cast<size_t>(buffer_size));
    vkUnmapMemory(m_device, staging_buffer_memory);

    CreateBuffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, drawable.indexBuffer, drawable.indexBufferMemory);

    CopyBuffer(staging_buffer, drawable.indexBuffer, buffer_size);

    vkDestroyBuffer(m_device, staging_buffer, nullptr);
    vkFreeMemory(m_device, staging_buffer_memory, nullptr);
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
    alloc_info.memoryTypeIndex = FindMemoryType(m_physical_device, mem_requirements.memoryTypeBits, properties);

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
    alloc_info.commandPool = m_command_pool;
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
    vkQueueSubmit(m_graphics_queue, 1, &submit_info, nullptr);
    // 等待传输操作完成，也可以使用栅栏，栅栏可以同步多个不同的内存传输操作，给驱动程序的优化空间也更大
    vkQueueWaitIdle(m_graphics_queue);

    vkFreeCommandBuffers(m_device, m_command_pool, 1, &command_buffer);
  }

  VkImageView CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspect_flags) {
    VkImageViewCreateInfo view_info{};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = image;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = format;
    view_info.subresourceRange.aspectMask = aspect_flags;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;

    VkImageView image_view{};
    if (VK_SUCCESS != vkCreateImageView(m_device, &view_info, nullptr, &image_view)) {
      throw std::runtime_error("failed to create texture image view");
    }

    return image_view;
  }

  /// @brief 创建描述符池，描述符集需要通过描述符池来创建
  void CreateDescriptorPool() {
    VkDescriptorPoolSize pool_size{};
    pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    pool_size.descriptorCount = static_cast<uint32_t>(k_max_frames_in_flight);

    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.poolSizeCount = 1;
    pool_info.pPoolSizes = &pool_size;
    pool_info.maxSets = static_cast<uint32_t>(k_max_frames_in_flight);  // 指定可以分配的最大描述符集个数
    pool_info.flags = 0;  // 可以用来设置独立的描述符集是否可以被清除掉，此处使用默认值

    if (VK_SUCCESS != vkCreateDescriptorPool(m_device, &pool_info, nullptr, &m_descriptor_pool)) {
      throw std::runtime_error("failed to create descriptor pool");
    }
  }

  /// @brief 创建指定格式的图像对象
  /// @param width
  /// @param height
  /// @param format
  /// @param tiling
  /// @param usage
  /// @param properties
  /// @param image
  /// @param imageMemory
  void CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage,
                   VkMemoryPropertyFlags /*properties*/, VkImage& image, VkDeviceMemory& image_memory) {
    // tiling成员变量可以是 VK_IMAGE_TILING_LINEAR 纹素以行主序的方式排列，可以直接访问图像
    //                     VK_IMAGE_TILING_OPTIMAL 纹素以一种对访问优化的方式排列
    // initialLayout成员变量可以是 VK_IMAGE_LAYOUT_UNDEFINED GPU不可用，纹素在第一次变换会被丢弃
    //                            VK_IMAGE_LAYOUT_PREINITIALIZED GPU不可用，纹素在第一次变换会被保留
    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.extent.width = static_cast<uint32_t>(width);
    image_info.extent.height = static_cast<uint32_t>(height);
    image_info.extent.depth = 1;
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.format = format;
    image_info.tiling = tiling;                            // 设置之后不可修改
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;  // 图像数据是接收方，不需要保留第一次变换时的纹理数据
    image_info.usage = usage;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;  // 只被一个队列族使用（支持传输操作的队列族），所以使用独占模式
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;          // 设置多重采样，只对用作附着的图像对象有效
    image_info.flags = 0;  // 可以用来设置稀疏图像的优化，比如体素地形没必要为“空气”部分分配内存

    if (VK_SUCCESS != vkCreateImage(m_device, &image_info, nullptr, &image)) {
      throw std::runtime_error("failed to create image");
    }

    VkMemoryRequirements mem_requirements{};
    vkGetImageMemoryRequirements(m_device, image, &mem_requirements);

    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_requirements.size;
    alloc_info.memoryTypeIndex =
      FindMemoryType(m_physical_device, mem_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (VK_SUCCESS != vkAllocateMemory(m_device, &alloc_info, nullptr, &image_memory)) {
      throw std::runtime_error("failed to allocate image memory");
    }
    vkBindImageMemory(m_device, image, image_memory, 0);
  }

  /// @brief 配置深度图像需要的资源
  void CreateDepthResources() {
    m_depth_format = FindDepthFormat();

    CreateImage(m_swap_chain_extent.width, m_swap_chain_extent.height, m_depth_format, VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_depth.image,
                m_depth.deviceMemory);

    m_depth.imageView = CreateImageView(m_depth.image, m_depth_format, VK_IMAGE_ASPECT_DEPTH_BIT);
  }

  /// @brief 查找一个符合要求又被设备支持的图像数据格式
  /// @param candidates
  /// @param tiling
  /// @param features
  /// @return
  VkFormat FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling,
                               VkFormatFeatureFlags features) const {
    for (auto format : candidates) {
      // 查找可用的深度图像格式
      VkFormatProperties props{};
      vkGetPhysicalDeviceFormatProperties(m_physical_device, format, &props);
      // props.bufferFeatures        表示数据格式支持缓冲
      // props.linearTilingFeatures  表示数据格式支持线性tiling模式
      // props.optimalTilingFeatures 表示数据格式支持优化tiling模式

      if (VK_IMAGE_TILING_LINEAR == tiling && (props.linearTilingFeatures & features) == features) {
        return format;
      }
      if (VK_IMAGE_TILING_OPTIMAL == tiling && (props.optimalTilingFeatures & features) == features) {
        return format;
      }
    }

    throw std::runtime_error("failed to find supported format");
  }

  /// @brief 查找适合作为深度附着的图像数据格式
  /// @return
  VkFormat FindDepthFormat() const {
    return FindSupportedFormat({VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
                               VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
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
  /// @param p_create_info
  /// @param p_allocator
  /// @param p_callback
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
  /// @param p_allocator
  static void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT callback,
                                            const VkAllocationCallbacks* p_allocator) noexcept {
    auto func = lvk_tidy::LoadInstanceProcAddress<PFN_vkDestroyDebugUtilsMessengerEXT>(
      instance, "vkDestroyDebugUtilsMessengerEXT");

    if (nullptr != func) {
      func(instance, callback, p_allocator);
    }
  }

  static void FramebufferResizeCallback(GLFWwindow* window, int /*widht*/, int /*height*/) noexcept {
    if (auto app = reinterpret_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(window))) {
      app->m_framebuffer_resized = true;
    }
  }

  /// @brief 光标位置回调
  /// @param window
  /// @param xpos
  /// @param ypos
  static void CursorPosCallback(GLFWwindow* window, double xpos, double ypos) noexcept {
    if (auto app = reinterpret_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(window))) {
      // 计算鼠标移动的偏移量
      double x_offset = xpos - app->m_mouse_last_x;
      double y_offset = ypos - app->m_mouse_last_y;

      if (app->m_is_rotating) {
        // 设置灵敏度，可以根据需要调整
        constexpr auto sensitivity = 0.5 / 180.0 * std::numbers::pi_v<double>;
        x_offset *= sensitivity;
        y_offset *= sensitivity;

        // 构造旋转四元数
        glm::vec3 x_axis = glm::normalize(glm::cross(app->m_view_up, app->m_eye_pos));
        glm::vec3 y_axis = glm::normalize(app->m_view_up);
        glm::quat x_rotation = glm::angleAxis(static_cast<float>(-y_offset), x_axis);
        glm::quat y_rotation = glm::angleAxis(static_cast<float>(-x_offset), y_axis);
        glm::quat rotation_quat = x_rotation * y_rotation;

        app->m_eye_pos = glm::vec3(glm::mat4(rotation_quat) * glm::vec4(app->m_eye_pos, 1.F));
        app->m_view_up = glm::vec3(glm::mat4(rotation_quat) * glm::vec4(app->m_view_up, 1.F));
        // app->m_viewUp = glm::normalize(glm::cross(xAxis, -app->m_eyePos));
      }

      if (app->m_is_translating) {
        x_offset /= (app->m_swap_chain_extent.width / 2);
        y_offset /= (app->m_swap_chain_extent.height / 2);

        glm::vec3 x_axis = glm::normalize(glm::cross(app->m_view_up, app->m_eye_pos));
        glm::vec3 y_axis = glm::normalize(app->m_view_up);
        glm::mat4 x_translation = glm::translate(glm::mat4(1.F), x_axis * static_cast<float>(-x_offset));
        glm::mat4 y_translation = glm::translate(glm::mat4(1.F), y_axis * static_cast<float>(y_offset));
        glm::mat4 translation = x_translation * y_translation;

        app->m_eye_pos = glm::vec3(translation * glm::vec4(app->m_eye_pos, 1.F));
        app->m_look_at = glm::vec3(translation * glm::vec4(app->m_look_at, 1.F));
      }

      // 更新上一次的鼠标位置
      app->m_mouse_last_x = xpos;
      app->m_mouse_last_y = ypos;
    }
  }

  /// @brief 鼠标滚轮滚动回调
  /// @param window
  /// @param xoffset
  /// @param yoffset 小于0向前滚动，大于0向后滚动
  static void ScrollCallback(GLFWwindow* window, double /*xoffset*/, double yoffset) noexcept {
    if (auto app = reinterpret_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(window))) {
      glm::vec3 direction = glm::normalize(app->m_eye_pos - app->m_look_at);
      float distance = glm::distance(app->m_eye_pos, app->m_look_at);
      glm::vec3 translate = direction * distance * 0.1F;

      if (yoffset < 0.0) {
        glm::mat4 mat = glm::translate(glm::mat4(1.F), translate);
        app->m_eye_pos = glm::vec3(mat * glm::vec4(app->m_eye_pos, 1.F));
      } else if (yoffset > 0.0) {
        glm::mat4 mat = glm::translate(glm::mat4(1.F), translate * -1.F);
        app->m_eye_pos = glm::vec3(mat * glm::vec4(app->m_eye_pos, 1.F));
      }
    }
  }

  /// @brief 鼠标按钮操作回调
  /// @param window
  /// @param button
  /// @param action
  /// @param mods
  static void MouseButtonCallback(GLFWwindow* window, int button, int action, int /*mods*/) noexcept {
    if (auto app = reinterpret_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(window))) {
      if (GLFW_PRESS == action && GLFW_MOUSE_BUTTON_RIGHT == button) {
        app->m_is_rotating = true;
        glfwGetCursorPos(window, &app->m_mouse_last_x, &app->m_mouse_last_y);
      } else if (GLFW_RELEASE == action && GLFW_MOUSE_BUTTON_RIGHT == button) {
        app->m_is_rotating = false;
      }

      if (GLFW_PRESS == action && GLFW_MOUSE_BUTTON_LEFT == button) {
        app->m_is_translating = true;
        glfwGetCursorPos(window, &app->m_mouse_last_x, &app->m_mouse_last_y);
      } else if (GLFW_RELEASE == action && GLFW_MOUSE_BUTTON_LEFT == button) {
        app->m_is_translating = false;
      }
    }
  }

  /// @brief 键盘操作回调
  /// @param window
  /// @param key
  /// @param scancode
  /// @param action
  /// @param mods
  static void KeyCallback(GLFWwindow* window, int key, int /*scancode*/, int action, int /*mods*/) noexcept {
    if (auto app = reinterpret_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(window))) {
      // 按下'r'/'R'恢复视角
      if (GLFW_PRESS == action && GLFW_KEY_R == key) {
        app->m_view_up = glm::vec3{0.F, 1.F, 0.F};
        app->m_eye_pos = glm::vec3{0.F, 0.F, -3.F};
        app->m_look_at = glm::vec3{0.F};
      }
    }
  }

public:
  std::tuple<glm::vec3, glm::vec3, glm::vec3> GetCameraParams() const noexcept {
    return std::forward_as_tuple(m_eye_pos, m_view_up, m_look_at);
  }

private:
  GLFWwindow* m_window{nullptr};
  VkInstance m_instance{nullptr};
  VkDebugUtilsMessengerEXT m_debug_messenger{nullptr};
  VkPhysicalDevice m_physical_device{nullptr};
  VkDevice m_device{nullptr};
  VkQueue m_graphics_queue{nullptr};  // 图形队列
  VkSurfaceKHR m_surface{nullptr};
  VkQueue m_present_queue{nullptr};  // 呈现队列
  VkSwapchainKHR m_swap_chain{nullptr};

  VkFormat m_swap_chain_image_format{};
  VkFormat m_depth_format{};
  VkExtent2D m_swap_chain_extent{};

  VkDescriptorPool m_descriptor_pool{nullptr};
  VkCommandPool m_command_pool{nullptr};

  std::vector<VkCommandBuffer> m_command_buffers{};
  std::vector<VkSemaphore> m_image_available_semaphores{};
  std::vector<VkSemaphore> m_render_finished_semaphores{};
  std::vector<VkFence> m_in_flight_fences{};
  size_t m_current_frame{0};
  bool m_framebuffer_resized{false};

  Drawable m_drawable_cube{};
  Drawable m_drawable_plane{};

  glm::vec3 m_view_up{0.F, 1.F, 0.F};
  glm::vec3 m_eye_pos{0.F, 0.F, -3.F};
  glm::vec3 m_look_at{0.F};
  double m_mouse_last_x{0.0};
  double m_mouse_last_y{0.0};
  bool m_is_rotating{false};
  bool m_is_translating{false};

  uint32_t m_min_image_count{0};
  VkDescriptorPool m_imgui_descriptor_pool{nullptr};

  std::vector<ImageData> m_colors{};
  ImageData m_depth{};

  RenderGraph m_render_graph{};
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

#endif  // TEST4
