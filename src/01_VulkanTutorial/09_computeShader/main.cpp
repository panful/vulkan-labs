
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <random>
#include <set>
#include <stdexcept>
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

const uint32_t k_width  = 800;
const uint32_t k_height = 600;

// 粒子数量
const uint32_t k_particle_count = 1024 * 8;

const int k_max_frames_in_flight = 2;

const std::vector<const char*> k_validation_layers = {"VK_LAYER_KHRONOS_validation"};

const std::vector<const char*> k_device_extensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

#ifdef NDEBUG
const bool k_enable_validation_layers = false;
#else
const bool k_enable_validation_layers = true;
#endif

VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* p_create_info,
    const VkAllocationCallbacks* p_allocator,
    VkDebugUtilsMessengerEXT* p_debug_messenger
)
{
    auto func = lvk_tidy::LoadInstanceProcAddress<PFN_vkCreateDebugUtilsMessengerEXT>(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr)
    {
        return func(instance, p_create_info, p_allocator, p_debug_messenger);
    }

    return VK_ERROR_EXTENSION_NOT_PRESENT;
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debug_messenger, const VkAllocationCallbacks* p_allocator)
{
    auto func = lvk_tidy::LoadInstanceProcAddress<PFN_vkDestroyDebugUtilsMessengerEXT>(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr)
    {
        func(instance, debug_messenger, p_allocator);
    }
}

struct QueueFamilyIndices
{
    std::optional<uint32_t> graphicsAndComputeFamily;
    std::optional<uint32_t> presentFamily;

    bool IsComplete()
    {
        return graphicsAndComputeFamily.has_value() && presentFamily.has_value();
    }
};

struct SwapChainSupportDetails
{
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

/// @brief 着色器 uniform
struct UniformBufferObject
{
    float deltaTime = 1.0F;
};

/// @brief 粒子顶点信息
struct Particle
{
    glm::vec2 position;
    glm::vec2 velocity;
    glm::vec4 color;

    static VkVertexInputBindingDescription GetBindingDescription()
    {
        VkVertexInputBindingDescription binding_description {};
        binding_description.binding   = 0;
        binding_description.stride    = sizeof(Particle);
        binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return binding_description;
    }

    static std::array<VkVertexInputAttributeDescription, 2> GetAttributeDescriptions()
    {
        std::array<VkVertexInputAttributeDescription, 2> attribute_descriptions {};

        attribute_descriptions[0].binding  = 0;
        attribute_descriptions[0].location = 0;
        attribute_descriptions[0].format   = VK_FORMAT_R32G32_SFLOAT;
        attribute_descriptions[0].offset   = offsetof(Particle, position);

        attribute_descriptions[1].binding  = 0;
        attribute_descriptions[1].location = 1;
        attribute_descriptions[1].format   = VK_FORMAT_R32G32B32A32_SFLOAT;
        attribute_descriptions[1].offset   = offsetof(Particle, color);

        return attribute_descriptions;
    }
};

class ComputeShaderApplication
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
        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        m_window = glfwCreateWindow(k_width, k_height, "Vulkan", nullptr, nullptr);
        glfwSetWindowUserPointer(m_window, this);
        glfwSetFramebufferSizeCallback(m_window, FramebufferResizeCallback);

        m_last_time = glfwGetTime();
    }

    void InitVulkan()
    {
        CreateInstance();
        SetupDebugMessenger();
        CreateSurface();
        PickPhysicalDevice();
        CreateLogicalDevice();
        CreateSwapChain();
        CreateImageViews();
        CreateRenderPass();
        CreateComputeDescriptorSetLayout();
        CreateGraphicsPipeline();
        CreateComputePipeline();
        CreateFramebuffers();
        CreateCommandPool();
        CreateShaderStorageBuffers();
        CreateUniformBuffers();
        CreateDescriptorPool();
        CreateComputeDescriptorSets();
        CreateCommandBuffers();
        CreateComputeCommandBuffers();
        CreateSyncObjects();
    }

    void MainLoop()
    {
        while (!glfwWindowShouldClose(m_window))
        {
            glfwPollEvents();
            DrawFrame();
            // We want to animate the particle system using the last frames time to get smooth, frame-rate independent animation
            double current_time = glfwGetTime();
            m_last_frame_time   = static_cast<float>(current_time - m_last_time) * 1000.F;
            m_last_time         = current_time;

            std::cout << "FPS: " << 1000.F / m_last_frame_time << '\n';
        }

        vkDeviceWaitIdle(m_device);
    }

    void CleanupSwapChain()
    {
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

    void Cleanup()
    {
        CleanupSwapChain();

        vkDestroyPipeline(m_device, m_graphics_pipeline, nullptr);
        vkDestroyPipelineLayout(m_device, m_pipeline_layout, nullptr);

        vkDestroyPipeline(m_device, m_compute_pipeline, nullptr);
        vkDestroyPipelineLayout(m_device, m_compute_pipeline_layout, nullptr);

        vkDestroyRenderPass(m_device, m_render_pass, nullptr);

        for (size_t i = 0; i < k_max_frames_in_flight; i++)
        {
            vkDestroyBuffer(m_device, m_uniform_buffers[i], nullptr);
            vkFreeMemory(m_device, m_uniform_buffers_memory[i], nullptr);
        }

        vkDestroyDescriptorPool(m_device, m_descriptor_pool, nullptr);

        vkDestroyDescriptorSetLayout(m_device, m_compute_descriptor_set_layout, nullptr);

        for (size_t i = 0; i < k_max_frames_in_flight; i++)
        {
            vkDestroyBuffer(m_device, m_shader_storage_buffers[i], nullptr);
            vkFreeMemory(m_device, m_shader_storage_buffers_memory[i], nullptr);
        }

        for (size_t i = 0; i < k_max_frames_in_flight; i++)
        {
            vkDestroySemaphore(m_device, m_render_finished_semaphores[i], nullptr);
            vkDestroySemaphore(m_device, m_image_available_semaphores[i], nullptr);
            vkDestroySemaphore(m_device, m_compute_finished_semaphores[i], nullptr);
            vkDestroyFence(m_device, m_in_flight_fences[i], nullptr);
            vkDestroyFence(m_device, m_compute_in_flight_fences[i], nullptr);
        }

        vkDestroyCommandPool(m_device, m_command_pool, nullptr);

        vkDestroyDevice(m_device, nullptr);

        if (k_enable_validation_layers)
        {
            DestroyDebugUtilsMessengerEXT(m_instance, m_debug_messenger, nullptr);
        }

        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
        vkDestroyInstance(m_instance, nullptr);

        glfwDestroyWindow(m_window);

        glfwTerminate();
    }

    void RecreateSwapChain()
    {
        int width  = 0;
        int height = 0;
        glfwGetFramebufferSize(m_window, &width, &height);
        while (width == 0 || height == 0)
        {
            glfwGetFramebufferSize(m_window, &width, &height);
            glfwWaitEvents();
        }

        vkDeviceWaitIdle(m_device);

        CleanupSwapChain();

        CreateSwapChain();
        CreateImageViews();
        CreateFramebuffers();
    }

    void CreateInstance()
    {
        if (k_enable_validation_layers && !CheckValidationLayerSupport())
        {
            throw std::runtime_error("validation layers requested, but not available!");
        }

        VkApplicationInfo app_info {};
        app_info.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app_info.pApplicationName   = "Hello Triangle";
        app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        app_info.pEngineName        = "No Engine";
        app_info.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
        app_info.apiVersion         = VK_API_VERSION_1_0;

        VkInstanceCreateInfo create_info {};
        create_info.sType            = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        create_info.pApplicationInfo = &app_info;

        auto extensions                     = GetRequiredExtensions();
        create_info.enabledExtensionCount   = static_cast<uint32_t>(extensions.size());
        create_info.ppEnabledExtensionNames = extensions.data();

        VkDebugUtilsMessengerCreateInfoEXT debug_create_info {};
        if (k_enable_validation_layers)
        {
            create_info.enabledLayerCount   = static_cast<uint32_t>(k_validation_layers.size());
            create_info.ppEnabledLayerNames = k_validation_layers.data();

            PopulateDebugMessengerCreateInfo(debug_create_info);
            create_info.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debug_create_info;
        }
        else
        {
            create_info.enabledLayerCount = 0;

            create_info.pNext = nullptr;
        }

        if (vkCreateInstance(&create_info, nullptr, &m_instance) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create instance!");
        }
    }

    void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& create_info)
    {
        create_info                 = {};
        create_info.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        create_info.pfnUserCallback = DebugCallback;
    }

    void SetupDebugMessenger()
    {
        if (!k_enable_validation_layers)
        {
            return;
        }

        VkDebugUtilsMessengerCreateInfoEXT create_info;
        PopulateDebugMessengerCreateInfo(create_info);

        if (CreateDebugUtilsMessengerEXT(m_instance, &create_info, nullptr, &m_debug_messenger) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to set up debug messenger!");
        }
    }

    void CreateSurface()
    {
        if (glfwCreateWindowSurface(m_instance, m_window, nullptr, &m_surface) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create window surface!");
        }
    }

    void PickPhysicalDevice()
    {
        uint32_t device_count = 0;
        vkEnumeratePhysicalDevices(m_instance, &device_count, nullptr);

        if (device_count == 0)
        {
            throw std::runtime_error("failed to find GPUs with Vulkan support!");
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

        if (m_physical_device == VK_NULL_HANDLE)
        {
            throw std::runtime_error("failed to find a suitable GPU!");
        }
    }

    void CreateLogicalDevice()
    {
        // 查找支持图形和计算的队列族
        QueueFamilyIndices indices = FindQueueFamilies(m_physical_device);

        std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
        std::set<uint32_t> unique_queue_families = {
            lvk_tidy::GetRequiredValue(indices.graphicsAndComputeFamily), lvk_tidy::GetRequiredValue(indices.presentFamily)
        };

        float queue_priority = 1.0F;
        for (uint32_t queue_family : unique_queue_families)
        {
            VkDeviceQueueCreateInfo queue_create_info {};
            queue_create_info.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queue_create_info.queueFamilyIndex = queue_family;
            queue_create_info.queueCount       = 1;
            queue_create_info.pQueuePriorities = &queue_priority;
            queue_create_infos.push_back(queue_create_info);
        }

        VkPhysicalDeviceFeatures device_features {};

        VkDeviceCreateInfo create_info {};
        create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

        create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
        create_info.pQueueCreateInfos    = queue_create_infos.data();

        create_info.pEnabledFeatures = &device_features;

        create_info.enabledExtensionCount   = static_cast<uint32_t>(k_device_extensions.size());
        create_info.ppEnabledExtensionNames = k_device_extensions.data();

        if (k_enable_validation_layers)
        {
            create_info.enabledLayerCount   = static_cast<uint32_t>(k_validation_layers.size());
            create_info.ppEnabledLayerNames = k_validation_layers.data();
        }
        else
        {
            create_info.enabledLayerCount = 0;
        }

        if (vkCreateDevice(m_physical_device, &create_info, nullptr, &m_device) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create logical device!");
        }

        vkGetDeviceQueue(m_device, lvk_tidy::GetRequiredValue(indices.graphicsAndComputeFamily), 0, &m_graphics_queue);
        vkGetDeviceQueue(m_device, lvk_tidy::GetRequiredValue(indices.graphicsAndComputeFamily), 0, &m_compute_queue);
        vkGetDeviceQueue(m_device, lvk_tidy::GetRequiredValue(indices.presentFamily), 0, &m_present_queue);
    }

    void CreateSwapChain()
    {
        SwapChainSupportDetails swap_chain_support = QuerySwapChainSupport(m_physical_device);

        VkSurfaceFormatKHR surface_format = ChooseSwapSurfaceFormat(swap_chain_support.formats);
        VkPresentModeKHR present_mode     = ChooseSwapPresentMode(swap_chain_support.presentModes);
        VkExtent2D extent                 = ChooseSwapExtent(swap_chain_support.capabilities);

        uint32_t image_count = swap_chain_support.capabilities.minImageCount + 1;
        if (swap_chain_support.capabilities.maxImageCount > 0 && image_count > swap_chain_support.capabilities.maxImageCount)
        {
            image_count = swap_chain_support.capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR create_info {};
        create_info.sType   = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        create_info.surface = m_surface;

        create_info.minImageCount    = image_count;
        create_info.imageFormat      = surface_format.format;
        create_info.imageColorSpace  = surface_format.colorSpace;
        create_info.imageExtent      = extent;
        create_info.imageArrayLayers = 1;
        create_info.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        QueueFamilyIndices indices = FindQueueFamilies(m_physical_device);
        const std::array<uint32_t, 2> queue_family_indices {
            lvk_tidy::GetRequiredValue(indices.graphicsAndComputeFamily), lvk_tidy::GetRequiredValue(indices.presentFamily)
        };

        if (indices.graphicsAndComputeFamily != indices.presentFamily)
        {
            create_info.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
            create_info.queueFamilyIndexCount = 2;
            create_info.pQueueFamilyIndices   = queue_family_indices.data();
        }
        else
        {
            create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }

        create_info.preTransform   = swap_chain_support.capabilities.currentTransform;
        create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        create_info.presentMode    = present_mode;
        create_info.clipped        = VK_TRUE;

        if (vkCreateSwapchainKHR(m_device, &create_info, nullptr, &m_swap_chain) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create swap chain!");
        }

        vkGetSwapchainImagesKHR(m_device, m_swap_chain, &image_count, nullptr);
        m_swap_chain_images.resize(image_count);
        vkGetSwapchainImagesKHR(m_device, m_swap_chain, &image_count, m_swap_chain_images.data());

        m_swap_chain_image_format = surface_format.format;
        m_swap_chain_extent       = extent;
    }

    void CreateImageViews()
    {
        m_swap_chain_image_views.resize(m_swap_chain_images.size());

        for (size_t i = 0; i < m_swap_chain_images.size(); i++)
        {
            VkImageViewCreateInfo create_info {};
            create_info.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            create_info.image                           = m_swap_chain_images[i];
            create_info.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
            create_info.format                          = m_swap_chain_image_format;
            create_info.components.r                    = VK_COMPONENT_SWIZZLE_IDENTITY;
            create_info.components.g                    = VK_COMPONENT_SWIZZLE_IDENTITY;
            create_info.components.b                    = VK_COMPONENT_SWIZZLE_IDENTITY;
            create_info.components.a                    = VK_COMPONENT_SWIZZLE_IDENTITY;
            create_info.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            create_info.subresourceRange.baseMipLevel   = 0;
            create_info.subresourceRange.levelCount     = 1;
            create_info.subresourceRange.baseArrayLayer = 0;
            create_info.subresourceRange.layerCount     = 1;

            if (vkCreateImageView(m_device, &create_info, nullptr, &m_swap_chain_image_views[i]) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to create image views!");
            }
        }
    }

    void CreateRenderPass()
    {
        VkAttachmentDescription color_attachment {};
        color_attachment.format         = m_swap_chain_image_format;
        color_attachment.samples        = VK_SAMPLE_COUNT_1_BIT;
        color_attachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color_attachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        color_attachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color_attachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        color_attachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference color_attachment_ref {};
        color_attachment_ref.attachment = 0;
        color_attachment_ref.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass {};
        subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments    = &color_attachment_ref;

        VkSubpassDependency dependency {};
        dependency.srcSubpass    = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass    = 0;
        dependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo render_pass_info {};
        render_pass_info.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        render_pass_info.attachmentCount = 1;
        render_pass_info.pAttachments    = &color_attachment;
        render_pass_info.subpassCount    = 1;
        render_pass_info.pSubpasses      = &subpass;
        render_pass_info.dependencyCount = 1;
        render_pass_info.pDependencies   = &dependency;

        if (vkCreateRenderPass(m_device, &render_pass_info, nullptr, &m_render_pass) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create render pass!");
        }
    }

    void CreateComputeDescriptorSetLayout()
    {
        std::array<VkDescriptorSetLayoutBinding, 3> layout_bindings {};

        // 计算着色器中的 uniform
        layout_bindings[0].binding            = 0;
        layout_bindings[0].descriptorCount    = 1;
        layout_bindings[0].descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        layout_bindings[0].pImmutableSamplers = nullptr;
        layout_bindings[0].stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;

        // 计算着色器中的 buffer
        layout_bindings[1].binding            = 1;
        layout_bindings[1].descriptorCount    = 1;
        layout_bindings[1].descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        layout_bindings[1].pImmutableSamplers = nullptr;
        layout_bindings[1].stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;

        // 计算着色器中的 buffer
        layout_bindings[2].binding            = 2;
        layout_bindings[2].descriptorCount    = 1;
        layout_bindings[2].descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        layout_bindings[2].pImmutableSamplers = nullptr;
        layout_bindings[2].stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutCreateInfo layout_info {};
        layout_info.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout_info.bindingCount = 3;
        layout_info.pBindings    = layout_bindings.data();

        if (vkCreateDescriptorSetLayout(m_device, &layout_info, nullptr, &m_compute_descriptor_set_layout) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create compute descriptor set layout!");
        }
    }

    void CreateGraphicsPipeline()
    {
        auto vert_shader_code = ReadFile(PROJECT_ASSETS_DIR "shaders/01_09_base_vert.spv");
        auto frag_shader_code = ReadFile(PROJECT_ASSETS_DIR "shaders/01_09_base_frag.spv");

        VkShaderModule vert_shader_module = CreateShaderModule(vert_shader_code);
        VkShaderModule frag_shader_module = CreateShaderModule(frag_shader_code);

        VkPipelineShaderStageCreateInfo vert_shader_stage_info {};
        vert_shader_stage_info.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vert_shader_stage_info.stage  = VK_SHADER_STAGE_VERTEX_BIT;
        vert_shader_stage_info.module = vert_shader_module;
        vert_shader_stage_info.pName  = "main";

        VkPipelineShaderStageCreateInfo frag_shader_stage_info {};
        frag_shader_stage_info.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        frag_shader_stage_info.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
        frag_shader_stage_info.module = frag_shader_module;
        frag_shader_stage_info.pName  = "main";

        const std::array<VkPipelineShaderStageCreateInfo, 2> shader_stages {vert_shader_stage_info, frag_shader_stage_info};

        VkPipelineVertexInputStateCreateInfo vertex_input_info {};
        vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        auto binding_description    = Particle::GetBindingDescription();
        auto attribute_descriptions = Particle::GetAttributeDescriptions();

        vertex_input_info.vertexBindingDescriptionCount   = 1;
        vertex_input_info.vertexAttributeDescriptionCount = static_cast<uint32_t>(attribute_descriptions.size());
        vertex_input_info.pVertexBindingDescriptions      = &binding_description;
        vertex_input_info.pVertexAttributeDescriptions    = attribute_descriptions.data();

        VkPipelineInputAssemblyStateCreateInfo input_assembly {};
        input_assembly.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        input_assembly.topology               = VK_PRIMITIVE_TOPOLOGY_POINT_LIST; // 粒子使用点类型绘制
        input_assembly.primitiveRestartEnable = VK_FALSE;

        VkPipelineViewportStateCreateInfo viewport_state {};
        viewport_state.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport_state.viewportCount = 1;
        viewport_state.scissorCount  = 1;

        VkPipelineRasterizationStateCreateInfo rasterizer {};
        rasterizer.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable        = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode             = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth               = 1.0F;
        rasterizer.cullMode                = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizer.depthBiasEnable         = VK_FALSE;

        VkPipelineMultisampleStateCreateInfo multisampling {};
        multisampling.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable  = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        // 混合，实现透明
        VkPipelineColorBlendAttachmentState color_blend_attachment {};
        color_blend_attachment.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        color_blend_attachment.blendEnable         = VK_TRUE;
        color_blend_attachment.colorBlendOp        = VK_BLEND_OP_ADD;
        color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        color_blend_attachment.alphaBlendOp        = VK_BLEND_OP_ADD;
        color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;

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

        std::vector<VkDynamicState> dynamic_states = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamic_state {};
        dynamic_state.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamic_state.dynamicStateCount = static_cast<uint32_t>(dynamic_states.size());
        dynamic_state.pDynamicStates    = dynamic_states.data();

        // 图形管线中没有使用 uniform
        VkPipelineLayoutCreateInfo pipeline_layout_info {};
        pipeline_layout_info.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_layout_info.setLayoutCount = 0;
        pipeline_layout_info.pSetLayouts    = nullptr;

        if (vkCreatePipelineLayout(m_device, &pipeline_layout_info, nullptr, &m_pipeline_layout) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create pipeline layout!");
        }

        VkGraphicsPipelineCreateInfo pipeline_info {};
        pipeline_info.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeline_info.stageCount          = 2;
        pipeline_info.pStages             = shader_stages.data();
        pipeline_info.pVertexInputState   = &vertex_input_info;
        pipeline_info.pInputAssemblyState = &input_assembly;
        pipeline_info.pViewportState      = &viewport_state;
        pipeline_info.pRasterizationState = &rasterizer;
        pipeline_info.pMultisampleState   = &multisampling;
        pipeline_info.pColorBlendState    = &color_blending;
        pipeline_info.pDynamicState       = &dynamic_state;
        pipeline_info.layout              = m_pipeline_layout;
        pipeline_info.renderPass          = m_render_pass;
        pipeline_info.subpass             = 0;
        pipeline_info.basePipelineHandle  = VK_NULL_HANDLE;

        if (vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &m_graphics_pipeline) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create graphics pipeline!");
        }

        vkDestroyShaderModule(m_device, frag_shader_module, nullptr);
        vkDestroyShaderModule(m_device, vert_shader_module, nullptr);
    }

    /// @brief 创建计算管线
    void CreateComputePipeline()
    {
        auto compute_shader_code = ReadFile(PROJECT_ASSETS_DIR "shaders/01_09_base_comp.spv");

        VkShaderModule compute_shader_module = CreateShaderModule(compute_shader_code);

        VkPipelineShaderStageCreateInfo compute_shader_stage_info {};
        compute_shader_stage_info.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        compute_shader_stage_info.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
        compute_shader_stage_info.module = compute_shader_module;
        compute_shader_stage_info.pName  = "main";

        // 计算着色器中的 uniform、buffer
        VkPipelineLayoutCreateInfo pipeline_layout_info {};
        pipeline_layout_info.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_layout_info.setLayoutCount = 1;
        pipeline_layout_info.pSetLayouts    = &m_compute_descriptor_set_layout;

        if (vkCreatePipelineLayout(m_device, &pipeline_layout_info, nullptr, &m_compute_pipeline_layout) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create compute pipeline layout!");
        }

        VkComputePipelineCreateInfo pipeline_info {};
        pipeline_info.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipeline_info.layout = m_compute_pipeline_layout;
        pipeline_info.stage  = compute_shader_stage_info;

        if (vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &m_compute_pipeline) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create compute pipeline!");
        }

        vkDestroyShaderModule(m_device, compute_shader_module, nullptr);
    }

    void CreateFramebuffers()
    {
        m_swap_chain_framebuffers.resize(m_swap_chain_image_views.size());

        for (size_t i = 0; i < m_swap_chain_image_views.size(); i++)
        {
            const std::array<VkImageView, 1> attachments {m_swap_chain_image_views[i]};

            VkFramebufferCreateInfo framebuffer_info {};
            framebuffer_info.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebuffer_info.renderPass      = m_render_pass;
            framebuffer_info.attachmentCount = 1;
            framebuffer_info.pAttachments    = attachments.data();
            framebuffer_info.width           = m_swap_chain_extent.width;
            framebuffer_info.height          = m_swap_chain_extent.height;
            framebuffer_info.layers          = 1;

            if (vkCreateFramebuffer(m_device, &framebuffer_info, nullptr, &m_swap_chain_framebuffers[i]) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to create framebuffer!");
            }
        }
    }

    void CreateCommandPool()
    {
        QueueFamilyIndices queue_family_indices = FindQueueFamilies(m_physical_device);

        VkCommandPoolCreateInfo pool_info {};
        pool_info.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pool_info.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        pool_info.queueFamilyIndex = lvk_tidy::GetRequiredValue(queue_family_indices.graphicsAndComputeFamily);

        if (vkCreateCommandPool(m_device, &pool_info, nullptr, &m_command_pool) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create graphics command pool!");
        }
    }

    /// @brief 顶点 vertex buffer
    void CreateShaderStorageBuffers()
    {
        // Initialize particles
        std::default_random_engine rnd_engine((unsigned)time(nullptr));
        std::uniform_real_distribution<float> rnd_dist(0.0F, 1.0F);

        // Initial particle positions on a circle
        std::vector<Particle> particles(k_particle_count);
        for (auto& particle : particles)
        {
            float r           = 0.25F * sqrt(rnd_dist(rnd_engine));
            float theta       = rnd_dist(rnd_engine) * 2.0F * 3.14159265358979323846F;
            float x           = r * cos(theta) * k_height / k_width;
            float y           = r * sin(theta);
            particle.position = glm::vec2(x, y);
            particle.velocity = glm::normalize(glm::vec2(x, y)) * 0.00025F;
            particle.color    = glm::vec4(rnd_dist(rnd_engine), rnd_dist(rnd_engine), rnd_dist(rnd_engine), 1.0F);
        }

        VkDeviceSize buffer_size = sizeof(Particle) * k_particle_count;

        // Create a staging buffer used to upload data to the gpu
        VkBuffer staging_buffer              = nullptr;
        VkDeviceMemory staging_buffer_memory = nullptr;
        CreateBuffer(
            buffer_size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            staging_buffer,
            staging_buffer_memory
        );

        void* data = nullptr;
        vkMapMemory(m_device, staging_buffer_memory, 0, buffer_size, 0, &data);
        memcpy(data, particles.data(), (size_t)buffer_size);
        vkUnmapMemory(m_device, staging_buffer_memory);

        m_shader_storage_buffers.resize(k_max_frames_in_flight);
        m_shader_storage_buffers_memory.resize(k_max_frames_in_flight);

        // Copy initial particle data to all storage buffers
        for (size_t i = 0; i < k_max_frames_in_flight; i++)
        {
            CreateBuffer(
                buffer_size,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                m_shader_storage_buffers[i],
                m_shader_storage_buffers_memory[i]
            );
            CopyBuffer(staging_buffer, m_shader_storage_buffers[i], buffer_size);
        }

        vkDestroyBuffer(m_device, staging_buffer, nullptr);
        vkFreeMemory(m_device, staging_buffer_memory, nullptr);
    }

    void CreateUniformBuffers()
    {
        VkDeviceSize buffer_size = sizeof(UniformBufferObject);

        m_uniform_buffers.resize(k_max_frames_in_flight);
        m_uniform_buffers_memory.resize(k_max_frames_in_flight);
        m_uniform_buffers_mapped.resize(k_max_frames_in_flight);

        for (size_t i = 0; i < k_max_frames_in_flight; i++)
        {
            CreateBuffer(
                buffer_size,
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                m_uniform_buffers[i],
                m_uniform_buffers_memory[i]
            );

            vkMapMemory(m_device, m_uniform_buffers_memory[i], 0, buffer_size, 0, &m_uniform_buffers_mapped[i]);
        }
    }

    void CreateDescriptorPool()
    {

        std::array<VkDescriptorPoolSize, 2> pool_sizes {};
        // 分配着色器的 uniform
        pool_sizes[0].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        pool_sizes[0].descriptorCount = static_cast<uint32_t>(k_max_frames_in_flight);

        // 分配着色器中的 buffer
        pool_sizes[1].type            = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        pool_sizes[1].descriptorCount = static_cast<uint32_t>(k_max_frames_in_flight) * 2;

        VkDescriptorPoolCreateInfo pool_info {};
        pool_info.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
        pool_info.pPoolSizes    = pool_sizes.data();
        pool_info.maxSets       = static_cast<uint32_t>(k_max_frames_in_flight);

        if (vkCreateDescriptorPool(m_device, &pool_info, nullptr, &m_descriptor_pool) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create descriptor pool!");
        }
    }

    void CreateComputeDescriptorSets()
    {
        std::vector<VkDescriptorSetLayout> layouts(k_max_frames_in_flight, m_compute_descriptor_set_layout);
        VkDescriptorSetAllocateInfo alloc_info {};
        alloc_info.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool     = m_descriptor_pool;
        alloc_info.descriptorSetCount = static_cast<uint32_t>(k_max_frames_in_flight);
        alloc_info.pSetLayouts        = layouts.data();

        m_compute_descriptor_sets.resize(k_max_frames_in_flight);
        if (vkAllocateDescriptorSets(m_device, &alloc_info, m_compute_descriptor_sets.data()) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to allocate descriptor sets!");
        }

        for (size_t i = 0; i < k_max_frames_in_flight; i++)
        {
            std::array<VkWriteDescriptorSet, 3> descriptor_writes {};

            VkDescriptorBufferInfo uniform_buffer_info {};
            uniform_buffer_info.buffer = m_uniform_buffers[i];
            uniform_buffer_info.offset = 0;
            uniform_buffer_info.range  = sizeof(UniformBufferObject);
            // 计算着色器中的 uniform
            descriptor_writes[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptor_writes[0].dstSet          = m_compute_descriptor_sets[i];
            descriptor_writes[0].dstBinding      = 0;
            descriptor_writes[0].dstArrayElement = 0;
            descriptor_writes[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptor_writes[0].descriptorCount = 1;
            descriptor_writes[0].pBufferInfo     = &uniform_buffer_info;

            VkDescriptorBufferInfo storage_buffer_info_last_frame {};
            storage_buffer_info_last_frame.buffer = m_shader_storage_buffers[(i - 1) % k_max_frames_in_flight];
            storage_buffer_info_last_frame.offset = 0;
            storage_buffer_info_last_frame.range  = sizeof(Particle) * k_particle_count;
            // 计算着色器中的 buffer
            descriptor_writes[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptor_writes[1].dstSet          = m_compute_descriptor_sets[i];
            descriptor_writes[1].dstBinding      = 1;
            descriptor_writes[1].dstArrayElement = 0;
            descriptor_writes[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descriptor_writes[1].descriptorCount = 1;
            descriptor_writes[1].pBufferInfo     = &storage_buffer_info_last_frame;

            VkDescriptorBufferInfo storage_buffer_info_current_frame {};
            storage_buffer_info_current_frame.buffer = m_shader_storage_buffers[i];
            storage_buffer_info_current_frame.offset = 0;
            storage_buffer_info_current_frame.range  = sizeof(Particle) * k_particle_count;
            // 计算着色器中的 buffer
            descriptor_writes[2].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptor_writes[2].dstSet          = m_compute_descriptor_sets[i];
            descriptor_writes[2].dstBinding      = 2;
            descriptor_writes[2].dstArrayElement = 0;
            descriptor_writes[2].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descriptor_writes[2].descriptorCount = 1;
            descriptor_writes[2].pBufferInfo     = &storage_buffer_info_current_frame;

            vkUpdateDescriptorSets(m_device, 3, descriptor_writes.data(), 0, nullptr);
        }
    }

    void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& buffer_memory)
    {
        VkBufferCreateInfo buffer_info {};
        buffer_info.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size        = size;
        buffer_info.usage       = usage;
        buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(m_device, &buffer_info, nullptr, &buffer) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create buffer!");
        }

        VkMemoryRequirements mem_requirements;
        vkGetBufferMemoryRequirements(m_device, buffer, &mem_requirements);

        VkMemoryAllocateInfo alloc_info {};
        alloc_info.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize  = mem_requirements.size;
        alloc_info.memoryTypeIndex = FindMemoryType(mem_requirements.memoryTypeBits, properties);

        if (vkAllocateMemory(m_device, &alloc_info, nullptr, &buffer_memory) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to allocate buffer memory!");
        }

        vkBindBufferMemory(m_device, buffer, buffer_memory, 0);
    }

    void CopyBuffer(VkBuffer src_buffer, VkBuffer dst_buffer, VkDeviceSize size)
    {
        VkCommandBufferAllocateInfo alloc_info {};
        alloc_info.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc_info.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.commandPool        = m_command_pool;
        alloc_info.commandBufferCount = 1;

        VkCommandBuffer command_buffer = nullptr;
        vkAllocateCommandBuffers(m_device, &alloc_info, &command_buffer);

        VkCommandBufferBeginInfo begin_info {};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(command_buffer, &begin_info);

        VkBufferCopy copy_region {};
        copy_region.size = size;
        vkCmdCopyBuffer(command_buffer, src_buffer, dst_buffer, 1, &copy_region);

        vkEndCommandBuffer(command_buffer);

        VkSubmitInfo submit_info {};
        submit_info.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers    = &command_buffer;

        vkQueueSubmit(m_graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
        vkQueueWaitIdle(m_graphics_queue);

        vkFreeCommandBuffers(m_device, m_command_pool, 1, &command_buffer);
    }

    uint32_t FindMemoryType(uint32_t type_filter, VkMemoryPropertyFlags properties)
    {
        VkPhysicalDeviceMemoryProperties mem_properties;
        vkGetPhysicalDeviceMemoryProperties(m_physical_device, &mem_properties);

        for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++)
        {
            if ((type_filter & (1 << i)) && (mem_properties.memoryTypes[i].propertyFlags & properties) == properties)
            {
                return i;
            }
        }

        throw std::runtime_error("failed to find suitable memory type!");
    }

    void CreateCommandBuffers()
    {
        m_command_buffers.resize(k_max_frames_in_flight);

        VkCommandBufferAllocateInfo alloc_info {};
        alloc_info.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc_info.commandPool        = m_command_pool;
        alloc_info.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.commandBufferCount = (uint32_t)m_command_buffers.size();

        if (vkAllocateCommandBuffers(m_device, &alloc_info, m_command_buffers.data()) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to allocate command buffers!");
        }
    }

    void CreateComputeCommandBuffers()
    {
        m_compute_command_buffers.resize(k_max_frames_in_flight);

        VkCommandBufferAllocateInfo alloc_info {};
        alloc_info.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc_info.commandPool        = m_command_pool;
        alloc_info.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.commandBufferCount = (uint32_t)m_compute_command_buffers.size();

        if (vkAllocateCommandBuffers(m_device, &alloc_info, m_compute_command_buffers.data()) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to allocate compute command buffers!");
        }
    }

    void RecordCommandBuffer(VkCommandBuffer command_buffer, uint32_t image_index)
    {
        VkCommandBufferBeginInfo begin_info {};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if (vkBeginCommandBuffer(command_buffer, &begin_info) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to begin recording command buffer!");
        }

        VkRenderPassBeginInfo render_pass_info {};
        render_pass_info.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        render_pass_info.renderPass        = m_render_pass;
        render_pass_info.framebuffer       = m_swap_chain_framebuffers[image_index];
        render_pass_info.renderArea.offset = {0, 0};
        render_pass_info.renderArea.extent = m_swap_chain_extent;

        VkClearValue clear_color         = {{{0.0F, 0.0F, 0.0F, 1.0F}}};
        render_pass_info.clearValueCount = 1;
        render_pass_info.pClearValues    = &clear_color;

        vkCmdBeginRenderPass(command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphics_pipeline);

        VkViewport viewport {};
        viewport.x        = 0.0F;
        viewport.y        = 0.0F;
        viewport.width    = (float)m_swap_chain_extent.width;
        viewport.height   = (float)m_swap_chain_extent.height;
        viewport.minDepth = 0.0F;
        viewport.maxDepth = 1.0F;
        vkCmdSetViewport(command_buffer, 0, 1, &viewport);

        VkRect2D scissor {};
        scissor.offset = {0, 0};
        scissor.extent = m_swap_chain_extent;
        vkCmdSetScissor(command_buffer, 0, 1, &scissor);

        const std::array<VkDeviceSize, 1> offsets {0};
        vkCmdBindVertexBuffers(command_buffer, 0, 1, &m_shader_storage_buffers[m_current_frame], offsets.data());

        vkCmdDraw(command_buffer, k_particle_count, 1, 0, 0);

        vkCmdEndRenderPass(command_buffer);

        if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to record command buffer!");
        }
    }

    void RecordComputeCommandBuffer(VkCommandBuffer command_buffer)
    {
        VkCommandBufferBeginInfo begin_info {};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if (vkBeginCommandBuffer(command_buffer, &begin_info) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to begin recording compute command buffer!");
        }

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_compute_pipeline);

        // 计算着色器中的 uniform 和 buffer
        vkCmdBindDescriptorSets(
            command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_compute_pipeline_layout, 0, 1, &m_compute_descriptor_sets[m_current_frame], 0, nullptr
        );

        // 派送计算任务到计算管线，后面三个参数依次是全局工作组大小(XYZ)
        vkCmdDispatch(command_buffer, k_particle_count / 256, 1, 1);

        if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to record compute command buffer!");
        }
    }

    void CreateSyncObjects()
    {
        m_image_available_semaphores.resize(k_max_frames_in_flight);
        m_render_finished_semaphores.resize(k_max_frames_in_flight);
        m_compute_finished_semaphores.resize(k_max_frames_in_flight);
        m_in_flight_fences.resize(k_max_frames_in_flight);
        m_compute_in_flight_fences.resize(k_max_frames_in_flight);

        VkSemaphoreCreateInfo semaphore_info {};
        semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fence_info {};
        fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (size_t i = 0; i < k_max_frames_in_flight; i++)
        {
            if (vkCreateSemaphore(m_device, &semaphore_info, nullptr, &m_image_available_semaphores[i]) != VK_SUCCESS
                || vkCreateSemaphore(m_device, &semaphore_info, nullptr, &m_render_finished_semaphores[i]) != VK_SUCCESS
                || vkCreateFence(m_device, &fence_info, nullptr, &m_in_flight_fences[i]) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to create graphics synchronization objects for a frame!");
            }
            if (vkCreateSemaphore(m_device, &semaphore_info, nullptr, &m_compute_finished_semaphores[i]) != VK_SUCCESS
                || vkCreateFence(m_device, &fence_info, nullptr, &m_compute_in_flight_fences[i]) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to create compute synchronization objects for a frame!");
            }
        }
    }

    void UpdateUniformBuffer(uint32_t current_image)
    {
        UniformBufferObject ubo {};
        ubo.deltaTime = m_last_frame_time * 2.0F;

        memcpy(m_uniform_buffers_mapped[current_image], &ubo, sizeof(ubo));
    }

    void DrawFrame()
    {
        VkSubmitInfo submit_info {};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        //--------------------------------------------------------------------
        // 计算管线
        vkWaitForFences(m_device, 1, &m_compute_in_flight_fences[m_current_frame], VK_TRUE, UINT64_MAX);

        UpdateUniformBuffer(m_current_frame);

        vkResetFences(m_device, 1, &m_compute_in_flight_fences[m_current_frame]);

        vkResetCommandBuffer(m_compute_command_buffers[m_current_frame], /*VkCommandBufferResetFlagBits*/ 0);
        RecordComputeCommandBuffer(m_compute_command_buffers[m_current_frame]);

        submit_info.commandBufferCount   = 1;
        submit_info.pCommandBuffers      = &m_compute_command_buffers[m_current_frame];
        submit_info.signalSemaphoreCount = 1;
        submit_info.pSignalSemaphores    = &m_compute_finished_semaphores[m_current_frame];

        if (vkQueueSubmit(m_compute_queue, 1, &submit_info, m_compute_in_flight_fences[m_current_frame]) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to submit compute command buffer!");
        };

        //--------------------------------------------------------------------
        // 图形管线
        vkWaitForFences(m_device, 1, &m_in_flight_fences[m_current_frame], VK_TRUE, UINT64_MAX);

        uint32_t image_index = 0;
        VkResult result =
            vkAcquireNextImageKHR(m_device, m_swap_chain, UINT64_MAX, m_image_available_semaphores[m_current_frame], VK_NULL_HANDLE, &image_index);

        if (result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            RecreateSwapChain();
            return;
        }
        if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        {
            throw std::runtime_error("failed to acquire swap chain image!");
        }

        vkResetFences(m_device, 1, &m_in_flight_fences[m_current_frame]);

        vkResetCommandBuffer(m_command_buffers[m_current_frame], /*VkCommandBufferResetFlagBits*/ 0);
        RecordCommandBuffer(m_command_buffers[m_current_frame], image_index);

        const std::array<VkSemaphore, 2> wait_semaphores {
            m_compute_finished_semaphores[m_current_frame], m_image_available_semaphores[m_current_frame]
        };
        const std::array<VkPipelineStageFlags, 2> wait_stages {VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submit_info       = {};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        submit_info.waitSemaphoreCount   = 2;
        submit_info.pWaitSemaphores      = wait_semaphores.data();
        submit_info.pWaitDstStageMask    = wait_stages.data();
        submit_info.commandBufferCount   = 1;
        submit_info.pCommandBuffers      = &m_command_buffers[m_current_frame];
        submit_info.signalSemaphoreCount = 1;
        submit_info.pSignalSemaphores    = &m_render_finished_semaphores[m_current_frame];

        if (vkQueueSubmit(m_graphics_queue, 1, &submit_info, m_in_flight_fences[m_current_frame]) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to submit draw command buffer!");
        }

        //--------------------------------------------------------------------
        // 呈现到屏幕
        VkPresentInfoKHR present_info {};
        present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

        present_info.waitSemaphoreCount = 1;
        present_info.pWaitSemaphores    = &m_render_finished_semaphores[m_current_frame];

        const std::array<VkSwapchainKHR, 1> swap_chains {m_swap_chain};
        present_info.swapchainCount = 1;
        present_info.pSwapchains    = swap_chains.data();

        present_info.pImageIndices = &image_index;

        result = vkQueuePresentKHR(m_present_queue, &present_info);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_framebuffer_resized)
        {
            m_framebuffer_resized = false;
            RecreateSwapChain();
        }
        else if (result != VK_SUCCESS)
        {
            throw std::runtime_error("failed to present swap chain image!");
        }

        m_current_frame = (m_current_frame + 1) % k_max_frames_in_flight;
    }

    VkShaderModule CreateShaderModule(const std::vector<char>& code)
    {
        VkShaderModuleCreateInfo create_info {};
        create_info.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        create_info.codeSize = code.size();
        create_info.pCode    = reinterpret_cast<const uint32_t*>(code.data());

        VkShaderModule shader_module = nullptr;
        if (vkCreateShaderModule(m_device, &create_info, nullptr, &shader_module) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create shader module!");
        }

        return shader_module;
    }

    VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& available_formats)
    {
        for (const auto& available_format : available_formats)
        {
            if (available_format.format == VK_FORMAT_B8G8R8A8_SRGB && available_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                return available_format;
            }
        }

        return available_formats[0];
    }

    VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& available_present_modes)
    {
        for (const auto& available_present_mode : available_present_modes)
        {
            if (available_present_mode == VK_PRESENT_MODE_MAILBOX_KHR)
            {
                return available_present_mode;
            }
        }

        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
    {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
        {
            return capabilities.currentExtent;
        }

        int width {0};

        int height {0};
        glfwGetFramebufferSize(m_window, &width, &height);

        VkExtent2D actual_extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};

        actual_extent.width  = std::clamp(actual_extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actual_extent.height = std::clamp(actual_extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return actual_extent;
    }

    SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device)
    {
        SwapChainSupportDetails details;

        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_surface, &details.capabilities);

        uint32_t format_count = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &format_count, nullptr);

        if (format_count != 0)
        {
            details.formats.resize(format_count);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &format_count, details.formats.data());
        }

        uint32_t present_mode_count = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &present_mode_count, nullptr);

        if (present_mode_count != 0)
        {
            details.presentModes.resize(present_mode_count);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &present_mode_count, details.presentModes.data());
        }

        return details;
    }

    bool IsDeviceSuitable(VkPhysicalDevice device)
    {
        QueueFamilyIndices indices = FindQueueFamilies(device);

        bool extensions_supported = CheckDeviceExtensionSupport(device);

        bool swap_chain_adequate = false;
        if (extensions_supported)
        {
            SwapChainSupportDetails swap_chain_support = QuerySwapChainSupport(device);
            swap_chain_adequate                        = !swap_chain_support.formats.empty() && !swap_chain_support.presentModes.empty();
        }

        return indices.IsComplete() && extensions_supported && swap_chain_adequate;
    }

    bool CheckDeviceExtensionSupport(VkPhysicalDevice device)
    {
        uint32_t extension_count = 0;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr);

        std::vector<VkExtensionProperties> available_extensions(extension_count);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, available_extensions.data());

        std::set<std::string> required_extensions(k_device_extensions.begin(), k_device_extensions.end());

        for (const auto& extension : available_extensions)
        {
            required_extensions.erase(static_cast<const char*>(extension.extensionName));
        }

        return required_extensions.empty();
    }

    QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device)
    {
        QueueFamilyIndices indices;

        uint32_t queue_family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);

        std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());

        int i = 0;
        for (const auto& queue_family : queue_families)
        {
            if ((queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) && (queue_family.queueFlags & VK_QUEUE_COMPUTE_BIT))
            {
                indices.graphicsAndComputeFamily = i;
            }

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

            i++;
        }

        return indices;
    }

    std::vector<const char*> GetRequiredExtensions()
    {
        uint32_t glfw_extension_count = 0;
        const char** glfw_extensions  = nullptr;
        glfw_extensions               = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

        std::vector<const char*> extensions(glfw_extensions, glfw_extensions + glfw_extension_count);

        if (k_enable_validation_layers)
        {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        return extensions;
    }

    bool CheckValidationLayerSupport()
    {
        uint32_t layer_count = 0;
        vkEnumerateInstanceLayerProperties(&layer_count, nullptr);

        std::vector<VkLayerProperties> available_layers(layer_count);
        vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());

        for (const char* layer_name : k_validation_layers)
        {
            bool layer_found = false;

            for (const auto& layer_properties : available_layers)
            {
                if (std::strcmp(layer_name, static_cast<const char*>(layer_properties.layerName)) == 0)
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

    static void FramebufferResizeCallback(GLFWwindow* window, int /*width*/, int /*height*/)
    {
        auto app                   = reinterpret_cast<ComputeShaderApplication*>(glfwGetWindowUserPointer(window));
        app->m_framebuffer_resized = true;
    }

    static std::vector<char> ReadFile(const std::string& filename)
    {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);

        if (!file.is_open())
        {
            throw std::runtime_error("failed to open file!");
        }

        size_t file_size = (size_t)file.tellg();
        std::vector<char> buffer(file_size);

        file.seekg(0);
        file.read(buffer.data(), static_cast<std::streamsize>(file_size));

        file.close();

        return buffer;
    }

    static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
        [[maybe_unused]] VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
        [[maybe_unused]] VkDebugUtilsMessageTypeFlagsEXT message_type,
        const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
        [[maybe_unused]] void* user_data
    )
    {
        std::cerr << "validation layer: " << callback_data->pMessage << '\n';

        return VK_FALSE;
    }

private:
    GLFWwindow* m_window;

    VkInstance m_instance;
    VkDebugUtilsMessengerEXT m_debug_messenger;
    VkSurfaceKHR m_surface;

    VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;
    VkDevice m_device;

    VkQueue m_graphics_queue; // 图形队列
    VkQueue m_compute_queue;  // 计算着色器队列
    VkQueue m_present_queue;  // 呈现队列

    VkSwapchainKHR m_swap_chain;
    std::vector<VkImage> m_swap_chain_images;
    VkFormat m_swap_chain_image_format;
    VkExtent2D m_swap_chain_extent;
    std::vector<VkImageView> m_swap_chain_image_views;
    std::vector<VkFramebuffer> m_swap_chain_framebuffers;

    VkRenderPass m_render_pass;
    VkPipelineLayout m_pipeline_layout;
    VkPipeline m_graphics_pipeline;

    VkDescriptorSetLayout m_compute_descriptor_set_layout;
    VkPipelineLayout m_compute_pipeline_layout;
    VkPipeline m_compute_pipeline; // 计算管线

    VkCommandPool m_command_pool;

    std::vector<VkBuffer> m_shader_storage_buffers; // 顶点缓冲
    std::vector<VkDeviceMemory> m_shader_storage_buffers_memory;

    std::vector<VkBuffer> m_uniform_buffers;
    std::vector<VkDeviceMemory> m_uniform_buffers_memory;
    std::vector<void*> m_uniform_buffers_mapped;

    VkDescriptorPool m_descriptor_pool;
    std::vector<VkDescriptorSet> m_compute_descriptor_sets;

    std::vector<VkCommandBuffer> m_command_buffers;         // 图形指令缓冲
    std::vector<VkCommandBuffer> m_compute_command_buffers; // 计算指令缓冲

    std::vector<VkSemaphore> m_image_available_semaphores;
    std::vector<VkSemaphore> m_render_finished_semaphores;
    std::vector<VkSemaphore> m_compute_finished_semaphores;
    std::vector<VkFence> m_in_flight_fences;
    std::vector<VkFence> m_compute_in_flight_fences;
    uint32_t m_current_frame = 0;

    float m_last_frame_time = 0.0F;

    bool m_framebuffer_resized = false;

    double m_last_time = 0.0F;
};

int main()
{
    ComputeShaderApplication app;

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
