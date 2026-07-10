#pragma once

#include <glm/vec3.hpp>

namespace lvk::point_cloud {
/// @brief 表示点云或八叉树节点的轴对齐包围盒。
class BoundingBox final {
public:
  BoundingBox() = default;

  /// @brief 使用最小点和最大点创建包围盒。
  /// @throws std::invalid_argument 任意轴的最小值大于最大值时抛出异常。
  BoundingBox(glm::dvec3 min, glm::dvec3 max);

  [[nodiscard]] glm::dvec3 GetMin() const noexcept;
  [[nodiscard]] glm::dvec3 GetMax() const noexcept;
  [[nodiscard]] glm::dvec3 GetBoundsCenter() const noexcept;
  [[nodiscard]] double GetBoundsRadius() const noexcept;

  void Expand(glm::dvec3 point) noexcept;

private:
  glm::dvec3 m_min{-1.0};
  glm::dvec3 m_max{1.0};
};
}  // namespace lvk::point_cloud
