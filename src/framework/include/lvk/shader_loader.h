#pragma once

#include <vulkan/vulkan.h>

#include <string_view>
#include <vector>

namespace lvk {
[[nodiscard]] std::vector<char> ReadBinaryFile(std::string_view file_path);
[[nodiscard]] VkShaderModule CreateShaderModule(VkDevice device, const std::vector<char>& shader_code);
}  // namespace lvk
