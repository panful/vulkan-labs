#include "bounding_box.h"

#include <algorithm>
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <stdexcept>

namespace lvk::point_cloud {
BoundingBox::BoundingBox(glm::dvec3 min, glm::dvec3 max) : m_min{min}, m_max{max} {
  const bool is_valid = min.x <= max.x && min.y <= max.y && min.z <= max.z;
  if (!is_valid) {
    throw std::invalid_argument{"Bounding box minimum must not exceed maximum"};
  }
}

glm::dvec3 BoundingBox::GetMin() const noexcept { return m_min; }

glm::dvec3 BoundingBox::GetMax() const noexcept { return m_max; }

glm::dvec3 BoundingBox::GetBoundsCenter() const noexcept { return (m_min + m_max) * 0.5; }

double BoundingBox::GetBoundsRadius() const noexcept { return std::max(glm::length((m_max - m_min) * 0.5), 0.001); }

void BoundingBox::Expand(glm::dvec3 point) noexcept {
  m_min = glm::min(m_min, point);
  m_max = glm::max(m_max, point);
}
}  // namespace lvk::point_cloud
