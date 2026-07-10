#pragma once

#include <vector>

#include "point_cloud_vertex.h"
#include "potree_point_cloud.h"

namespace lvk::point_cloud {
/// @brief 将已加载的 Potree 节点解码为当前渲染管线使用的顶点。
/// @details 当前实现解码 int32 position 属性；数据集没有颜色属性时使用白色。
/// @param point_cloud 节点所属点云，在调用期间必须保持有效。
/// @param node_index 已通过 PotreePointCloud::LoadNodeData 加载的节点索引。
/// @return 可用于创建 Vulkan 顶点缓冲区的顶点数组。
[[nodiscard]] std::vector<PointCloudVertex> LoadAndDecodePotreeNodeVertices(PotreePointCloud& point_cloud,
                                                                            PotreeNodeIndex node_index);
}  // namespace lvk::point_cloud
