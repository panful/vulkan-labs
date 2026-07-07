#pragma once

#include <vulkan/vulkan.h>

#include <string_view>
#include <vector>

namespace lvk {
/// @brief 按二进制方式读取文件内容。
/// @throws std::runtime_error 文件无法打开、查询大小失败或读取失败时抛出。
[[nodiscard]] std::vector<char> ReadBinaryFile(std::string_view file_path);

/// @brief 从 SPIR-V 字节码创建 Vulkan shader module。
/// @details 函数会校验字节码非空、4 字节对齐并检查 SPIR-V magic number。
/// @throws std::runtime_error 字节码不是有效 SPIR-V 或 Vulkan 创建失败时抛出。
[[nodiscard]] VkShaderModule CreateShaderModule(VkDevice device, const std::vector<char>& shader_code);
}  // namespace lvk
