#include "lvk/vulkan_context.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstring>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>

#include "lvk/glfw_window.h"
#include "lvk/vk_utils.h"

namespace lvk {
namespace {
constexpr const char* k_validation_layer_name{"VK_LAYER_KHRONOS_validation"};

void AppendUnique(std::vector<const char*>& values, const char* value) {
  const auto iter{std::ranges::find_if(values, [value](const char* entry) { return 0 == std::strcmp(entry, value); })};
  if (iter == values.end()) {
    values.emplace_back(value);
  }
}
}  // namespace

bool QueueFamilyIndices::IsComplete() const noexcept { return has_graphics_family && has_present_family; }

VulkanContext::VulkanContext(const ApplicationDesc& desc, const GlfwWindow& window)
    : m_enable_validation_layers(desc.enable_validation_layers),
      m_device_features(desc.device_features),
      m_validation_layers{k_validation_layer_name},
      m_device_extensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME} {
  for (const char* extension : desc.device_extensions) {
    AppendUnique(m_device_extensions, extension);
  }

  CreateInstance(desc);
  SetupDebugMessenger();
  CreateSurface(window);
  PickPhysicalDevice();
  CreateLogicalDevice();
}

VulkanContext::~VulkanContext() noexcept {
  if (VK_NULL_HANDLE != m_device) {
    vkDestroyDevice(m_device, nullptr);
    m_device = VK_NULL_HANDLE;
  }

  if (VK_NULL_HANDLE != m_surface) {
    vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
    m_surface = VK_NULL_HANDLE;
  }

  if (m_enable_validation_layers && VK_NULL_HANDLE != m_debug_messenger) {
    DestroyDebugUtilsMessenger(m_instance, m_debug_messenger, nullptr);
    m_debug_messenger = VK_NULL_HANDLE;
  }

  if (VK_NULL_HANDLE != m_instance) {
    vkDestroyInstance(m_instance, nullptr);
    m_instance = VK_NULL_HANDLE;
  }
}

VkInstance VulkanContext::GetInstance() const noexcept { return m_instance; }

VkSurfaceKHR VulkanContext::GetSurface() const noexcept { return m_surface; }

VkPhysicalDevice VulkanContext::GetPhysicalDevice() const noexcept { return m_physical_device; }

VkDevice VulkanContext::GetDevice() const noexcept { return m_device; }

VkQueue VulkanContext::GetGraphicsQueue() const noexcept { return m_graphics_queue; }

VkQueue VulkanContext::GetPresentQueue() const noexcept { return m_present_queue; }

uint32_t VulkanContext::GetGraphicsQueueFamily() const noexcept { return m_queue_family_indices.graphics_family; }

uint32_t VulkanContext::GetPresentQueueFamily() const noexcept { return m_queue_family_indices.present_family; }

const std::vector<const char*>& VulkanContext::GetDeviceExtensions() const noexcept { return m_device_extensions; }

bool VulkanContext::IsValidationEnabled() const noexcept { return m_enable_validation_layers; }

void VulkanContext::CreateInstance(const ApplicationDesc& desc) {
  if (m_enable_validation_layers && !CheckValidationLayerSupport()) {
    throw std::runtime_error("validation layers requested, but not available");
  }

  VkApplicationInfo app_info{};
  app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app_info.pApplicationName = desc.title.c_str();
  app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  app_info.pEngineName = "Learning Vulkan Sample Framework";
  app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  app_info.apiVersion = desc.api_version;

  const std::vector<const char*> required_extensions{GetRequiredInstanceExtensions(desc)};

  VkInstanceCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  create_info.pApplicationInfo = &app_info;
  create_info.enabledExtensionCount = static_cast<uint32_t>(required_extensions.size());
  create_info.ppEnabledExtensionNames = required_extensions.data();

  VkDebugUtilsMessengerCreateInfoEXT debug_create_info{};
  if (m_enable_validation_layers) {
    PopulateDebugMessengerCreateInfo(debug_create_info);
    create_info.enabledLayerCount = static_cast<uint32_t>(m_validation_layers.size());
    create_info.ppEnabledLayerNames = m_validation_layers.data();
    create_info.pNext = &debug_create_info;
  }

  CheckVkResult(vkCreateInstance(&create_info, nullptr, &m_instance), "failed to create Vulkan instance");
}

void VulkanContext::SetupDebugMessenger() {
  if (!m_enable_validation_layers) {
    return;
  }

  VkDebugUtilsMessengerCreateInfoEXT create_info{};
  PopulateDebugMessengerCreateInfo(create_info);
  CheckVkResult(CreateDebugUtilsMessenger(m_instance, &create_info, nullptr, &m_debug_messenger),
                "failed to create debug messenger");
}

void VulkanContext::CreateSurface(const GlfwWindow& window) {
  CheckVkResult(glfwCreateWindowSurface(m_instance, window.GetNativeWindow(), nullptr, &m_surface),
                "failed to create window surface");
}

void VulkanContext::PickPhysicalDevice() {
  uint32_t device_count{};
  CheckVkResult(vkEnumeratePhysicalDevices(m_instance, &device_count, nullptr), "failed to enumerate physical devices");

  if (0 == device_count) {
    throw std::runtime_error("failed to find GPUs with Vulkan support");
  }

  std::vector<VkPhysicalDevice> devices(device_count);
  CheckVkResult(vkEnumeratePhysicalDevices(m_instance, &device_count, devices.data()),
                "failed to enumerate physical devices");

  for (VkPhysicalDevice physical_device : devices) {
    if (IsDeviceSuitable(physical_device)) {
      m_physical_device = physical_device;
      m_queue_family_indices = FindQueueFamilies(physical_device);
      return;
    }
  }

  throw std::runtime_error("failed to find a suitable GPU");
}

void VulkanContext::CreateLogicalDevice() {
  std::vector<VkDeviceQueueCreateInfo> queue_create_infos{};
  std::set<uint32_t> unique_queue_families{m_queue_family_indices.graphics_family,
                                           m_queue_family_indices.present_family};

  float queue_priority{1.0F};
  for (uint32_t queue_family : unique_queue_families) {
    VkDeviceQueueCreateInfo queue_create_info{};
    queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create_info.queueFamilyIndex = queue_family;
    queue_create_info.queueCount = 1;
    queue_create_info.pQueuePriorities = &queue_priority;
    queue_create_infos.emplace_back(queue_create_info);
  }

  VkDeviceCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
  create_info.pQueueCreateInfos = queue_create_infos.data();
  create_info.pEnabledFeatures = &m_device_features;
  create_info.enabledExtensionCount = static_cast<uint32_t>(m_device_extensions.size());
  create_info.ppEnabledExtensionNames = m_device_extensions.data();

  if (m_enable_validation_layers) {
    create_info.enabledLayerCount = static_cast<uint32_t>(m_validation_layers.size());
    create_info.ppEnabledLayerNames = m_validation_layers.data();
  }

  CheckVkResult(vkCreateDevice(m_physical_device, &create_info, nullptr, &m_device), "failed to create logical device");
  vkGetDeviceQueue(m_device, m_queue_family_indices.graphics_family, 0, &m_graphics_queue);
  vkGetDeviceQueue(m_device, m_queue_family_indices.present_family, 0, &m_present_queue);
}

bool VulkanContext::CheckValidationLayerSupport() const {
  uint32_t layer_count{};
  CheckVkResult(vkEnumerateInstanceLayerProperties(&layer_count, nullptr), "failed to enumerate validation layers");

  std::vector<VkLayerProperties> available_layers(layer_count);
  CheckVkResult(vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data()),
                "failed to enumerate validation layers");

  for (const char* layer_name : m_validation_layers) {
    const bool found{std::ranges::any_of(available_layers, [layer_name](const VkLayerProperties& layer_properties) {
      return 0 ==
             std::strcmp(layer_name,
                         layer_properties.layerName);  // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
    })};
    if (!found) {
      return false;
    }
  }

  return true;
}

std::vector<const char*> VulkanContext::GetRequiredInstanceExtensions(const ApplicationDesc& desc) const {
  uint32_t glfw_extension_count{};
  const char** glfw_extensions{glfwGetRequiredInstanceExtensions(&glfw_extension_count)};
  if (nullptr == glfw_extensions) {
    throw std::runtime_error("failed to get GLFW required instance extensions");
  }

  std::vector<const char*> extensions(glfw_extensions, glfw_extensions + glfw_extension_count);
  if (m_enable_validation_layers) {
    AppendUnique(extensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  }

  for (const char* extension : desc.instance_extensions) {
    AppendUnique(extensions, extension);
  }

  return extensions;
}

QueueFamilyIndices VulkanContext::FindQueueFamilies(VkPhysicalDevice physical_device) const {
  QueueFamilyIndices indices{};

  uint32_t queue_family_count{};
  vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, nullptr);

  std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
  vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, queue_families.data());

  for (uint32_t i{}; i < queue_families.size(); ++i) {
    if (0U != (queue_families.at(i).queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
      indices.graphics_family = i;
      indices.has_graphics_family = true;
    }

    VkBool32 present_support{VK_FALSE};
    CheckVkResult(vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, m_surface, &present_support),
                  "failed to query surface support");
    if (VK_TRUE == present_support) {
      indices.present_family = i;
      indices.has_present_family = true;
    }

    if (indices.IsComplete()) {
      break;
    }
  }

  return indices;
}

bool VulkanContext::IsDeviceSuitable(VkPhysicalDevice physical_device) const {
  const QueueFamilyIndices indices{FindQueueFamilies(physical_device)};
  if (!indices.IsComplete() || !CheckDeviceExtensionSupport(physical_device)) {
    return false;
  }

  uint32_t format_count{};
  vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, m_surface, &format_count, nullptr);
  uint32_t present_mode_count{};
  vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, m_surface, &present_mode_count, nullptr);
  return 0 != format_count && 0 != present_mode_count;
}

bool VulkanContext::CheckDeviceExtensionSupport(VkPhysicalDevice physical_device) const {
  uint32_t extension_count{};
  CheckVkResult(vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extension_count, nullptr),
                "failed to enumerate device extensions");

  std::vector<VkExtensionProperties> available_extensions(extension_count);
  CheckVkResult(
    vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extension_count, available_extensions.data()),
    "failed to enumerate device extensions");

  std::set<std::string> required_extensions(m_device_extensions.begin(), m_device_extensions.end());
  for (const VkExtensionProperties& extension : available_extensions) {
    required_extensions.erase(extension.extensionName);  // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
  }

  return required_extensions.empty();
}

void VulkanContext::PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& create_info) noexcept {
  create_info = {};
  create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  create_info.messageSeverity =
    VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  create_info.pfnUserCallback = DebugCallback;
}

VkBool32 VulkanContext::DebugCallback([[maybe_unused]] VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
                                      [[maybe_unused]] VkDebugUtilsMessageTypeFlagsEXT message_type,
                                      const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
                                      [[maybe_unused]] void* user_data) noexcept {
  std::clog << "Vulkan validation: " << callback_data->pMessage << '\n';
  return VK_FALSE;
}

VkResult VulkanContext::CreateDebugUtilsMessenger(VkInstance instance,
                                                  const VkDebugUtilsMessengerCreateInfoEXT* create_info,
                                                  const VkAllocationCallbacks* allocator,
                                                  VkDebugUtilsMessengerEXT* debug_messenger) noexcept {
  auto function{
    LoadInstanceProcAddress<PFN_vkCreateDebugUtilsMessengerEXT>(instance, "vkCreateDebugUtilsMessengerEXT")};
  if (nullptr == function) {
    return VK_ERROR_EXTENSION_NOT_PRESENT;
  }

  return function(instance, create_info, allocator, debug_messenger);
}

void VulkanContext::DestroyDebugUtilsMessenger(VkInstance instance, VkDebugUtilsMessengerEXT debug_messenger,
                                               const VkAllocationCallbacks* allocator) noexcept {
  auto function{
    LoadInstanceProcAddress<PFN_vkDestroyDebugUtilsMessengerEXT>(instance, "vkDestroyDebugUtilsMessengerEXT")};
  if (nullptr != function) {
    function(instance, debug_messenger, allocator);
  }
}
}  // namespace lvk
