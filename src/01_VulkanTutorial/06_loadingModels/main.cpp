/**
 * 1. obj
 * 2. gltf
 */

#define TEST2

#ifdef TEST1

#define GLFW_INCLUDE_VULKAN         // 定义这个宏之后 glfw3.h 文件就会包含 Vulkan 的头文件
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS           // glm函数的参数使用弧度
#define GLM_FORCE_DEPTH_ZERO_TO_ONE // 透视矩阵深度值范围 [-1, 1] => [0, 1]
#define GLM_FORCE_CXX17
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image/stb_image.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj/tiny_obj_loader.h>

#include <algorithm>
#include <array>
#include <bit>
#include <fstream>
#include <iostream>
#include <optional>
#include <set>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace lvk_tidy {
template <typename T>
[[nodiscard]] T& GetRequiredValue(std::optional<T>& value)
{
    if (!value.has_value())
    {
        throw std::bad_optional_access {};
    }

    return *value;
}

template <typename T>
[[nodiscard]] const T& GetRequiredValue(const std::optional<T>& value)
{
    if (!value.has_value())
    {
        throw std::bad_optional_access {};
    }

    return *value;
}

template <typename T>
[[nodiscard]] T LoadInstanceProcAddress(VkInstance instance, const char* name) noexcept
{
    return std::bit_cast<T>(vkGetInstanceProcAddr(instance, name));
}

template <typename T>
[[nodiscard]] T LoadDeviceProcAddress(VkDevice device, const char* name) noexcept
{
    return std::bit_cast<T>(vkGetDeviceProcAddr(device, name));
}
} // namespace lvk_tidy

// 窗口默认大小
constexpr uint32_t WIDTH  = 800;
constexpr uint32_t HEIGHT = 600;

// 同时并行处理的帧数
constexpr int MAX_FRAMES_IN_FLIGHT = 2;

// 需要开启的校验层的名称
const std::vector<const char*> g_validationLayers = {"VK_LAYER_KHRONOS_validation"};
// 交换链扩展
const std::vector<const char*> g_deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

// 是否启用校验层
#ifdef NDEBUG
const bool k_enable_validation_layers = false;
#else
const bool k_enable_validation_layers = true;
#endif // NDEBUG

struct Vertex
{
    glm::vec3 pos {0.f, 0.f, 0.f};
    glm::vec3 color {0.f, 0.f, 0.f};
    glm::vec2 texCoord {0.f, 0.f};

    bool operator==(const Vertex& other) const
    {
        return pos == other.pos && color == other.color && texCoord == other.texCoord;
    }

    static constexpr VkVertexInputBindingDescription GetBindingDescription() noexcept
    {
        VkVertexInputBindingDescription bindingDescription {};

        bindingDescription.binding   = 0;
        bindingDescription.stride    = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return bindingDescription;
    }

    static constexpr std::array<VkVertexInputAttributeDescription, 3> GetAttributeDescriptions() noexcept
    {
        std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions {};

        attributeDescriptions[0].binding  = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format   = VK_FORMAT_R32G32B32_SFLOAT; // vec3
        attributeDescriptions[0].offset   = offsetof(Vertex, pos);

        attributeDescriptions[1].binding  = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format   = VK_FORMAT_R32G32B32_SFLOAT; // vec3
        attributeDescriptions[1].offset   = offsetof(Vertex, color);

        attributeDescriptions[2].binding  = 0;
        attributeDescriptions[2].location = 2;                       // 和顶点着色器的 layout(location = 2) in 对应
        attributeDescriptions[2].format   = VK_FORMAT_R32G32_SFLOAT; // vec2
        attributeDescriptions[2].offset   = offsetof(Vertex, texCoord);

        return attributeDescriptions;
    }
};

namespace std {
template <>
struct hash<Vertex>
{
    size_t operator()(Vertex const& vertex) const
    {
        return ((hash<glm::vec3>()(vertex.pos) ^ (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^ (hash<glm::vec2>()(vertex.texCoord) << 1);
    }
};
} // namespace std

struct UniformBufferObject
{
    glm::mat4 model {glm::mat4(1.f)};
    glm::mat4 view {glm::mat4(1.f)};
    glm::mat4 proj {glm::mat4(1.f)};
};

/// @brief 支持图形和呈现的队列族
struct QueueFamilyIndices
{
    std::optional<uint32_t> graphicsFamily {};
    std::optional<uint32_t> presentFamily {};

    constexpr bool IsComplete() const noexcept
    {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

/// @brief 交换链属性信息
struct SwapChainSupportDetails
{
    VkSurfaceCapabilitiesKHR capabilities {};
    std::vector<VkSurfaceFormatKHR> foramts;
    std::vector<VkPresentModeKHR> presentModes;
};

class HelloTriangleApplication
{
public:
    void Run()
    {
        InitWindow();
        InitVulkan();
        MainLoop();
        Cleanup();
    }

private:
    void InitWindow()
    {
        if (GLFW_FALSE == glfwInit())
        {
            throw std::runtime_error("failed to init GLFW");
        }

        // 禁止 glfw 创建 OpenGL 上下文
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        m_window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);

        glfwSetWindowUserPointer(m_window, this);
        glfwSetFramebufferSizeCallback(m_window, FramebufferResizeCallback);
    }

    void InitVulkan()
    {
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
        CreateTextureImage();
        CreateTextureImageView();
        CreateTextureSampler();
        LoadModel();
        CreateVertexBuffer();
        CreateIndexBuffer();
        CreateUniformBuffers();
        CreateDescriptorPool();
        CreateDescriptorSets();
        CreateCommandBuffers();
        CreateSyncObjects();
    }

    void MainLoop()
    {
        while (!glfwWindowShouldClose(m_window))
        {
            glfwPollEvents();
            DrawFrame();
        }

        // 等待逻辑设备的操作结束执行
        // DrawFrame 函数中的操作是异步执行的，关闭窗口跳出while循环时，绘制操作和呈现操作可能仍在执行，不能进行清除操作
        vkDeviceWaitIdle(m_device);
    }

    void Cleanup() noexcept
    {
        CleanupSwapChain();

        vkDestroySampler(m_device, m_textureSampler, nullptr);
        vkDestroyImageView(m_device, m_textureImageView, nullptr);
        vkDestroyImage(m_device, m_textureImage, nullptr);
        vkFreeMemory(m_device, m_textureImageMemory, nullptr);

        vkDestroyBuffer(m_device, m_vertexBuffer, nullptr);
        vkFreeMemory(m_device, m_vertexBufferMemory, nullptr);
        vkDestroyBuffer(m_device, m_indexBuffer, nullptr);
        vkFreeMemory(m_device, m_indexBufferMemory, nullptr);

        vkDestroyPipeline(m_device, m_graphicsPipeline, nullptr);
        vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
        vkDestroyRenderPass(m_device, m_renderPass, nullptr);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            vkDestroyBuffer(m_device, m_uniformBuffers.at(i), nullptr);
            vkFreeMemory(m_device, m_uniformBuffersMemory.at(i), nullptr);
        }

        vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            vkDestroySemaphore(m_device, m_imageAvailableSemaphores.at(i), nullptr);
            vkDestroySemaphore(m_device, m_renderFinishedSemaphores.at(i), nullptr);
            vkDestroyFence(m_device, m_inFlightFences.at(i), nullptr);
        }

        vkDestroyCommandPool(m_device, m_commandPool, nullptr);
        vkDestroyDevice(m_device, nullptr);

        if (k_enable_validation_layers)
        {
            DestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
        }

        vkDestroySurfaceKHR(m_instance, m_surface, nullptr); // 清理表面对象，必须在 Vulkan 实例清理之前
        vkDestroyInstance(m_instance, nullptr);

        glfwDestroyWindow(m_window);
        glfwTerminate();
    }

private:
    /// @brief 创建 Vulkan 实例
    void CreateInstance()
    {
        if (k_enable_validation_layers && !CheckValidationLayerSupport())
        {
            throw std::runtime_error("validation layers requested, but not available");
        }

        // 应用程序的信息，这些信息可能会作为驱动程序的优化依据，让驱动做一些特殊的优化
        VkApplicationInfo appInfo  = {};
        appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pNext              = nullptr;
        appInfo.pApplicationName   = "Hello Triangle";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName        = "No Engine";
        appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion         = VK_API_VERSION_1_0;

        // 指定驱动程序需要使用的全局扩展和校验层，全局是指对整个应用程序都有效，而不仅仅是某一个设备
        VkInstanceCreateInfo createInfo = {};
        createInfo.sType                = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo     = &appInfo;

        // 指定需要的全局扩展
        auto requiredExtensions            = GetRequiredExtensions();
        createInfo.enabledExtensionCount   = static_cast<uint32_t>(requiredExtensions.size());
        createInfo.ppEnabledExtensionNames = requiredExtensions.data();

        // 指定全局校验层
        if (k_enable_validation_layers)
        {
            createInfo.enabledLayerCount   = static_cast<uint32_t>(g_validationLayers.size());
            createInfo.ppEnabledLayerNames = g_validationLayers.data();

            VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = {};
            PopulateDebugMessengerCreateInfo(debugCreateInfo);

            createInfo.pNext = &debugCreateInfo;
        }
        else
        {
            createInfo.enabledLayerCount = 0;
            createInfo.pNext             = nullptr;
        }

        // 创建 Vulkan 实例，用来初始化 Vulkan 库
        // 1.包含创建信息的结构体指针
        // 2.自定义的分配器回调函数
        // 3.指向实例句柄存储位置的指针
        if (VK_SUCCESS != vkCreateInstance(&createInfo, nullptr, &m_instance))
        {
            throw std::runtime_error("failed to create instance");
        }
    }

    /// @brief 检查需要开启的校验层是否被支持
    /// @return
    bool CheckValidationLayerSupport() const noexcept
    {
        // 获取所有可用的校验层列表
        uint32_t layerCount = 0;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
        std::cout << "-------------------------------------------\n"
                  << "All available layers:\n";
        for (const auto& layer : availableLayers)
        {
            std::cout << static_cast<const char*>(layer.layerName) << '\n';
        }

        // 检查需要开启的校验层是否可以在所有可用的校验层列表中找到
        for (const char* layerName : g_validationLayers)
        {
            bool layerFound {false};

            for (const auto& layerProperties : availableLayers)
            {
                if (0 == std::strcmp(layerName, static_cast<const char*>(layerProperties.layerName)))
                {
                    layerFound = true;
                    break;
                }
            }

            if (!layerFound)
            {
                return false;
            }
        }

        return true;
    }

    /// @brief 获取所有需要开启的扩展
    /// @return
    std::vector<const char*> GetRequiredExtensions() const noexcept
    {
        // 获取 Vulkan 支持的所有扩展
        uint32_t extensionCount = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> extensions(extensionCount);
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());
        std::cout << "-------------------------------------------\n"
                  << "All supported extensions:\n";
        for (const auto& e : extensions)
        {
            std::cout << static_cast<const char*>(e.extensionName) << '\n';
        }

        // Vulkan 是一个与平台无关的 API ，所以需要一个和窗口系统交互的扩展
        // 使用 glfw 获取这个扩展
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        std::cout << "-------------------------------------------\n"
                  << "GLFW extensions:\n";
        for (size_t i = 0; i < glfwExtensionCount; ++i)
        {
            std::cout << glfwExtensions[i] << '\n';
        }

        // 将需要开启的所有扩展添加到列表并返回
        std::vector<const char*> requiredExtensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
        if (k_enable_validation_layers)
        {
            // 根据需要开启调试报告相关的扩展
            requiredExtensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        return requiredExtensions;
    }

    /// @brief 设置回调函数来接受调试信息
    void SetupDebugCallback()
    {
        if (!k_enable_validation_layers)
        {
            return;
        }

        VkDebugUtilsMessengerCreateInfoEXT createInfo = {};
        PopulateDebugMessengerCreateInfo(createInfo);

        if (VK_SUCCESS != CreateDebugUtilsMessengerEXT(m_instance, &createInfo, nullptr, &m_debugMessenger))
        {
            throw std::runtime_error("failed to set up debug callback");
        }
    }

    /// @brief 选择一个满足需求的物理设备（显卡）
    /// @details 可以选择任意数量的显卡并同时使用它们
    void PickPhysicalDevice()
    {
        // 获取支持 Vulkan 的显卡数量
        uint32_t deviceCount {0};
        vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);

        if (0 == deviceCount)
        {
            throw std::runtime_error("failed to find GPUs with Vulkan support");
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

        for (const auto& device : devices)
        {
            if (IsDeviceSuitable(device))
            {
                m_physicalDevice = device;
                break;
            }
        }

        if (nullptr == m_physicalDevice)
        {
            throw std::runtime_error("failed to find a suitable GPU");
        }
    }

    /// @brief 检查显卡是否满足需求
    /// @param device
    /// @return
    bool IsDeviceSuitable(VkPhysicalDevice device) const noexcept
    {
        // 获取基本的设置属性，name、type以及Vulkan版本等等
        // VkPhysicalDeviceProperties deviceProperties;
        // vkGetPhysicalDeviceProperties(device, &deviceProperties);

        // 获取对纹理的压缩、64位浮点数和多视图渲染等可选功能的支持
        // VkPhysicalDeviceFeatures deviceFeatures;
        // vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

        auto indices             = FindQueueFamilies(device);
        auto extensionsSupported = CheckDeviceExtensionSupported(device);

        bool swapChainAdequate {false};
        if (extensionsSupported)
        {
            auto swapChainSupport = QuerySwapChainSupport(device);
            swapChainAdequate     = !swapChainSupport.foramts.empty() & !swapChainSupport.presentModes.empty();
        }

        // 是否包含各向异性过滤特性
        VkPhysicalDeviceFeatures supportedFeatures {};
        vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

        return indices.IsComplete() && extensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy;
    }

    /// @brief 查找满足需求的队列族
    /// @details 不同的队列族支持不同的类型的指令，例如计算、内存传输、绘图等指令
    /// @param device
    /// @return
    QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device) const noexcept
    {
        QueueFamilyIndices indices;

        // 获取物理设备支持的队列族列表
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        for (uint32_t i = 0; i < queueFamilies.size(); ++i)
        {
            // 图形队列族
            if (queueFamilies.at(i).queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                indices.graphicsFamily = i;
            }

            // 呈现队列族
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &presentSupport);

            if (presentSupport)
            {
                indices.presentFamily = i;
            }

            if (indices.IsComplete())
            {
                break;
            }
        }

        return indices;
    }

    /// @brief 创建逻辑设备作为和物理设备交互的接口
    void CreateLogicalDevice()
    {
        auto indices = FindQueueFamilies(m_physicalDevice);

        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::set<uint32_t> uniqueQueueFamilies {
            lvk_tidy::GetRequiredValue(indices.graphicsFamily), lvk_tidy::GetRequiredValue(indices.presentFamily)
        };

        // 控制指令缓存执行顺序的优先级，即使只有一个队列也要显示指定优先级，范围：[0.0, 1.0]
        float queuePriority {1.f};
        for (auto queueFamily : uniqueQueueFamilies)
        {
            // 描述队列簇中预要申请使用的队列数量
            // 当前可用的驱动程序所提供的队列簇只允许创建少量的队列，并且很多时候没有必要创建多个队列
            // 因为可以在多个线程上创建所有命令缓冲区，然后在主线程一次性的以较低开销的调用提交队列
            VkDeviceQueueCreateInfo queueCreateInfo {};
            queueCreateInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount       = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        // 指定应用程序使用的设备特性（例如几何着色器）
        VkPhysicalDeviceFeatures deviceFeatures = {};
        deviceFeatures.samplerAnisotropy        = VK_TRUE;

        VkDeviceCreateInfo createInfo      = {};
        createInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.queueCreateInfoCount    = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos       = queueCreateInfos.data();
        createInfo.pEnabledFeatures        = &deviceFeatures;
        createInfo.enabledExtensionCount   = static_cast<uint32_t>(g_deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = g_deviceExtensions.data();

        // 根据需要对设备和 Vulkan 实例使用相同的校验层
        if (k_enable_validation_layers)
        {
            createInfo.enabledLayerCount   = static_cast<uint32_t>(g_validationLayers.size());
            createInfo.ppEnabledLayerNames = g_validationLayers.data();
        }
        else
        {
            createInfo.enabledLayerCount = 0;
        }

        // 创建逻辑设备
        if (VK_SUCCESS != vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device))
        {
            throw std::runtime_error("failed to create logical device");
        }

        // 获取指定队列族的队列句柄，设备队列在逻辑设备被销毁时隐式清理
        // 此处的队列簇可能相同，也就是以相同的参数调用了两次这个函数
        // 1.逻辑设备对象
        // 2.队列族索引
        // 3.队列索引，因为只创建了一个队列，所以此处使用索引0
        // 4.用来存储返回的队列句柄的内存地址
        vkGetDeviceQueue(m_device, lvk_tidy::GetRequiredValue(indices.graphicsFamily), 0, &m_graphicsQueue);
        vkGetDeviceQueue(m_device, lvk_tidy::GetRequiredValue(indices.presentFamily), 0, &m_presentQueue);
    }

    /// @brief 创建表面，需要在程序退出前清理
    /// @details 不同平台创建表面的方式不一样，这里使用 GLFW 统一创建
    void CreateSurface()
    {
        // Vulkan 不能直接与窗口系统进行交互（是一个与平台特性无关的API集合），surface是 Vulkan 与窗体系统的连接桥梁
        // 需要在instance创建之后立即创建窗体surface，因为它会影响物理设备的选择
        // 窗体surface本身对于 Vulkan 也是非强制的，不需要同 OpenGL 一样必须要创建窗体surface
        if (VK_SUCCESS != glfwCreateWindowSurface(m_instance, m_window, nullptr, &m_surface))
        {
            throw std::runtime_error("failed to create window surface");
        }
    }

    /// @brief 检测所需的扩展是否支持
    /// @param device
    /// @return
    bool CheckDeviceExtensionSupported(VkPhysicalDevice device) const noexcept
    {
        uint32_t extensionCount {0};
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

        std::set<std::string> requiredExtensions(std::begin(g_deviceExtensions), std::end(g_deviceExtensions));

        for (const auto& extension : availableExtensions)
        {
            requiredExtensions.erase(static_cast<const char*>(extension.extensionName));
        }

        // 如果为空，则支持
        return requiredExtensions.empty();
    }

    /// @brief 查询交换链支持的细节信息
    /// @param device
    /// @return
    SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device) const noexcept
    {
        SwapChainSupportDetails details;

        // 查询基础表面特性
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_surface, &details.capabilities);

        // 查询表面支持的格式，确保集合对于所有有效的格式可扩充
        uint32_t formatCount {0};
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, nullptr);
        if (0 != formatCount)
        {
            details.foramts.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, details.foramts.data());
        }

        // 查询表面支持的呈现模式
        uint32_t presentModeCount {0};
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount, nullptr);
        if (0 != presentModeCount)
        {
            details.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount, details.presentModes.data());
        }

        return details;
    }

    /// @brief 选择合适的表面格式
    /// @param availableFormats
    /// @return
    VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) const noexcept
    {
        // VkSurfaceFormatKHR 结构都包含一个 format 和一个 colorSpace 成员
        // format 成员变量指定色彩通道和类型
        // colorSpace 成员描述 SRGB 颜色空间是否通过 VK_COLOR_SPACE_SRGB_NONLINEAR_KHR 标志支持
        // 在较早版本的规范中，这个标志名为 VK_COLORSPACE_SRGB_NONLINEAR_KHR

        // 表面没有自己的首选格式，直接返回指定的格式
        if (1 == availableFormats.size() && availableFormats.front().format == VK_FORMAT_UNDEFINED)
        {
            return {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
        }

        // 遍历格式列表，如果想要的格式存在则直接返回
        for (const auto& availableFormat : availableFormats)
        {
            if (VK_FORMAT_B8G8R8A8_UNORM == availableFormat.format && VK_COLOR_SPACE_SRGB_NONLINEAR_KHR == availableFormat.colorSpace)
            {
                return availableFormat;
            }
        }

        // 如果以上两种方式都失效，通过“优良”进行打分排序，大多数情况下会选择第一个格式作为理想的选择
        return availableFormats.front();
    }

    /// @brief 查找最佳的可用呈现模式，模式是非常重要的，因为它代表了在屏幕呈现图像的条件
    /// @param availablePresentModes
    /// @return
    VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) const noexcept
    {
        // 以下4种模式可以使用
        // 1.VK_PRESENT_MODE_IMMEDIATE_KHR: 应用程序提交的图像被立即传输到屏幕呈现，这种模式可能会造成撕裂效果。
        // 2.VK_PRESENT_MODE_FIFO_KHR:
        // 交换链被看作一个队列，当显示内容需要刷新的时候，显示设备从队列的前面获取图像，并且程序将渲染完成的图像插入队列的后面。
        // 如果队列是满的程序会等待。这种规模与视频游戏的垂直同步很类似。显示设备的刷新时刻被成为“垂直中断”。
        // 3.VK_PRESENT_MODE_FIFO_RELAXED_KHR: 该模式与上一个模式略有不同的地方为，如果应用程序存在延迟，即接受最后一个垂直同步信号时队列空了，
        // 将不会等待下一个垂直同步信号，而是将图像直接传送。这样做可能导致可见的撕裂效果。
        // 4.VK_PRESENT_MODE_MAILBOX_KHR: 这是第二种模式的变种。当交换链队列满的时候，选择新的替换旧的图像，从而替代阻塞应用程序的情形。
        // 这种模式通常用来实现三重缓冲区，与标准的垂直同步双缓冲相比，它可以有效避免延迟带来的撕裂效果。

        VkPresentModeKHR bestMode = VK_PRESENT_MODE_FIFO_KHR;

        for (const auto& availablePresentMode : availablePresentModes)
        {
            if (VK_PRESENT_MODE_MAILBOX_KHR == availablePresentMode)
            {
                return availablePresentMode;
            }
            else if (VK_PRESENT_MODE_IMMEDIATE_KHR == availablePresentMode)
            {
                bestMode = availablePresentMode;
            }
        }

        return bestMode;
    }

    /// @brief 设置交换范围，交换范围是交换链中图像的分辨率
    /// @param capabilities
    /// @return
    VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) const noexcept
    {
        // 如果是最大值则表示允许我们自己选择对于窗口最合适的交换范围
        if (std::numeric_limits<uint32_t>::max() != capabilities.currentExtent.width)
        {
            return capabilities.currentExtent;
        }
        else
        {
            int width {0};
            int height {0};
            glfwGetFramebufferSize(m_window, &width, &height);

            // 使用GLFW窗口的大小来设置交换链中图像的分辨率
            VkExtent2D actualExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};

            actualExtent.width  = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

            return actualExtent;
        }
    }

    /// @brief 创建交换链
    void CreateSwapChain()
    {
        SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(m_physicalDevice);
        VkSurfaceFormatKHR surfaceFormat         = ChooseSwapSurfaceFormat(swapChainSupport.foramts);
        VkPresentModeKHR presentMode             = ChooseSwapPresentMode(swapChainSupport.presentModes);
        VkExtent2D extent                        = ChooseSwapExtent(swapChainSupport.capabilities);

        // 交换链中的图像数量，可以理解为队列的长度，指定运行时图像的最小数量
        // maxImageCount数值为0代表除了内存之外没有限制
        uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
        if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
        {
            imageCount = swapChainSupport.capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR createInfo = {};
        createInfo.sType                    = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface                  = m_surface;
        createInfo.minImageCount            = imageCount;
        createInfo.imageFormat              = surfaceFormat.format;
        createInfo.imageColorSpace          = surfaceFormat.colorSpace;
        createInfo.imageExtent              = extent;
        createInfo.imageArrayLayers         = 1;                                   // 图像包含的层次，通常为1，3d图像大于1
        createInfo.imageUsage               = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // 指定在图像上进行怎样的操作，比如显示（颜色附件）、后处理等

        auto indices = FindQueueFamilies(m_physicalDevice);
        const std::array<uint32_t, 2> queueFamilyIndices {
            static_cast<uint32_t>(lvk_tidy::GetRequiredValue(indices.graphicsFamily)),
            static_cast<uint32_t>(lvk_tidy::GetRequiredValue(indices.presentFamily))
        };

        // 在多个队列族使用交换链图像的方式
        if (indices.graphicsFamily != indices.presentFamily)
        {
            // 图形队列和呈现队列不是同一个队列
            // VK_SHARING_MODE_CONCURRENT 表示图像可以在多个队列族间使用，不需要显式地改变图像所有权
            createInfo.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices   = queueFamilyIndices.data();
        }
        else
        {
            // VK_SHARING_MODE_EXCLUSIVE 表示一张图像同一时间只能被一个队列族所拥有，这种模式性能最佳
            createInfo.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE;
            createInfo.queueFamilyIndexCount = 0;
            createInfo.pQueueFamilyIndices   = nullptr;
        }

        // 指定一个固定的变换操作（需要交换链具有supportedTransforms特性），此处不进行任何变换
        createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
        // 指定 alpha 通道是否被用来和窗口系统中的其他窗口进行混合操作
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        // 设置呈现模式
        createInfo.presentMode = presentMode;
        // 设置为true，不关心被遮蔽的像素数据，比如由于其他的窗体置于前方时或者渲染的部分内容存在于可视区域之外，
        // 除非真的需要读取这些像素获数据进行处理（设置为true回读窗口像素可能会有问题），否则可以开启裁剪获得最佳性能。
        createInfo.clipped = VK_TRUE;
        // 交换链重建
        // Vulkan运行时，交换链可能在某些条件下被替换，比如窗口调整大小或者交换链需要重新分配更大的图像队列。
        // 在这种情况下，交换链实际上需要重新分配创建，并且必须在此字段中指定对旧的引用，用以回收资源
        createInfo.oldSwapchain = VK_NULL_HANDLE;

        if (VK_SUCCESS != vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &m_swapChain))
        {
            throw std::runtime_error("failed to create swap chain");
        }

        // 获取交换链中图像，图像会在交换链销毁的同时自动清理
        // 之前给VkSwapchainCreateInfoKHR 设置了期望的图像数量，但是实际运行允许创建更多的图像数量，因此需要重新获取数量
        vkGetSwapchainImagesKHR(m_device, m_swapChain, &imageCount, nullptr);
        m_swapChainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(m_device, m_swapChain, &imageCount, m_swapChainImages.data());

        m_swapChainImageFormat = surfaceFormat.format;
        m_swapChainExtent      = extent;
    }

    /// @brief 创建图像视图
    /// @details 任何 VkImage 对象都需要通过 VkImageView 来绑定访问，包括处于交换链中的、处于渲染管线中的
    void CreateImageViews()
    {
        m_swapChainImageViews.resize(m_swapChainImages.size());

        for (size_t i = 0; i < m_swapChainImages.size(); ++i)
        {
            m_swapChainImageViews.at(i) = CreateImageView(m_swapChainImages.at(i), m_swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
        }
    }

    /// @brief 创建图形管线
    /// @details 在 Vulkan 中几乎不允许对图形管线进行动态设置，也就意味着每一种状态都需要提前创建一个图形管线
    void CreateGraphicsPipeline()
    {
        auto vertShaderCode = ReadFile(PROJECT_ASSETS_DIR "shaders/01_06_base_vert.spv");
        auto fragShaderCode = ReadFile(PROJECT_ASSETS_DIR "shaders/01_06_base_frag.spv");

        VkShaderModule vertShaderModule = CreateShaderModule(vertShaderCode);
        VkShaderModule fragShaderModule = CreateShaderModule(fragShaderCode);

        VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
        vertShaderStageInfo.sType                           = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertShaderStageInfo.stage                           = VK_SHADER_STAGE_VERTEX_BIT;
        vertShaderStageInfo.module                          = vertShaderModule;
        vertShaderStageInfo.pName                           = "main";  // 指定调用的着色器函数，同一份代码可以实现多个着色器
        vertShaderStageInfo.pSpecializationInfo             = nullptr; // 设置着色器常量

        VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
        fragShaderStageInfo.sType                           = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShaderStageInfo.stage                           = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStageInfo.module                          = fragShaderModule;
        fragShaderStageInfo.pName                           = "main";
        fragShaderStageInfo.pSpecializationInfo             = nullptr;

        auto bindingDescription    = Vertex::GetBindingDescription();
        auto attributeDescriptions = Vertex::GetAttributeDescriptions();

        // 顶点信息
        VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
        vertexInputInfo.sType                                = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount        = 1;
        vertexInputInfo.pVertexBindingDescriptions           = &bindingDescription;
        vertexInputInfo.vertexAttributeDescriptionCount      = static_cast<uint32_t>(attributeDescriptions.size());
        vertexInputInfo.pVertexAttributeDescriptions         = attributeDescriptions.data();

        // 拓扑信息
        VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
        inputAssembly.sType                                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology                               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; // 指定绘制的图元类型：点、线、三角形
        inputAssembly.primitiveRestartEnable                 = VK_FALSE;

        // 视口
        VkViewport viewport = {};
        viewport.x          = 0.f;
        viewport.y          = 0.f;
        viewport.width      = static_cast<float>(m_swapChainExtent.width);
        viewport.height     = static_cast<float>(m_swapChainExtent.height);
        viewport.minDepth   = 0.f; // 深度值范围，必须在 [0.f, 1.f]之间
        viewport.maxDepth   = 1.f;

        // 裁剪
        VkRect2D scissor = {};
        scissor.offset   = {0, 0};
        scissor.extent   = m_swapChainExtent;

        VkPipelineViewportStateCreateInfo viewportState = {};
        viewportState.sType                             = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount                     = 1;
        viewportState.pViewports                        = &viewport;
        viewportState.scissorCount                      = 1;
        viewportState.pScissors                         = &scissor;

        // 光栅化
        VkPipelineRasterizationStateCreateInfo rasterizer = {};
        rasterizer.sType                                  = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable                       = VK_FALSE;
        rasterizer.rasterizerDiscardEnable                = VK_FALSE;                        // 设置为true会禁止一切片段输出到帧缓冲
        rasterizer.polygonMode                            = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth                              = 1.f;
        rasterizer.cullMode                               = VK_CULL_MODE_NONE;               // 表面剔除类型，正面、背面、双面剔除
        rasterizer.frontFace                              = VK_FRONT_FACE_COUNTER_CLOCKWISE; // 指定顺时针的顶点序是正面还是反面
        rasterizer.depthBiasEnable                        = VK_FALSE;
        rasterizer.depthBiasConstantFactor                = 1.f;
        rasterizer.depthBiasClamp                         = 0.f;
        rasterizer.depthBiasSlopeFactor                   = 0.f;

        // 多重采样
        VkPipelineMultisampleStateCreateInfo multisampling = {};
        multisampling.sType                                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable                  = VK_FALSE;
        multisampling.rasterizationSamples                 = VK_SAMPLE_COUNT_1_BIT;
        multisampling.minSampleShading                     = 1.f;
        multisampling.pSampleMask                          = nullptr;
        multisampling.alphaToCoverageEnable                = VK_FALSE;
        multisampling.alphaToOneEnable                     = VK_FALSE;

        // 深度和模板测试
        VkPipelineDepthStencilStateCreateInfo depthStencil = {};
        depthStencil.sType                                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable                       = VK_TRUE;            // 是否启用深度测试
        depthStencil.depthWriteEnable                      = VK_TRUE;            // 深度测试通过后是否写入深度缓冲
        depthStencil.depthCompareOp                        = VK_COMPARE_OP_LESS; // 深度值比较方式
        depthStencil.depthBoundsTestEnable                 = VK_FALSE;           // 指定可选的深度范围测试
        depthStencil.minDepthBounds                        = 0.f;
        depthStencil.maxDepthBounds                        = 1.f;
        depthStencil.stencilTestEnable                     = VK_FALSE;           // 模板测试
        depthStencil.front                                 = {};
        depthStencil.back                                  = {};

        // 颜色混合，可以对指定帧缓冲单独设置，也可以设置全局颜色混合方式
        VkPipelineColorBlendAttachmentState colorBlendAttachment {};
        colorBlendAttachment.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo colorBlending {};
        colorBlending.sType             = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable     = VK_FALSE;
        colorBlending.logicOp           = VK_LOGIC_OP_COPY;
        colorBlending.attachmentCount   = 1;
        colorBlending.pAttachments      = &colorBlendAttachment;
        colorBlending.blendConstants[0] = 0.0f;
        colorBlending.blendConstants[1] = 0.0f;
        colorBlending.blendConstants[2] = 0.0f;
        colorBlending.blendConstants[3] = 0.0f;

        // 动态状态，视口大小、线宽、混合常量等
        std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamicState {};
        dynamicState.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates    = dynamicStates.data();

        // 管线布局，在着色器中使用 uniform 变量
        VkPipelineLayoutCreateInfo pipelineLayoutInfo {};
        pipelineLayoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount         = 1;
        pipelineLayoutInfo.pSetLayouts            = &m_descriptorSetLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 0;

        if (VK_SUCCESS != vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &m_pipelineLayout))
        {
            throw std::runtime_error("failed to create pipeline layout");
        }

        const std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages {vertShaderStageInfo, fragShaderStageInfo};

        // 完整的图形管线包括：着色器阶段、固定功能状态、管线布局、渲染流程
        VkGraphicsPipelineCreateInfo pipelineInfo = {};
        pipelineInfo.sType                        = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount                   = 2;
        pipelineInfo.pStages                      = shaderStages.data();
        pipelineInfo.pVertexInputState            = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState          = &inputAssembly;
        pipelineInfo.pViewportState               = &viewportState;
        pipelineInfo.pRasterizationState          = &rasterizer;
        pipelineInfo.pMultisampleState            = &multisampling;
        pipelineInfo.pDepthStencilState           = nullptr;
        pipelineInfo.pColorBlendState             = &colorBlending;
        pipelineInfo.pDynamicState                = &dynamicState;
        pipelineInfo.layout                       = m_pipelineLayout;
        pipelineInfo.renderPass                   = m_renderPass;
        pipelineInfo.subpass                      = 0;       // 子流程在子流程数组中的索引
        pipelineInfo.basePipelineHandle           = nullptr; // 以一个创建好的图形管线为基础创建一个新的图形管线
        pipelineInfo.basePipelineIndex            = -1;      // 只有该结构体的成员 flags 被设置为 VK_PIPELINE_CREATE_DERIVATIVE_BIT 才有效
        pipelineInfo.pDepthStencilState           = &depthStencil;

        if (VK_SUCCESS != vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_graphicsPipeline))
        {
            throw std::runtime_error("failed to create graphics pipeline");
        }

        vkDestroyShaderModule(m_device, fragShaderModule, nullptr);
        vkDestroyShaderModule(m_device, vertShaderModule, nullptr);
    }

    /// @brief 使用着色器字节码数组创建 VkShaderModule 对象
    /// @param code
    /// @return
    VkShaderModule CreateShaderModule(const std::vector<char>& code) const
    {
        VkShaderModuleCreateInfo createInfo = {};
        createInfo.sType                    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize                 = code.size();
        createInfo.pCode                    = reinterpret_cast<const uint32_t*>(code.data());

        VkShaderModule shaderModule;
        if (VK_SUCCESS != vkCreateShaderModule(m_device, &createInfo, nullptr, &shaderModule))
        {
            throw std::runtime_error("failed to create shader module");
        }

        return shaderModule;
    }

    /// @brief 创建渲染流程对象，用于渲染的帧缓冲附着，指定颜色和深度缓冲以及采样数
    void CreateRenderPass()
    {
        // 附着描述
        VkAttachmentDescription colorAttachment = {};
        colorAttachment.format                  = m_swapChainImageFormat;           // 颜色缓冲附着的格式
        colorAttachment.samples                 = VK_SAMPLE_COUNT_1_BIT;            // 采样数
        colorAttachment.loadOp                  = VK_ATTACHMENT_LOAD_OP_CLEAR;      // 渲染之前对附着中的数据（颜色和深度）进行操作
        colorAttachment.storeOp                 = VK_ATTACHMENT_STORE_OP_STORE;     // 渲染之后对附着中的数据（颜色和深度）进行操作
        colorAttachment.stencilLoadOp           = VK_ATTACHMENT_LOAD_OP_DONT_CARE;  // 渲染之前对模板缓冲的操作
        colorAttachment.stencilStoreOp          = VK_ATTACHMENT_STORE_OP_DONT_CARE; // 渲染之后对模板缓冲的操作
        colorAttachment.initialLayout           = VK_IMAGE_LAYOUT_UNDEFINED;        // 渲染流程开始前的图像布局方式
        colorAttachment.finalLayout             = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;  // 渲染流程结束后的图像布局方式

        VkAttachmentDescription depthAttachment = {};
        depthAttachment.format                  = FindDepthFormat();
        depthAttachment.samples                 = VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.loadOp                  = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp                 = VK_ATTACHMENT_STORE_OP_DONT_CARE; // 绘制结束后不需要从深度缓冲复制深度数据
        depthAttachment.stencilLoadOp           = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp          = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout           = VK_IMAGE_LAYOUT_UNDEFINED;        // 不需要读取之前深度图像数据
        depthAttachment.finalLayout             = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        // 子流程引用的附着
        VkAttachmentReference colorAttachmentRef = {};
        colorAttachmentRef.attachment            = 0; // 索引，对应于FrameBuffer的附件数组的索引
        colorAttachmentRef.layout                = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depthAttachmentRef = {};
        depthAttachmentRef.attachment            = 1;
        depthAttachmentRef.layout                = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        // 子流程
        VkSubpassDescription subpass    = {};
        subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS; // 图形渲染子流程
        subpass.colorAttachmentCount    = 1;                               // 颜色附着个数
        subpass.pColorAttachments       = &colorAttachmentRef;             // 指定颜色附着
        subpass.pDepthStencilAttachment = &depthAttachmentRef;             // 指定深度模板附着，深度模板附着只能是一个，所以不用设置数量

        // 渲染流程使用的依赖信息
        VkSubpassDependency dependency = {};
        dependency.srcSubpass          = VK_SUBPASS_EXTERNAL; // 渲染流程开始前的子流程，为了避免出现循环依赖，dst的值必须大于src的值
        dependency.dstSubpass          = 0;                   // 渲染流程结束后的子流程
        dependency.srcStageMask =
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT; // 指定需要等待的管线阶段
        dependency.srcAccessMask = 0;                                                                   // 指定子流程将进行的操作类型
        dependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        std::array<VkAttachmentDescription, 2> attachments {colorAttachment, depthAttachment};

        VkRenderPassCreateInfo renderPassInfo = {};
        renderPassInfo.sType                  = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount        = static_cast<uint32_t>(attachments.size());
        renderPassInfo.pAttachments           = attachments.data();
        renderPassInfo.subpassCount           = 1;
        renderPassInfo.pSubpasses             = &subpass;
        renderPassInfo.dependencyCount        = 1;
        renderPassInfo.pDependencies          = &dependency;

        if (VK_SUCCESS != vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_renderPass))
        {
            throw std::runtime_error("failed to create render pass");
        }
    }

    /// @brief 创建帧缓冲对象，附着需要绑定到帧缓冲对象上使用
    void CreateFramebuffers()
    {
        m_swapChainFramebuffers.resize(m_swapChainImageViews.size());

        for (size_t i = 0; i < m_swapChainImageViews.size(); ++i)
        {
            // 颜色附着和深度（模板）附着
            // 和每个交换链图像对应不同的颜色附着不同，因为信号量的原因，只有一个subpass同时执行，所以一个深度附着即可
            std::array<VkImageView, 2> attachments {m_swapChainImageViews.at(i), m_depthImageView};

            VkFramebufferCreateInfo framebufferInfo = {};
            framebufferInfo.sType                   = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass              = m_renderPass;
            framebufferInfo.attachmentCount         = static_cast<uint32_t>(attachments.size());
            framebufferInfo.pAttachments            = attachments.data();
            framebufferInfo.width                   = m_swapChainExtent.width;
            framebufferInfo.height                  = m_swapChainExtent.height;
            framebufferInfo.layers                  = 1;

            if (VK_SUCCESS != vkCreateFramebuffer(m_device, &framebufferInfo, nullptr, &m_swapChainFramebuffers.at(i)))
            {
                throw std::runtime_error("failed to create framebuffer");
            }
        }
    }

    /// @brief 创建指令池，用于管理指令缓冲对象使用的内存，并负责指令缓冲对象的分配
    void CreateCommandPool()
    {
        auto indices = FindQueueFamilies(m_physicalDevice);

        // VK_COMMAND_POOL_CREATE_TRANSIENT_BIT 指定从Pool中分配的CommandBuffer将是短暂的，意味着它们将在相对较短的时间内被重置或释放
        // VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT 允许从Pool中分配的任何CommandBuffer被单独重置到inital状态
        // 没有设置这个flag则不能使用 vkResetCommandBuffer
        // VK_COMMAND_POOL_CREATE_PROTECTED_BIT 指定从Pool中分配的CommandBuffer是受保护的CommandBuffer
        VkCommandPoolCreateInfo poolInfo = {};
        poolInfo.sType                   = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags                   = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex        = lvk_tidy::GetRequiredValue(indices.graphicsFamily);

        if (VK_SUCCESS != vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPool))
        {
            throw std::runtime_error("failed to create command pool");
        }
    }

    /// @brief 为交换链中的每一个图像创建指令缓冲对象，使用它记录绘制指令
    void CreateCommandBuffers()
    {
        m_commandBuffers.resize(m_swapChainFramebuffers.size());

        VkCommandBufferAllocateInfo allocInfo = {};
        allocInfo.sType                       = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool                 = m_commandPool;
        allocInfo.level                       = VK_COMMAND_BUFFER_LEVEL_PRIMARY; // 指定是主要还是辅助指令缓冲对象
        allocInfo.commandBufferCount          = static_cast<uint32_t>(m_commandBuffers.size());

        if (VK_SUCCESS != vkAllocateCommandBuffers(m_device, &allocInfo, m_commandBuffers.data()))
        {
            throw std::runtime_error("failed to allocate command buffers");
        }
    }

    /// @brief 记录指令到指令缓冲
    void RecordCommandBuffer(VkCommandBuffer commandBuffer, VkFramebuffer framebuffer) const
    {
        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType                    = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags                    = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT; // 指定怎样使用指令缓冲
        beginInfo.pInheritanceInfo         = nullptr;                                      // 只用于辅助指令缓冲，指定从调用它的主要指令缓冲继承的状态

        if (VK_SUCCESS != vkBeginCommandBuffer(commandBuffer, &beginInfo))
        {
            throw std::runtime_error("failed to begin recording command buffer");
        }

        std::array<VkClearValue, 2> clearValues {};
        clearValues.at(0).color = {
            {.1f, .2f, .3f, 1.f}
        };                                         // 清除色，相当于背景色
        clearValues.at(1).depthStencil = {1.f, 0}; // Vulkan 的深度范围是 [0.0, 1.0] 1.0对应视锥体的远平面，初始化时应该设置为远平面的值

        VkRenderPassBeginInfo renderPassInfo = {};
        renderPassInfo.sType                 = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass            = m_renderPass;                              // 指定使用的渲染流程对象
        renderPassInfo.framebuffer           = framebuffer;                               // 指定使用的帧缓冲对象
        renderPassInfo.renderArea.offset     = {0, 0};                                    // 指定用于渲染的区域
        renderPassInfo.renderArea.extent     = m_swapChainExtent;
        renderPassInfo.clearValueCount       = static_cast<uint32_t>(clearValues.size()); // 指定使用 VK_ATTACHMENT_LOAD_OP_CLEAR 标记后使用的清除值
        renderPassInfo.pClearValues          = clearValues.data();

        // 所有可以记录指令到指令缓冲的函数，函数名都带有一个 vkCmd 前缀

        // 开始一个渲染流程
        // 1.用于记录指令的指令缓冲对象
        // 2.使用的渲染流程的信息
        // 3.指定渲染流程如何提供绘制指令的标记
        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        // 绑定图形管线
        // 2.指定管线对象是图形管线还是计算管线
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);

        VkViewport viewport = {};
        viewport.x          = 0.f;
        viewport.y          = 0.f;
        viewport.width      = static_cast<float>(m_swapChainExtent.width);
        viewport.height     = static_cast<float>(m_swapChainExtent.height);
        viewport.minDepth   = 0.f;
        viewport.maxDepth   = 1.f;

        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        VkRect2D scissor = {};
        scissor.offset   = {0, 0};
        scissor.extent   = m_swapChainExtent;

        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        const std::array<VkBuffer, 1> vertexBuffers {m_vertexBuffer};
        const std::array<VkDeviceSize, 1> offsets {0};
        // 绑定顶点缓冲
        // 2.偏移值
        // 3.顶点缓冲数量
        // 4.需要绑定的顶点缓冲数组
        // 5.顶点数据在顶点缓冲中的偏移值数组
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers.data(), offsets.data());

        // 绑定索引缓冲
        vkCmdBindIndexBuffer(commandBuffer, m_indexBuffer, 0, VK_INDEX_TYPE_UINT32);

        // 为每个交换链图像绑定对应的描述符集
        // 2.指定绑定的是图形管线还是计算管线，因为描述符集并不是图形管线所独有的
        // 3.描述符所使用的布局
        // 4.描述符集的第一个元素索引
        // 5.需要绑定的描述符集个数
        // 6.用于绑定的描述符集数组
        // 7.8.指定动态描述符的数组偏移
        vkCmdBindDescriptorSets(
            commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descriptorSets.at(m_currentFrame), 0, nullptr
        );

        // 提交绘制操作到指定缓冲
        vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(m_indices.size()), 1, 0, 0, 0);

        // 结束渲染流程
        vkCmdEndRenderPass(commandBuffer);

        // 结束记录指令到指令缓冲
        if (VK_SUCCESS != vkEndCommandBuffer(commandBuffer))
        {
            throw std::runtime_error("failed to record command buffer");
        }
    }

    /// @brief 绘制每一帧
    /// @details 流程：从交换链获取一张图像、对帧缓冲附着执行指令缓冲中的渲染指令、返回渲染后的图像到交换链进行呈现操作
    void DrawFrame()
    {
        // 等待一组栅栏中的一个或全部栅栏发出信号，即上一次提交的指令结束执行
        // 使用栅栏可以进行CPU与GPU之间的同步，防止超过 MAX_FRAMES_IN_FLIGHT 帧的指令同时被提交执行
        vkWaitForFences(m_device, 1, &m_inFlightFences.at(m_currentFrame), VK_TRUE, std::numeric_limits<uint64_t>::max());

        // 从交换链获取一张图像
        uint32_t imageIndex {0};
        // 3.获取图像的超时时间，此处禁用图像获取超时
        // 4.通知的同步对象
        // 5.输出可用的交换链图像的索引
        VkResult result = vkAcquireNextImageKHR(
            m_device, m_swapChain, std::numeric_limits<uint64_t>::max(), m_imageAvailableSemaphores.at(m_currentFrame), VK_NULL_HANDLE, &imageIndex
        );

        // VK_ERROR_OUT_OF_DATE_KHR 交换链不能继续使用，通常发生在窗口大小改变后
        // VK_SUBOPTIMAL_KHR 交换链仍然可以使用，但表面属性已经不能准确匹配
        if (VK_ERROR_OUT_OF_DATE_KHR == result)
        {
            RecreateSwapChain();
            return;
        }
        else if (VK_SUCCESS != result && VK_SUBOPTIMAL_KHR != result)
        {
            throw std::runtime_error("failed to acquire swap chain image");
        }

        // 每一帧都更新uniform
        UpdateUniformBuffer(static_cast<uint32_t>(m_currentFrame));

        // 手动将栅栏重置为未发出信号的状态（必须手动设置）
        vkResetFences(m_device, 1, &m_inFlightFences.at(m_currentFrame));
        vkResetCommandBuffer(m_commandBuffers.at(imageIndex), 0);
        RecordCommandBuffer(m_commandBuffers.at(imageIndex), m_swapChainFramebuffers.at(imageIndex));

        const std::array<VkPipelineStageFlags, 1> waitStages {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

        VkSubmitInfo submitInfo         = {};
        submitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount   = 1;
        submitInfo.pWaitSemaphores      = &m_imageAvailableSemaphores.at(m_currentFrame); // 指定队列开始执行前需要等待的信号量
        submitInfo.pWaitDstStageMask    = waitStages.data();                              // 指定需要等待的管线阶段
        submitInfo.commandBufferCount   = 1;
        submitInfo.pCommandBuffers      = &m_commandBuffers[imageIndex];                  // 指定实际被提交执行的指令缓冲对象
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores    = &m_renderFinishedSemaphores.at(m_currentFrame); // 指定在指令缓冲执行结束后发出信号的信号量对象

        // 提交指令缓冲给图形指令队列
        // 如果不等待上一次提交的指令结束执行，可能会导致内存泄漏
        // 1.vkQueueWaitIdle 可以等待上一次的指令结束执行，2.也可以同时渲染多帧解决该问题（使用栅栏）
        // vkQueueSubmit 的最后一个参数用来指定在指令缓冲执行结束后需要发起信号的栅栏对象
        if (VK_SUCCESS != vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_inFlightFences.at(m_currentFrame)))
        {
            throw std::runtime_error("failed to submit draw command buffer");
        }

        VkPresentInfoKHR presentInfo   = {};
        presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores    = &m_renderFinishedSemaphores.at(m_currentFrame); // 指定开始呈现操作需要等待的信号量
        presentInfo.swapchainCount     = 1;
        presentInfo.pSwapchains        = &m_swapChain;                                   // 指定用于呈现图像的交换链
        presentInfo.pImageIndices      = &imageIndex;                                    // 指定需要呈现的图像在交换链中的索引
        presentInfo.pResults           = nullptr;                                        // 可以通过该变量获取每个交换链的呈现操作是否成功的信息

        // 请求交换链进行图像呈现操作
        result = vkQueuePresentKHR(m_presentQueue, &presentInfo);

        if (VK_ERROR_OUT_OF_DATE_KHR == result || VK_SUBOPTIMAL_KHR == result || m_framebufferResized)
        {
            m_framebufferResized = false;
            // 交换链不完全匹配时也重建交换链
            RecreateSwapChain();
        }
        else if (VK_SUCCESS != result)
        {
            throw std::runtime_error("failed to presend swap chain image");
        }

        // 更新当前帧索引
        m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    /// @brief 创建同步对象，用于发出图像已经被获取可以开始渲染和渲染已经结束可以开始呈现的信号
    void CreateSyncObjects()
    {
        m_imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        m_renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        m_inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

        VkSemaphoreCreateInfo semaphoreInfo = {};
        semaphoreInfo.sType                 = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo = {};
        fenceInfo.sType             = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags             = VK_FENCE_CREATE_SIGNALED_BIT; // 初始状态设置为已发出信号，避免 vkWaitForFences 一直等待

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            if (VK_SUCCESS != vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_imageAvailableSemaphores.at(i))
                || VK_SUCCESS != vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_renderFinishedSemaphores.at(i))
                || vkCreateFence(m_device, &fenceInfo, nullptr, &m_inFlightFences.at(i)))
            {
                throw std::runtime_error("failed to create synchronization objects for a frame");
            }
        }
    }

    /// @brief 重建交换链
    void RecreateSwapChain()
    {
        int width {0};
        int height {0};
        glfwGetFramebufferSize(m_window, &width, &height);
        while (0 == width || 0 == height)
        {
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
        CreateDepthResources(); // 窗口大小改变后重新创建深度图像资源
        CreateFramebuffers();
    }

    /// @brief 清除交换链相关对象
    void CleanupSwapChain() noexcept
    {
        // 窗口大小变化时需要对深度缓冲进行处理，让深度缓冲的大小和新的窗口大小匹配
        vkDestroyImageView(m_device, m_depthImageView, nullptr);
        vkDestroyImage(m_device, m_depthImage, nullptr);
        vkFreeMemory(m_device, m_depthImageMemory, nullptr);

        for (auto framebuffer : m_swapChainFramebuffers)
        {
            vkDestroyFramebuffer(m_device, framebuffer, nullptr);
        }

        for (auto imageView : m_swapChainImageViews)
        {
            vkDestroyImageView(m_device, imageView, nullptr);
        }

        vkDestroySwapchainKHR(m_device, m_swapChain, nullptr);
    }

    /// @brief 设置调试扩展信息
    /// @param createInfo
    void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) const noexcept
    {
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        // 设置回调函数处理的消息级别
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        // 设置回调函数处理的消息类型
        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        // 设置回调函数
        createInfo.pfnUserCallback = DebugCallback;
        // 设置用户自定义数据，是可选的
        createInfo.pUserData = nullptr;
    }

    /// @brief 创建顶点缓冲
    void CreateVertexBuffer()
    {
        VkDeviceSize bufferSize = sizeof(Vertex) * m_vertices.size();

        // 为了提升性能，使用一个临时（暂存）缓冲，先将顶点数据加载到临时缓冲，再复制到顶点缓冲
        VkBuffer stagingBuffer {};
        VkDeviceMemory stagingBufferMemory {};

        // 创建一个CPU可见的缓冲作为临时缓冲
        // VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT 用于从CPU写入数据
        // VK_MEMORY_PROPERTY_HOST_COHERENT_BIT 可以保证数据被立即复制到缓冲关联的内存
        CreateBuffer(
            bufferSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            stagingBuffer,
            stagingBufferMemory
        );

        void* data {nullptr};
        // 将缓冲关联的内存映射到CPU可以访问的内存
        vkMapMemory(m_device, stagingBufferMemory, 0, bufferSize, 0, &data);
        // 将顶点数据复制到映射后的内存
        std::memcpy(data, m_vertices.data(), static_cast<size_t>(bufferSize));
        // 结束内存映射
        vkUnmapMemory(m_device, stagingBufferMemory);

        // 创建一个显卡读取较快的缓冲作为真正的顶点缓冲
        // 具有 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT 标记的内存最适合显卡读取，CPU通常不能访问这种类型的内存
        CreateBuffer(
            bufferSize,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            m_vertexBuffer,
            m_vertexBufferMemory
        );

        // m_vertexBuffer 现在关联的内存是设备所有的（显卡），不能使用 vkMapMemory 函数对它关联的内存进行映射
        // 从临时（暂存）缓冲复制数据到显卡读取较快的缓冲中
        CopyBuffer(stagingBuffer, m_vertexBuffer, bufferSize);

        vkDestroyBuffer(m_device, stagingBuffer, nullptr);
        vkFreeMemory(m_device, stagingBufferMemory, nullptr);

        // 驱动程序可能并不会立即复制数据到缓冲关联的内存中去，因为处理器都有缓存，写入内存的数据并不一定在多个核心同时可见
        // 保证数据被立即复制到缓冲关联的内存可以使用以下函数，或者使用 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT 属性的内存类型
        // 使用函数的方法性能更好
        // 写入数据到映射的内存后，调用 vkFlushMappedMemoryRanges
        // 读取映射的内存数据前，调用 vkInvalidateMappedMemoryRanges
    }

    /// @brief 创建索引缓冲
    void CreateIndexBuffer()
    {
        VkDeviceSize bufferSize = sizeof(m_indices.front()) * m_indices.size();

        VkBuffer stagingBuffer {};
        VkDeviceMemory stagingBufferMemory {};

        CreateBuffer(
            bufferSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            stagingBuffer,
            stagingBufferMemory
        );

        void* data {nullptr};
        vkMapMemory(m_device, stagingBufferMemory, 0, bufferSize, 0, &data);
        std::memcpy(data, m_indices.data(), static_cast<size_t>(bufferSize));
        vkUnmapMemory(m_device, stagingBufferMemory);

        CreateBuffer(
            bufferSize,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            m_indexBuffer,
            m_indexBufferMemory
        );

        CopyBuffer(stagingBuffer, m_indexBuffer, bufferSize);

        vkDestroyBuffer(m_device, stagingBuffer, nullptr);
        vkFreeMemory(m_device, stagingBufferMemory, nullptr);
    }

    /// @brief 创建指定类型的缓冲
    /// @details Vulkan 的缓冲是可以存储任意数据的可以被显卡读取的内存，不仅可以存储顶点数据
    /// @param size
    /// @param usage
    /// @param properties
    /// @param buffer
    /// @param bufferMemory
    void
    CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) const
    {
        VkBufferCreateInfo bufferInfo = {};
        bufferInfo.sType              = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size               = size;                      // 缓冲的字节大小
        bufferInfo.usage              = usage;                     // 缓冲中的数据使用目的，可以使用位或来指定多个目的
        bufferInfo.sharingMode        = VK_SHARING_MODE_EXCLUSIVE; // 缓冲可以被特定的队列族所拥有，也可以在多个队列族共享
        bufferInfo.flags              = 0;                         // 配置缓冲的内存稀疏程度，0表示使用默认值

        if (VK_SUCCESS != vkCreateBuffer(m_device, &bufferInfo, nullptr, &buffer))
        {
            throw std::runtime_error("failed to create vertex buffer");
        }

        // 缓冲创建好之后还需要分配内存，首先获取缓冲的内存需求
        // size: 缓冲需要的内存的字节大小，可能和bufferInfo.size的值不同
        // alignment: 缓冲在实际被分配的内存中的开始位置，依赖于bufferInfo的usage和flags
        // memoryTypeBits: 指示适合该缓冲使用的内存类型的位域
        VkMemoryRequirements memRequirements {};
        vkGetBufferMemoryRequirements(m_device, buffer, &memRequirements);

        VkMemoryAllocateInfo allocInfo = {};
        allocInfo.sType                = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize       = memRequirements.size;
        allocInfo.memoryTypeIndex      = FindMemoryType(memRequirements.memoryTypeBits, properties);

        if (VK_SUCCESS != vkAllocateMemory(m_device, &allocInfo, nullptr, &bufferMemory))
        {
            throw std::runtime_error("failed to allocate buffer memory");
        }

        // 4. 偏移值，需要满足能够被 memRequirements.alighment 整除
        vkBindBufferMemory(m_device, buffer, bufferMemory, 0);
    }

    VkCommandBuffer BeginSingleTimeCommands() const noexcept
    {
        VkCommandBufferAllocateInfo allocInfo = {};
        allocInfo.sType                       = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level                       = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool                 = m_commandPool;
        allocInfo.commandBufferCount          = 1;

        VkCommandBuffer commandBuffer {};
        vkAllocateCommandBuffers(m_device, &allocInfo, &commandBuffer);

        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType                    = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags                    = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; // 指定如何使用这个指令缓冲

        vkBeginCommandBuffer(commandBuffer, &beginInfo);
        return commandBuffer;
    }

    void EndSingleTimeCommands(VkCommandBuffer commandBuffer) const noexcept
    {
        vkEndCommandBuffer(commandBuffer);

        VkSubmitInfo submitInfo       = {};
        submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers    = &commandBuffer;

        // 提交到内存传输指令队列执行内存传输
        vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, nullptr);
        // 等待传输操作完成，也可以使用栅栏，栅栏可以同步多个不同的内存传输操作，给驱动程序的优化空间也更大
        vkQueueWaitIdle(m_graphicsQueue);

        vkFreeCommandBuffers(m_device, m_commandPool, 1, &commandBuffer);
    }

    /// @brief 在缓冲之间复制数据
    /// @param srcBuffer
    /// @param dstBuffer
    /// @param size
    void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) const noexcept
    {
        VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

        VkBufferCopy copyRegion = {};
        copyRegion.srcOffset    = 0;
        copyRegion.dstOffset    = 0;
        copyRegion.size         = size;
        vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

        EndSingleTimeCommands(commandBuffer);
    }

    /// @brief 查找最合适的内存类型
    /// @details 不同类型的内存所允许进行的操作以及操作的效率有所不同
    /// @param typeFilter 指定需要的内存类型的位域
    /// @param properties
    /// @return
    uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const
    {
        // 查找物理设备可用的内存类型
        // memoryHeaps 内存来源，比如显存以及显存用尽后的位与主存中的交换空间
        VkPhysicalDeviceMemoryProperties memProperties {};
        vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProperties);

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i)
        {
            if (typeFilter & (1 << i) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
            {
                return i;
            }
        }

        throw std::runtime_error("failed to find suitable memory type");
    }

    /// @brief 创建着色器使用的每一个描述符绑定信息
    /// @details 描述符池对象必须包含描述符集信息才可以分配对应的描述集
    void CreateDescriptorSetLayout()
    {
        // 以下三个值(N)共同指定了描述符的绑定点
        // VkWriteDescriptorSet.dstBinding = N;
        // VkDescriptorSetLayoutBinding.binding = N;
        // Shader::layout(binding = N) uniform

        VkDescriptorSetLayoutBinding uboLayoutBinding = {};
        uboLayoutBinding.binding                      = 9;
        uboLayoutBinding.descriptorType               = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboLayoutBinding.descriptorCount              = 1;                          // uniform 缓冲对象数组的大小
        uboLayoutBinding.stageFlags                   = VK_SHADER_STAGE_VERTEX_BIT; // 指定在哪一个着色器阶段使用
        uboLayoutBinding.pImmutableSamplers           = nullptr;                    // 指定图像采样相关的属性

        VkDescriptorSetLayoutBinding samplerLayoutBinding {};
        samplerLayoutBinding.binding            = 6;
        samplerLayoutBinding.descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        samplerLayoutBinding.descriptorCount    = 1;
        samplerLayoutBinding.pImmutableSamplers = nullptr;
        samplerLayoutBinding.stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT; // 指定在片段着色器使用

        std::array<VkDescriptorSetLayoutBinding, 2> bindings = {uboLayoutBinding, samplerLayoutBinding};

        VkDescriptorSetLayoutCreateInfo layoutInfo = {};
        layoutInfo.sType                           = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount                    = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings                       = bindings.data();

        if (VK_SUCCESS != vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &m_descriptorSetLayout))
        {
            throw std::runtime_error("failed to create descriptor set layout");
        }
    }

    /// @brief 创建Uniform Buffer
    /// @details 由于缓冲需要频繁更新，所以此处使用暂存缓冲并不会带来性能提升
    void CreateUniformBuffers()
    {
        VkDeviceSize bufferSize = sizeof(UniformBufferObject);

        m_uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
        m_uniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
        m_uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            CreateBuffer(
                bufferSize,
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                m_uniformBuffers.at(i),
                m_uniformBuffersMemory.at(i)
            );
            vkMapMemory(m_device, m_uniformBuffersMemory.at(i), 0, bufferSize, 0, &m_uniformBuffersMapped.at(i));
        }
    }

    /// @brief 在绘制每一帧时更新uniform
    /// @param currentImage
    void UpdateUniformBuffer(uint32_t currentImage)
    {
        auto aspect = static_cast<float>(m_swapChainExtent.width) / static_cast<float>(m_swapChainExtent.height);

        UniformBufferObject ubo {};
        const auto time = static_cast<float>(glfwGetTime());
        ubo.model       = glm::rotate(glm::mat4(1.f), time * glm::radians(45.f), glm::vec3(1.f, 1.f, 0.f));
        ubo.view        = glm::lookAt(glm::vec3(2.f, 2.f, 2.f), glm::vec3(0.f), glm::vec3(0.f, 0.f, 1.f));
        ubo.proj        = glm::perspective(glm::radians(45.f), aspect, 0.1f, 100.f);

        // glm的裁剪坐标的Y轴和 Vulkan 是相反的
        ubo.proj[1][1] *= -1;

        // 将变换矩阵的数据复制到uniform缓冲
        std::memcpy(m_uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
    }

    /// @brief 创建描述符池，描述符集需要通过描述符池来创建
    void CreateDescriptorPool()
    {
        std::array<VkDescriptorPoolSize, 2> poolSizes {};

        poolSizes.at(0).type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes.at(0).descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
        poolSizes.at(1).type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes.at(1).descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

        VkDescriptorPoolCreateInfo poolInfo {};
        poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes    = poolSizes.data();
        poolInfo.maxSets       = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT); // 指定可以分配的最大描述符集个数
        poolInfo.flags         = 0;                                           // 可以用来设置独立的描述符集是否可以被清除掉，此处使用默认值

        if (VK_SUCCESS != vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_descriptorPool))
        {
            throw std::runtime_error("failed to create descriptor pool");
        }
    }

    /// @brief 创建描述符集
    void CreateDescriptorSets()
    {
        // 描述符布局对象的个数要匹配描述符集对象的个数
        std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, m_descriptorSetLayout);

        VkDescriptorSetAllocateInfo allocInfo {};
        allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool     = m_descriptorPool; // 指定分配描述符集对象的描述符池
        allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
        allocInfo.pSetLayouts        = layouts.data();

        // 描述符集对象会在描述符池对象清除时自动被清除
        // 在这里给每一个交换链图像使用相同的描述符布局创建对应的描述符集
        m_descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
        if (VK_SUCCESS != vkAllocateDescriptorSets(m_device, &allocInfo, m_descriptorSets.data()))
        {
            throw std::runtime_error("failed to allocate descriptor sets");
        }

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            VkDescriptorBufferInfo bufferInfo {};
            bufferInfo.buffer = m_uniformBuffers.at(i);
            bufferInfo.offset = 0;
            bufferInfo.range  = sizeof(UniformBufferObject);

            VkDescriptorImageInfo imageInfo {};
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.imageView   = m_textureImageView;
            imageInfo.sampler     = m_textureSampler;

            std::array<VkWriteDescriptorSet, 2> descriptorWrites {};

            // mvp变换矩阵
            descriptorWrites.at(0).sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites.at(0).dstSet           = m_descriptorSets.at(i); // 指定要更新的描述符集对象
            descriptorWrites.at(0).dstBinding       = 9;                      // 指定缓冲绑定
            descriptorWrites.at(0).dstArrayElement  = 0;                      // 描述符数组的第一个元素的索引（没有数组就使用0）
            descriptorWrites.at(0).descriptorType   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrites.at(0).descriptorCount  = 1;
            descriptorWrites.at(0).pBufferInfo      = &bufferInfo;            // 指定描述符引用的缓冲数据
            descriptorWrites.at(0).pImageInfo       = nullptr;                // 指定描述符引用的图像数据
            descriptorWrites.at(0).pTexelBufferView = nullptr;                // 指定描述符引用的缓冲视图

            // 纹理采样器
            descriptorWrites.at(1).sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites.at(1).dstSet           = m_descriptorSets.at(i);
            descriptorWrites.at(1).dstBinding       = 6;
            descriptorWrites.at(1).dstArrayElement  = 0;
            descriptorWrites.at(1).descriptorType   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrites.at(1).descriptorCount  = 1;
            descriptorWrites.at(1).pBufferInfo      = nullptr;
            descriptorWrites.at(1).pImageInfo       = &imageInfo;
            descriptorWrites.at(1).pTexelBufferView = nullptr;

            // 更新描述符的配置
            vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
        }
    }

    void CreateTextureImage()
    {
        // STBI_rgb_alpha 强制使用alpha通道，如果没有会被添加一个默认的alpha值，texChannels返回图像实际的通道数
        int texWidth {0}, texHeight {0}, texChannels {0};
        auto pixels = stbi_load(PROJECT_ASSETS_DIR "models/viking_room/viking_room.png", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
        std::cout << "image extent: " << texWidth << '\t' << texHeight << '\t' << texChannels << '\n';
        if (!pixels)
        {
            throw std::runtime_error("failed to load texture image");
        }

        VkDeviceSize imageSize = texWidth * texHeight * 4;

        VkBuffer stagingBuffer {};
        VkDeviceMemory stagingBufferMemory {};

        CreateBuffer(
            imageSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            stagingBuffer,
            stagingBufferMemory
        );

        void* data {nullptr};
        vkMapMemory(m_device, stagingBufferMemory, 0, imageSize, 0, &data);
        std::memcpy(data, pixels, static_cast<size_t>(imageSize));
        vkUnmapMemory(m_device, stagingBufferMemory);

        stbi_image_free(pixels);

        CreateImage(
            texWidth,
            texHeight,
            VK_FORMAT_R8G8B8A8_SRGB,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            m_textureImage,
            m_textureImageMemory
        );

        // 图像布局的适用场合：
        // VK_IMAGE_LAYOUT_PRESENT_SRC_KHR 适合呈现操作
        // VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL 适合作为颜色附着，在片段着色器中写入颜色数据
        // VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL 适合作为传输数据的来源，比如 vkCmdCopyImageToBuffer
        // VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL 适合作为传输操作的目的位置，比如 vkCmdCopyBufferToImage
        // VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL 适合在着色器中进行采样操作

        // 变换纹理图像到 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
        // 旧布局设置为 VK_IMAGE_LAYOUT_UNDEFINED ，因为不需要读取复制之前的图像内容
        TransitionImageLayout(m_textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        // 执行图像数据复制操作
        CopyBufferToImage(stagingBuffer, m_textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
        // 将图像转换为能够在着色器中采样的纹理数据图像
        TransitionImageLayout(
            m_textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        );

        vkDestroyBuffer(m_device, stagingBuffer, nullptr);
        vkFreeMemory(m_device, stagingBufferMemory, nullptr);
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
    void CreateImage(
        uint32_t width,
        uint32_t height,
        VkFormat format,
        VkImageTiling tiling,
        VkImageUsageFlags usage,
        VkMemoryPropertyFlags properties,
        VkImage& image,
        VkDeviceMemory& imageMemory
    )

    {
        // tiling成员变量可以是 VK_IMAGE_TILING_LINEAR 纹素以行主序的方式排列，可以直接访问图像
        //                     VK_IMAGE_TILING_OPTIMAL 纹素以一种对访问优化的方式排列
        // initialLayout成员变量可以是 VK_IMAGE_LAYOUT_UNDEFINED GPU不可用，纹素在第一次变换会被丢弃
        //                            VK_IMAGE_LAYOUT_PREINITIALIZED GPU不可用，纹素在第一次变换会被保留
        VkImageCreateInfo imageInfo {};
        imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType     = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width  = static_cast<uint32_t>(width);
        imageInfo.extent.height = static_cast<uint32_t>(height);
        imageInfo.extent.depth  = 1;
        imageInfo.mipLevels     = 1;
        imageInfo.arrayLayers   = 1;
        imageInfo.format        = format;
        imageInfo.tiling        = tiling;                    // 设置之后不可修改
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // 图像数据是接收方，不需要保留第一次变换时的纹理数据
        imageInfo.usage         = usage;
        imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE; // 只被一个队列族使用（支持传输操作的队列族），所以使用独占模式
        imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;     // 设置多重采样，只对用作附着的图像对象有效
        imageInfo.flags         = 0;                         // 可以用来设置稀疏图像的优化，比如体素地形没必要为“空气”部分分配内存

        if (VK_SUCCESS != vkCreateImage(m_device, &imageInfo, nullptr, &image))
        {
            throw std::runtime_error("failed to create image");
        }

        VkMemoryRequirements memRequirements {};
        vkGetImageMemoryRequirements(m_device, image, &memRequirements);

        VkMemoryAllocateInfo allocInfo {};
        allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize  = memRequirements.size;
        allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        if (VK_SUCCESS != vkAllocateMemory(m_device, &allocInfo, nullptr, &imageMemory))
        {
            throw std::runtime_error("failed to allocate image memory");
        }
        vkBindImageMemory(m_device, image, imageMemory, 0);
    }

    /// @brief 图像布局变换
    /// @param image
    /// @param format
    /// @param oldLayout
    /// @param newLayout
    void TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) const
    {
        VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

        VkImageMemoryBarrier barrier {};
        barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout                       = oldLayout;
        barrier.newLayout                       = newLayout;
        barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;   // 如果不进行队列所有权传输，必须这样设置
        barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;   // 同上
        barrier.image                           = image;
        barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT; // 设置布局变换影响的范围
        barrier.subresourceRange.baseMipLevel   = 0;
        barrier.subresourceRange.levelCount     = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount     = 1;
        barrier.srcAccessMask                   = 0;
        barrier.dstAccessMask                   = 0;

        // 指定变换屏障掩码
        VkPipelineStageFlags sourceStage {};
        VkPipelineStageFlags destinationStage {};
        if (VK_IMAGE_LAYOUT_UNDEFINED == oldLayout && VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL == newLayout)
        {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

            sourceStage      = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT; // 这是一个伪阶段
        }
        else if (VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL == oldLayout && VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL == newLayout)
        {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            sourceStage      = VK_PIPELINE_STAGE_TRANSFER_BIT;
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
        else
        {
            throw std::invalid_argument("unsupported layout transition");
        }

        // (pipeline barrier)管线屏障主要被用来同步资源访问，比如保证图像在被读取之前数据被写入，也可以被用来变换图像布局，
        // 如果队列的所有模式为 VK_SHARING_MODE_EXCLUSIVE 还可以被用来传递队列所有权
        // (image memory barrier)图像内存屏障可以对图像布局进行变换
        // (buffer memory barrier)缓冲对象也有实现同样效果的缓冲内存屏障
        // 1.指定发生在屏障之前的管线阶段
        // 2.指定发生在屏障之后的管线阶段
        // 6.用于引用3种可用的管线屏障数组（内存、缓冲、图像）
        vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        EndSingleTimeCommands(commandBuffer);
    }

    void CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t widht, uint32_t height) const noexcept
    {
        VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

        // 用于指定将数据复制到图像的哪一部分
        VkBufferImageCopy region {};
        region.bufferOffset                    = 0; // 要复制的数据在缓冲中的偏移位置
        region.bufferRowLength                 = 0; // 数据在内存中的存放方式，这两个成员（和bufferImageHeight）
        region.bufferImageHeight               = 0; // 可以对每行图像数据使用额外的空间进行对齐，设置为0数据将会在内存中紧凑存放
        region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel       = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount     = 1;
        region.imageOffset                     = {0, 0, 0};
        region.imageExtent                     = {widht, height, 1};

        // 4.指定目的图像当前使用的图像布局
        // 最后一个参数为数组时可以一次从一个缓冲复制数据到多个不同的图像对象
        vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        EndSingleTimeCommands(commandBuffer);
    }

    void CreateTextureImageView()
    {
        m_textureImageView = CreateImageView(m_textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
    }

    VkImageView CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags)
    {
        VkImageViewCreateInfo viewInfo {};
        viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image                           = image;
        viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format                          = format;
        viewInfo.subresourceRange.aspectMask     = aspectFlags;
        viewInfo.subresourceRange.baseMipLevel   = 0;
        viewInfo.subresourceRange.levelCount     = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount     = 1;

        VkImageView imageView {};
        if (VK_SUCCESS != vkCreateImageView(m_device, &viewInfo, nullptr, &imageView))
        {
            throw std::runtime_error("failed to create texture image view");
        }

        return imageView;
    }

    void CreateTextureSampler()
    {
        VkPhysicalDeviceProperties properties {};
        vkGetPhysicalDeviceProperties(m_physicalDevice, &properties);

        VkSamplerCreateInfo samplerInfo {};
        samplerInfo.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter    = VK_FILTER_LINEAR;               // 指定纹理放大时使用的插值方法
        samplerInfo.minFilter    = VK_FILTER_LINEAR;               // 指定纹理缩小时使用的插值方法
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT; // 指定寻址模式
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        // samplerInfo.anisotropyEnable = VK_TRUE; // 使用各向异性过滤（需要检查是否支持）
        // samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
        samplerInfo.anisotropyEnable        = VK_FALSE;                         // 不使用各向异性过滤
        samplerInfo.maxAnisotropy           = 1.0;
        samplerInfo.borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK; // 使用BORDER超出范围时边界颜色
        samplerInfo.unnormalizedCoordinates = VK_FALSE;                         // 坐标系统，false为[0,1] true为[0,w]和[0,h]
        samplerInfo.compareEnable           = VK_FALSE;                         // 将样本和一个设定的值比较，阴影贴图会用到
        samplerInfo.compareOp               = VK_COMPARE_OP_ALWAYS;
        samplerInfo.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;    // 设置分级细化，可以看作是过滤操作的一种
        samplerInfo.mipLodBias              = 0.f;
        samplerInfo.minLod                  = 0.f;
        samplerInfo.maxLod                  = 0.f;

        if (VK_SUCCESS != vkCreateSampler(m_device, &samplerInfo, nullptr, &m_textureSampler))
        {
            throw std::runtime_error("failed to create texture sampler");
        }
    }

    /// @brief 配置深度图像需要的资源
    void CreateDepthResources()
    {
        auto depthFormat = FindDepthFormat();

        CreateImage(
            m_swapChainExtent.width,
            m_swapChainExtent.height,
            depthFormat,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            m_depthImage,
            m_depthImageMemory
        );

        m_depthImageView = CreateImageView(m_depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

        // TransitionImageLayout(m_depthImage, depthFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    }

    /// @brief 查找一个符合要求又被设备支持的图像数据格式
    /// @param candidates
    /// @param tiling
    /// @param features
    /// @return
    VkFormat FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) const
    {
        for (auto format : candidates)
        {
            // 查找可用的深度图像格式
            VkFormatProperties props {};
            vkGetPhysicalDeviceFormatProperties(m_physicalDevice, format, &props);
            // props.bufferFeatures        表示数据格式支持缓冲
            // props.linearTilingFeatures  表示数据格式支持线性tiling模式
            // props.optimalTilingFeatures 表示数据格式支持优化tiling模式

            if (VK_IMAGE_TILING_LINEAR == tiling && (props.linearTilingFeatures & features) == features)
            {
                return format;
            }
            else if (VK_IMAGE_TILING_OPTIMAL == tiling && (props.optimalTilingFeatures & features) == features)
            {
                return format;
            }
        }

        throw std::runtime_error("failed to find supported format");
    }

    /// @brief 查找适合作为深度附着的图像数据格式
    /// @return
    VkFormat FindDepthFormat() const
    {
        return FindSupportedFormat(
            {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
        );
    }

    void LoadModel()
    {
        tinyobj::attrib_t attrib {};
        std::vector<tinyobj::shape_t> shapes {};
        std::vector<tinyobj::material_t> materials {};
        std::string warn {}, err {};

        if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, PROJECT_ASSETS_DIR "models/viking_room/viking_room.obj"))
        {
            throw std::runtime_error(warn + err);
        }

        std::unordered_map<Vertex, uint32_t> uniqueVertices {};

        for (const auto& shape : shapes)
        {
            for (const auto& index : shape.mesh.indices)
            {
                Vertex vertex {};

                vertex.pos = {
                    attrib.vertices[3 * index.vertex_index + 0],
                    attrib.vertices[3 * index.vertex_index + 1],
                    attrib.vertices[3 * index.vertex_index + 2]
                };

                vertex.texCoord = {attrib.texcoords[2 * index.texcoord_index + 0], 1.0f - attrib.texcoords[2 * index.texcoord_index + 1]};

                vertex.color = {1.0f, 1.0f, 1.0f};

                if (uniqueVertices.count(vertex) == 0)
                {
                    uniqueVertices[vertex] = static_cast<uint32_t>(m_vertices.size());
                    m_vertices.push_back(vertex);
                }

                m_indices.push_back(uniqueVertices[vertex]);
            }
        }

        std::cout << "vertices size: " << m_vertices.size() << "\tindices size: " << m_indices.size() << '\n';
    }

private:
    /// @brief 接受调试信息的回调函数
    /// @param message_severity 消息的级别：诊断、资源创建、警告、不合法或可能造成崩溃的操作
    /// @param message_type 发生了与规范和性能无关的事件、出现了违反规范的错误、进行了可能影响 Vulkan 性能的行为
    /// @param callback_data 包含了调试信息的字符串、存储有和消息相关的 Vulkan 对象句柄的数组、数组中的对象个数
    /// @param user_data 指向了设置回调函数时，传递的数据指针
    /// @return 引发校验层处理的 Vulkan API 调用是否中断，通常只在测试校验层本身时会返回true，其余都应该返回 VK_FALSE
    static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
        [[maybe_unused]] VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
        [[maybe_unused]] VkDebugUtilsMessageTypeFlagsEXT message_type,
        const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
        [[maybe_unused]] void* user_data
    ) noexcept
    {
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
    static VkResult CreateDebugUtilsMessengerEXT(
        VkInstance instance,
        const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
        const VkAllocationCallbacks* pAllocator,
        VkDebugUtilsMessengerEXT* pCallback
    ) noexcept
    {
        // vkCreateDebugUtilsMessengerEXT是一个扩展函数，不会被 Vulkan 库自动加载，所以需要手动加载
        auto func = lvk_tidy::LoadInstanceProcAddress<PFN_vkCreateDebugUtilsMessengerEXT>(instance, "vkCreateDebugUtilsMessengerEXT");

        if (nullptr != func)
        {
            return func(instance, pCreateInfo, pAllocator, pCallback);
        }
        else
        {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
    }

    /// @brief 代理函数，用来加载 Vulkan 扩展函数 vkDestroyDebugUtilsMessengerEXT
    /// @param instance
    /// @param callback
    /// @param pAllocator
    static void
    DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT callback, const VkAllocationCallbacks* pAllocator) noexcept
    {
        auto func = lvk_tidy::LoadInstanceProcAddress<PFN_vkDestroyDebugUtilsMessengerEXT>(instance, "vkDestroyDebugUtilsMessengerEXT");

        if (nullptr != func)
        {
            func(instance, callback, pAllocator);
        }
    }

    /// @brief 读取二进制着色器文件
    /// @param fileName
    /// @return
    static std::vector<char> ReadFile(const std::string& fileName)
    {
        std::ifstream file(fileName, std::ios::ate | std::ios::binary);
        if (!file.is_open())
        {
            throw std::runtime_error("failed to open file: " + fileName);
        }

        size_t fileSize = static_cast<size_t>(file.tellg());
        std::vector<char> buffer(fileSize);
        file.seekg(0);
        file.read(buffer.data(), fileSize);
        file.close();
        return buffer;
    }

    static void FramebufferResizeCallback(GLFWwindow* window, int widht, int height) noexcept
    {
        auto app                  = reinterpret_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(window));
        app->m_framebufferResized = true;
    }

private:
    GLFWwindow* m_window {nullptr};
    VkInstance m_instance {nullptr};
    VkDebugUtilsMessengerEXT m_debugMessenger {nullptr};
    VkPhysicalDevice m_physicalDevice {nullptr};
    VkDevice m_device {nullptr};
    VkQueue m_graphicsQueue {nullptr}; // 图形队列
    VkSurfaceKHR m_surface {nullptr};
    VkQueue m_presentQueue {nullptr};  // 呈现队列
    VkSwapchainKHR m_swapChain {nullptr};
    std::vector<VkImage> m_swapChainImages {};
    VkFormat m_swapChainImageFormat {};
    VkExtent2D m_swapChainExtent {};
    std::vector<VkImageView> m_swapChainImageViews {};
    VkRenderPass m_renderPass {nullptr};
    VkPipelineLayout m_pipelineLayout {nullptr};
    VkPipeline m_graphicsPipeline {nullptr};
    std::vector<VkFramebuffer> m_swapChainFramebuffers {};
    VkCommandPool m_commandPool {nullptr};
    std::vector<VkCommandBuffer> m_commandBuffers {};
    std::vector<VkSemaphore> m_imageAvailableSemaphores {};
    std::vector<VkSemaphore> m_renderFinishedSemaphores {};
    std::vector<VkFence> m_inFlightFences {};
    size_t m_currentFrame {0};
    bool m_framebufferResized {false};
    VkBuffer m_vertexBuffer {nullptr};
    VkDeviceMemory m_vertexBufferMemory {nullptr};
    VkBuffer m_indexBuffer {nullptr};
    VkDeviceMemory m_indexBufferMemory {nullptr};
    VkDescriptorSetLayout m_descriptorSetLayout {nullptr};
    std::vector<VkBuffer> m_uniformBuffers {};
    std::vector<VkDeviceMemory> m_uniformBuffersMemory {};
    std::vector<void*> m_uniformBuffersMapped {};
    VkDescriptorPool m_descriptorPool {nullptr};
    std::vector<VkDescriptorSet> m_descriptorSets {};
    VkImage m_textureImage {nullptr};
    VkDeviceMemory m_textureImageMemory {nullptr};
    VkImageView m_textureImageView {nullptr};
    VkSampler m_textureSampler {nullptr};
    VkImage m_depthImage {nullptr};
    VkDeviceMemory m_depthImageMemory {nullptr};
    VkImageView m_depthImageView {nullptr};
    std::vector<Vertex> m_vertices {};
    std::vector<uint32_t> m_indices {};
};

int main()
{
    HelloTriangleApplication app;

    try
    {
        app.Run();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

#endif // TEST1

#ifdef TEST2

#pragma warning(disable : 4996)

#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_USE_CPP14
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf/tiny_gltf.h>

#define GLFW_INCLUDE_VULKAN         // 定义这个宏之后 glfw3.h 文件就会包含 Vulkan 的头文件
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS           // glm函数的参数使用弧度
#define GLM_FORCE_DEPTH_ZERO_TO_ONE // 透视矩阵深度值范围 [-1, 1] => [0, 1]
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include <algorithm>
#include <array>
#include <bit>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numbers>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace lvk_tidy {
template <typename T>
[[nodiscard]] T& GetRequiredValue(std::optional<T>& value)
{
    if (!value.has_value())
    {
        throw std::bad_optional_access {};
    }

    return *value;
}

template <typename T>
[[nodiscard]] const T& GetRequiredValue(const std::optional<T>& value)
{
    if (!value.has_value())
    {
        throw std::bad_optional_access {};
    }

    return *value;
}

template <typename T>
[[nodiscard]] T LoadInstanceProcAddress(VkInstance instance, const char* name) noexcept
{
    return std::bit_cast<T>(vkGetInstanceProcAddr(instance, name));
}

template <typename T>
[[nodiscard]] T LoadDeviceProcAddress(VkDevice device, const char* name) noexcept
{
    return std::bit_cast<T>(vkGetDeviceProcAddr(device, name));
}
} // namespace lvk_tidy

// 窗口默认大小
constexpr uint32_t k_width  = 800;
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
#endif // NDEBUG

struct Sampler
{
    VkSampler sampler {nullptr};
};

struct Image
{
    std::string name {};

    VkImage image {nullptr};
    VkImageView imageView {nullptr};
    VkDeviceMemory imageMemory {nullptr};
};

struct DescriptorSets
{
    std::vector<VkDescriptorSet> descriptorSets {};
};

struct Texture
{
    std::string sampler;
    std::string image;

    std::unique_ptr<DescriptorSets> descriptorSets {std::make_unique<DescriptorSets>()};
};

struct Uniform
{
    std::vector<void*> mappeds {};
    std::vector<VkBuffer> buffers {};
    std::vector<VkDeviceMemory> memorys {};

    std::unique_ptr<DescriptorSets> descriptorSets {std::make_unique<DescriptorSets>()};

    void UpdateUniform(size_t current_frame, const void* data, size_t size)
    {
        std::memcpy(mappeds[current_frame], data, size);
    }
};

struct PbrMetallicRoughness
{
    std::optional<std::unique_ptr<Uniform>> baseColorFactor {};
    std::optional<std::string> baseColorTexture {};

    std::optional<std::unique_ptr<Uniform>> roughnessMetallicFactor {};
    std::optional<std::string> metallicRoughnessTexture {};
};

struct Material
{
    std::string name {};

    std::unique_ptr<PbrMetallicRoughness> pbrMetallicRoughness {std::make_unique<PbrMetallicRoughness>()};

    std::optional<std::string> normalTexture {};
    std::optional<std::string> emissiveTexture {};
    std::optional<std::string> occlusionTexture {};
};

struct Buffer
{
    VkBuffer buffer {nullptr};
    VkDeviceMemory bufferMemory {nullptr};
};

enum class LightingMode : int
{
    None,
    Phong,
    BlinnPhong,
    PBR,
    RayTracing
};

enum class ColoringMode : int
{
    DirectRGB,      ///< 使用 Uniform 设置整个 Drawable 的颜色
    VertexRGB,      ///< 每个顶点一个颜色
    CellRGB,        ///< 每个单元一个颜色（点、线、三角面）
    TextureMapping, ///< 纹理映射
};

enum class AnimationMode : uint8_t
{
    None,  ///< 无动画
    Morph, ///< 变形
    Skin,  ///< 蒙皮
};

struct Primitive
{
    bool VertexColoring()
    {
        return false;
    }

    bool CellColoring()
    {
        return false;
    }

    bool DirectColoring()
    {
        return material && material->pbrMetallicRoughness && material->pbrMetallicRoughness->baseColorFactor;
    }

    bool TextureColoring()
    {
        return material && material->pbrMetallicRoughness && material->pbrMetallicRoughness->baseColorTexture && texCoord;
    }

    bool Phong()
    {
        return false;
    }

    bool BlinnPhong()
    {
        return false;
    }

    bool Pbr()
    {
        return material && material->pbrMetallicRoughness && material->pbrMetallicRoughness->baseColorFactor
            && material->pbrMetallicRoughness->roughnessMetallicFactor && normal;
    }

    ColoringMode coloringMode {ColoringMode::DirectRGB};
    LightingMode lightingMode {LightingMode::None};

    std::string name {"primitive"};

    std::string position {};
    std::string index {};

    VkIndexType indexType {};
    uint32_t indexCount {0};

    std::unique_ptr<Material> material {};

    std::optional<std::string> texCoord {};
    std::optional<std::string> normal {};
    std::optional<std::string> color {};

    AnimationMode animationMode {AnimationMode::None};

    std::optional<std::string> joints {};
    std::optional<std::string> weights {};

    std::optional<std::vector<std::string>> morphPosition {};
    std::optional<std::unique_ptr<Uniform>> animationWeight {};
};

struct Mesh
{
    std::string name {};

    std::vector<std::unique_ptr<Primitive>> primitives {};
};

struct Camera
{
    std::string name {};
};

struct Skin
{
    std::unique_ptr<Uniform> matrix {};
    std::vector<glm::mat4> inverseBindMatrices {};
    std::vector<int> joints {}; // 元素是 gltf 中 Node 的索引，骨骼动画的关节
};

struct Node
{
    std::string name {};
    int index {};

    Node* parent {};

    std::map<std::string, glm::mat4> matrices {};

    glm::mat4 GetLocalMatrix() const
    {
        if (matrices.contains("matrix"))
        {
            return matrices.at("matrix");
        }

        auto translation = matrices.contains("translation") ? matrices.at("translation") : glm::mat4(1.F);
        auto rotation    = matrices.contains("rotation") ? matrices.at("rotation") : glm::mat4(1.F);
        auto scale       = matrices.contains("scale") ? matrices.at("scale") : glm::mat4(1.F);

        return translation * rotation * scale;
    }

    std::unique_ptr<Uniform> modelUniform {};

    std::unique_ptr<Skin> skin {};

    std::unique_ptr<Mesh> mesh {};
    std::unique_ptr<Camera> camera {};

    std::vector<std::unique_ptr<Node>> children {};
};

struct Scene
{
    std::string name {};

    std::vector<std::unique_ptr<Node>> nodes {};
};

enum class PipelineType : uint8_t
{
    DcNl,    // direct color + none light
    TcNl,
    DcPl,
    TcPl,    // texture color + pbr light
    DcNlAW2, // direct color + none light + animation weights[2]
    DcNlAS2, // direct color + none light + animation skins[2]
};

struct DescriptorSetLayout
{
    VkDescriptorSetLayout descriptorSetLayout {nullptr};
};

struct Pipeline
{
    VkPipeline pipeline {nullptr};
    VkPipelineLayout pipelineLayout {nullptr};
    std::vector<std::string> descriptorSetLayouts {};
};

struct ModelAttributes
{
    bool visibility {true};
    bool animation {false};

    double animationStartTime {};

    std::string infomation {};
};

struct AnimationChannel
{
    int sampler {};

    // TODO: 这两个成员应该组织为结构体
    int node {};
    std::string path {};
};

struct AnimationSampler
{
    std::vector<float> input {};
    std::vector<float> output {};
};

struct Animation
{
    std::vector<std::unique_ptr<AnimationChannel>> channels {};
    std::vector<std::unique_ptr<AnimationSampler>> samplers {};
};

struct Model
{
    ModelAttributes attributes {};
    tinygltf::Model gltfModel {};

    // TODO: 应该将 gltf 模型的所有 Node 都保存在此处，scene 中只保存索引
    // Model 应该当和 gltfModel 的成员保持一致，这样方便访问
    // 定义 LoadNode LoadImage LoadTexture LoadSampler 等函数
    std::vector<std::unique_ptr<Scene>> scenes {};

    std::vector<std::unique_ptr<Animation>> animations {};

    std::unordered_map<std::string, std::unique_ptr<Image>> images;
    std::unordered_map<std::string, std::unique_ptr<Buffer>> buffers;
    std::unordered_map<std::string, std::unique_ptr<Sampler>> samplers;
    std::unordered_map<std::string, std::unique_ptr<Texture>> textures;
};

struct Context
{
    std::unordered_map<PipelineType, std::unique_ptr<Pipeline>> pipelines;
    std::unordered_map<std::string, std::unique_ptr<DescriptorSetLayout>> descriptorSetLayouts;
};

struct PushConstantVP
{
    alignas(16) glm::mat4 view {glm::mat4(1.F)};
    alignas(16) glm::mat4 proj {glm::mat4(1.F)};
};

struct PushConstantCamPos
{
    alignas(16) glm::vec3 position;
};

struct Vertex
{
    using PositionType = glm::vec3;
    using NormalType   = glm::vec3;
    using TexCoordType = glm::vec2;
    using WeightType   = glm::vec4;
    using JointType    = glm::vec4;

    static std::vector<VkVertexInputBindingDescription> GetBindingDescriptions(const PipelineType pipeline_type) noexcept
    {
        std::vector<VkVertexInputBindingDescription> binding_descriptions {};
        binding_descriptions.emplace_back(0, static_cast<uint32_t>(sizeof(PositionType)), VK_VERTEX_INPUT_RATE_VERTEX);

        switch (pipeline_type)
        {
            case PipelineType::DcNl:
            {
            }
            break;
            case PipelineType::TcNl:
            {
                binding_descriptions.emplace_back(1, static_cast<uint32_t>(sizeof(TexCoordType)), VK_VERTEX_INPUT_RATE_VERTEX);
            }
            break;
            case PipelineType::DcPl:
            {
                binding_descriptions.emplace_back(1, static_cast<uint32_t>(sizeof(NormalType)), VK_VERTEX_INPUT_RATE_VERTEX);
            }
            break;
            case PipelineType::TcPl:
            {
                binding_descriptions.emplace_back(1, static_cast<uint32_t>(sizeof(NormalType)), VK_VERTEX_INPUT_RATE_VERTEX);
                binding_descriptions.emplace_back(2, static_cast<uint32_t>(sizeof(TexCoordType)), VK_VERTEX_INPUT_RATE_VERTEX);
            }
            break;
            case PipelineType::DcNlAW2:
            {
                binding_descriptions.emplace_back(1, static_cast<uint32_t>(sizeof(PositionType)), VK_VERTEX_INPUT_RATE_VERTEX);
                binding_descriptions.emplace_back(2, static_cast<uint32_t>(sizeof(PositionType)), VK_VERTEX_INPUT_RATE_VERTEX);
            }
            break;
            case PipelineType::DcNlAS2:
            {
                binding_descriptions.emplace_back(1, static_cast<uint32_t>(sizeof(WeightType)), VK_VERTEX_INPUT_RATE_VERTEX);
                binding_descriptions.emplace_back(2, static_cast<uint32_t>(sizeof(JointType)), VK_VERTEX_INPUT_RATE_VERTEX);
            }
            break;
            default:
                break;
        }

        return binding_descriptions;
    }

    static std::vector<VkVertexInputAttributeDescription> GetAttributeDescriptions(const PipelineType pipeline_type) noexcept
    {
        std::vector<VkVertexInputAttributeDescription> attribute_descriptions {};
        attribute_descriptions.emplace_back(0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0);

        switch (pipeline_type)
        {
            case PipelineType::DcNl:
            {
            }
            break;
            case PipelineType::TcNl:
            {
                attribute_descriptions.emplace_back(1, 1, VK_FORMAT_R32G32_SFLOAT, 0);
            }
            break;
            case PipelineType::DcPl:
            {
                attribute_descriptions.emplace_back(1, 1, VK_FORMAT_R32G32B32_SFLOAT, 0);
            }
            break;
            case PipelineType::TcPl:
            {
                attribute_descriptions.emplace_back(1, 1, VK_FORMAT_R32G32B32_SFLOAT, 0);
                attribute_descriptions.emplace_back(2, 2, VK_FORMAT_R32G32_SFLOAT, 0);
            }
            break;
            case PipelineType::DcNlAW2:
            {
                attribute_descriptions.emplace_back(1, 1, VK_FORMAT_R32G32B32_SFLOAT, 0);
                attribute_descriptions.emplace_back(2, 2, VK_FORMAT_R32G32B32_SFLOAT, 0);
            }
            break;
            case PipelineType::DcNlAS2:
            {
                attribute_descriptions.emplace_back(1, 1, VK_FORMAT_R32G32B32A32_SFLOAT, 0);
                attribute_descriptions.emplace_back(2, 2, VK_FORMAT_R32G32B32A32_SFLOAT, 0);
            }
            break;
            default:
                break;
        }

        return attribute_descriptions;
    }
};

/// @brief 支持图形和呈现的队列族
struct QueueFamilyIndices
{
    std::optional<uint32_t> graphicsFamily {};
    std::optional<uint32_t> presentFamily {};

    constexpr bool IsComplete() const noexcept
    {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

/// @brief 交换链属性信息
struct SwapChainSupportDetails
{
    VkSurfaceCapabilitiesKHR capabilities {};
    std::vector<VkSurfaceFormatKHR> foramts;
    std::vector<VkPresentModeKHR> presentModes;
};

class HelloTriangleApplication
{
public:
    void Run()
    {
        InitWindow();
        InitVulkan();
        MainLoop();
        Cleanup();
    }

private:
    void InitWindow()
    {
        if (GLFW_FALSE == glfwInit())
        {
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

    void InitVulkan()
    {
        CreateInstance();
        SetupDebugCallback();
        CreateSurface();
        PickPhysicalDevice();
        CreateLogicalDevice();
        CreateSwapChain();
        CreateImageViews();
        CreateRenderPass();
        CreateCommandPool();
        CreateDepthResources();
        CreateFramebuffers();
        CreateDescriptorPool();
        LoadModels();
        CreateImGuiDescriptorPool();
        CreateCommandBuffers();
        CreateSyncObjects();
        InitImGui();
    }

    void MainLoop()
    {
        while (!glfwWindowShouldClose(m_window))
        {
            glfwPollEvents();
            PrepareImGui();
            DrawFrame();
        }

        // 等待逻辑设备的操作结束执行
        // DrawFrame 函数中的操作是异步执行的，关闭窗口跳出while循环时，绘制操作和呈现操作可能仍在执行，不能进行清除操作
        vkDeviceWaitIdle(m_device);
    }

    void DestroyNode(const std::unique_ptr<Node>& node) noexcept
    {
        auto destroy_uniform = [this](const std::unique_ptr<Uniform>& uniform) {
            for (size_t i = 0; i < k_max_frames_in_flight; ++i)
            {
                vkDestroyBuffer(m_device, uniform->buffers[i], nullptr);
                vkFreeMemory(m_device, uniform->memorys[i], nullptr);
            }
        };

        if (node->skin && node->skin->matrix)
        {
            destroy_uniform(node->skin->matrix);
        }

        if (node->mesh)
        {
            destroy_uniform(node->modelUniform);

            for (const auto& primitive : node->mesh->primitives)
            {
                const auto& pbr = primitive->material->pbrMetallicRoughness;

                if (auto& option_color = pbr->baseColorFactor)
                {
                    destroy_uniform(lvk_tidy::GetRequiredValue(option_color));
                }

                if (auto& option_roughness_metallic = pbr->roughnessMetallicFactor)
                {
                    destroy_uniform(lvk_tidy::GetRequiredValue(option_roughness_metallic));
                }

                if (auto& option_animation = primitive->animationWeight)
                {
                    destroy_uniform(lvk_tidy::GetRequiredValue(option_animation));
                }
            }
        }

        for (auto& child : node->children)
        {
            DestroyNode(child);
        }
    }

    void DestroyModel() noexcept
    {
        for (const auto& [_, model] : m_models)
        {
            for (auto& scene : model->scenes)
            {
                for (auto& node : scene->nodes)
                {
                    DestroyNode(node);
                }
            }

            for (const auto& [_, buffer] : model->buffers)
            {
                vkDestroyBuffer(m_device, buffer->buffer, nullptr);
                vkFreeMemory(m_device, buffer->bufferMemory, nullptr);
            }

            for (const auto& [_, image] : model->images)
            {
                vkDestroyImage(m_device, image->image, nullptr);
                vkFreeMemory(m_device, image->imageMemory, nullptr);
                vkDestroyImageView(m_device, image->imageView, nullptr);
            }

            for (const auto& [_, sampler] : model->samplers)
            {
                vkDestroySampler(m_device, sampler->sampler, nullptr);
            }
        }

        for (const auto& [_, pipeline] : m_context->pipelines)
        {
            vkDestroyPipeline(m_device, pipeline->pipeline, nullptr);
            vkDestroyPipelineLayout(m_device, pipeline->pipelineLayout, nullptr);
        }

        for (const auto& [_, descriptorSetLayout] : m_context->descriptorSetLayouts)
        {
            vkDestroyDescriptorSetLayout(m_device, descriptorSetLayout->descriptorSetLayout, nullptr);
        }
    }

    void Cleanup() noexcept
    {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        CleanupSwapChain();
        DestroyModel();

        vkDestroyRenderPass(m_device, m_render_pass, nullptr);
        vkDestroyDescriptorPool(m_device, m_imgui_descriptor_pool, nullptr);
        vkDestroyDescriptorPool(m_device, m_descriptor_pool, nullptr);

        for (size_t i = 0; i < k_max_frames_in_flight; ++i)
        {
            vkDestroySemaphore(m_device, m_image_available_semaphores.at(i), nullptr);
            vkDestroySemaphore(m_device, m_render_finished_semaphores.at(i), nullptr);
            vkDestroyFence(m_device, m_in_flight_fences.at(i), nullptr);
        }

        vkDestroyCommandPool(m_device, m_command_pool, nullptr);
        vkDestroyDevice(m_device, nullptr);

        if (k_enable_validation_layers)
        {
            DestroyDebugUtilsMessengerEXT(m_instance, m_debug_messenger, nullptr);
        }

        vkDestroySurfaceKHR(m_instance, m_surface, nullptr); // 清理表面对象，必须在 Vulkan 实例清理之前
        vkDestroyInstance(m_instance, nullptr);

        glfwDestroyWindow(m_window);
        glfwTerminate();
    }

private:
    void LoadModels()
    {
        CreateDescriptorSetLayouts();
        CreatePipelines();

        m_models.try_emplace(PROJECT_ASSETS_DIR "models/morph.gltf", std::make_unique<Model>());
        m_models.try_emplace(PROJECT_ASSETS_DIR "models/test.gltf", std::make_unique<Model>());
        m_models.try_emplace(PROJECT_ASSETS_DIR "models/teapot.gltf", std::make_unique<Model>());
        m_models.try_emplace(PROJECT_ASSETS_DIR "models/sphere.gltf", std::make_unique<Model>());
        m_models.try_emplace(PROJECT_ASSETS_DIR "models/WaterBottle/WaterBottle.gltf", std::make_unique<Model>());
        m_models.try_emplace(PROJECT_ASSETS_DIR "models/FlightHelmet/FlightHelmet.gltf", std::make_unique<Model>());
        m_models.try_emplace(PROJECT_ASSETS_DIR "models/skin.gltf", std::make_unique<Model>());

        for (const auto& [name, model] : m_models)
        {
            LoadModel(name, model);
        }
    }

    void LoadModel(const std::string& file_name, const std::unique_ptr<Model>& model)
    {
        std::string err {};
        std::string warn {};

        bool res {false};
        std::filesystem::path path(file_name);
        if (path.has_extension())
        {
            auto ex = path.extension();
            if (path.extension() == ".glb")
            {
                res = m_gltf_loader.LoadBinaryFromFile(&model->gltfModel, &err, &warn, path.string());
            }
            if (path.extension() == ".gltf")
            {
                res = m_gltf_loader.LoadASCIIFromFile(&model->gltfModel, &err, &warn, path.string());
            }
        }

        if (!warn.empty())
        {
            std::cout << "WARN: " << warn << '\n';
        }

        if (!err.empty())
        {
            std::cout << "ERR: " << err << '\n';
        }

        if (!res)
        {
            throw std::runtime_error("failed to load gltf model");
        }

        ParseModel(model);
    }

    std::vector<float> ParseAnimationBuffer(const std::unique_ptr<Model>& model, const tinygltf::Accessor& accessor)
    {
        size_t size {1};
        switch (accessor.type)
        {
            case TINYGLTF_TYPE_VEC4:
                size = 4;
                break;
            case TINYGLTF_TYPE_SCALAR:
                size = 1;
                break;
            default:
                std::cout << std::format("ParseAnimationBuffer: type {} not supported\n", accessor.type);
                break;
        }

        if (accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT)
        {
            std::cout << std::format("ParseAnimationBuffer: componentType {} not supported\n", accessor.componentType);
        }

        const auto& buffer_view = model->gltfModel.bufferViews[accessor.bufferView];
        const auto data_pointer = &model->gltfModel.buffers[buffer_view.buffer].data[accessor.byteOffset + buffer_view.byteOffset];

        return std::vector<float>(reinterpret_cast<float*>(data_pointer), reinterpret_cast<float*>(data_pointer) + accessor.count * size);
    }

    void ParseModel(const std::unique_ptr<Model>& model) noexcept
    {
        for (const auto& animation : model->gltfModel.animations)
        {
            auto temp_animation = std::make_unique<Animation>();

            for (const auto& sampler : animation.samplers)
            {
                auto temp_sampler = std::make_unique<AnimationSampler>();
                if (sampler.input >= 0)
                {
                    const auto& accessor = model->gltfModel.accessors[sampler.input];
                    temp_sampler->input  = ParseAnimationBuffer(model, accessor);
                }
                if (sampler.output >= 0)
                {
                    const auto& accessor = model->gltfModel.accessors[sampler.output];
                    temp_sampler->output = ParseAnimationBuffer(model, accessor);
                }

                temp_animation->samplers.emplace_back(std::move(temp_sampler));
            }

            for (const auto& channel : animation.channels)
            {
                auto temp_channel     = std::make_unique<AnimationChannel>();
                temp_channel->sampler = channel.sampler;
                temp_channel->node    = channel.target_node;
                temp_channel->path    = channel.target_path;

                temp_animation->channels.emplace_back(std::move(temp_channel));
            }

            model->animations.emplace_back(std::move(temp_animation));
        }

        uint32_t imgui_scene {0};
        for (const auto& scene : model->gltfModel.scenes)
        {
            model->attributes.infomation += std::format("Scene {} : {}\n", imgui_scene++, scene.name);

            auto temp_scene  = std::make_unique<Scene>();
            temp_scene->name = scene.name;

            for (const auto& node_index : scene.nodes)
            {
                ParseNode(model, temp_scene->nodes, node_index);
            }

            model->scenes.emplace_back(std::move(temp_scene));
        }
    }

    void ParseNode(const std::unique_ptr<Model>& model, std::vector<std::unique_ptr<Node>>& nodes, int node_index, Node* parent = nullptr)
    {
        const tinygltf::Node& node = model->gltfModel.nodes[node_index];
        model->attributes.infomation += std::format("  Node {} :\n", node.name);

        auto temp_node    = std::make_unique<Node>();
        temp_node->name   = node.name;
        temp_node->index  = node_index;
        temp_node->parent = parent;

        if (node.matrix.size() == 16)
        {
            temp_node->matrices["matrix"] = glm::make_mat4(node.matrix.data());
        }
        else
        {
            if (node.translation.size() == 3)
            {
                temp_node->matrices["translation"] = glm::translate(glm::mat4(1.F), glm::vec3(glm::make_vec3(node.translation.data())));
            }
            if (node.rotation.size() == 4)
            {
                temp_node->matrices["rotation"] = glm::toMat4(glm::make_quat(node.rotation.data()));
            }
            if (node.scale.size() == 3)
            {
                temp_node->matrices["scale"] = glm::scale(glm::mat4(1.F), glm::vec3(glm::make_vec3(node.scale.data())));
            }
        }

        if (node.skin >= 0)
        {
            auto temp_skin = std::make_unique<Skin>();

            const tinygltf::Skin& skin = model->gltfModel.skins[node.skin];

            if (skin.inverseBindMatrices >= 0)
            {
                const auto& accessor = model->gltfModel.accessors[skin.inverseBindMatrices];
                const auto& buffer   = ParseBuffer(model, accessor);

                for (size_t i = 0; i + 15 < std::get<4>(buffer).size(); i += 16)
                {
                    temp_skin->inverseBindMatrices.emplace_back(glm::make_mat4(std::get<4>(buffer).data() + i));
                }

                temp_skin->matrix = CreateStorageBuffer(
                    std::get<0>(buffer), std::get<1>(buffer), m_context->descriptorSetLayouts.at("uniform_animation_skin")->descriptorSetLayout
                );
            }

            temp_skin->joints = skin.joints;
            temp_node->skin   = std::move(temp_skin);
        }

        if (node.mesh >= 0)
        {
            temp_node->modelUniform =
                CreateUniforms(GetNodeMatrix(temp_node.get()), m_context->descriptorSetLayouts.at("uniform_model")->descriptorSetLayout);

            temp_node->mesh = std::make_unique<Mesh>();
            ParseMesh(model, temp_node->mesh, model->gltfModel.meshes[node.mesh]);
        }

        if (node.camera >= 0)
        {
            const auto& camera = model->gltfModel.cameras[node.camera];

            temp_node->camera       = std::make_unique<Camera>();
            temp_node->camera->name = camera.name;
        }

        for (auto child : node.children)
        {
            ParseNode(model, temp_node->children, child, temp_node.get());
        }

        nodes.emplace_back(std::move(temp_node));
    }

    /// @brief
    /// @param accessor
    /// @return dataPointer dataSize elementCount bufferInfo rawData->floatData
    std::tuple<const void*, size_t, uint32_t, std::string, std::vector<float>>
    ParseBuffer(const std::unique_ptr<Model>& model, const tinygltf::Accessor& accessor)
    {
        const auto& buffer_view  = model->gltfModel.bufferViews[accessor.bufferView];
        const auto data_pointer  = &model->gltfModel.buffers[buffer_view.buffer].data[accessor.byteOffset + buffer_view.byteOffset];
        const auto element_count = accessor.count;

        auto data_size = element_count;
        auto count     = element_count;
        switch (accessor.type)
        {
            case TINYGLTF_TYPE_VEC2:
                data_size *= 2;
                count *= 2;
                break;
            case TINYGLTF_TYPE_VEC3:
                data_size *= 3;
                count *= 3;
                break;
            case TINYGLTF_TYPE_VEC4:
                data_size *= 4;
                count *= 4;
                break;
            case TINYGLTF_TYPE_MAT4:
                data_size *= 16;
                count *= 16;
                break;
            default:
                std::cout << std::format("ParseBuffer: type {} not supported\n", accessor.type);
                break;
        }

        std::vector<float> f_data {};
        f_data.reserve(count);
        switch (accessor.componentType)
        {
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
            {
                data_size *= sizeof(unsigned short);

                auto temp_ptr = reinterpret_cast<unsigned short*>(data_pointer);
                for (size_t i = 0; i < count; ++i)
                {
                    f_data.emplace_back(static_cast<float>(*temp_ptr++));
                }
            }
            break;
            case TINYGLTF_COMPONENT_TYPE_FLOAT:
            {
                data_size *= sizeof(float);

                auto temp_ptr = reinterpret_cast<float*>(data_pointer);
                for (size_t i = 0; i < count; ++i)
                {
                    f_data.emplace_back(*temp_ptr++);
                }
            }
            break;
            case TINYGLTF_COMPONENT_TYPE_DOUBLE:
            {
                data_size *= sizeof(double);

                auto temp_ptr = reinterpret_cast<double*>(data_pointer);
                for (size_t i = 0; i < count; ++i)
                {
                    f_data.emplace_back(static_cast<float>(*temp_ptr++));
                }
            }
            break;
            default:
                std::cout << std::format("ParseBuffer: componentType {} not supported\n", accessor.componentType);
                break;
        }

        auto buffer_info = "ptr: " + std::to_string(reinterpret_cast<std::uintptr_t>(data_pointer)) + "\tsize: " + std::to_string(data_size);

        return std::make_tuple(data_pointer, count * sizeof(float), static_cast<uint32_t>(element_count), buffer_info, f_data);
    }

    std::string ParseImage(const std::unique_ptr<Model>& model, const tinygltf::Image& image)
    {
        std::unique_ptr<Image> temp_image {};
        std::string image_info {};

        VkFormat format {};
        auto data_size = static_cast<size_t>(image.width * image.height);
        if (!image.as_is)
        {
            switch (image.component)
            {
                case 3:
                {
                    data_size *= 3;
                    switch (image.pixel_type)
                    {
                        case TINYGLTF_COMPONENT_TYPE_BYTE:
                            data_size *= sizeof(int8_t);
                            format = VK_FORMAT_R8G8B8_SRGB;
                            break;
                        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                            data_size *= sizeof(uint8_t);
                            format = VK_FORMAT_R8G8B8_UNORM;
                            break;
                        default:
                            throw std::runtime_error(std::format("ParseImage: pixel_type {} is not supported", image.pixel_type));
                            break;
                    }
                }
                break;
                case 4:
                {
                    data_size *= 4;
                    switch (image.pixel_type)
                    {
                        case TINYGLTF_COMPONENT_TYPE_BYTE:
                            data_size *= sizeof(int8_t);
                            format = VK_FORMAT_R8G8B8A8_SRGB;
                            break;
                        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                            data_size *= sizeof(uint8_t);
                            format = VK_FORMAT_R8G8B8A8_UNORM;
                            break;
                        default:
                            throw std::runtime_error(std::format("ParseImage: pixel type {} is not supported", image.pixel_type));
                            break;
                    }
                }
                break;
                default:
                    throw std::runtime_error(std::format("ParseImage: component {} is not supported", image.component));
                    break;
            }

            image_info += ("ptr: " + std::to_string(reinterpret_cast<std::uintptr_t>(image.image.data())) + "\tsize: " + std::to_string(data_size));

            if (!model->images.contains(image_info))
            {
                temp_image = CreateTextureImage(image.image.data(), data_size, image.width, image.height, format);
            }
        }
        else
        {
            throw std::runtime_error("this image not supported");
        }

        temp_image->name = image.name;
        model->images.try_emplace(image_info, std::move(temp_image));

        return image_info;
    }

    void ParseMesh(const std::unique_ptr<Model>& model, const std::unique_ptr<Mesh>& mesh, const tinygltf::Mesh& gltf_mesh)
    {
        model->attributes.infomation += std::format("    Mesh : {}\n", gltf_mesh.name);

        mesh->name = gltf_mesh.name;

        uint32_t imgui_primitive {0};
        for (const auto& primitive : gltf_mesh.primitives)
        {
            model->attributes.infomation += std::format("      Primitive {} :\n", imgui_primitive++);
            auto temp_primitive = std::make_unique<Primitive>();

            if (!gltf_mesh.weights.empty())
            {
                std::array<float, 2> weights {static_cast<float>(gltf_mesh.weights[0]), static_cast<float>(gltf_mesh.weights[1])};
                temp_primitive->animationWeight =
                    CreateUniforms(weights.data(), m_context->descriptorSetLayouts.at("uniform_animation_morph")->descriptorSetLayout);
            }

            for (const auto& target : primitive.targets)
            {
                if (target.contains("POSITION"))
                {
                    const auto& accessor = model->gltfModel.accessors[target.at("POSITION")];
                    const auto& buffer   = ParseBuffer(model, accessor);

                    model->attributes.infomation += std::format("        Target: Position : {}\n", std::get<2>(buffer));

                    if (!model->buffers.contains(std::get<3>(buffer)))
                    {
                        model->buffers.try_emplace(std::get<3>(buffer), CreateVertexBuffer(std::get<1>(buffer), std::get<0>(buffer)));
                    }

                    if (!temp_primitive->morphPosition)
                    {
                        temp_primitive->morphPosition = std::vector<std::string> {};
                    }

                    temp_primitive->animationMode = AnimationMode::Morph;
                    lvk_tidy::GetRequiredValue(temp_primitive->morphPosition).emplace_back(std::get<3>(buffer));
                }
            }

            if (primitive.attributes.contains("POSITION"))
            {
                const auto& accessor = model->gltfModel.accessors[primitive.attributes.at("POSITION")];
                const auto& buffer   = ParseBuffer(model, accessor);

                model->attributes.infomation += std::format("        Position : {}\n", std::get<2>(buffer));

                if (!model->buffers.contains(std::get<3>(buffer)))
                {
                    model->buffers.try_emplace(std::get<3>(buffer), CreateVertexBuffer(std::get<1>(buffer), std::get<0>(buffer)));
                }

                temp_primitive->position = std::get<3>(buffer);
            }

            if (primitive.attributes.contains("JOINTS_0"))
            {
                const auto& accessor = model->gltfModel.accessors[primitive.attributes.at("JOINTS_0")];
                const auto& buffer   = ParseBuffer(model, accessor);

                model->attributes.infomation += std::format("        JOINTS_0 : {}\n", std::get<2>(buffer));

                if (!model->buffers.contains(std::get<3>(buffer)))
                {
                    if (!std::get<4>(buffer).empty())
                    {
                        model->buffers.try_emplace(std::get<3>(buffer), CreateVertexBuffer(std::get<1>(buffer), std::get<4>(buffer).data()));
                    }
                }

                temp_primitive->animationMode = AnimationMode::Skin;
                temp_primitive->joints        = std::get<3>(buffer);
            }

            if (primitive.attributes.contains("WEIGHTS_0"))
            {
                const auto& accessor = model->gltfModel.accessors[primitive.attributes.at("WEIGHTS_0")];
                const auto& buffer   = ParseBuffer(model, accessor);

                model->attributes.infomation += std::format("        WEIGHTS_0 : {}\n", std::get<2>(buffer));

                if (!model->buffers.contains(std::get<3>(buffer)))
                {
                    model->buffers.try_emplace(std::get<3>(buffer), CreateVertexBuffer(std::get<1>(buffer), std::get<0>(buffer)));
                }

                temp_primitive->weights = std::get<3>(buffer);
            }

            if (primitive.attributes.contains("NORMAL"))
            {
                const auto& accessor = model->gltfModel.accessors[primitive.attributes.at("NORMAL")];
                const auto& buffer   = ParseBuffer(model, accessor);

                model->attributes.infomation += std::format("        Normal : {}\n", std::get<2>(buffer));

                if (!model->buffers.contains(std::get<3>(buffer)))
                {
                    model->buffers.try_emplace(std::get<3>(buffer), CreateVertexBuffer(std::get<1>(buffer), std::get<0>(buffer)));
                }

                temp_primitive->normal = std::get<3>(buffer);
            }

            if (primitive.attributes.contains("TEXCOORD_0"))
            {
                auto& accessor     = model->gltfModel.accessors[primitive.attributes.at("TEXCOORD_0")];
                const auto& buffer = ParseBuffer(model, accessor);

                model->attributes.infomation += std::format("        TexCoord_0 : {}\n", std::get<2>(buffer));

                if (!model->buffers.contains(std::get<3>(buffer)))
                {
                    model->buffers.try_emplace(std::get<3>(buffer), CreateVertexBuffer(std::get<1>(buffer), std::get<0>(buffer)));
                }

                temp_primitive->texCoord = std::get<3>(buffer);
            }

            if (primitive.attributes.contains("TANGENT"))
            {
            }

            if (primitive.indices >= 0)
            {
                auto accessor     = model->gltfModel.accessors[primitive.indices];
                auto buffer_view  = model->gltfModel.bufferViews[accessor.bufferView];
                auto data_pointer = &model->gltfModel.buffers[buffer_view.buffer].data[accessor.byteOffset + buffer_view.byteOffset];
                auto buffer_size  = accessor.count;

                temp_primitive->indexCount = static_cast<uint32_t>(accessor.count);

                model->attributes.infomation += std::format("        Index : {}\n", accessor.count);

                if (TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER != buffer_view.target)
                {
                    throw std::runtime_error("Only supports vkCmdDrawIndexed");
                }

                switch (accessor.componentType)
                {
                    case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT:
                    {
                        temp_primitive->indexType = VK_INDEX_TYPE_UINT32;
                        buffer_size *= sizeof(uint32_t);
                    }
                    break;
                    case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT:
                    {
                        temp_primitive->indexType = VK_INDEX_TYPE_UINT16;
                        buffer_size *= sizeof(uint16_t);
                    }
                    break;
                    default:
                        break;
                }

                auto buffer_info =
                    "ptr: " + std::to_string(reinterpret_cast<std::uintptr_t>(data_pointer)) + "\tsize: " + std::to_string(buffer_size);
                if (!model->buffers.contains(buffer_info))
                {
                    model->buffers.try_emplace(buffer_info, CreateIndexBuffer(buffer_size, data_pointer));
                }

                temp_primitive->index = std::move(buffer_info);
            }

            temp_primitive->material = std::make_unique<Material>();
            if (primitive.material >= 0)
            {
                model->attributes.infomation += std::format("        Material : {}\n", model->gltfModel.materials[primitive.material].name);

                const auto& material = model->gltfModel.materials[primitive.material];

                temp_primitive->material->name = material.name;

                std::array<float, 4> color {
                    static_cast<float>(material.pbrMetallicRoughness.baseColorFactor[0]),
                    static_cast<float>(material.pbrMetallicRoughness.baseColorFactor[1]),
                    static_cast<float>(material.pbrMetallicRoughness.baseColorFactor[2]),
                    static_cast<float>(material.pbrMetallicRoughness.baseColorFactor[3]),
                };

                temp_primitive->material->pbrMetallicRoughness->baseColorFactor =
                    CreateUniforms(color, m_context->descriptorSetLayouts.at("uniform_color")->descriptorSetLayout);

                std::array<float, 2> roughness_metallic_factor {
                    static_cast<float>(material.pbrMetallicRoughness.roughnessFactor),
                    static_cast<float>(material.pbrMetallicRoughness.metallicFactor)
                };
                temp_primitive->material->pbrMetallicRoughness->roughnessMetallicFactor = CreateUniforms(
                    roughness_metallic_factor, m_context->descriptorSetLayouts.at("uniform_roughnessFactor_metallicFactor")->descriptorSetLayout
                );

                if (material.pbrMetallicRoughness.metallicRoughnessTexture.index >= 0)
                {
                    auto texture_index = material.pbrMetallicRoughness.metallicRoughnessTexture.index;
                    auto temp_texture  = std::make_unique<Texture>();
                    auto texture_info  = std::to_string(texture_index);

                    const auto& texture = model->gltfModel.textures[texture_index];

                    if (texture.source >= 0)
                    {
                        const auto& image   = model->gltfModel.images[texture.source];
                        temp_texture->image = ParseImage(model, image);
                    }

                    // if (texture.sampler >= 0)
                    // {
                    //     const auto& sampler = model->gltfModel.samplers[texture.sampler];
                    // }
                    // else
                    {
                        std::string sampler_info = "default sampler";
                        temp_texture->sampler    = sampler_info;

                        if (!model->samplers.contains(sampler_info))
                        {
                            auto temp_sampler     = std::make_unique<Sampler>();
                            temp_sampler->sampler = CreateTextureSampler();
                            model->samplers.try_emplace(sampler_info, std::move(temp_sampler));
                        }
                    }

                    CreateTextureUniforms(model, temp_texture, m_context->descriptorSetLayouts.at("uniform_sampler")->descriptorSetLayout);

                    model->textures.try_emplace(texture_info, std::move(temp_texture));
                    temp_primitive->material->pbrMetallicRoughness->metallicRoughnessTexture = texture_info;
                }

                if (material.pbrMetallicRoughness.baseColorTexture.index >= 0)
                {
                    auto texture_index = material.pbrMetallicRoughness.baseColorTexture.index;
                    auto temp_texture  = std::make_unique<Texture>();
                    auto texture_info  = std::to_string(texture_index);

                    const auto& texture = model->gltfModel.textures[texture_index];

                    if (texture.source >= 0)
                    {
                        const auto& image   = model->gltfModel.images[texture.source];
                        temp_texture->image = ParseImage(model, image);
                    }

                    // if (texture.sampler >= 0)
                    // {
                    //     const auto& sampler = model->gltfModel.samplers[texture.sampler];
                    // }
                    // else
                    {
                        std::string sampler_info = "default sampler";
                        temp_texture->sampler    = sampler_info;

                        if (!model->samplers.contains(sampler_info))
                        {
                            auto temp_sampler     = std::make_unique<Sampler>();
                            temp_sampler->sampler = CreateTextureSampler();
                            model->samplers.try_emplace(sampler_info, std::move(temp_sampler));
                        }
                    }

                    CreateTextureUniforms(model, temp_texture, m_context->descriptorSetLayouts.at("uniform_sampler")->descriptorSetLayout);

                    model->textures.try_emplace(texture_info, std::move(temp_texture));
                    temp_primitive->material->pbrMetallicRoughness->baseColorTexture = texture_info;
                }
            }
            else // 如果没有材质，则创建一个默认材质
            {
                temp_primitive->material->name = "default material";

                std::array<float, 4> color {0.F, 1.F, 0.F, 1.F};

                temp_primitive->material->pbrMetallicRoughness->baseColorFactor =
                    CreateUniforms(color, m_context->descriptorSetLayouts.at("uniform_color")->descriptorSetLayout);
            }

            mesh->primitives.emplace_back(std::move(temp_primitive));
        }
    }

    void DrawNode(const std::unique_ptr<Model>& model, VkCommandBuffer command_buffer, const std::unique_ptr<Node>& node) const
    {
        if (node->mesh)
        {
            for (const auto& primitive : node->mesh->primitives)
            {
                std::vector<VkBuffer> vertex_buffers {model->buffers.at(primitive->position)->buffer};
                std::vector<VkDeviceSize> offsets {0};

                std::vector<VkDescriptorSet> descriptor_sets {node->modelUniform->descriptorSets->descriptorSets[m_current_frame]};
                VkPipelineLayout pipeline_layout {};
                VkPipeline pipeline {};

                switch (primitive->lightingMode)
                {
                    case LightingMode::None:
                        switch (primitive->coloringMode)
                        {
                            case ColoringMode::DirectRGB:
                            {
                                if (AnimationMode::Morph == primitive->animationMode && model->attributes.animation)
                                {
                                    pipeline        = m_context->pipelines.at(PipelineType::DcNlAW2)->pipeline;
                                    pipeline_layout = m_context->pipelines.at(PipelineType::DcNlAW2)->pipelineLayout;

                                    descriptor_sets.emplace_back(
                                        lvk_tidy::GetRequiredValue(primitive->animationWeight)->descriptorSets->descriptorSets[m_current_frame]
                                    );

                                    auto& base_color = lvk_tidy::GetRequiredValue(primitive->material->pbrMetallicRoughness->baseColorFactor);
                                    descriptor_sets.emplace_back(base_color->descriptorSets->descriptorSets[m_current_frame]);

                                    vertex_buffers.emplace_back(model->buffers.at(lvk_tidy::GetRequiredValue(primitive->morphPosition)[0])->buffer);
                                    vertex_buffers.emplace_back(model->buffers.at(lvk_tidy::GetRequiredValue(primitive->morphPosition)[1])->buffer);
                                    offsets.emplace_back(0);
                                    offsets.emplace_back(0);
                                }
                                else if (AnimationMode::Skin == primitive->animationMode && model->attributes.animation)
                                {
                                    pipeline        = m_context->pipelines.at(PipelineType::DcNlAS2)->pipeline;
                                    pipeline_layout = m_context->pipelines.at(PipelineType::DcNlAS2)->pipelineLayout;

                                    descriptor_sets.emplace_back(node->skin->matrix->descriptorSets->descriptorSets[m_current_frame]);

                                    auto& base_color = lvk_tidy::GetRequiredValue(primitive->material->pbrMetallicRoughness->baseColorFactor);
                                    descriptor_sets.emplace_back(base_color->descriptorSets->descriptorSets[m_current_frame]);

                                    vertex_buffers.emplace_back(model->buffers.at(lvk_tidy::GetRequiredValue(primitive->weights))->buffer);
                                    vertex_buffers.emplace_back(model->buffers.at(lvk_tidy::GetRequiredValue(primitive->joints))->buffer);
                                    offsets.emplace_back(0);
                                    offsets.emplace_back(0);
                                }
                                else
                                {
                                    pipeline        = m_context->pipelines.at(PipelineType::DcNl)->pipeline;
                                    pipeline_layout = m_context->pipelines.at(PipelineType::DcNl)->pipelineLayout;

                                    auto& base_color = lvk_tidy::GetRequiredValue(primitive->material->pbrMetallicRoughness->baseColorFactor);
                                    descriptor_sets.emplace_back(base_color->descriptorSets->descriptorSets[m_current_frame]);
                                }
                            }
                            break;
                            case ColoringMode::TextureMapping:
                            {
                                pipeline        = m_context->pipelines.at(PipelineType::TcNl)->pipeline;
                                pipeline_layout = m_context->pipelines.at(PipelineType::TcNl)->pipelineLayout;

                                auto& base_texture = lvk_tidy::GetRequiredValue(primitive->material->pbrMetallicRoughness->baseColorTexture);
                                auto& texture      = model->textures.at(base_texture);
                                descriptor_sets.emplace_back(texture->descriptorSets->descriptorSets[m_current_frame]);

                                vertex_buffers.emplace_back(model->buffers.at(lvk_tidy::GetRequiredValue(primitive->texCoord))->buffer);
                                offsets.emplace_back(0);
                            }
                            break;
                            default:
                                break;
                        }
                        break;
                    case LightingMode::PBR:
                        switch (primitive->coloringMode)
                        {
                            case ColoringMode::DirectRGB:
                            {
                                vertex_buffers.emplace_back(model->buffers.at(lvk_tidy::GetRequiredValue(primitive->normal))->buffer);
                                offsets.emplace_back(0);

                                pipeline        = m_context->pipelines.at(PipelineType::DcPl)->pipeline;
                                pipeline_layout = m_context->pipelines.at(PipelineType::DcPl)->pipelineLayout;

                                auto& rm_factor = lvk_tidy::GetRequiredValue(primitive->material->pbrMetallicRoughness->roughnessMetallicFactor);
                                descriptor_sets.emplace_back(rm_factor->descriptorSets->descriptorSets[m_current_frame]);

                                auto& base_color = lvk_tidy::GetRequiredValue(primitive->material->pbrMetallicRoughness->baseColorFactor);
                                descriptor_sets.emplace_back(base_color->descriptorSets->descriptorSets[m_current_frame]);
                            }
                            break;
                            case ColoringMode::TextureMapping:
                            {
                                vertex_buffers.emplace_back(model->buffers.at(lvk_tidy::GetRequiredValue(primitive->normal))->buffer);
                                vertex_buffers.emplace_back(model->buffers.at(lvk_tidy::GetRequiredValue(primitive->texCoord))->buffer);
                                offsets.emplace_back(0);
                                offsets.emplace_back(0);

                                pipeline        = m_context->pipelines.at(PipelineType::TcPl)->pipeline;
                                pipeline_layout = m_context->pipelines.at(PipelineType::TcPl)->pipelineLayout;

                                auto& rm_factor = lvk_tidy::GetRequiredValue(primitive->material->pbrMetallicRoughness->roughnessMetallicFactor);
                                descriptor_sets.emplace_back(rm_factor->descriptorSets->descriptorSets[m_current_frame]);

                                auto& base_texture = lvk_tidy::GetRequiredValue(primitive->material->pbrMetallicRoughness->baseColorTexture);
                                auto& texture      = model->textures.at(base_texture);
                                descriptor_sets.emplace_back(texture->descriptorSets->descriptorSets[m_current_frame]);
                            }
                            break;
                            default:
                                break;
                        }
                        break;
                    default:
                        break;
                }

                vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

                vkCmdBindDescriptorSets(
                    command_buffer,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    pipeline_layout,
                    0,
                    static_cast<uint32_t>(descriptor_sets.size()),
                    descriptor_sets.data(),
                    0,
                    nullptr
                );

                auto aspect = static_cast<float>(m_swap_chain_extent.width) / static_cast<float>(m_swap_chain_extent.height);

                PushConstantVP pc {
                    .view = glm::lookAt(m_eye_pos, m_look_at, m_view_up),
                    .proj = glm::perspective(glm::radians(45.F), aspect, 0.1F, 100.F),
                };
                pc.proj[1][1] *= -1;
                vkCmdPushConstants(command_buffer, pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstantVP), &pc);

                if (LightingMode::None != primitive->lightingMode)
                {
                    PushConstantCamPos pc_cam_pos {.position = m_eye_pos};
                    vkCmdPushConstants(
                        command_buffer, pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(PushConstantVP), sizeof(PushConstantCamPos), &pc_cam_pos
                    );
                }

                vkCmdBindVertexBuffers(command_buffer, 0, static_cast<uint32_t>(vertex_buffers.size()), vertex_buffers.data(), offsets.data());
                vkCmdBindIndexBuffer(command_buffer, model->buffers.at(primitive->index)->buffer, 0, primitive->indexType);
                vkCmdDrawIndexed(command_buffer, primitive->indexCount, 1, 0, 0, 0);
            }
        }

        for (const auto& child : node->children)
        {
            DrawNode(model, command_buffer, child);
        }
    }

    glm::mat4 GetNodeMatrix(Node* node) const
    {
        auto matrix = node->GetLocalMatrix();
        auto parent = node->parent;
        while (parent)
        {
            matrix = GetNodeMatrix(parent) * matrix;
            parent = parent->parent;
        }
        return matrix;
    }

    Node* FindNodeForIndex(int index, Node* node) const
    {
        if (node->index == index)
        {
            return node;
        }

        for (const auto& child : node->children)
        {
            return FindNodeForIndex(index, child.get());
        }

        return nullptr;
    }

    void UpdateNodeJoint(const std::unique_ptr<Scene>& /*scene*/, const std::unique_ptr<Node>& node) const
    {
        if (node->skin)
        {
            auto& joints = node->skin->joints;
            std::vector<glm::mat4> joint_matrices(joints.size());
            auto inverse_transform = glm::inverse(GetNodeMatrix(node.get()));

            for (size_t i = 0; i < joints.size(); ++i)
            {
                auto temp_node    = FindNodeForIndex(joints[i], node.get());
                joint_matrices[i] = GetNodeMatrix(temp_node) * node->skin->inverseBindMatrices[(i + 1) % 2];
                joint_matrices[i] = inverse_transform * joint_matrices[i];
            }

            node->skin->matrix->UpdateUniform(m_current_frame, joint_matrices.data(), sizeof(glm::mat4) * joint_matrices.size());
        }
    }

    void UpdateNodeAnimation(const std::unique_ptr<Model>& model, const std::unique_ptr<Node>& node) const noexcept
    {
        for (const auto& animation : model->animations)
        {
            for (const auto& channel : animation->channels)
            {
                auto temp_node = FindNodeForIndex(channel->node, node.get());

                const auto& input  = animation->samplers[channel->sampler]->input;
                const auto& output = animation->samplers[channel->sampler]->output;

                auto time = glfwGetTime() - model->attributes.animationStartTime;
                auto t    = static_cast<float>(std::fmod(time, input.back() - input.front()));

                // 蒙皮
                if (temp_node && channel->path == "rotation" && temp_node->matrices.contains("rotation"))
                {
                    for (size_t i = 1; i < input.size(); ++i)
                    {
                        if (input[i] > t)
                        {
                            auto q1 = glm::make_quat(output.data() + (i - 1) * 4);
                            auto q2 = glm::make_quat(output.data() + i * 4);

                            auto a = (t - input[i - 1]) / (input[i] - input[i - 1]);

                            temp_node->matrices["rotation"] = glm::toMat4(glm::normalize(glm::slerp(q1, q2, a)));
                            break;
                        }
                    }
                }

                if (temp_node && channel->path == "translation" && temp_node->matrices.contains("translation"))
                {
                    for (size_t i = 1; i < input.size(); ++i)
                    {
                        if (input[i] > t)
                        {
                            auto t1 = glm::make_vec3(output.data() + (i - 1) * 3);
                            auto t2 = glm::make_vec3(output.data() + i * 3);

                            auto a = (t - input[i - 1]) / (input[i] - input[i - 1]);

                            auto translation = glm::mix(t1, t2, a);

                            temp_node->matrices["translation"] = glm::translate(glm::mat4(1.F), translation);
                            break;
                        }
                    }
                }

                // 变形
                if (!node->skin && node->mesh)
                {
                    // form.
                    std::array<float, 2> weights {};
                    for (size_t i = 1; i < input.size(); ++i)
                    {
                        if (input[i] > t)
                        {
                            weights[0] = static_cast<float>(
                                output[(i - 1) * 2] + (t - input[i - 1]) / (input[i] - input[i - 1]) * (output[i * 2] - output[(i - 1) * 2])
                            );
                            weights[1] = static_cast<float>(
                                output[(i - 1) * 2 + 1]
                                + (t - input[i - 1]) / (input[i] - input[i - 1]) * (output[i * 2 + 1] - output[(i - 1) * 2 + 1])
                            );
                            break;
                        }
                    }

                    for (const auto& primitive : node->mesh->primitives)
                    {
                        lvk_tidy::GetRequiredValue(primitive->animationWeight)->UpdateUniform(m_current_frame, weights.data(), sizeof(float) * 2);
                    }
                }
            }
        }

        for (const auto& child : node->children)
        {
            UpdateNodeAnimation(model, child);
        }
    }

    void DrawGLTFModel(const std::unique_ptr<Model>& model, VkCommandBuffer command_buffer) const noexcept
    {
        if (!model->animations.empty() && model->attributes.animation)
        {
            for (const auto& scene : model->scenes)
            {
                for (const auto& node : scene->nodes)
                {
                    // 先更新动画再更新蒙皮（动画会更新所有节点的变换数据，蒙皮需要根据这些数据构造 GLSL 中使用的数据）
                    UpdateNodeAnimation(model, node);
                    UpdateNodeJoint(scene, node);
                }
            }
        }

        for (const auto& scene : model->scenes)
        {
            for (const auto& node : scene->nodes)
            {
                DrawNode(model, command_buffer, node);
            }
        }
    }

    std::unique_ptr<Image> CreateTextureImage(const void* data_pointer, size_t data_size, uint32_t width, uint32_t height, VkFormat format)
    {
        auto image = std::make_unique<Image>();

        VkBuffer staging_buffer {};
        VkDeviceMemory staging_buffer_memory {};

        CreateBuffer(
            data_size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            staging_buffer,
            staging_buffer_memory
        );

        void* data {nullptr};
        vkMapMemory(m_device, staging_buffer_memory, 0, data_size, 0, &data);
        std::memcpy(data, data_pointer, data_size);
        vkUnmapMemory(m_device, staging_buffer_memory);

        CreateImage(
            width,
            height,
            format,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            image->image,
            image->imageMemory
        );

        TransitionImageLayout(image->image, format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        CopyBufferToImage(staging_buffer, image->image, width, height);
        TransitionImageLayout(image->image, format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        vkDestroyBuffer(m_device, staging_buffer, nullptr);
        vkFreeMemory(m_device, staging_buffer_memory, nullptr);

        image->imageView = CreateImageView(image->image, format, VK_IMAGE_ASPECT_COLOR_BIT);

        return image;
    }

    VkSampler CreateTextureSampler()
    {
        VkSampler sampler {};

        VkSamplerCreateInfo sampler_info {};
        sampler_info.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampler_info.magFilter               = VK_FILTER_LINEAR;
        sampler_info.minFilter               = VK_FILTER_LINEAR;
        sampler_info.addressModeU            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_info.addressModeV            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_info.addressModeW            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_info.anisotropyEnable        = VK_FALSE;
        sampler_info.maxAnisotropy           = 1.0;
        sampler_info.borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        sampler_info.unnormalizedCoordinates = VK_FALSE;
        sampler_info.compareEnable           = VK_FALSE;
        sampler_info.compareOp               = VK_COMPARE_OP_ALWAYS;
        sampler_info.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        sampler_info.mipLodBias              = 0.F;
        sampler_info.minLod                  = 0.F;
        sampler_info.maxLod                  = 0.F;

        if (VK_SUCCESS != vkCreateSampler(m_device, &sampler_info, nullptr, &sampler))
        {
            throw std::runtime_error("failed to create texture sampler");
        }

        return sampler;
    }

    void CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t widht, uint32_t height)
    {
        VkCommandBuffer command_buffer = BeginSingleTimeCommands();

        VkBufferImageCopy region {};
        region.bufferOffset                    = 0;
        region.bufferRowLength                 = 0;
        region.bufferImageHeight               = 0;
        region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel       = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount     = 1;
        region.imageOffset                     = {0, 0, 0};
        region.imageExtent                     = {widht, height, 1};

        vkCmdCopyBufferToImage(command_buffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        EndSingleTimeCommands(command_buffer);
    }

    void TransitionImageLayout(VkImage image, VkFormat /*format*/, VkImageLayout old_layout, VkImageLayout new_layout)
    {
        VkCommandBuffer command_buffer = BeginSingleTimeCommands();

        VkImageMemoryBarrier barrier {};
        barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout                       = old_layout;
        barrier.newLayout                       = new_layout;
        barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        barrier.image                           = image;
        barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel   = 0;
        barrier.subresourceRange.levelCount     = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount     = 1;
        barrier.srcAccessMask                   = 0;
        barrier.dstAccessMask                   = 0;

        // 指定变换屏障掩码
        VkPipelineStageFlags source_stage {};
        VkPipelineStageFlags destination_stage {};
        if (VK_IMAGE_LAYOUT_UNDEFINED == old_layout && VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL == new_layout)
        {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

            source_stage      = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destination_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        }
        else if (VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL == old_layout && VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL == new_layout)
        {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            source_stage      = VK_PIPELINE_STAGE_TRANSFER_BIT;
            destination_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
        else
        {
            throw std::invalid_argument("unsupported layout transition");
        }

        vkCmdPipelineBarrier(command_buffer, source_stage, destination_stage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        EndSingleTimeCommands(command_buffer);
    }

    VkCommandBuffer BeginSingleTimeCommands() const noexcept
    {
        VkCommandBufferAllocateInfo alloc_info = {};
        alloc_info.sType                       = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc_info.level                       = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.commandPool                 = m_command_pool;
        alloc_info.commandBufferCount          = 1;

        VkCommandBuffer command_buffer {};
        vkAllocateCommandBuffers(m_device, &alloc_info, &command_buffer);

        VkCommandBufferBeginInfo begin_info = {};
        begin_info.sType                    = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags                    = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; // 指定如何使用这个指令缓冲

        vkBeginCommandBuffer(command_buffer, &begin_info);
        return command_buffer;
    }

    void EndSingleTimeCommands(VkCommandBuffer command_buffer) const noexcept
    {
        vkEndCommandBuffer(command_buffer);

        VkSubmitInfo submit_info       = {};
        submit_info.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers    = &command_buffer;

        vkQueueSubmit(m_graphics_queue, 1, &submit_info, nullptr);
        vkQueueWaitIdle(m_graphics_queue);

        vkFreeCommandBuffers(m_device, m_command_pool, 1, &command_buffer);
    }

    /// @brief 创建顶点缓冲
    std::unique_ptr<Buffer> CreateVertexBuffer(const VkDeviceSize buffer_size, const void* data_pointer)
    {
        return CreateDrawableBuffer(buffer_size, data_pointer, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    }

    /// @brief 创建索引缓冲
    std::unique_ptr<Buffer> CreateIndexBuffer(const VkDeviceSize buffer_size, const void* data_pointer)
    {
        return CreateDrawableBuffer(buffer_size, data_pointer, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    }

    std::unique_ptr<Buffer> CreateDrawableBuffer(const VkDeviceSize buffer_size, const void* data_pointer, VkBufferUsageFlagBits usage)
    {
        auto buffer = std::make_unique<Buffer>();

        VkBuffer staging_buffer {};
        VkDeviceMemory staging_buffer_memory {};

        CreateBuffer(
            buffer_size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            staging_buffer,
            staging_buffer_memory
        );

        void* data {nullptr};
        vkMapMemory(m_device, staging_buffer_memory, 0, buffer_size, 0, &data);
        std::memcpy(data, data_pointer, buffer_size);
        vkUnmapMemory(m_device, staging_buffer_memory);

        CreateBuffer(
            buffer_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, buffer->buffer, buffer->bufferMemory
        );

        CopyBuffer(staging_buffer, buffer->buffer, buffer_size);

        vkDestroyBuffer(m_device, staging_buffer, nullptr);
        vkFreeMemory(m_device, staging_buffer_memory, nullptr);

        return buffer;
    }

    void ImguiNode(const std::unique_ptr<Node>& node, uint32_t node_index)
    {
        if (ImGui::TreeNode(("node " + std::to_string(node_index++)).c_str()))
        {
            if (node->mesh)
            {
                if (ImGui::TreeNode("Mesh"))
                {
                    uint32_t primitive_index {};
                    for (const auto& primitive : node->mesh->primitives)
                    {
                        if (ImGui::TreeNode(("primitive " + std::to_string(primitive_index++)).c_str()))
                        {
                            int coloring = static_cast<int>(primitive->coloringMode);
                            int lighting = static_cast<int>(primitive->lightingMode);
                            if (ImGui::TreeNode("Coloring"))
                            {
                                if (primitive->DirectColoring())
                                {
                                    ImGui::RadioButton("Direct", &coloring, static_cast<int>(ColoringMode::DirectRGB));
                                }
                                if (primitive->VertexColoring())
                                {
                                    ImGui::SameLine();
                                    ImGui::RadioButton("Vertex", &coloring, static_cast<int>(ColoringMode::VertexRGB));
                                }
                                if (primitive->CellColoring())
                                {
                                    ImGui::SameLine();
                                    ImGui::RadioButton("Cell", &coloring, static_cast<int>(ColoringMode::CellRGB));
                                }
                                if (primitive->TextureColoring())
                                {
                                    ImGui::SameLine();
                                    ImGui::RadioButton("Texture", &coloring, static_cast<int>(ColoringMode::TextureMapping));
                                }
                                ImGui::TreePop();
                            }
                            primitive->coloringMode = static_cast<ColoringMode>(coloring);

                            if (ImGui::TreeNode("Lighting"))
                            {
                                ImGui::RadioButton("None", &lighting, static_cast<int>(LightingMode::None));
                                if (primitive->Phong())
                                {
                                    ImGui::SameLine();
                                    ImGui::RadioButton("Phong", &lighting, static_cast<int>(LightingMode::Phong));
                                }
                                if (primitive->BlinnPhong())
                                {
                                    ImGui::SameLine();
                                    ImGui::RadioButton("BlinnPhong", &lighting, static_cast<int>(LightingMode::BlinnPhong));
                                }
                                if (primitive->Pbr())
                                {
                                    ImGui::SameLine();
                                    ImGui::RadioButton("PBR", &lighting, static_cast<int>(LightingMode::PBR));
                                }
                                ImGui::TreePop();
                            }
                            primitive->lightingMode = static_cast<LightingMode>(lighting);

                            ImGui::TreePop();
                        }
                    }

                    ImGui::TreePop();
                }
            }

            if (node->camera)
            {
                if (ImGui::TreeNode("Camera"))
                {
                    ImGui::TextUnformatted("TODO");
                    ImGui::TreePop();
                }
            }

            ImGui::TreePop();
        }

        for (const auto& child : node->children)
        {
            ImguiNode(child, node_index++);
        }
    }

    /// @brief 添加需要渲染的GUI控件
    void PrepareImGui()
    {
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(10, 10));
        ImGui::Begin("Display Infomation", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

        for (const auto& [name, model] : m_models)
        {
            std::filesystem::path path(name);
            if (ImGui::TreeNode(path.filename().string().c_str()))
            {
                ImGui::Checkbox("Display", &model->attributes.visibility);
                if (ImGui::Checkbox("Animation", &model->attributes.animation))
                {
                    model->attributes.animationStartTime = glfwGetTime();
                }

                uint32_t scene_index {};
                for (auto& scene : model->scenes)
                {
                    if (ImGui::TreeNode(("scene " + std::to_string(scene_index++)).c_str()))
                    {
                        uint32_t node_index {};
                        for (auto& node : scene->nodes)
                        {
                            ImguiNode(node, node_index++);
                        }

                        ImGui::TreePop();
                    }
                }

                if (ImGui::TreeNode("Infomation"))
                {
                    ImGui::TextUnformatted(model->attributes.infomation.c_str());
                    ImGui::TreePop();
                }

                ImGui::TreePop();
            }
        }

        ImGui::End();

        ImGui::Render();
    }

    /// @brief 创建 Vulkan 实例
    void CreateInstance()
    {
        if (k_enable_validation_layers && !CheckValidationLayerSupport())
        {
            throw std::runtime_error("validation layers requested, but not available");
        }

        // 应用程序的信息，这些信息可能会作为驱动程序的优化依据，让驱动做一些特殊的优化
        VkApplicationInfo app_info  = {};
        app_info.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app_info.pNext              = nullptr;
        app_info.pApplicationName   = "Hello Triangle";
        app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        app_info.pEngineName        = "No Engine";
        app_info.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
        app_info.apiVersion         = VK_API_VERSION_1_0;

        // 指定驱动程序需要使用的全局扩展和校验层，全局是指对整个应用程序都有效，而不仅仅是某一个设备
        VkInstanceCreateInfo create_info = {};
        create_info.sType                = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        create_info.pApplicationInfo     = &app_info;

        // 指定需要的全局扩展
        auto required_extensions            = GetRequiredExtensions();
        create_info.enabledExtensionCount   = static_cast<uint32_t>(required_extensions.size());
        create_info.ppEnabledExtensionNames = required_extensions.data();

        // 指定全局校验层
        if (k_enable_validation_layers)
        {
            create_info.enabledLayerCount   = static_cast<uint32_t>(k_validation_layers.size());
            create_info.ppEnabledLayerNames = k_validation_layers.data();

            VkDebugUtilsMessengerCreateInfoEXT debug_create_info = {};
            PopulateDebugMessengerCreateInfo(debug_create_info);

            create_info.pNext = &debug_create_info;
        }
        else
        {
            create_info.enabledLayerCount = 0;
            create_info.pNext             = nullptr;
        }

        // 创建 Vulkan 实例，用来初始化 Vulkan 库
        // 1.包含创建信息的结构体指针
        // 2.自定义的分配器回调函数
        // 3.指向实例句柄存储位置的指针
        if (VK_SUCCESS != vkCreateInstance(&create_info, nullptr, &m_instance))
        {
            throw std::runtime_error("failed to create instance");
        }
    }

    /// @brief 检查需要开启的校验层是否被支持
    /// @return
    bool CheckValidationLayerSupport() const noexcept
    {
        // 获取所有可用的校验层列表
        uint32_t layer_count = 0;
        vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
        std::vector<VkLayerProperties> available_layers(layer_count);
        vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());
        std::cout << "-------------------------------------------\n"
                  << "All available layers:\n";
        for (const auto& layer : available_layers)
        {
            std::cout << static_cast<const char*>(layer.layerName) << '\n';
        }

        // 检查需要开启的校验层是否可以在所有可用的校验层列表中找到
        for (const char* layer_name : k_validation_layers)
        {
            bool layer_found {false};

            for (const auto& layer_properties : available_layers)
            {
                if (0 == std::strcmp(layer_name, static_cast<const char*>(layer_properties.layerName)))
                {
                    layer_found = true;
                    break;
                }
            }

            if (!layer_found)
            {
                return false;
            }
        }

        return true;
    }

    /// @brief 获取所有需要开启的扩展
    /// @return
    std::vector<const char*> GetRequiredExtensions() const noexcept
    {
        // 获取 Vulkan 支持的所有扩展
        uint32_t extension_count = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);
        std::vector<VkExtensionProperties> extensions(extension_count);
        vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, extensions.data());
        std::cout << "-------------------------------------------\n"
                  << "All supported extensions:\n";
        for (const auto& e : extensions)
        {
            std::cout << static_cast<const char*>(e.extensionName) << '\n';
        }

        // Vulkan 是一个与平台无关的 API ，所以需要一个和窗口系统交互的扩展
        // 使用 glfw 获取这个扩展
        uint32_t glfw_extension_count = 0;
        const char** glfw_extensions  = glfwGetRequiredInstanceExtensions(&glfw_extension_count);
        std::cout << "-------------------------------------------\n"
                  << "GLFW extensions:\n";
        for (size_t i = 0; i < glfw_extension_count; ++i)
        {
            std::cout << glfw_extensions[i] << '\n';
        }

        // 将需要开启的所有扩展添加到列表并返回
        std::vector<const char*> required_extensions(glfw_extensions, glfw_extensions + glfw_extension_count);
        if (k_enable_validation_layers)
        {
            // 根据需要开启调试报告相关的扩展
            required_extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        return required_extensions;
    }

    /// @brief 设置回调函数来接受调试信息
    void SetupDebugCallback()
    {
        if (!k_enable_validation_layers)
        {
            return;
        }

        VkDebugUtilsMessengerCreateInfoEXT create_info = {};
        PopulateDebugMessengerCreateInfo(create_info);

        if (VK_SUCCESS != CreateDebugUtilsMessengerEXT(m_instance, &create_info, nullptr, &m_debug_messenger))
        {
            throw std::runtime_error("failed to set up debug callback");
        }
    }

    /// @brief 选择一个满足需求的物理设备（显卡）
    /// @details 可以选择任意数量的显卡并同时使用它们
    void PickPhysicalDevice()
    {
        // 获取支持 Vulkan 的显卡数量
        uint32_t device_count {0};
        vkEnumeratePhysicalDevices(m_instance, &device_count, nullptr);

        if (0 == device_count)
        {
            throw std::runtime_error("failed to find GPUs with Vulkan support");
        }

        std::vector<VkPhysicalDevice> devices(device_count);
        vkEnumeratePhysicalDevices(m_instance, &device_count, devices.data());

        for (const auto& device : devices)
        {
            if (IsDeviceSuitable(device))
            {
                m_physical_device = device;
                break;
            }
        }

        if (nullptr == m_physical_device)
        {
            throw std::runtime_error("failed to find a suitable GPU");
        }
    }

    /// @brief 检查显卡是否满足需求
    /// @param device
    /// @return
    bool IsDeviceSuitable(VkPhysicalDevice device) const noexcept
    {
        // 获取基本的设置属性，name、type以及Vulkan版本等等
        // VkPhysicalDeviceProperties deviceProperties;
        // vkGetPhysicalDeviceProperties(device, &deviceProperties);

        // 获取对纹理的压缩、64位浮点数和多视图渲染等可选功能的支持
        // VkPhysicalDeviceFeatures deviceFeatures;
        // vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

        auto indices              = FindQueueFamilies(device);
        auto extensions_supported = CheckDeviceExtensionSupported(device);

        bool swap_chain_adequate {false};
        if (extensions_supported)
        {
            auto swap_chain_support = QuerySwapChainSupport(device);
            swap_chain_adequate     = !swap_chain_support.foramts.empty() & !swap_chain_support.presentModes.empty();
        }

        return indices.IsComplete() && extensions_supported && swap_chain_adequate;
    }

    /// @brief 查找满足需求的队列族
    /// @details 不同的队列族支持不同的类型的指令，例如计算、内存传输、绘图等指令
    /// @param device
    /// @return
    QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device) const noexcept
    {
        QueueFamilyIndices indices;

        // 获取物理设备支持的队列族列表
        uint32_t queue_family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);
        std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());

        for (uint32_t i = 0; i < queue_families.size(); ++i)
        {
            // 图形队列族
            if (queue_families.at(i).queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                indices.graphicsFamily = i;
            }

            // 呈现队列族
            VkBool32 present_support = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &present_support);

            if (present_support)
            {
                indices.presentFamily = i;
            }

            if (indices.IsComplete())
            {
                break;
            }
        }

        return indices;
    }

    /// @brief 创建逻辑设备作为和物理设备交互的接口
    void CreateLogicalDevice()
    {
        auto indices = FindQueueFamilies(m_physical_device);

        std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
        std::set<uint32_t> unique_queue_families {
            lvk_tidy::GetRequiredValue(indices.graphicsFamily), lvk_tidy::GetRequiredValue(indices.presentFamily)
        };

        // 控制指令缓存执行顺序的优先级，即使只有一个队列也要显示指定优先级，范围：[0.0, 1.0]
        float queue_priority {1.F};
        for (auto queue_family : unique_queue_families)
        {
            // 描述队列簇中预要申请使用的队列数量
            // 当前可用的驱动程序所提供的队列簇只允许创建少量的队列，并且很多时候没有必要创建多个队列
            // 因为可以在多个线程上创建所有命令缓冲区，然后在主线程一次性的以较低开销的调用提交队列
            VkDeviceQueueCreateInfo queue_create_info {};
            queue_create_info.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queue_create_info.queueFamilyIndex = queue_family;
            queue_create_info.queueCount       = 1;
            queue_create_info.pQueuePriorities = &queue_priority;
            queue_create_infos.push_back(queue_create_info);
        }

        // 指定应用程序使用的设备特性（例如几何着色器）
        VkPhysicalDeviceFeatures device_features = {};

        VkDeviceCreateInfo create_info      = {};
        create_info.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        create_info.queueCreateInfoCount    = static_cast<uint32_t>(queue_create_infos.size());
        create_info.pQueueCreateInfos       = queue_create_infos.data();
        create_info.pEnabledFeatures        = &device_features;
        create_info.enabledExtensionCount   = static_cast<uint32_t>(k_device_extensions.size());
        create_info.ppEnabledExtensionNames = k_device_extensions.data();

        // 根据需要对设备和 Vulkan 实例使用相同的校验层
        if (k_enable_validation_layers)
        {
            create_info.enabledLayerCount   = static_cast<uint32_t>(k_validation_layers.size());
            create_info.ppEnabledLayerNames = k_validation_layers.data();
        }
        else
        {
            create_info.enabledLayerCount = 0;
        }

        // 创建逻辑设备
        if (VK_SUCCESS != vkCreateDevice(m_physical_device, &create_info, nullptr, &m_device))
        {
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
    void CreateSurface()
    {
        // Vulkan 不能直接与窗口系统进行交互（是一个与平台特性无关的API集合），surface是 Vulkan 与窗体系统的连接桥梁
        // 需要在instance创建之后立即创建窗体surface，因为它会影响物理设备的选择
        // 窗体surface本身对于 Vulkan 也是非强制的，不需要同 OpenGL 一样必须要创建窗体surface
        if (VK_SUCCESS != glfwCreateWindowSurface(m_instance, m_window, nullptr, &m_surface))
        {
            throw std::runtime_error("failed to create window surface");
        }
    }

    /// @brief 检测所需的扩展是否支持
    /// @param device
    /// @return
    bool CheckDeviceExtensionSupported(VkPhysicalDevice device) const noexcept
    {
        uint32_t extension_count {0};
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr);
        std::vector<VkExtensionProperties> available_extensions(extension_count);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, available_extensions.data());

        std::set<std::string> required_extensions(std::begin(k_device_extensions), std::end(k_device_extensions));

        for (const auto& extension : available_extensions)
        {
            required_extensions.erase(static_cast<const char*>(extension.extensionName));
        }

        // 如果为空，则支持
        return required_extensions.empty();
    }

    /// @brief 查询交换链支持的细节信息
    /// @param device
    /// @return
    SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device) const noexcept
    {
        SwapChainSupportDetails details;

        // 查询基础表面特性
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_surface, &details.capabilities);

        // 查询表面支持的格式，确保集合对于所有有效的格式可扩充
        uint32_t format_count {0};
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &format_count, nullptr);
        if (0 != format_count)
        {
            details.foramts.resize(format_count);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &format_count, details.foramts.data());
        }

        // 查询表面支持的呈现模式
        uint32_t present_mode_count {0};
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &present_mode_count, nullptr);
        if (0 != present_mode_count)
        {
            details.presentModes.resize(present_mode_count);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &present_mode_count, details.presentModes.data());
        }

        return details;
    }

    /// @brief 选择合适的表面格式
    /// @param availableFormats
    /// @return
    VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& available_formats) const noexcept
    {
        // VkSurfaceFormatKHR 结构都包含一个 format 和一个 colorSpace 成员
        // format 成员变量指定色彩通道和类型
        // colorSpace 成员描述 SRGB 颜色空间是否通过 VK_COLOR_SPACE_SRGB_NONLINEAR_KHR 标志支持
        // 在较早版本的规范中，这个标志名为 VK_COLORSPACE_SRGB_NONLINEAR_KHR

        // 表面没有自己的首选格式，直接返回指定的格式
        if (1 == available_formats.size() && available_formats.front().format == VK_FORMAT_UNDEFINED)
        {
            return {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
        }

        // 遍历格式列表，如果想要的格式存在则直接返回
        for (const auto& available_format : available_formats)
        {
            if (VK_FORMAT_B8G8R8A8_UNORM == available_format.format && VK_COLOR_SPACE_SRGB_NONLINEAR_KHR == available_format.colorSpace)
            {
                return available_format;
            }
        }

        // 如果以上两种方式都失效，通过“优良”进行打分排序，大多数情况下会选择第一个格式作为理想的选择
        return available_formats.front();
    }

    /// @brief 查找最佳的可用呈现模式，模式是非常重要的，因为它代表了在屏幕呈现图像的条件
    /// @param availablePresentModes
    /// @return
    VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& available_present_modes) const noexcept
    {
        // 以下4种模式可以使用
        // 1.VK_PRESENT_MODE_IMMEDIATE_KHR: 应用程序提交的图像被立即传输到屏幕呈现，这种模式可能会造成撕裂效果。
        // 2.VK_PRESENT_MODE_FIFO_KHR:
        // 交换链被看作一个队列，当显示内容需要刷新的时候，显示设备从队列的前面获取图像，并且程序将渲染完成的图像插入队列的后面。
        // 如果队列是满的程序会等待。这种规模与视频游戏的垂直同步很类似。显示设备的刷新时刻被成为“垂直中断”。
        // 3.VK_PRESENT_MODE_FIFO_RELAXED_KHR: 该模式与上一个模式略有不同的地方为，如果应用程序存在延迟，即接受最后一个垂直同步信号时队列空了，
        // 将不会等待下一个垂直同步信号，而是将图像直接传送。这样做可能导致可见的撕裂效果。
        // 4.VK_PRESENT_MODE_MAILBOX_KHR: 这是第二种模式的变种。当交换链队列满的时候，选择新的替换旧的图像，从而替代阻塞应用程序的情形。
        // 这种模式通常用来实现三重缓冲区，与标准的垂直同步双缓冲相比，它可以有效避免延迟带来的撕裂效果。

        VkPresentModeKHR best_mode = VK_PRESENT_MODE_FIFO_KHR;

        for (const auto& available_present_mode : available_present_modes)
        {
            if (VK_PRESENT_MODE_MAILBOX_KHR == available_present_mode)
            {
                return available_present_mode;
            }
            if (VK_PRESENT_MODE_IMMEDIATE_KHR == available_present_mode)
            {
                best_mode = available_present_mode;
            }
        }

        return best_mode;
    }

    /// @brief 设置交换范围，交换范围是交换链中图像的分辨率
    /// @param capabilities
    /// @return
    VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) const noexcept
    {
        // 如果是最大值则表示允许我们自己选择对于窗口最合适的交换范围
        if (std::numeric_limits<uint32_t>::max() != capabilities.currentExtent.width)
        {
            return capabilities.currentExtent;
        }

        int width {0};

        int height {0};
        glfwGetFramebufferSize(m_window, &width, &height);

        // 使用GLFW窗口的大小来设置交换链中图像的分辨率
        VkExtent2D actual_extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};

        actual_extent.width  = std::clamp(actual_extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actual_extent.height = std::clamp(actual_extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return actual_extent;
    }

    /// @brief 创建交换链
    void CreateSwapChain()
    {
        SwapChainSupportDetails swap_chain_support = QuerySwapChainSupport(m_physical_device);
        VkSurfaceFormatKHR surface_format          = ChooseSwapSurfaceFormat(swap_chain_support.foramts);
        VkPresentModeKHR present_mode              = ChooseSwapPresentMode(swap_chain_support.presentModes);
        VkExtent2D extent                          = ChooseSwapExtent(swap_chain_support.capabilities);

        // 交换链中的图像数量，可以理解为队列的长度，指定运行时图像的最小数量
        // maxImageCount数值为0代表除了内存之外没有限制
        m_min_image_count = swap_chain_support.capabilities.minImageCount + 1;
        if (swap_chain_support.capabilities.maxImageCount > 0 && m_min_image_count > swap_chain_support.capabilities.maxImageCount)
        {
            m_min_image_count = swap_chain_support.capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR create_info = {};
        create_info.sType                    = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        create_info.surface                  = m_surface;
        create_info.minImageCount            = m_min_image_count;
        create_info.imageFormat              = surface_format.format;
        create_info.imageColorSpace          = surface_format.colorSpace;
        create_info.imageExtent              = extent;
        create_info.imageArrayLayers         = 1;                                   // 图像包含的层次，通常为1，3d图像大于1
        create_info.imageUsage               = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // 指定在图像上进行怎样的操作，比如显示（颜色附件）、后处理等

        auto indices = FindQueueFamilies(m_physical_device);
        const std::array<uint32_t, 2> queue_family_indices {
            static_cast<uint32_t>(lvk_tidy::GetRequiredValue(indices.graphicsFamily)),
            static_cast<uint32_t>(lvk_tidy::GetRequiredValue(indices.presentFamily))
        };

        // 在多个队列族使用交换链图像的方式
        if (indices.graphicsFamily != indices.presentFamily)
        {
            // 图形队列和呈现队列不是同一个队列
            // VK_SHARING_MODE_CONCURRENT 表示图像可以在多个队列族间使用，不需要显式地改变图像所有权
            create_info.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
            create_info.queueFamilyIndexCount = 2;
            create_info.pQueueFamilyIndices   = queue_family_indices.data();
        }
        else
        {
            // VK_SHARING_MODE_EXCLUSIVE 表示一张图像同一时间只能被一个队列族所拥有，这种模式性能最佳
            create_info.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE;
            create_info.queueFamilyIndexCount = 0;
            create_info.pQueueFamilyIndices   = nullptr;
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

        if (VK_SUCCESS != vkCreateSwapchainKHR(m_device, &create_info, nullptr, &m_swap_chain))
        {
            throw std::runtime_error("failed to create swap chain");
        }

        // 获取交换链中图像，图像会在交换链销毁的同时自动清理
        // 之前给VkSwapchainCreateInfoKHR 设置了期望的图像数量，但是实际运行允许创建更多的图像数量，因此需要重新获取数量
        vkGetSwapchainImagesKHR(m_device, m_swap_chain, &m_min_image_count, nullptr);
        m_swap_chain_images.resize(m_min_image_count);
        vkGetSwapchainImagesKHR(m_device, m_swap_chain, &m_min_image_count, m_swap_chain_images.data());

        m_swap_chain_image_format = surface_format.format;
        m_swap_chain_extent       = extent;
    }

    /// @brief 创建图像视图
    /// @details 任何 VkImage 对象都需要通过 VkImageView 来绑定访问，包括处于交换链中的、处于渲染管线中的
    void CreateImageViews()
    {
        m_swap_chain_image_views.resize(m_swap_chain_images.size());

        for (size_t i = 0; i < m_swap_chain_images.size(); ++i)
        {
            VkImageViewCreateInfo create_info           = {};
            create_info.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            create_info.image                           = m_swap_chain_images.at(i);
            create_info.viewType                        = VK_IMAGE_VIEW_TYPE_2D;         // 1d 2d 3d cube纹理
            create_info.format                          = m_swap_chain_image_format;
            create_info.components.r                    = VK_COMPONENT_SWIZZLE_IDENTITY; // 图像颜色通过的映射
            create_info.components.g                    = VK_COMPONENT_SWIZZLE_IDENTITY;
            create_info.components.b                    = VK_COMPONENT_SWIZZLE_IDENTITY;
            create_info.components.a                    = VK_COMPONENT_SWIZZLE_IDENTITY;
            create_info.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT; // 指定图像的用途，图像的哪一部分可以被访问
            create_info.subresourceRange.baseMipLevel   = 0;
            create_info.subresourceRange.levelCount     = 1;
            create_info.subresourceRange.baseArrayLayer = 0;
            create_info.subresourceRange.layerCount     = 1;

            if (VK_SUCCESS != vkCreateImageView(m_device, &create_info, nullptr, &m_swap_chain_image_views.at(i)))
            {
                throw std::runtime_error("failed to create image views");
            }
        }
    }

    void CreatePipelines()
    {
        m_context->pipelines.try_emplace(
            PipelineType::DcNl,
            CreateGraphicsPipeline(
                PipelineType::DcNl, PROJECT_ASSETS_DIR "shaders/01_06_dc_nl_vert.spv", PROJECT_ASSETS_DIR "shaders/01_06_dc_nl_frag.spv"
            )
        );

        m_context->pipelines.try_emplace(
            PipelineType::DcPl,
            CreateGraphicsPipeline(
                PipelineType::DcPl, PROJECT_ASSETS_DIR "shaders/01_06_dc_pl_vert.spv", PROJECT_ASSETS_DIR "shaders/01_06_dc_pl_frag.spv"
            )
        );

        m_context->pipelines.try_emplace(
            PipelineType::TcNl,
            CreateGraphicsPipeline(
                PipelineType::TcNl, PROJECT_ASSETS_DIR "shaders/01_06_tc_nl_vert.spv", PROJECT_ASSETS_DIR "shaders/01_06_tc_nl_frag.spv"
            )
        );

        m_context->pipelines.try_emplace(
            PipelineType::TcPl,
            CreateGraphicsPipeline(
                PipelineType::TcPl, PROJECT_ASSETS_DIR "shaders/01_06_tc_pl_vert.spv", PROJECT_ASSETS_DIR "shaders/01_06_tc_pl_frag.spv"
            )
        );

        m_context->pipelines.try_emplace(
            PipelineType::DcNlAW2,
            CreateGraphicsPipeline(
                PipelineType::DcNlAW2, PROJECT_ASSETS_DIR "shaders/01_06_dc_nl_aw2_vert.spv", PROJECT_ASSETS_DIR "shaders/01_06_dc_nl_aw2_frag.spv"
            )
        );

        m_context->pipelines.try_emplace(
            PipelineType::DcNlAS2,
            CreateGraphicsPipeline(
                PipelineType::DcNlAS2, PROJECT_ASSETS_DIR "shaders/01_06_dc_nl_as2_vert.spv", PROJECT_ASSETS_DIR "shaders/01_06_dc_nl_as2_frag.spv"
            )
        );
    }

    /// @brief 创建图形管线
    /// @details 在 Vulkan 中几乎不允许对图形管线进行动态设置，也就意味着每一种状态都需要提前创建一个图形管线
    std::unique_ptr<Pipeline> CreateGraphicsPipeline(PipelineType pipeline_type, const std::string& vert_path, const std::string& frag_path)
    {
        auto pipeline = std::make_unique<Pipeline>();

        auto vert_shader_code = ReadFile(vert_path);
        auto frag_shader_code = ReadFile(frag_path);

        VkShaderModule vert_shader_module = CreateShaderModule(vert_shader_code);
        VkShaderModule frag_shader_module = CreateShaderModule(frag_shader_code);

        VkPipelineShaderStageCreateInfo vert_shader_stage_info = {};
        vert_shader_stage_info.sType                           = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vert_shader_stage_info.stage                           = VK_SHADER_STAGE_VERTEX_BIT;
        vert_shader_stage_info.module                          = vert_shader_module;
        vert_shader_stage_info.pName                           = "main";  // 指定调用的着色器函数，同一份代码可以实现多个着色器
        vert_shader_stage_info.pSpecializationInfo             = nullptr; // 设置着色器常量

        VkPipelineShaderStageCreateInfo frag_shader_stage_info = {};
        frag_shader_stage_info.sType                           = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        frag_shader_stage_info.stage                           = VK_SHADER_STAGE_FRAGMENT_BIT;
        frag_shader_stage_info.module                          = frag_shader_module;
        frag_shader_stage_info.pName                           = "main";
        frag_shader_stage_info.pSpecializationInfo             = nullptr;

        auto binding_descriptions   = Vertex::GetBindingDescriptions(pipeline_type);
        auto attribute_descriptions = Vertex::GetAttributeDescriptions(pipeline_type);

        // 顶点信息
        VkPipelineVertexInputStateCreateInfo vertex_input_info = {};
        vertex_input_info.sType                                = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertex_input_info.vertexBindingDescriptionCount        = static_cast<uint32_t>(binding_descriptions.size());
        vertex_input_info.pVertexBindingDescriptions           = binding_descriptions.data();
        vertex_input_info.vertexAttributeDescriptionCount      = static_cast<uint32_t>(attribute_descriptions.size());
        vertex_input_info.pVertexAttributeDescriptions         = attribute_descriptions.data();

        // 拓扑信息
        VkPipelineInputAssemblyStateCreateInfo input_assembly = {};
        input_assembly.sType                                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        input_assembly.topology                               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; // 指定绘制的图元类型：点、线、三角形
        input_assembly.primitiveRestartEnable                 = VK_FALSE;

        // 视口
        VkViewport viewport = {};
        viewport.x          = 0.F;
        viewport.y          = 0.F;
        viewport.width      = static_cast<float>(m_swap_chain_extent.width);
        viewport.height     = static_cast<float>(m_swap_chain_extent.height);
        viewport.minDepth   = 0.F; // 深度值范围，必须在 [0.f, 1.f]之间
        viewport.maxDepth   = 1.F;

        // 裁剪
        VkRect2D scissor = {};
        scissor.offset   = {0, 0};
        scissor.extent   = m_swap_chain_extent;

        VkPipelineViewportStateCreateInfo viewport_state = {};
        viewport_state.sType                             = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport_state.viewportCount                     = 1;
        viewport_state.pViewports                        = &viewport;
        viewport_state.scissorCount                      = 1;
        viewport_state.pScissors                         = &scissor;

        // 光栅化
        VkPipelineRasterizationStateCreateInfo rasterizer = {};
        rasterizer.sType                                  = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable                       = VK_FALSE;
        rasterizer.rasterizerDiscardEnable                = VK_FALSE;                        // 设置为true会禁止一切片段输出到帧缓冲
        rasterizer.polygonMode                            = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth                              = 1.F;
        rasterizer.cullMode                               = VK_CULL_MODE_NONE;               // 表面剔除类型，正面、背面、双面剔除
        rasterizer.frontFace                              = VK_FRONT_FACE_COUNTER_CLOCKWISE; // 指定顺时针的顶点序是正面还是反面
        rasterizer.depthBiasEnable                        = VK_FALSE;
        rasterizer.depthBiasConstantFactor                = 1.F;
        rasterizer.depthBiasClamp                         = 0.F;
        rasterizer.depthBiasSlopeFactor                   = 0.F;

        // 多重采样
        VkPipelineMultisampleStateCreateInfo multisampling = {};
        multisampling.sType                                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable                  = VK_FALSE;
        multisampling.rasterizationSamples                 = VK_SAMPLE_COUNT_1_BIT;
        multisampling.minSampleShading                     = 1.F;
        multisampling.pSampleMask                          = nullptr;
        multisampling.alphaToCoverageEnable                = VK_FALSE;
        multisampling.alphaToOneEnable                     = VK_FALSE;

        // 深度和模板测试
        VkPipelineDepthStencilStateCreateInfo depth_stencil = {};
        depth_stencil.sType                                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depth_stencil.depthTestEnable                       = VK_TRUE;            // 是否启用深度测试
        depth_stencil.depthWriteEnable                      = VK_TRUE;            // 深度测试通过后是否写入深度缓冲
        depth_stencil.depthCompareOp                        = VK_COMPARE_OP_LESS; // 深度值比较方式
        depth_stencil.depthBoundsTestEnable                 = VK_FALSE;           // 指定可选的深度范围测试
        depth_stencil.minDepthBounds                        = 0.F;
        depth_stencil.maxDepthBounds                        = 1.F;
        depth_stencil.stencilTestEnable                     = VK_FALSE;           // 模板测试
        depth_stencil.front                                 = {};
        depth_stencil.back                                  = {};

        // 颜色混合，可以对指定帧缓冲单独设置，也可以设置全局颜色混合方式
        VkPipelineColorBlendAttachmentState color_blend_attachment {};
        color_blend_attachment.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        color_blend_attachment.blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo color_blending {};
        color_blending.sType             = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        color_blending.logicOpEnable     = VK_FALSE;
        color_blending.logicOp           = VK_LOGIC_OP_COPY;
        color_blending.attachmentCount   = 1;
        color_blending.pAttachments      = &color_blend_attachment;
        color_blending.blendConstants[0] = 0.0F;
        color_blending.blendConstants[1] = 0.0F;
        color_blending.blendConstants[2] = 0.0F;
        color_blending.blendConstants[3] = 0.0F;

        // 动态状态，视口大小、线宽、混合常量等
        std::vector<VkDynamicState> dynamic_states = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamic_state {};
        dynamic_state.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamic_state.dynamicStateCount = static_cast<uint32_t>(dynamic_states.size());
        dynamic_state.pDynamicStates    = dynamic_states.data();

        std::vector<VkPushConstantRange> push_constant_ranges {};

        VkPushConstantRange push_constant_range_vp = {};
        push_constant_range_vp.stageFlags          = VK_SHADER_STAGE_VERTEX_BIT;
        push_constant_range_vp.offset              = 0;
        push_constant_range_vp.size                = sizeof(PushConstantVP);

        push_constant_ranges.emplace_back(push_constant_range_vp);

        std::vector<VkDescriptorSetLayout> descriptor_set_layouts {};
        descriptor_set_layouts.emplace_back(m_context->descriptorSetLayouts.at("uniform_model")->descriptorSetLayout);

        switch (pipeline_type)
        {
            case PipelineType::DcNl:
            {
                descriptor_set_layouts.emplace_back(m_context->descriptorSetLayouts.at("uniform_color")->descriptorSetLayout);
            }
            break;
            case PipelineType::TcNl:
            {
                descriptor_set_layouts.emplace_back(m_context->descriptorSetLayouts.at("uniform_sampler")->descriptorSetLayout);
            }
            break;
            case PipelineType::DcPl:
            {
                VkPushConstantRange push_constant_range_cam_pos = {};
                push_constant_range_cam_pos.stageFlags          = VK_SHADER_STAGE_FRAGMENT_BIT;
                push_constant_range_cam_pos.offset              = sizeof(PushConstantVP);
                push_constant_range_cam_pos.size                = sizeof(PushConstantCamPos);

                push_constant_ranges.emplace_back(push_constant_range_cam_pos);

                descriptor_set_layouts.emplace_back(
                    m_context->descriptorSetLayouts.at("uniform_roughnessFactor_metallicFactor")->descriptorSetLayout
                );
                descriptor_set_layouts.emplace_back(m_context->descriptorSetLayouts.at("uniform_color")->descriptorSetLayout);
            }
            break;
            case PipelineType::TcPl:
            {
                VkPushConstantRange push_constant_range_cam_pos = {};
                push_constant_range_cam_pos.stageFlags          = VK_SHADER_STAGE_FRAGMENT_BIT;
                push_constant_range_cam_pos.offset              = sizeof(PushConstantVP);
                push_constant_range_cam_pos.size                = sizeof(PushConstantCamPos);

                push_constant_ranges.emplace_back(push_constant_range_cam_pos);

                descriptor_set_layouts.emplace_back(
                    m_context->descriptorSetLayouts.at("uniform_roughnessFactor_metallicFactor")->descriptorSetLayout
                );
                descriptor_set_layouts.emplace_back(m_context->descriptorSetLayouts.at("uniform_sampler")->descriptorSetLayout);
            }
            break;
            case PipelineType::DcNlAW2:
            {
                descriptor_set_layouts.emplace_back(m_context->descriptorSetLayouts.at("uniform_animation_morph")->descriptorSetLayout);
                descriptor_set_layouts.emplace_back(m_context->descriptorSetLayouts.at("uniform_color")->descriptorSetLayout);
            }
            break;
            case PipelineType::DcNlAS2:
            {
                descriptor_set_layouts.emplace_back(m_context->descriptorSetLayouts.at("uniform_animation_skin")->descriptorSetLayout);
                descriptor_set_layouts.emplace_back(m_context->descriptorSetLayouts.at("uniform_color")->descriptorSetLayout);
            }
            break;
            default:
                break;
        }

        // 管线布局，在着色器中使用 uniform 变量
        VkPipelineLayoutCreateInfo pipeline_layout_info {};
        pipeline_layout_info.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_layout_info.setLayoutCount         = static_cast<uint32_t>(descriptor_set_layouts.size());
        pipeline_layout_info.pSetLayouts            = descriptor_set_layouts.data();
        pipeline_layout_info.pushConstantRangeCount = static_cast<uint32_t>(push_constant_ranges.size());
        pipeline_layout_info.pPushConstantRanges    = push_constant_ranges.data();

        if (VK_SUCCESS != vkCreatePipelineLayout(m_device, &pipeline_layout_info, nullptr, &pipeline->pipelineLayout))
        {
            throw std::runtime_error("failed to create pipeline layout");
        }

        const std::array<VkPipelineShaderStageCreateInfo, 2> shader_stages {vert_shader_stage_info, frag_shader_stage_info};

        // 完整的图形管线包括：着色器阶段、固定功能状态、管线布局、渲染流程
        VkGraphicsPipelineCreateInfo pipeline_info = {};
        pipeline_info.sType                        = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeline_info.stageCount                   = 2;
        pipeline_info.pStages                      = shader_stages.data();
        pipeline_info.pVertexInputState            = &vertex_input_info;
        pipeline_info.pInputAssemblyState          = &input_assembly;
        pipeline_info.pViewportState               = &viewport_state;
        pipeline_info.pRasterizationState          = &rasterizer;
        pipeline_info.pMultisampleState            = &multisampling;
        pipeline_info.pDepthStencilState           = nullptr;
        pipeline_info.pColorBlendState             = &color_blending;
        pipeline_info.pDynamicState                = &dynamic_state;
        pipeline_info.layout                       = pipeline->pipelineLayout;
        pipeline_info.renderPass                   = m_render_pass;
        pipeline_info.subpass                      = 0;
        pipeline_info.basePipelineHandle           = nullptr;
        pipeline_info.basePipelineIndex            = -1;
        pipeline_info.pDepthStencilState           = &depth_stencil;

        if (VK_SUCCESS != vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline->pipeline))
        {
            throw std::runtime_error("failed to create graphics pipeline");
        }

        vkDestroyShaderModule(m_device, frag_shader_module, nullptr);
        vkDestroyShaderModule(m_device, vert_shader_module, nullptr);

        return pipeline;
    }

    /// @brief 使用着色器字节码数组创建 VkShaderModule 对象
    /// @param code
    /// @return
    VkShaderModule CreateShaderModule(const std::vector<char>& code) const
    {
        VkShaderModuleCreateInfo create_info = {};
        create_info.sType                    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        create_info.codeSize                 = code.size();
        create_info.pCode                    = reinterpret_cast<const uint32_t*>(code.data());

        VkShaderModule shader_module = nullptr;
        if (VK_SUCCESS != vkCreateShaderModule(m_device, &create_info, nullptr, &shader_module))
        {
            throw std::runtime_error("failed to create shader module");
        }

        return shader_module;
    }

    /// @brief 创建渲染流程对象，用于渲染的帧缓冲附着，指定颜色和深度缓冲以及采样数
    void CreateRenderPass()
    {
        // 附着描述
        VkAttachmentDescription color_attachment = {};
        color_attachment.format                  = m_swap_chain_image_format;        // 颜色缓冲附着的格式
        color_attachment.samples                 = VK_SAMPLE_COUNT_1_BIT;            // 采样数
        color_attachment.loadOp                  = VK_ATTACHMENT_LOAD_OP_CLEAR;      // 渲染之前对附着中的数据（颜色和深度）进行操作
        color_attachment.storeOp                 = VK_ATTACHMENT_STORE_OP_STORE;     // 渲染之后对附着中的数据（颜色和深度）进行操作
        color_attachment.stencilLoadOp           = VK_ATTACHMENT_LOAD_OP_DONT_CARE;  // 渲染之前对模板缓冲的操作
        color_attachment.stencilStoreOp          = VK_ATTACHMENT_STORE_OP_DONT_CARE; // 渲染之后对模板缓冲的操作
        color_attachment.initialLayout           = VK_IMAGE_LAYOUT_UNDEFINED;        // 渲染流程开始前的图像布局方式
        color_attachment.finalLayout             = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;  // 渲染流程结束后的图像布局方式

        VkAttachmentDescription depth_attachment = {};
        depth_attachment.format                  = FindDepthFormat();
        depth_attachment.samples                 = VK_SAMPLE_COUNT_1_BIT;
        depth_attachment.loadOp                  = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depth_attachment.storeOp                 = VK_ATTACHMENT_STORE_OP_DONT_CARE; // 绘制结束后不需要从深度缓冲复制深度数据
        depth_attachment.stencilLoadOp           = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depth_attachment.stencilStoreOp          = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth_attachment.initialLayout           = VK_IMAGE_LAYOUT_UNDEFINED;        // 不需要读取之前深度图像数据
        depth_attachment.finalLayout             = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        // 子流程引用的附着
        VkAttachmentReference color_attachment_ref = {};
        color_attachment_ref.attachment            = 0; // 索引，对应于片段着色器中的 layout(location = 0) out
        color_attachment_ref.layout                = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depth_attachment_ref = {};
        depth_attachment_ref.attachment            = 1;
        depth_attachment_ref.layout                = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        // 子流程
        VkSubpassDescription subpass    = {};
        subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS; // 图形渲染子流程
        subpass.colorAttachmentCount    = 1;                               // 颜色附着个数
        subpass.pColorAttachments       = &color_attachment_ref;           // 指定颜色附着
        subpass.pDepthStencilAttachment = &depth_attachment_ref;           // 指定深度模板附着，深度模板附着只能是一个，所以不用设置数量

        // 渲染流程使用的依赖信息
        VkSubpassDependency dependency = {};
        dependency.srcSubpass          = VK_SUBPASS_EXTERNAL; // 渲染流程开始前的子流程，为了避免出现循环依赖，dst的值必须大于src的值
        dependency.dstSubpass          = 0;                   // 渲染流程结束后的子流程
        dependency.srcStageMask =
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT; // 指定需要等待的管线阶段
        dependency.srcAccessMask = 0;                                                                   // 指定子流程将进行的操作类型
        dependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        std::array<VkAttachmentDescription, 2> attachments {color_attachment, depth_attachment};

        VkRenderPassCreateInfo render_pass_info = {};
        render_pass_info.sType                  = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        render_pass_info.attachmentCount        = static_cast<uint32_t>(attachments.size());
        render_pass_info.pAttachments           = attachments.data();
        render_pass_info.subpassCount           = 1;
        render_pass_info.pSubpasses             = &subpass;
        render_pass_info.dependencyCount        = 1;
        render_pass_info.pDependencies          = &dependency;

        if (VK_SUCCESS != vkCreateRenderPass(m_device, &render_pass_info, nullptr, &m_render_pass))
        {
            throw std::runtime_error("failed to create render pass");
        }
    }

    /// @brief 创建帧缓冲对象，附着需要绑定到帧缓冲对象上使用
    void CreateFramebuffers()
    {
        m_swap_chain_framebuffers.resize(m_swap_chain_image_views.size());

        for (size_t i = 0; i < m_swap_chain_image_views.size(); ++i)
        {
            std::array<VkImageView, 2> attachments {m_swap_chain_image_views.at(i), m_depth_image_view};

            VkFramebufferCreateInfo framebuffer_info = {};
            framebuffer_info.sType                   = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebuffer_info.renderPass              = m_render_pass;
            framebuffer_info.attachmentCount         = static_cast<uint32_t>(attachments.size());
            framebuffer_info.pAttachments            = attachments.data();
            framebuffer_info.width                   = m_swap_chain_extent.width;
            framebuffer_info.height                  = m_swap_chain_extent.height;
            framebuffer_info.layers                  = 1;

            if (VK_SUCCESS != vkCreateFramebuffer(m_device, &framebuffer_info, nullptr, &m_swap_chain_framebuffers.at(i)))
            {
                throw std::runtime_error("failed to create framebuffer");
            }
        }
    }

    /// @brief 初始化ImGui
    void InitImGui()
    {
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.DisplaySize = ImVec2((float)(k_width), (float)(k_height));
        io.Fonts->Build();

        ImGui::StyleColorsDark();
        ImGui_ImplGlfw_InitForVulkan(m_window, true);

        ImGui_ImplVulkan_InitInfo init_info = {};
        init_info.Instance                  = m_instance;
        init_info.PhysicalDevice            = m_physical_device;
        init_info.Device                    = m_device;
        init_info.Queue                     = m_present_queue;
        init_info.DescriptorPool            = m_imgui_descriptor_pool;
        init_info.Subpass                   = 0;
        init_info.MinImageCount             = m_min_image_count;
        init_info.ImageCount                = m_min_image_count;
        init_info.MSAASamples               = VK_SAMPLE_COUNT_1_BIT;

        ImGui_ImplVulkan_Init(&init_info, m_render_pass);

        vkResetCommandPool(m_device, m_command_pool, 0);

        VkCommandBufferBeginInfo begin_info = {};
        begin_info.sType                    = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(m_command_buffers[m_current_frame], &begin_info);

        ImGui_ImplVulkan_CreateFontsTexture(m_command_buffers[m_current_frame]);

        VkSubmitInfo end_info       = {};
        end_info.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        end_info.commandBufferCount = 1;
        end_info.pCommandBuffers    = &m_command_buffers[m_current_frame];
        vkEndCommandBuffer(m_command_buffers[m_current_frame]);

        vkQueueSubmit(m_graphics_queue, 1, &end_info, VK_NULL_HANDLE);
        vkDeviceWaitIdle(m_device);

        ImGui_ImplVulkan_DestroyFontUploadObjects();
    }

    /// @brief 创建指令池，用于管理指令缓冲对象使用的内存，并负责指令缓冲对象的分配
    void CreateCommandPool()
    {
        auto indices = FindQueueFamilies(m_physical_device);

        // VK_COMMAND_POOL_CREATE_TRANSIENT_BIT 指定从Pool中分配的CommandBuffer将是短暂的，意味着它们将在相对较短的时间内被重置或释放
        // VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT 允许从Pool中分配的任何CommandBuffer被单独重置到inital状态
        // 没有设置这个flag则不能使用 vkResetCommandBuffer
        // VK_COMMAND_POOL_CREATE_PROTECTED_BIT 指定从Pool中分配的CommandBuffer是受保护的CommandBuffer
        VkCommandPoolCreateInfo pool_info = {};
        pool_info.sType                   = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pool_info.flags                   = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        pool_info.queueFamilyIndex        = lvk_tidy::GetRequiredValue(indices.graphicsFamily);

        if (VK_SUCCESS != vkCreateCommandPool(m_device, &pool_info, nullptr, &m_command_pool))
        {
            throw std::runtime_error("failed to create command pool");
        }
    }

    /// @brief 为交换链中的每一个图像创建指令缓冲对象，使用它记录绘制指令
    void CreateCommandBuffers()
    {
        m_command_buffers.resize(m_swap_chain_framebuffers.size());

        VkCommandBufferAllocateInfo alloc_info = {};
        alloc_info.sType                       = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc_info.commandPool                 = m_command_pool;
        alloc_info.level                       = VK_COMMAND_BUFFER_LEVEL_PRIMARY; // 指定是主要还是辅助指令缓冲对象
        alloc_info.commandBufferCount          = static_cast<uint32_t>(m_command_buffers.size());

        if (VK_SUCCESS != vkAllocateCommandBuffers(m_device, &alloc_info, m_command_buffers.data()))
        {
            throw std::runtime_error("failed to allocate command buffers");
        }
    }

    /// @brief 记录指令到指令缓冲
    void RecordCommandBuffer(VkCommandBuffer command_buffer, VkFramebuffer framebuffer) const
    {
        VkCommandBufferBeginInfo begin_info = {};
        begin_info.sType                    = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags                    = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT; // 指定怎样使用指令缓冲
        begin_info.pInheritanceInfo         = nullptr; // 只用于辅助指令缓冲，指定从调用它的主要指令缓冲继承的状态

        if (VK_SUCCESS != vkBeginCommandBuffer(command_buffer, &begin_info))
        {
            throw std::runtime_error("failed to begin recording command buffer");
        }

        // 清除色，相当于背景色
        std::array<VkClearValue, 2> clear_values {};
        clear_values.at(0).color = {
            {.1f, .2f, .3f, 1.F}
        };
        clear_values.at(1).depthStencil = {1.F, 0};

        VkRenderPassBeginInfo render_pass_info = {};
        render_pass_info.sType                 = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        render_pass_info.renderPass            = m_render_pass;                        // 指定使用的渲染流程对象
        render_pass_info.framebuffer           = framebuffer;                          // 指定使用的帧缓冲对象
        render_pass_info.renderArea.offset     = {0, 0};                               // 指定用于渲染的区域
        render_pass_info.renderArea.extent     = m_swap_chain_extent;
        render_pass_info.clearValueCount = static_cast<uint32_t>(clear_values.size()); // 指定使用 VK_ATTACHMENT_LOAD_OP_CLEAR 标记后使用的清除值
        render_pass_info.pClearValues    = clear_values.data();

        // 所有可以记录指令到指令缓冲的函数，函数名都带有一个 vkCmd 前缀

        // 开始一个渲染流程
        // 1.用于记录指令的指令缓冲对象
        // 2.使用的渲染流程的信息
        // 3.指定渲染流程如何提供绘制指令的标记
        vkCmdBeginRenderPass(command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport viewport = {};
        viewport.x          = 0.F;
        viewport.y          = 0.F;
        viewport.width      = static_cast<float>(m_swap_chain_extent.width);
        viewport.height     = static_cast<float>(m_swap_chain_extent.height);
        viewport.minDepth   = 0.F;
        viewport.maxDepth   = 1.F;

        vkCmdSetViewport(command_buffer, 0, 1, &viewport);

        VkRect2D scissor = {};
        scissor.offset   = {0, 0};
        scissor.extent   = m_swap_chain_extent;

        vkCmdSetScissor(command_buffer, 0, 1, &scissor);

        for (const auto& [_, model] : m_models)
        {
            if (model && model->attributes.visibility)
            {
                DrawGLTFModel(model, command_buffer);
            }
        }

        // 绘制ImGui
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), command_buffer);

        // 结束渲染流程
        vkCmdEndRenderPass(command_buffer);

        // 结束记录指令到指令缓冲
        if (VK_SUCCESS != vkEndCommandBuffer(command_buffer))
        {
            throw std::runtime_error("failed to record command buffer");
        }
    }

    /// @brief 绘制每一帧
    /// @details 流程：从交换链获取一张图像、对帧缓冲附着执行指令缓冲中的渲染指令、返回渲染后的图像到交换链进行呈现操作
    void DrawFrame()
    {
        // 等待一组栅栏中的一个或全部栅栏发出信号，即上一次提交的指令结束执行
        // 使用栅栏可以进行CPU与GPU之间的同步，防止超过 MAX_FRAMES_IN_FLIGHT 帧的指令同时被提交执行
        vkWaitForFences(m_device, 1, &m_in_flight_fences.at(m_current_frame), VK_TRUE, std::numeric_limits<uint64_t>::max());

        // 从交换链获取一张图像
        uint32_t image_index {0};
        // 3.获取图像的超时时间，此处禁用图像获取超时
        // 4.通知的同步对象
        // 5.输出可用的交换链图像的索引
        VkResult result = vkAcquireNextImageKHR(
            m_device,
            m_swap_chain,
            std::numeric_limits<uint64_t>::max(),
            m_image_available_semaphores.at(m_current_frame),
            VK_NULL_HANDLE,
            &image_index
        );

        // VK_ERROR_OUT_OF_DATE_KHR 交换链不能继续使用，通常发生在窗口大小改变后
        // VK_SUBOPTIMAL_KHR 交换链仍然可以使用，但表面属性已经不能准确匹配
        if (VK_ERROR_OUT_OF_DATE_KHR == result)
        {
            RecreateSwapChain();
            return;
        }
        if (VK_SUCCESS != result && VK_SUBOPTIMAL_KHR != result)
        {
            throw std::runtime_error("failed to acquire swap chain image");
        }

        // 手动将栅栏重置为未发出信号的状态（必须手动设置）
        vkResetFences(m_device, 1, &m_in_flight_fences.at(m_current_frame));
        vkResetCommandBuffer(m_command_buffers.at(image_index), 0);
        RecordCommandBuffer(m_command_buffers.at(image_index), m_swap_chain_framebuffers.at(image_index));

        const std::array<VkPipelineStageFlags, 1> wait_stages {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

        VkSubmitInfo submit_info         = {};
        submit_info.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.waitSemaphoreCount   = 1;
        submit_info.pWaitSemaphores      = &m_image_available_semaphores.at(m_current_frame); // 指定队列开始执行前需要等待的信号量
        submit_info.pWaitDstStageMask    = wait_stages.data();                                // 指定需要等待的管线阶段
        submit_info.commandBufferCount   = 1;
        submit_info.pCommandBuffers      = &m_command_buffers[image_index];                   // 指定实际被提交执行的指令缓冲对象
        submit_info.signalSemaphoreCount = 1;
        submit_info.pSignalSemaphores    = &m_render_finished_semaphores.at(m_current_frame); // 指定在指令缓冲执行结束后发出信号的信号量对象

        // 提交指令缓冲给图形指令队列
        // 如果不等待上一次提交的指令结束执行，可能会导致内存泄漏
        // 1.vkQueueWaitIdle 可以等待上一次的指令结束执行，2.也可以同时渲染多帧解决该问题（使用栅栏）
        // vkQueueSubmit 的最后一个参数用来指定在指令缓冲执行结束后需要发起信号的栅栏对象
        if (VK_SUCCESS != vkQueueSubmit(m_graphics_queue, 1, &submit_info, m_in_flight_fences.at(m_current_frame)))
        {
            throw std::runtime_error("failed to submit draw command buffer");
        }

        VkPresentInfoKHR present_info   = {};
        present_info.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present_info.waitSemaphoreCount = 1;
        present_info.pWaitSemaphores    = &m_render_finished_semaphores.at(m_current_frame); // 指定开始呈现操作需要等待的信号量
        present_info.swapchainCount     = 1;
        present_info.pSwapchains        = &m_swap_chain;                                     // 指定用于呈现图像的交换链
        present_info.pImageIndices      = &image_index;                                      // 指定需要呈现的图像在交换链中的索引
        present_info.pResults           = nullptr;                                           // 可以通过该变量获取每个交换链的呈现操作是否成功的信息

        // 请求交换链进行图像呈现操作
        result = vkQueuePresentKHR(m_present_queue, &present_info);

        if (VK_ERROR_OUT_OF_DATE_KHR == result || VK_SUBOPTIMAL_KHR == result || m_framebuffer_resized)
        {
            m_framebuffer_resized = false;
            // 交换链不完全匹配时也重建交换链
            RecreateSwapChain();
        }
        else if (VK_SUCCESS != result)
        {
            throw std::runtime_error("failed to presend swap chain image");
        }

        // 更新当前帧索引
        m_current_frame = (m_current_frame + 1) % k_max_frames_in_flight;
    }

    /// @brief 创建同步对象，用于发出图像已经被获取可以开始渲染和渲染已经结束可以开始呈现的信号
    void CreateSyncObjects()
    {
        m_image_available_semaphores.resize(k_max_frames_in_flight);
        m_render_finished_semaphores.resize(k_max_frames_in_flight);
        m_in_flight_fences.resize(k_max_frames_in_flight);

        VkSemaphoreCreateInfo semaphore_info = {};
        semaphore_info.sType                 = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fence_info = {};
        fence_info.sType             = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_info.flags             = VK_FENCE_CREATE_SIGNALED_BIT; // 初始状态设置为已发出信号，避免 vkWaitForFences 一直等待

        for (size_t i = 0; i < k_max_frames_in_flight; ++i)
        {
            if (VK_SUCCESS != vkCreateSemaphore(m_device, &semaphore_info, nullptr, &m_image_available_semaphores.at(i))
                || VK_SUCCESS != vkCreateSemaphore(m_device, &semaphore_info, nullptr, &m_render_finished_semaphores.at(i))
                || VK_SUCCESS != vkCreateFence(m_device, &fence_info, nullptr, &m_in_flight_fences.at(i)))
            {
                throw std::runtime_error("failed to create synchronization objects for a frame");
            }
        }
    }

    /// @brief 重建交换链
    void RecreateSwapChain()
    {
        int width {0};
        int height {0};
        glfwGetFramebufferSize(m_window, &width, &height);
        while (0 == width || 0 == height)
        {
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

    /// @brief 清除交换链相关对象
    void CleanupSwapChain() noexcept
    {
        vkDestroyImageView(m_device, m_depth_image_view, nullptr);
        vkDestroyImage(m_device, m_depth_image, nullptr);
        vkFreeMemory(m_device, m_depth_image_memory, nullptr);

        for (auto framebuffer : m_swap_chain_framebuffers)
        {
            vkDestroyFramebuffer(m_device, framebuffer, nullptr);
        }

        for (auto image_view : m_swap_chain_image_views)
        {
            vkDestroyImageView(m_device, image_view, nullptr);
        }

        vkDestroySwapchainKHR(m_device, m_swap_chain, nullptr);
    }

    /// @brief 设置调试扩展信息
    /// @param createInfo
    void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& create_info) const noexcept
    {
        create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        // 设置回调函数处理的消息级别
        create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        // 设置回调函数处理的消息类型
        create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
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
    void
    CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& buffer_memory) const
    {
        VkBufferCreateInfo buffer_info = {};
        buffer_info.sType              = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size               = size;                      // 缓冲的字节大小
        buffer_info.usage              = usage;                     // 缓冲中的数据使用目的，可以使用位或来指定多个目的
        buffer_info.sharingMode        = VK_SHARING_MODE_EXCLUSIVE; // 缓冲可以被特定的队列族所拥有，也可以在多个队列族共享
        buffer_info.flags              = 0;                         // 配置缓冲的内存稀疏程度，0表示使用默认值

        if (VK_SUCCESS != vkCreateBuffer(m_device, &buffer_info, nullptr, &buffer))
        {
            throw std::runtime_error("failed to create vertex buffer");
        }

        // 缓冲创建好之后还需要分配内存，首先获取缓冲的内存需求
        // size: 缓冲需要的内存的字节大小，可能和bufferInfo.size的值不同
        // alignment: 缓冲在实际被分配的内存中的开始位置，依赖于bufferInfo的usage和flags
        // memoryTypeBits: 指示适合该缓冲使用的内存类型的位域
        VkMemoryRequirements mem_requirements {};
        vkGetBufferMemoryRequirements(m_device, buffer, &mem_requirements);

        VkMemoryAllocateInfo alloc_info = {};
        alloc_info.sType                = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize       = mem_requirements.size;
        alloc_info.memoryTypeIndex      = FindMemoryType(mem_requirements.memoryTypeBits, properties);

        if (VK_SUCCESS != vkAllocateMemory(m_device, &alloc_info, nullptr, &buffer_memory))
        {
            throw std::runtime_error("failed to allocate buffer memory");
        }

        // 4. 偏移值，需要满足能够被 memRequirements.alighment 整除
        vkBindBufferMemory(m_device, buffer, buffer_memory, 0);
    }

    /// @brief 在缓冲之间复制数据
    /// @param srcBuffer
    /// @param dstBuffer
    /// @param size
    void CopyBuffer(VkBuffer src_buffer, VkBuffer dst_buffer, VkDeviceSize size) const noexcept
    {
        VkCommandBufferAllocateInfo alloc_info = {};
        alloc_info.sType                       = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc_info.level                       = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.commandPool                 = m_command_pool;
        alloc_info.commandBufferCount          = 1;

        VkCommandBuffer command_buffer {};
        vkAllocateCommandBuffers(m_device, &alloc_info, &command_buffer);

        VkCommandBufferBeginInfo begin_info = {};
        begin_info.sType                    = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags                    = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; // 指定如何使用这个指令缓冲

        vkBeginCommandBuffer(command_buffer, &begin_info);
        VkBufferCopy copy_region = {};
        copy_region.srcOffset    = 0;
        copy_region.dstOffset    = 0;
        copy_region.size         = size;
        vkCmdCopyBuffer(command_buffer, src_buffer, dst_buffer, 1, &copy_region);
        vkEndCommandBuffer(command_buffer);

        VkSubmitInfo submit_info       = {};
        submit_info.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers    = &command_buffer;

        // 提交到内存传输指令队列执行内存传输
        vkQueueSubmit(m_graphics_queue, 1, &submit_info, nullptr);
        // 等待传输操作完成，也可以使用栅栏，栅栏可以同步多个不同的内存传输操作，给驱动程序的优化空间也更大
        vkQueueWaitIdle(m_graphics_queue);

        vkFreeCommandBuffers(m_device, m_command_pool, 1, &command_buffer);
    }

    VkImageView CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspect_flags)
    {
        VkImageViewCreateInfo view_info {};
        view_info.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image                           = image;
        view_info.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format                          = format;
        view_info.subresourceRange.aspectMask     = aspect_flags;
        view_info.subresourceRange.baseMipLevel   = 0;
        view_info.subresourceRange.levelCount     = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount     = 1;

        VkImageView image_view {};
        if (VK_SUCCESS != vkCreateImageView(m_device, &view_info, nullptr, &image_view))
        {
            throw std::runtime_error("failed to create texture image view");
        }

        return image_view;
    }

    /// @brief 查找最合适的内存类型
    /// @details 不同类型的内存所允许进行的操作以及操作的效率有所不同
    /// @param typeFilter 指定需要的内存类型的位域
    /// @param properties
    /// @return
    uint32_t FindMemoryType(uint32_t type_filter, VkMemoryPropertyFlags properties) const
    {
        // 查找物理设备可用的内存类型
        // memoryHeaps 内存来源，比如显存以及显存用尽后的位与主存中的交换空间
        VkPhysicalDeviceMemoryProperties mem_properties {};
        vkGetPhysicalDeviceMemoryProperties(m_physical_device, &mem_properties);

        for (uint32_t i = 0; i < mem_properties.memoryTypeCount; ++i)
        {
            if (type_filter & (1 << i) && (mem_properties.memoryTypes[i].propertyFlags & properties) == properties)
            {
                return i;
            }
        }

        throw std::runtime_error("failed to find suitable memory type");
    }

    /// @brief 创建着色器使用的每一个描述符绑定信息
    void CreateDescriptorSetLayouts()
    {
        // 只有 stageFlags 不同，其他都相同，是否可以考虑只保留 vertex fragment 两种描述符绑定信息
        {
            auto descriptor_set_layout = std::make_unique<DescriptorSetLayout>();

            VkDescriptorSetLayoutBinding ubo_layout_binding = {};
            ubo_layout_binding.binding                      = 0;
            ubo_layout_binding.descriptorType               = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            ubo_layout_binding.descriptorCount              = 1;
            ubo_layout_binding.stageFlags                   = VK_SHADER_STAGE_VERTEX_BIT;
            ubo_layout_binding.pImmutableSamplers           = nullptr;

            VkDescriptorSetLayoutCreateInfo layout_info = {};
            layout_info.sType                           = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            layout_info.bindingCount                    = 1;
            layout_info.pBindings                       = &ubo_layout_binding;

            if (VK_SUCCESS != vkCreateDescriptorSetLayout(m_device, &layout_info, nullptr, &descriptor_set_layout->descriptorSetLayout))
            {
                throw std::runtime_error("failed to create descriptor set layout");
            }

            m_context->descriptorSetLayouts.try_emplace("uniform_model", std::move(descriptor_set_layout));
        }

        {
            auto descriptor_set_layout = std::make_unique<DescriptorSetLayout>();

            VkDescriptorSetLayoutBinding ubo_layout_binding = {};
            ubo_layout_binding.binding                      = 0;
            ubo_layout_binding.descriptorType               = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            ubo_layout_binding.descriptorCount              = 1;
            ubo_layout_binding.stageFlags                   = VK_SHADER_STAGE_VERTEX_BIT;
            ubo_layout_binding.pImmutableSamplers           = nullptr;

            VkDescriptorSetLayoutCreateInfo layout_info = {};
            layout_info.sType                           = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            layout_info.bindingCount                    = 1;
            layout_info.pBindings                       = &ubo_layout_binding;

            if (VK_SUCCESS != vkCreateDescriptorSetLayout(m_device, &layout_info, nullptr, &descriptor_set_layout->descriptorSetLayout))
            {
                throw std::runtime_error("failed to create descriptor set layout");
            }

            m_context->descriptorSetLayouts.try_emplace("uniform_animation_morph", std::move(descriptor_set_layout));
        }

        {
            auto descriptor_set_layout = std::make_unique<DescriptorSetLayout>();

            VkDescriptorSetLayoutBinding ubo_layout_binding = {};
            ubo_layout_binding.binding                      = 0;
            ubo_layout_binding.descriptorType               = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            ubo_layout_binding.descriptorCount              = 1;
            ubo_layout_binding.stageFlags                   = VK_SHADER_STAGE_VERTEX_BIT;
            ubo_layout_binding.pImmutableSamplers           = nullptr;

            VkDescriptorSetLayoutCreateInfo layout_info = {};
            layout_info.sType                           = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            layout_info.bindingCount                    = 1;
            layout_info.pBindings                       = &ubo_layout_binding;

            if (VK_SUCCESS != vkCreateDescriptorSetLayout(m_device, &layout_info, nullptr, &descriptor_set_layout->descriptorSetLayout))
            {
                throw std::runtime_error("failed to create descriptor set layout");
            }

            m_context->descriptorSetLayouts.try_emplace("uniform_animation_skin", std::move(descriptor_set_layout));
        }

        {
            auto descriptor_set_layout = std::make_unique<DescriptorSetLayout>();

            VkDescriptorSetLayoutBinding ubo_layout_binding = {};
            ubo_layout_binding.binding                      = 0;
            ubo_layout_binding.descriptorType               = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            ubo_layout_binding.descriptorCount              = 1;
            ubo_layout_binding.stageFlags                   = VK_SHADER_STAGE_FRAGMENT_BIT;
            ubo_layout_binding.pImmutableSamplers           = nullptr;

            VkDescriptorSetLayoutCreateInfo layout_info = {};
            layout_info.sType                           = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            layout_info.bindingCount                    = 1;
            layout_info.pBindings                       = &ubo_layout_binding;

            if (VK_SUCCESS != vkCreateDescriptorSetLayout(m_device, &layout_info, nullptr, &descriptor_set_layout->descriptorSetLayout))
            {
                throw std::runtime_error("failed to create descriptor set layout");
            }

            m_context->descriptorSetLayouts.try_emplace("uniform_color", std::move(descriptor_set_layout));
        }

        {
            auto descriptor_set_layout = std::make_unique<DescriptorSetLayout>();

            VkDescriptorSetLayoutBinding ubo_layout_binding = {};
            ubo_layout_binding.binding                      = 0;
            ubo_layout_binding.descriptorType               = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            ubo_layout_binding.descriptorCount              = 1;
            ubo_layout_binding.stageFlags                   = VK_SHADER_STAGE_FRAGMENT_BIT;
            ubo_layout_binding.pImmutableSamplers           = nullptr;

            VkDescriptorSetLayoutCreateInfo layout_info = {};
            layout_info.sType                           = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            layout_info.bindingCount                    = 1;
            layout_info.pBindings                       = &ubo_layout_binding;

            if (VK_SUCCESS != vkCreateDescriptorSetLayout(m_device, &layout_info, nullptr, &descriptor_set_layout->descriptorSetLayout))
            {
                throw std::runtime_error("failed to create descriptor set layout");
            }

            m_context->descriptorSetLayouts.try_emplace("uniform_roughnessFactor_metallicFactor", std::move(descriptor_set_layout));
        }

        {
            auto descriptor_set_layout = std::make_unique<DescriptorSetLayout>();

            VkDescriptorSetLayoutBinding sampler_layout_binding {};
            sampler_layout_binding.binding            = 0;
            sampler_layout_binding.descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            sampler_layout_binding.descriptorCount    = 1;
            sampler_layout_binding.pImmutableSamplers = nullptr;
            sampler_layout_binding.stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;

            VkDescriptorSetLayoutCreateInfo layout_info = {};
            layout_info.sType                           = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            layout_info.bindingCount                    = 1;
            layout_info.pBindings                       = &sampler_layout_binding;

            if (VK_SUCCESS != vkCreateDescriptorSetLayout(m_device, &layout_info, nullptr, &descriptor_set_layout->descriptorSetLayout))
            {
                throw std::runtime_error("failed to create descriptor set layout");
            }

            m_context->descriptorSetLayouts.try_emplace("uniform_sampler", std::move(descriptor_set_layout));
        }
    }

    void
    CreateTextureUniforms(const std::unique_ptr<Model>& model, const std::unique_ptr<Texture>& texture, VkDescriptorSetLayout descriptor_set_layout)
    {
        texture->descriptorSets = std::make_unique<DescriptorSets>();

        std::vector<VkDescriptorSetLayout> layouts(k_max_frames_in_flight, descriptor_set_layout);

        VkDescriptorSetAllocateInfo alloc_info {};
        alloc_info.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool     = m_descriptor_pool;
        alloc_info.descriptorSetCount = static_cast<uint32_t>(k_max_frames_in_flight);
        alloc_info.pSetLayouts        = layouts.data();

        texture->descriptorSets->descriptorSets.resize(k_max_frames_in_flight);
        if (VK_SUCCESS != vkAllocateDescriptorSets(m_device, &alloc_info, texture->descriptorSets->descriptorSets.data()))
        {
            throw std::runtime_error("failed to allocate descriptor sets");
        }

        for (size_t i = 0; i < k_max_frames_in_flight; ++i)
        {
            VkDescriptorImageInfo image_info {};
            image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            image_info.imageView   = model->images.at(texture->image)->imageView;
            image_info.sampler     = model->samplers.at(texture->sampler)->sampler;

            VkWriteDescriptorSet descriptor_write {};
            descriptor_write.sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptor_write.dstSet           = texture->descriptorSets->descriptorSets[i];
            descriptor_write.dstBinding       = 0;
            descriptor_write.dstArrayElement  = 0;
            descriptor_write.descriptorType   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptor_write.descriptorCount  = 1;
            descriptor_write.pBufferInfo      = nullptr;
            descriptor_write.pImageInfo       = &image_info;
            descriptor_write.pTexelBufferView = nullptr;

            vkUpdateDescriptorSets(m_device, 1, &descriptor_write, 0, nullptr);
        }
    }

    std::unique_ptr<Uniform> CreateStorageBuffer(const void* data, VkDeviceSize buffer_size, VkDescriptorSetLayout descriptor_set_layout)
    {
        auto uniform = std::make_unique<Uniform>();

        uniform->buffers.resize(k_max_frames_in_flight);
        uniform->memorys.resize(k_max_frames_in_flight);
        uniform->mappeds.resize(k_max_frames_in_flight);

        for (size_t i = 0; i < k_max_frames_in_flight; ++i)
        {
            CreateBuffer(
                buffer_size,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                uniform->buffers[i],
                uniform->memorys[i]
            );
            vkMapMemory(m_device, uniform->memorys[i], 0, buffer_size, 0, &uniform->mappeds[i]);
        }

        //---------------------------------------------------------------------
        for (size_t i = 0; i < k_max_frames_in_flight; ++i)
        {
            std::memcpy(uniform->mappeds[i], data, buffer_size);
        }

        //---------------------------------------------------------------------
        std::vector<VkDescriptorSetLayout> layouts(k_max_frames_in_flight, descriptor_set_layout);

        VkDescriptorSetAllocateInfo alloc_info {};
        alloc_info.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool     = m_descriptor_pool;
        alloc_info.descriptorSetCount = static_cast<uint32_t>(k_max_frames_in_flight);
        alloc_info.pSetLayouts        = layouts.data();

        uniform->descriptorSets->descriptorSets.resize(k_max_frames_in_flight);
        if (VK_SUCCESS != vkAllocateDescriptorSets(m_device, &alloc_info, uniform->descriptorSets->descriptorSets.data()))
        {
            throw std::runtime_error("failed to allocate descriptor sets");
        }

        for (size_t i = 0; i < k_max_frames_in_flight; ++i)
        {
            VkDescriptorBufferInfo buffer_info {};
            buffer_info.buffer = uniform->buffers[i];
            buffer_info.offset = 0;
            buffer_info.range  = buffer_size;

            VkWriteDescriptorSet descriptor_write {};
            descriptor_write.sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptor_write.dstSet           = uniform->descriptorSets->descriptorSets[i];
            descriptor_write.dstBinding       = 0;
            descriptor_write.dstArrayElement  = 0;
            descriptor_write.descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descriptor_write.descriptorCount  = 1;
            descriptor_write.pBufferInfo      = &buffer_info;
            descriptor_write.pImageInfo       = nullptr;
            descriptor_write.pTexelBufferView = nullptr;

            vkUpdateDescriptorSets(m_device, 1, &descriptor_write, 0, nullptr);
        }

        return uniform;
    }

    template <typename T>
    std::unique_ptr<Uniform> CreateUniforms(const T& data, VkDescriptorSetLayout descriptor_set_layout)
    {
        auto uniform = std::make_unique<Uniform>();

        VkDeviceSize buffer_size = sizeof(T);

        uniform->buffers.resize(k_max_frames_in_flight);
        uniform->memorys.resize(k_max_frames_in_flight);
        uniform->mappeds.resize(k_max_frames_in_flight);

        for (size_t i = 0; i < k_max_frames_in_flight; ++i)
        {
            CreateBuffer(
                buffer_size,
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                uniform->buffers[i],
                uniform->memorys[i]
            );
            vkMapMemory(m_device, uniform->memorys[i], 0, buffer_size, 0, &uniform->mappeds[i]);
        }

        //---------------------------------------------------------------------
        for (size_t i = 0; i < k_max_frames_in_flight; ++i)
        {
            std::memcpy(uniform->mappeds[i], &data, sizeof(T));
        }

        //---------------------------------------------------------------------
        std::vector<VkDescriptorSetLayout> layouts(k_max_frames_in_flight, descriptor_set_layout);

        VkDescriptorSetAllocateInfo alloc_info {};
        alloc_info.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool     = m_descriptor_pool;
        alloc_info.descriptorSetCount = static_cast<uint32_t>(k_max_frames_in_flight);
        alloc_info.pSetLayouts        = layouts.data();

        uniform->descriptorSets->descriptorSets.resize(k_max_frames_in_flight);
        if (VK_SUCCESS != vkAllocateDescriptorSets(m_device, &alloc_info, uniform->descriptorSets->descriptorSets.data()))
        {
            throw std::runtime_error("failed to allocate descriptor sets");
        }

        for (size_t i = 0; i < k_max_frames_in_flight; ++i)
        {
            VkDescriptorBufferInfo buffer_info {};
            buffer_info.buffer = uniform->buffers[i];
            buffer_info.offset = 0;
            buffer_info.range  = sizeof(T);

            VkWriteDescriptorSet descriptor_write {};
            descriptor_write.sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptor_write.dstSet           = uniform->descriptorSets->descriptorSets[i];
            descriptor_write.dstBinding       = 0;
            descriptor_write.dstArrayElement  = 0;
            descriptor_write.descriptorType   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptor_write.descriptorCount  = 1;
            descriptor_write.pBufferInfo      = &buffer_info;
            descriptor_write.pImageInfo       = nullptr;
            descriptor_write.pTexelBufferView = nullptr;

            vkUpdateDescriptorSets(m_device, 1, &descriptor_write, 0, nullptr);
        }

        return uniform;
    }

    /// @brief 创建描述符池，描述符集需要通过描述符池来创建
    void CreateDescriptorPool()
    {
        std::array<VkDescriptorPoolSize, 3> pool_sizes {};
        pool_sizes[0].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        pool_sizes[0].descriptorCount = static_cast<uint32_t>(k_max_frames_in_flight) * 100;
        pool_sizes[1].type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        pool_sizes[1].descriptorCount = static_cast<uint32_t>(k_max_frames_in_flight) * 100;
        pool_sizes[2].type            = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        pool_sizes[2].descriptorCount = static_cast<uint32_t>(k_max_frames_in_flight) * 100;

        VkDescriptorPoolCreateInfo pool_info {};
        pool_info.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
        pool_info.pPoolSizes    = pool_sizes.data();
        pool_info.maxSets       = static_cast<uint32_t>(k_max_frames_in_flight) * 100;
        pool_info.flags         = 0;

        if (VK_SUCCESS != vkCreateDescriptorPool(m_device, &pool_info, nullptr, &m_descriptor_pool))
        {
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
    void CreateImage(
        uint32_t width,
        uint32_t height,
        VkFormat format,
        VkImageTiling tiling,
        VkImageUsageFlags usage,
        VkMemoryPropertyFlags /*properties*/,
        VkImage& image,
        VkDeviceMemory& image_memory
    )

    {
        // tiling成员变量可以是 VK_IMAGE_TILING_LINEAR 纹素以行主序的方式排列，可以直接访问图像
        //                     VK_IMAGE_TILING_OPTIMAL 纹素以一种对访问优化的方式排列
        // initialLayout成员变量可以是 VK_IMAGE_LAYOUT_UNDEFINED GPU不可用，纹素在第一次变换会被丢弃
        //                            VK_IMAGE_LAYOUT_PREINITIALIZED GPU不可用，纹素在第一次变换会被保留
        VkImageCreateInfo image_info {};
        image_info.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_info.imageType     = VK_IMAGE_TYPE_2D;
        image_info.extent.width  = static_cast<uint32_t>(width);
        image_info.extent.height = static_cast<uint32_t>(height);
        image_info.extent.depth  = 1;
        image_info.mipLevels     = 1;
        image_info.arrayLayers   = 1;
        image_info.format        = format;
        image_info.tiling        = tiling;                    // 设置之后不可修改
        image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // 图像数据是接收方，不需要保留第一次变换时的纹理数据
        image_info.usage         = usage;
        image_info.sharingMode   = VK_SHARING_MODE_EXCLUSIVE; // 只被一个队列族使用（支持传输操作的队列族），所以使用独占模式
        image_info.samples       = VK_SAMPLE_COUNT_1_BIT;     // 设置多重采样，只对用作附着的图像对象有效
        image_info.flags         = 0;                         // 可以用来设置稀疏图像的优化，比如体素地形没必要为“空气”部分分配内存

        if (VK_SUCCESS != vkCreateImage(m_device, &image_info, nullptr, &image))
        {
            throw std::runtime_error("failed to create image");
        }

        VkMemoryRequirements mem_requirements {};
        vkGetImageMemoryRequirements(m_device, image, &mem_requirements);

        VkMemoryAllocateInfo alloc_info {};
        alloc_info.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize  = mem_requirements.size;
        alloc_info.memoryTypeIndex = FindMemoryType(mem_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        if (VK_SUCCESS != vkAllocateMemory(m_device, &alloc_info, nullptr, &image_memory))
        {
            throw std::runtime_error("failed to allocate image memory");
        }
        vkBindImageMemory(m_device, image, image_memory, 0);
    }

    /// @brief 配置深度图像需要的资源
    void CreateDepthResources()
    {
        auto depth_format = FindDepthFormat();

        CreateImage(
            m_swap_chain_extent.width,
            m_swap_chain_extent.height,
            depth_format,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            m_depth_image,
            m_depth_image_memory
        );

        m_depth_image_view = CreateImageView(m_depth_image, depth_format, VK_IMAGE_ASPECT_DEPTH_BIT);
    }

    /// @brief 查找一个符合要求又被设备支持的图像数据格式
    /// @param candidates
    /// @param tiling
    /// @param features
    /// @return
    VkFormat FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) const
    {
        for (auto format : candidates)
        {
            // 查找可用的深度图像格式
            VkFormatProperties props {};
            vkGetPhysicalDeviceFormatProperties(m_physical_device, format, &props);
            // props.bufferFeatures        表示数据格式支持缓冲
            // props.linearTilingFeatures  表示数据格式支持线性tiling模式
            // props.optimalTilingFeatures 表示数据格式支持优化tiling模式

            if (VK_IMAGE_TILING_LINEAR == tiling && (props.linearTilingFeatures & features) == features)
            {
                return format;
            }
            if (VK_IMAGE_TILING_OPTIMAL == tiling && (props.optimalTilingFeatures & features) == features)
            {
                return format;
            }
        }

        throw std::runtime_error("failed to find supported format");
    }

    /// @brief 查找适合作为深度附着的图像数据格式
    /// @return
    VkFormat FindDepthFormat() const
    {
        return FindSupportedFormat(
            {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
        );
    }

    /// @brief 创建ImGui的描述符池
    void CreateImGuiDescriptorPool()
    {
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
        pool_info.sType                      = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags                      = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.maxSets                    = 1000 * static_cast<uint32_t>(pool_sizes.size());
        pool_info.poolSizeCount              = static_cast<uint32_t>(pool_sizes.size());
        pool_info.pPoolSizes                 = pool_sizes.data();
        if (VK_SUCCESS != vkCreateDescriptorPool(m_device, &pool_info, nullptr, &m_imgui_descriptor_pool))
        {
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
    static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
        [[maybe_unused]] VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
        [[maybe_unused]] VkDebugUtilsMessageTypeFlagsEXT message_type,
        const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
        [[maybe_unused]] void* user_data
    ) noexcept
    {
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
    static VkResult CreateDebugUtilsMessengerEXT(
        VkInstance instance,
        const VkDebugUtilsMessengerCreateInfoEXT* p_create_info,
        const VkAllocationCallbacks* p_allocator,
        VkDebugUtilsMessengerEXT* p_callback
    ) noexcept
    {
        // vkCreateDebugUtilsMessengerEXT是一个扩展函数，不会被 Vulkan 库自动加载，所以需要手动加载
        auto func = lvk_tidy::LoadInstanceProcAddress<PFN_vkCreateDebugUtilsMessengerEXT>(instance, "vkCreateDebugUtilsMessengerEXT");

        if (nullptr != func)
        {
            return func(instance, p_create_info, p_allocator, p_callback);
        }

        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }

    /// @brief 代理函数，用来加载 Vulkan 扩展函数 vkDestroyDebugUtilsMessengerEXT
    /// @param instance
    /// @param callback
    /// @param pAllocator
    static void
    DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT callback, const VkAllocationCallbacks* p_allocator) noexcept
    {
        auto func = lvk_tidy::LoadInstanceProcAddress<PFN_vkDestroyDebugUtilsMessengerEXT>(instance, "vkDestroyDebugUtilsMessengerEXT");

        if (nullptr != func)
        {
            func(instance, callback, p_allocator);
        }
    }

    /// @brief 读取二进制着色器文件
    /// @param fileName
    /// @return
    static std::vector<char> ReadFile(const std::string& file_name)
    {
        std::ifstream file(file_name, std::ios::ate | std::ios::binary);
        if (!file.is_open())
        {
            throw std::runtime_error("failed to open file: " + file_name);
        }

        size_t file_size = static_cast<size_t>(file.tellg());
        std::vector<char> buffer(file_size);
        file.seekg(0);
        file.read(buffer.data(), static_cast<std::streamsize>(file_size));
        file.close();
        return buffer;
    }

    static void FramebufferResizeCallback(GLFWwindow* window, int /*widht*/, int /*height*/) noexcept
    {
        if (auto app = reinterpret_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(window)))
        {
            app->m_framebuffer_resized = true;
        }
    }

    /// @brief 光标位置回调
    /// @param window
    /// @param xpos
    /// @param ypos
    static void CursorPosCallback(GLFWwindow* window, double xpos, double ypos) noexcept
    {
        if (ImGui::GetIO().WantCaptureMouse)
        {
            return;
        }

        if (auto app = reinterpret_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(window)))
        {
            // 计算鼠标移动的偏移量
            double x_offset = xpos - app->m_mouse_last_x;
            double y_offset = ypos - app->m_mouse_last_y;

            if (app->m_is_rotating)
            {
                // 设置灵敏度，可以根据需要调整
                constexpr auto sensitivity = 0.5 / 180.0 * std::numbers::pi_v<double>;
                x_offset *= sensitivity;
                y_offset *= sensitivity;

                // 构造旋转四元数
                glm::vec3 x_axis        = glm::normalize(glm::cross(app->m_view_up, app->m_eye_pos));
                glm::vec3 y_axis        = glm::normalize(app->m_view_up);
                glm::quat x_rotation    = glm::angleAxis(static_cast<float>(-y_offset), x_axis);
                glm::quat y_rotation    = glm::angleAxis(static_cast<float>(-x_offset), y_axis);
                glm::quat rotation_quat = x_rotation * y_rotation;

                app->m_eye_pos = glm::vec3(glm::mat4(rotation_quat) * glm::vec4(app->m_eye_pos, 1.F));
                app->m_view_up = glm::vec3(glm::mat4(rotation_quat) * glm::vec4(app->m_view_up, 1.F));
                // app->m_viewUp = glm::normalize(glm::cross(xAxis, -app->m_eyePos));
            }

            if (app->m_is_translating)
            {
                x_offset /= (app->m_swap_chain_extent.width / 2);
                y_offset /= (app->m_swap_chain_extent.height / 2);

                glm::vec3 x_axis        = glm::normalize(glm::cross(app->m_view_up, app->m_eye_pos));
                glm::vec3 y_axis        = glm::normalize(app->m_view_up);
                glm::mat4 x_translation = glm::translate(glm::mat4(1.F), x_axis * static_cast<float>(-x_offset));
                glm::mat4 y_translation = glm::translate(glm::mat4(1.F), y_axis * static_cast<float>(y_offset));
                glm::mat4 translation   = x_translation * y_translation;

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
    static void ScrollCallback(GLFWwindow* window, double /*xoffset*/, double yoffset) noexcept
    {
        if (ImGui::GetIO().WantCaptureMouse)
        {
            return;
        }

        if (auto app = reinterpret_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(window)))
        {
            glm::vec3 direction = glm::normalize(app->m_eye_pos - app->m_look_at);
            float distance      = glm::distance(app->m_eye_pos, app->m_look_at);
            glm::vec3 translate = direction * distance * 0.1F;

            if (yoffset < 0.0)
            {
                glm::mat4 mat  = glm::translate(glm::mat4(1.F), translate);
                app->m_eye_pos = glm::vec3(mat * glm::vec4(app->m_eye_pos, 1.F));
            }
            else if (yoffset > 0.0)
            {
                glm::mat4 mat  = glm::translate(glm::mat4(1.F), translate * -1.F);
                app->m_eye_pos = glm::vec3(mat * glm::vec4(app->m_eye_pos, 1.F));
            }
        }
    }

    /// @brief 鼠标按钮操作回调
    /// @param window
    /// @param button
    /// @param action
    /// @param mods
    static void MouseButtonCallback(GLFWwindow* window, int button, int action, int /*mods*/) noexcept
    {
        if (auto app = reinterpret_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(window)))
        {
            if (GLFW_PRESS == action && GLFW_MOUSE_BUTTON_RIGHT == button)
            {
                app->m_is_rotating = true;
                glfwGetCursorPos(window, &app->m_mouse_last_x, &app->m_mouse_last_y);
            }
            else if (GLFW_RELEASE == action && GLFW_MOUSE_BUTTON_RIGHT == button)
            {
                app->m_is_rotating = false;
            }

            if (GLFW_PRESS == action && GLFW_MOUSE_BUTTON_LEFT == button)
            {
                app->m_is_translating = true;
                glfwGetCursorPos(window, &app->m_mouse_last_x, &app->m_mouse_last_y);
            }
            else if (GLFW_RELEASE == action && GLFW_MOUSE_BUTTON_LEFT == button)
            {
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
    static void KeyCallback(GLFWwindow* window, int key, int /*scancode*/, int action, int /*mods*/) noexcept
    {
        if (auto app = reinterpret_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(window)))
        {
            // 按下'r'/'R'恢复视角
            if (GLFW_PRESS == action && GLFW_KEY_R == key)
            {
                app->m_view_up = glm::vec3 {0.F, 1.F, 0.F};
                app->m_eye_pos = glm::vec3 {0.F, 0.F, -3.F};
                app->m_look_at = glm::vec3 {0.F};
            }
        }
    }

private:
    GLFWwindow* m_window {nullptr};
    VkInstance m_instance {nullptr};
    VkDebugUtilsMessengerEXT m_debug_messenger {nullptr};
    VkPhysicalDevice m_physical_device {nullptr};
    VkDevice m_device {nullptr};
    VkQueue m_graphics_queue {nullptr}; // 图形队列
    VkSurfaceKHR m_surface {nullptr};
    VkQueue m_present_queue {nullptr};  // 呈现队列
    VkSwapchainKHR m_swap_chain {nullptr};
    std::vector<VkImage> m_swap_chain_images {};
    VkFormat m_swap_chain_image_format {};
    VkExtent2D m_swap_chain_extent {};
    std::vector<VkImageView> m_swap_chain_image_views {};
    VkRenderPass m_render_pass {nullptr};
    VkDescriptorPool m_descriptor_pool {nullptr};
    std::vector<VkFramebuffer> m_swap_chain_framebuffers {};
    VkCommandPool m_command_pool {nullptr};
    std::vector<VkCommandBuffer> m_command_buffers {};
    std::vector<VkSemaphore> m_image_available_semaphores {};
    std::vector<VkSemaphore> m_render_finished_semaphores {};
    std::vector<VkFence> m_in_flight_fences {};
    size_t m_current_frame {0};
    bool m_framebuffer_resized {false};

    VkImage m_depth_image {nullptr};
    VkDeviceMemory m_depth_image_memory {nullptr};
    VkImageView m_depth_image_view {nullptr};

    glm::vec3 m_view_up {0.F, 1.F, 0.F};
    glm::vec3 m_eye_pos {0.F, 0.F, -3.F};
    glm::vec3 m_look_at {0.F};
    double m_mouse_last_x {0.0};
    double m_mouse_last_y {0.0};
    bool m_is_rotating {false};
    bool m_is_translating {false};

    uint32_t m_min_image_count {0};
    VkDescriptorPool m_imgui_descriptor_pool {nullptr};

    std::unique_ptr<Context> m_context {std::make_unique<Context>()};
    std::unordered_map<std::string, std::unique_ptr<Model>> m_models {};

    tinygltf::TinyGLTF m_gltf_loader {};
};

int main()
{
    HelloTriangleApplication app;

    try
    {
        app.Run();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

#endif // TEST2
