#include "lvk/shader_loader.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>

#include "lvk/vk_utils.h"

namespace lvk {
namespace {
constexpr uint32_t k_spirv_magic_number{0x07230203};
constexpr size_t k_spirv_word_size{sizeof(uint32_t)};

[[nodiscard]] std::vector<uint32_t> ToSpirvWords(const std::vector<char>& shader_code) {
  if (shader_code.empty()) {
    throw std::runtime_error("shader code is empty");
  }
  if (0U != shader_code.size() % k_spirv_word_size) {
    throw std::runtime_error("shader code size is not aligned to SPIR-V word size");
  }

  std::vector<uint32_t> shader_words(shader_code.size() / k_spirv_word_size);
  // vector<char> 的 data 对齐不保证满足 uint32_t 要求，拷贝到 uint32_t 缓冲区后再交给 Vulkan。
  std::memcpy(shader_words.data(), shader_code.data(), shader_code.size());
  if (shader_words.front() != k_spirv_magic_number) {
    throw std::runtime_error("shader code is not a SPIR-V binary");
  }

  return shader_words;
}
}  // namespace

std::vector<char> ReadBinaryFile(std::string_view file_path) {
  std::ifstream file{std::string(file_path), std::ios::ate | std::ios::binary};
  if (!file.is_open()) {
    throw std::runtime_error("failed to open file: " + std::string(file_path));
  }

  const auto file_size{file.tellg()};
  if (file_size < 0) {
    throw std::runtime_error("failed to query file size: " + std::string(file_path));
  }

  // 使用 ate 先拿大小，再一次性读取，适合 SPIR-V 这类小型二进制资源。
  std::vector<char> buffer(static_cast<size_t>(file_size));
  file.seekg(0);
  file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
  if (!file) {
    throw std::runtime_error("failed to read file: " + std::string(file_path));
  }
  return buffer;
}

VkShaderModule CreateShaderModule(VkDevice device, const std::vector<char>& shader_code) {
  const std::vector<uint32_t> shader_words{ToSpirvWords(shader_code)};

  VkShaderModuleCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  create_info.codeSize = shader_words.size() * k_spirv_word_size;
  create_info.pCode = shader_words.data();

  VkShaderModule shader_module{VK_NULL_HANDLE};
  CheckVkResult(vkCreateShaderModule(device, &create_info, nullptr, &shader_module), "failed to create shader module");
  return shader_module;
}
}  // namespace lvk
