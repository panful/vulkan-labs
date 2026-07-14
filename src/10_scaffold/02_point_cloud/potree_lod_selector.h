#pragma once

#include <cstdint>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <vector>

#include "potree_point_cloud.h"

namespace lvk::point_cloud {
struct PotreeLodSelectionParams {
  glm::dmat4 view_projection{1.0};
  glm::dvec3 camera_position{};
  double focal_length_pixels{};
  double target_pixel_spacing{1.5};
  std::uint64_t point_budget{2'000'000};
};

struct PotreeLodSelectionInfo {
  std::vector<PotreeNodeIndex> node_indices{};
  std::uint64_t point_count{};
};

/// @brief 根据视锥体、屏幕空间点间距和点数预算选择 Potree 加法式 LOD 节点。
/// @details
/// - 返回结果包含被细化节点的父节点。
/// - 可见子节点按屏幕空间误差逐个选择，使有限预算优先用于投影间距较大的区域。
/// - 最高优先级节点无法加入时停止选择，不再用低优先级节点填补剩余预算。
/// @param point_cloud 提供只读层级和节点包围盒，在调用期间必须保持有效。
/// @param params 当前帧相机与 LOD 参数。
/// @return 按选择优先级排列的节点及其总点数。
[[nodiscard]] PotreeLodSelectionInfo SelectPotreeLod(const PotreePointCloud& point_cloud,
                                                     const PotreeLodSelectionParams& params);
}  // namespace lvk::point_cloud
