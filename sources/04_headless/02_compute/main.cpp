
#include <vulkan/vulkan.h>

#define GLM_FORCE_RADIANS           // glm函数的参数使用弧度
#define GLM_FORCE_DEPTH_ZERO_TO_ONE // 透视矩阵深度值范围 [-1, 1] => [0, 1]
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>
#include <numbers>
#include <optional>
#include <set>
#include <stdexcept>
#include <vector>

constexpr size_t MAX_FRAMES_IN_FLIGHT { 2 };

// 需要开启的校验层的名称
const std::vector<const char*> g_validationLayers = { "VK_LAYER_KHRONOS_validation" };

// 是否启用校验层
#ifdef NDEBUG
const bool g_enableValidationLayers = false;
#else
const bool g_enableValidationLayers = true;
#endif // NDEBUG

struct UBOCompute
{
    uint32_t a {};
    uint32_t b {};
    uint32_t c {};
    uint32_t d {};
};

struct BUFCompute
{
    uint32_t result {};
};

/// @brief 支持图形、计算、传输、呈现的队列族
struct QueueFamilyIndices
{
    std::optional<uint32_t> graphicsFamily {};
    std::optional<uint32_t> presentFamily {};
    std::optional<uint32_t> computeFamily {};
    std::optional<uint32_t> transferFamily {};

    constexpr bool IsComplete() const noexcept
    {
        return computeFamily.has_value() && transferFamily.has_value();
    }
};

class HelloTriangleApplication
{
public:
    void Run()
    {
        InitVulkan();
        MainLoop();
        Cleanup();
    }

private:
    void InitVulkan()
    {
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

    void MainLoop()
    {
        static int count = 10;
        while (count-- > 0)
        {
            DrawFrame();
        }

        // 等待逻辑设备的操作结束执行
        // DrawFrame 函数中的操作是异步执行的，关闭窗口跳出while循环时，绘制操作和呈现操作可能仍在执行，不能进行清除操作
        vkDeviceWaitIdle(m_device);
    }

    void Cleanup() noexcept
    {
        vkDestroyPipeline(m_device, m_computePipeline, nullptr);
        vkDestroyPipelineLayout(m_device, m_computePipelineLayout, nullptr);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {

            vkDestroyBuffer(m_device, m_computeUboBuffers.at(i), nullptr);
            vkFreeMemory(m_device, m_computeUboBuffersMemory.at(i), nullptr);

            vkDestroyBuffer(m_device, m_computeResultBuffers.at(i), nullptr);
            vkFreeMemory(m_device, m_computeResultBuffersMemory.at(i), nullptr);
        }

        vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(m_device, m_computeDescriptorSetLayout, nullptr);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            vkDestroyFence(m_device, m_computeInFlightFences.at(i), nullptr);
        }

        vkDestroyCommandPool(m_device, m_computeCommandPool, nullptr);
        if (m_queueFamilyIndices.computeFamily != m_queueFamilyIndices.transferFamily)
        {
            vkDestroyCommandPool(m_device, m_transferCommandPool, nullptr);
        }

        vkDestroyDevice(m_device, nullptr);

        if (g_enableValidationLayers)
        {
            DestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
        }

        vkDestroyInstance(m_instance, nullptr);
    }

private:
    void CreateComputeCommandBuffers()
    {
        m_computeCommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

        VkCommandBufferAllocateInfo allocInfo = {};
        allocInfo.sType                       = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool                 = m_computeCommandPool;
        allocInfo.level                       = VK_COMMAND_BUFFER_LEVEL_PRIMARY; // 指定是主要还是辅助指令缓冲对象
        allocInfo.commandBufferCount          = static_cast<uint32_t>(m_computeCommandBuffers.size());

        if (VK_SUCCESS != vkAllocateCommandBuffers(m_device, &allocInfo, m_computeCommandBuffers.data()))
        {
            throw std::runtime_error("failed to allocate command buffers");
        }
    }

    void CreateComputeUniformBuffers()
    {
        // 输入
        VkDeviceSize computeUboBufferSize = sizeof(UBOCompute);

        m_computeUboBuffers.resize(MAX_FRAMES_IN_FLIGHT);
        m_computeUboBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
        m_computeUboBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            CreateBuffer(computeUboBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m_computeUboBuffers.at(i),
                m_computeUboBuffersMemory.at(i));
            vkMapMemory(m_device, m_computeUboBuffersMemory.at(i), 0, computeUboBufferSize, 0, &m_computeUboBuffersMapped.at(i));
        }

        // 输出
        VkDeviceSize indirectDrawBufferSize = sizeof(BUFCompute);

        m_computeResultBuffers.resize(MAX_FRAMES_IN_FLIGHT);
        m_computeResultBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
        m_computeResultBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            CreateBuffer(indirectDrawBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m_computeResultBuffers.at(i),
                m_computeResultBuffersMemory.at(i));
            vkMapMemory(m_device, m_computeResultBuffersMemory.at(i), 0, indirectDrawBufferSize, 0, &m_computeResultBuffersMapped.at(i));
        }
    }

    void UpdateComputeUniformBuffer(size_t currentImage)
    {
        static uint32_t n { 0 };
        n++;

        UBOCompute ubo { n, n, n, n };
        std::memcpy(m_computeUboBuffersMapped[currentImage], &ubo, sizeof(ubo));
    }

    void CreateComputeDescriptorSetLayout()
    {
        VkDescriptorSetLayoutBinding ubo = {};
        ubo.binding                      = 0;                                 // 绑定点
        ubo.descriptorType               = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; // 描述符类型
        ubo.stageFlags                   = VK_SHADER_STAGE_COMPUTE_BIT;       // 指定在哪一个着色器阶段使用
        ubo.descriptorCount              = 1;
        ubo.pImmutableSamplers           = nullptr;

        VkDescriptorSetLayoutBinding buf = {};
        buf.binding                      = 1;
        buf.descriptorType               = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        buf.stageFlags                   = VK_SHADER_STAGE_COMPUTE_BIT;
        buf.descriptorCount              = 1;
        buf.pImmutableSamplers           = nullptr;

        std::array bindings = { ubo, buf };

        VkDescriptorSetLayoutCreateInfo layoutInfo = {};
        layoutInfo.sType                           = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount                    = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings                       = bindings.data();

        if (VK_SUCCESS != vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &m_computeDescriptorSetLayout))
        {
            throw std::runtime_error("failed to create descriptor set layout");
        }
    }

    void CreateComputeDescriptorSets()
    {
        // 描述符布局对象的个数要匹配描述符集对象的个数
        std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, m_computeDescriptorSetLayout);

        VkDescriptorSetAllocateInfo allocInfo {};
        allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool     = m_descriptorPool; // 指定分配描述符集对象的描述符池
        allocInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
        allocInfo.pSetLayouts        = layouts.data();

        // 描述符集对象会在描述符池对象清除时自动被清除
        // 在这里给每一个交换链图像使用相同的描述符布局创建对应的描述符集
        m_computeDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
        if (VK_SUCCESS != vkAllocateDescriptorSets(m_device, &allocInfo, m_computeDescriptorSets.data()))
        {
            throw std::runtime_error("failed to allocate compute descriptor sets");
        }

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            VkDescriptorBufferInfo computeUboBufferInfo {};
            computeUboBufferInfo.buffer = m_computeUboBuffers.at(i);
            computeUboBufferInfo.offset = 0;
            computeUboBufferInfo.range  = sizeof(UBOCompute);

            VkDescriptorBufferInfo indirectDrawBufferInfo {};
            indirectDrawBufferInfo.buffer = m_computeResultBuffers.at(i);
            indirectDrawBufferInfo.offset = 0;
            indirectDrawBufferInfo.range  = sizeof(BUFCompute);

            std::array<VkWriteDescriptorSet, 2> descriptorWrites {};

            // 计算着色器中的 Uniform
            descriptorWrites.at(0).sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites.at(0).dstSet           = m_computeDescriptorSets.at(i);
            descriptorWrites.at(0).dstBinding       = 0;                                 // 绑定点
            descriptorWrites.at(0).dstArrayElement  = 0;
            descriptorWrites.at(0).descriptorType   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; // 对应着色器中的 Uniform
            descriptorWrites.at(0).descriptorCount  = 1;
            descriptorWrites.at(0).pBufferInfo      = &computeUboBufferInfo;             // 指定描述符引用的缓冲数据
            descriptorWrites.at(0).pImageInfo       = nullptr;                           // 指定描述符引用的图像数据
            descriptorWrites.at(0).pTexelBufferView = nullptr;                           // 指定描述符引用的缓冲视图

            // 计算着色器中的 Buffer
            descriptorWrites.at(1).sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites.at(1).dstSet           = m_computeDescriptorSets.at(i);
            descriptorWrites.at(1).dstBinding       = 1;                                 // 绑定点
            descriptorWrites.at(1).dstArrayElement  = 0;
            descriptorWrites.at(1).descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; // 对应着色器中的 buffer
            descriptorWrites.at(1).descriptorCount  = 1;
            descriptorWrites.at(1).pBufferInfo      = &indirectDrawBufferInfo;           // 指定描述符引用的缓冲数据
            descriptorWrites.at(1).pImageInfo       = nullptr;                           // 指定描述符引用的图像数据
            descriptorWrites.at(1).pTexelBufferView = nullptr;                           // 指定描述符引用的缓冲视图

            // 更新描述符的配置
            vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
        }
    }

    void CreateComputePipeline()
    {
        auto computeShaderCode = ReadFile("../resources/shaders/04_02_base_comp.spv");

        VkShaderModule computeShaderModule = CreateShaderModule(computeShaderCode);

        VkPipelineShaderStageCreateInfo computeShaderStageInfo {};
        computeShaderStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        computeShaderStageInfo.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
        computeShaderStageInfo.module = computeShaderModule;
        computeShaderStageInfo.pName  = "main";

        // 计算着色器中的 uniform、buffer
        VkPipelineLayoutCreateInfo pipelineLayoutInfo {};
        pipelineLayoutInfo.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts    = &m_computeDescriptorSetLayout;

        if (vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &m_computePipelineLayout) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create compute pipeline layout!");
        }

        VkComputePipelineCreateInfo pipelineInfo {};
        pipelineInfo.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineInfo.layout = m_computePipelineLayout;
        pipelineInfo.stage  = computeShaderStageInfo;

        if (vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_computePipeline) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create compute pipeline!");
        }

        vkDestroyShaderModule(m_device, computeShaderModule, nullptr);
    }

    void CreateComputeSyncObjects()
    {
        m_computeInFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

        VkSemaphoreCreateInfo semaphoreInfo = {};
        semaphoreInfo.sType                 = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo = {};
        fenceInfo.sType             = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags             = VK_FENCE_CREATE_SIGNALED_BIT; // 初始状态设置为已发出信号，避免 vkWaitForFences 一直等待

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            if (VK_SUCCESS != vkCreateFence(m_device, &fenceInfo, nullptr, &m_computeInFlightFences.at(i)))
            {
                throw std::runtime_error("failed to create compute synchronization objects for a frame");
            }
        }
    }

    void RecordComputeCommandBuffer(const VkCommandBuffer commandBuffer)
    {
        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType                    = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if (VK_SUCCESS != vkBeginCommandBuffer(commandBuffer, &beginInfo))
        {
            throw std::runtime_error("failed to begin recording compute command buffer");
        }

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_computePipeline);
        vkCmdBindDescriptorSets(
            commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_computePipelineLayout, 0, 1, &m_computeDescriptorSets.at(m_currentFrame), 0, 0);
        vkCmdDispatch(commandBuffer, 1, 1, 1);

        vkEndCommandBuffer(commandBuffer);
    }

    /// @brief 创建 Vulkan 实例
    void CreateInstance()
    {
        if (g_enableValidationLayers && !CheckValidationLayerSupport())
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
        if (g_enableValidationLayers)
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
            std::cout << layer.layerName << '\n';
        }

        // 检查需要开启的校验层是否可以在所有可用的校验层列表中找到
        for (const char* layerName : g_validationLayers)
        {
            bool layerFound { false };

            for (const auto& layerProperties : availableLayers)
            {
                if (0 == std::strcmp(layerName, layerProperties.layerName))
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
            std::cout << e.extensionName << '\n';
        }

        // 将需要开启的所有扩展添加到列表并返回
        std::vector<const char*> requiredExtensions;
        if (g_enableValidationLayers)
        {
            // 根据需要开启调试报告相关的扩展
            requiredExtensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        return requiredExtensions;
    }

    /// @brief 设置回调函数来接受调试信息
    void SetupDebugCallback()
    {
        if (!g_enableValidationLayers)
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
        uint32_t deviceCount { 0 };
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
    bool IsDeviceSuitable(const VkPhysicalDevice device) noexcept
    {
        // 获取基本的设置属性，name、type以及Vulkan版本等等
        // VkPhysicalDeviceProperties deviceProperties;
        // vkGetPhysicalDeviceProperties(device, &deviceProperties);

        // 获取对纹理的压缩、64位浮点数和多视图渲染等可选功能的支持
        // VkPhysicalDeviceFeatures deviceFeatures;
        // vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

        m_queueFamilyIndices     = FindQueueFamilies(device);
        auto extensionsSupported = CheckDeviceExtensionSupported(device);

        return m_queueFamilyIndices.IsComplete() && extensionsSupported;
    }

    /// @brief 查找满足需求的队列族
    /// @details 不同的队列族支持不同的类型的指令，例如计算、内存传输、绘图等指令
    /// @param device
    /// @return
    QueueFamilyIndices FindQueueFamilies(const VkPhysicalDevice device) const noexcept
    {
        // 获取物理设备支持的队列族列表
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        // 查找专用队列
        auto getDedicatedQueue = [&queueFamilies](VkQueueFlagBits queueFlagBits) -> std::optional<uint32_t> {
            for (size_t i = 0; i < queueFamilies.size(); ++i)
            {
                if (queueFlagBits == queueFamilies.at(i).queueFlags)
                {
                    return static_cast<uint32_t>(i);
                }
            }

            return std::nullopt;
        };

        QueueFamilyIndices indices {};
        indices.graphicsFamily = getDedicatedQueue(VK_QUEUE_GRAPHICS_BIT);
        indices.computeFamily  = getDedicatedQueue(VK_QUEUE_COMPUTE_BIT);
        indices.transferFamily = getDedicatedQueue(VK_QUEUE_TRANSFER_BIT);

        auto getSupportQueue = [&queueFamilies](VkQueueFlagBits queueFlagBits, size_t index) -> std::optional<uint32_t> {
            if (0 != (queueFlagBits & queueFamilies.at(index).queueFlags))
            {
                return static_cast<uint32_t>(index);
            }
            return std::nullopt;
        };

        for (size_t i = 0; i < queueFamilies.size(); ++i)
        {
            // 如果没有专用队列，则使用第一个支持指定类型的队列
            if (!indices.graphicsFamily.has_value())
            {
                indices.graphicsFamily = getSupportQueue(VK_QUEUE_GRAPHICS_BIT, i);
            }
            if (!indices.computeFamily.has_value())
            {
                indices.computeFamily = getSupportQueue(VK_QUEUE_COMPUTE_BIT, i);
            }
            if (!indices.transferFamily.has_value())
            {
                indices.transferFamily = getSupportQueue(VK_QUEUE_TRANSFER_BIT, i);
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
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::set<uint32_t> uniqueQueueFamilies { m_queueFamilyIndices.computeFamily.value(), m_queueFamilyIndices.transferFamily.value() };

        // 控制指令缓存执行顺序的优先级，即使只有一个队列也要显示指定优先级，范围：[0.0, 1.0]
        float queuePriority { 1.f };
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

        VkDeviceCreateInfo createInfo   = {};
        createInfo.sType                = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos    = queueCreateInfos.data();
        createInfo.pEnabledFeatures     = &deviceFeatures;

        // 根据需要对设备和 Vulkan 实例使用相同的校验层
        if (g_enableValidationLayers)
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

        vkGetDeviceQueue(m_device, m_queueFamilyIndices.computeFamily.value(), 0, &m_computeQueue);
        vkGetDeviceQueue(m_device, m_queueFamilyIndices.transferFamily.value(), 0, &m_transferQueue);
    }

    /// @brief 检测所需的扩展是否支持
    /// @param device
    /// @return
    bool CheckDeviceExtensionSupported(const VkPhysicalDevice device) const noexcept
    {
        uint32_t extensionCount { 0 };
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

        std::set<std::string> requiredExtensions; // 暂时不需要任何扩展
        for (const auto& extension : availableExtensions)
        {
            requiredExtensions.erase(extension.extensionName);
        }

        // 如果为空，则支持
        return requiredExtensions.empty();
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

    /// @brief 创建指令池，用于管理指令缓冲对象使用的内存，并负责指令缓冲对象的分配
    void CreateCommandPool()
    {
        // VK_COMMAND_POOL_CREATE_TRANSIENT_BIT 指定从Pool中分配的CommandBuffer将是短暂的，意味着它们将在相对较短的时间内被重置或释放
        // VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT 允许从Pool中分配的任何CommandBuffer被单独重置到inital状态
        // 没有设置这个flag则不能使用 vkResetCommandBuffer
        // VK_COMMAND_POOL_CREATE_PROTECTED_BIT 指定从Pool中分配的CommandBuffer是受保护的CommandBuffer
        VkCommandPoolCreateInfo poolInfo = {};
        poolInfo.sType                   = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags                   = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex        = m_queueFamilyIndices.computeFamily.value();

        if (VK_SUCCESS != vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_computeCommandPool))
        {
            throw std::runtime_error("failed to create command pool");
        }

        if (m_queueFamilyIndices.computeFamily != m_queueFamilyIndices.transferFamily)
        {
            poolInfo.queueFamilyIndex = m_queueFamilyIndices.transferFamily.value();
            if (VK_SUCCESS != vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_transferCommandPool))
            {
                throw std::runtime_error("failed to create command pool");
            }
        }
        else
        {
            m_transferCommandPool = m_computeCommandPool;
        }
    }

    void DrawFrame()
    {
        UpdateComputeUniformBuffer(m_currentFrame);
        vkWaitForFences(m_device, 1, &m_computeInFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);
        vkResetFences(m_device, 1, &m_computeInFlightFences[m_currentFrame]);

        // 回读计算结果
        auto result = reinterpret_cast<BUFCompute*>(m_computeResultBuffersMapped.at(m_currentFrame));
        std::cout << result->result << '\n';

        vkResetCommandBuffer(m_computeCommandBuffers[m_currentFrame], /*VkCommandBufferResetFlagBits*/ 0);
        RecordComputeCommandBuffer(m_computeCommandBuffers[m_currentFrame]);

        VkSubmitInfo submitInfo {};
        submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers    = &m_computeCommandBuffers[m_currentFrame];

        if (vkQueueSubmit(m_computeQueue, 1, &submitInfo, m_computeInFlightFences[m_currentFrame]) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to submit compute command buffer!");
        };

        // 更新当前帧索引
        m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
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

    /// @brief 创建指定类型的缓冲
    /// @details Vulkan 的缓冲是可以存储任意数据的可以被显卡读取的内存，不仅可以存储顶点数据
    /// @param size
    /// @param usage
    /// @param properties
    /// @param buffer
    /// @param bufferMemory
    void CreateBuffer(
        VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) const
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

    /// @brief 在缓冲之间复制数据
    /// @param srcBuffer
    /// @param dstBuffer
    /// @param size
    void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) const noexcept
    {
        VkCommandBufferAllocateInfo allocInfo = {};
        allocInfo.sType                       = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level                       = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool                 = m_transferCommandPool;
        allocInfo.commandBufferCount          = 1;

        VkCommandBuffer commandBuffer {};
        vkAllocateCommandBuffers(m_device, &allocInfo, &commandBuffer);

        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType                    = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags                    = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; // 指定如何使用这个指令缓冲

        vkBeginCommandBuffer(commandBuffer, &beginInfo);
        VkBufferCopy copyRegion = {};
        copyRegion.srcOffset    = 0;
        copyRegion.dstOffset    = 0;
        copyRegion.size         = size;
        vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
        vkEndCommandBuffer(commandBuffer);

        VkSubmitInfo submitInfo       = {};
        submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers    = &commandBuffer;

        // 提交到内存传输指令队列执行内存传输
        vkQueueSubmit(m_transferQueue, 1, &submitInfo, nullptr);
        // 等待传输操作完成，也可以使用栅栏，栅栏可以同步多个不同的内存传输操作，给驱动程序的优化空间也更大
        vkQueueWaitIdle(m_transferQueue);

        vkFreeCommandBuffers(m_device, m_transferCommandPool, 1, &commandBuffer);
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

    /// @brief 创建描述符池，描述符集需要通过描述符池来创建
    void CreateDescriptorPool()
    {
        std::array<VkDescriptorPoolSize, 2> poolSizes {};

        poolSizes.at(0).type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes.at(0).descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
        poolSizes.at(1).type            = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        poolSizes.at(1).descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

        VkDescriptorPoolCreateInfo poolInfo {};
        poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes    = poolSizes.data();
        poolInfo.maxSets       = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
        poolInfo.flags         = 0; // 可以用来设置独立的描述符集是否可以被清除掉，此处使用默认值

        if (VK_SUCCESS != vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_descriptorPool))
        {
            throw std::runtime_error("failed to create descriptor pool");
        }
    }

private:
    /// @brief 接受调试信息的回调函数
    /// @param messageSeverity 消息的级别：诊断、资源创建、警告、不合法或可能造成崩溃的操作
    /// @param messageType 发生了与规范和性能无关的事件、出现了违反规范的错误、进行了可能影响 Vulkan 性能的行为
    /// @param pCallbackData 包含了调试信息的字符串、存储有和消息相关的 Vulkan 对象句柄的数组、数组中的对象个数
    /// @param pUserData 指向了设置回调函数时，传递的数据指针
    /// @return 引发校验层处理的 Vulkan API 调用是否中断，通常只在测试校验层本身时会返回true，其余都应该返回 VK_FALSE
    static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData) noexcept
    {
        std::clog << "===========================================\n"
                  << "Debug::validation layer: " << pCallbackData->pMessage << '\n';

        return VK_FALSE;
    }

    /// @brief 代理函数，用来加载 Vulkan 扩展函数 vkCreateDebugUtilsMessengerEXT
    /// @param instance
    /// @param pCreateInfo
    /// @param pAllocator
    /// @param pCallback
    /// @return
    static VkResult CreateDebugUtilsMessengerEXT(VkInstance instance,
        const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
        const VkAllocationCallbacks* pAllocator,
        VkDebugUtilsMessengerEXT* pCallback) noexcept
    {
        // vkCreateDebugUtilsMessengerEXT是一个扩展函数，不会被 Vulkan 库自动加载，所以需要手动加载
        auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");

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
    static void DestroyDebugUtilsMessengerEXT(
        VkInstance instance, VkDebugUtilsMessengerEXT callback, const VkAllocationCallbacks* pAllocator) noexcept
    {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");

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

private:
    VkInstance m_instance { nullptr };
    VkDebugUtilsMessengerEXT m_debugMessenger { nullptr };
    VkPhysicalDevice m_physicalDevice { nullptr };
    VkDevice m_device { nullptr };

    size_t m_currentFrame { 0 };
    VkDescriptorPool m_descriptorPool { nullptr };
    QueueFamilyIndices m_queueFamilyIndices {};
    VkCommandPool m_computeCommandPool { nullptr };
    VkCommandPool m_transferCommandPool { nullptr };
    VkQueue m_transferQueue { nullptr }; // 传输队列
    VkQueue m_computeQueue { nullptr };  // 计算队列

    VkPipeline m_computePipeline { nullptr };
    VkPipelineLayout m_computePipelineLayout { nullptr };
    std::vector<VkCommandBuffer> m_computeCommandBuffers {};
    VkDescriptorSetLayout m_computeDescriptorSetLayout { nullptr };
    std::vector<VkDescriptorSet> m_computeDescriptorSets {};
    std::vector<VkFence> m_computeInFlightFences {};

    std::vector<VkBuffer> m_computeUboBuffers {};
    std::vector<VkDeviceMemory> m_computeUboBuffersMemory {};
    std::vector<void*> m_computeUboBuffersMapped {};
    std::vector<VkBuffer> m_computeResultBuffers {};
    std::vector<VkDeviceMemory> m_computeResultBuffersMemory {};
    std::vector<void*> m_computeResultBuffersMapped {};
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
