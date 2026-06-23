#include "lvk/shader_loader.h"

#include <fstream>
#include <stdexcept>
#include <string>

#include "lvk/vk_utils.h"

namespace lvk {
std::vector<char> ReadBinaryFile(std::string_view file_path) {
  std::ifstream file{std::string(file_path), std::ios::ate | std::ios::binary};
  if (!file.is_open()) {
    throw std::runtime_error("failed to open file: " + std::string(file_path));
  }

  const auto file_size{file.tellg()};
  if (file_size < 0) {
    throw std::runtime_error("failed to query file size: " + std::string(file_path));
  }

  std::vector<char> buffer(static_cast<size_t>(file_size));
  file.seekg(0);
  file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
  return buffer;
}

VkShaderModule CreateShaderModule(VkDevice device, const std::vector<char>& shader_code) {
  VkShaderModuleCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  create_info.codeSize = shader_code.size();
  create_info.pCode = reinterpret_cast<const uint32_t*>(shader_code.data());

  VkShaderModule shader_module{VK_NULL_HANDLE};
  CheckVkResult(vkCreateShaderModule(device, &create_info, nullptr, &shader_module), "failed to create shader module");
  return shader_module;
}
}  // namespace lvk
