#include "potree_lod_selector.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/vec4.hpp>
#include <limits>
#include <optional>
#include <queue>

namespace lvk::point_cloud {
namespace {
struct Plane {
  glm::dvec3 normal{};
  double distance{};
};

using Frustum = std::array<Plane, 6>;

struct LodCandidate {
  PotreeNodeIndex node_index{};
  double screen_spacing_pixels{};
};

struct LodCandidateCompare {
  [[nodiscard]] bool operator()(const LodCandidate& left, const LodCandidate& right) const noexcept {
    if (left.screen_spacing_pixels != right.screen_spacing_pixels) {
      return left.screen_spacing_pixels < right.screen_spacing_pixels;
    }
    return left.node_index > right.node_index;
  }
};

[[nodiscard]] glm::dvec4 GetMatrixRow(const glm::dmat4& matrix, glm::length_t row_index) noexcept {
  return {matrix[0][row_index], matrix[1][row_index], matrix[2][row_index], matrix[3][row_index]};
}

[[nodiscard]] Plane NormalizePlane(glm::dvec4 coefficients) noexcept {
  const glm::dvec3 normal{coefficients};
  const double normal_length{glm::length(normal)};
  if (normal_length <= std::numeric_limits<double>::epsilon()) {
    return {};
  }
  return {normal / normal_length, coefficients.w / normal_length};
}

[[nodiscard]] Frustum BuildFrustum(const glm::dmat4& view_projection) noexcept {
  const glm::dvec4 row_0{GetMatrixRow(view_projection, 0)};
  const glm::dvec4 row_1{GetMatrixRow(view_projection, 1)};
  const glm::dvec4 row_2{GetMatrixRow(view_projection, 2)};
  const glm::dvec4 row_3{GetMatrixRow(view_projection, 3)};

  // Vulkan 的 NDC 深度范围是 [0, 1]，因此 near plane 直接取 row_2。
  return {
    NormalizePlane(row_3 + row_0), NormalizePlane(row_3 - row_0), NormalizePlane(row_3 + row_1),
    NormalizePlane(row_3 - row_1), NormalizePlane(row_2),         NormalizePlane(row_3 - row_2),
  };
}

[[nodiscard]] bool IntersectsFrustum(const BoundingBox& bounds, const Frustum& frustum) noexcept {
  const glm::dvec3 bounds_min{bounds.GetMin()};
  const glm::dvec3 bounds_max{bounds.GetMax()};
  for (const Plane& plane : frustum) {
    const glm::dvec3 positive_vertex{
      plane.normal.x >= 0.0 ? bounds_max.x : bounds_min.x,
      plane.normal.y >= 0.0 ? bounds_max.y : bounds_min.y,
      plane.normal.z >= 0.0 ? bounds_max.z : bounds_min.z,
    };
    if (glm::dot(plane.normal, positive_vertex) + plane.distance < 0.0) {
      return false;
    }
  }
  return true;
}

[[nodiscard]] double GetDistanceToBounds(glm::dvec3 camera_position, const BoundingBox& bounds) noexcept {
  const glm::dvec3 closest_point{glm::clamp(camera_position, bounds.GetMin(), bounds.GetMax())};
  return std::max(glm::length(camera_position - closest_point), 1e-6);
}

[[nodiscard]] double GetScreenSpacingPixels(const PotreeMetadataInfo& metadata, const PotreeNodeInfo& node,
                                            const PotreeLodSelectionParams& params) noexcept {
  const double node_spacing{metadata.spacing * std::pow(0.5, static_cast<double>(node.level))};
  const double distance{GetDistanceToBounds(params.camera_position, node.bounds)};
  return node_spacing * params.focal_length_pixels / distance;
}

[[nodiscard]] bool CanAddPoints(std::uint64_t selected_point_count, std::uint64_t additional_point_count,
                                std::uint64_t point_budget) noexcept {
  return selected_point_count <= point_budget && additional_point_count <= point_budget - selected_point_count;
}
}  // namespace

PotreeLodSelectionInfo SelectPotreeLod(const PotreePointCloud& point_cloud, const PotreeLodSelectionParams& params) {
  PotreeLodSelectionInfo selection{};
  if (params.focal_length_pixels <= 0.0 || params.target_pixel_spacing <= 0.0 || params.point_budget == 0) {
    return selection;
  }

  const Frustum frustum{BuildFrustum(params.view_projection)};
  const PotreeMetadataInfo& metadata{point_cloud.GetMetadata()};
  const PotreeNodeIndex root_node_index{point_cloud.GetRootNodeIndex()};
  const PotreeNodeInfo& root_node{point_cloud.GetNode(root_node_index)};
  if (!IntersectsFrustum(root_node.bounds, frustum)) {
    return selection;
  }

  selection.node_indices.push_back(root_node_index);
  selection.point_count = root_node.point_count;

  std::priority_queue<LodCandidate, std::vector<LodCandidate>, LodCandidateCompare> candidates{};
  const double root_screen_spacing_pixels{GetScreenSpacingPixels(metadata, root_node, params)};
  if (root_screen_spacing_pixels > params.target_pixel_spacing) {
    for (const std::optional<PotreeNodeIndex> child_index : root_node.child_indices) {
      if (!child_index.has_value()) {
        continue;
      }

      const PotreeNodeInfo& child{point_cloud.GetNode(*child_index)};
      if (IntersectsFrustum(child.bounds, frustum)) {
        candidates.push({*child_index, GetScreenSpacingPixels(metadata, child, params)});
      }
    }
  }

  while (!candidates.empty()) {
    const LodCandidate candidate{candidates.top()};
    candidates.pop();

    const PotreeNodeInfo& node{point_cloud.GetNode(candidate.node_index)};
    if (!CanAddPoints(selection.point_count, node.point_count, params.point_budget)) {
      // 当前节点具有最高屏幕空间误差；停止选择可避免低优先级节点占用剩余预算。
      break;
    }

    selection.node_indices.push_back(candidate.node_index);
    selection.point_count += node.point_count;

    if (candidate.screen_spacing_pixels <= params.target_pixel_spacing) {
      continue;
    }

    for (const std::optional<PotreeNodeIndex> child_index : node.child_indices) {
      if (!child_index.has_value()) {
        continue;
      }

      const PotreeNodeInfo& child{point_cloud.GetNode(*child_index)};
      if (!IntersectsFrustum(child.bounds, frustum)) {
        continue;
      }
      candidates.push({*child_index, GetScreenSpacingPixels(metadata, child, params)});
    }
  }

  return selection;
}
}  // namespace lvk::point_cloud
