#pragma once

#include <vector>

#include "point_cloud_vertex.h"
#include "potree_point_cloud.h"

namespace lvk::point_cloud {
/// @brief 将 Potree 节点读取为当前渲染管线使用的整数顶点。
/// @details
/// - 位置保持 Potree 的 int32 编码值，世界坐标由 GPU 使用 metadata 中的 offset 和 scale 解码。
/// - 优先解码 rgb/rgba 属性；节点没有颜色属性时使用白色。
/// @param point_cloud 节点所属点云，在调用期间必须保持有效。
/// @param node_index 需要读取的节点索引。
/// @return 可用于创建 Vulkan 顶点缓冲区的顶点数组。
[[nodiscard]] std::vector<PointCloudVertex> LoadPotreeNodeVertices(PotreePointCloud& point_cloud,
                                                                   PotreeNodeIndex node_index);
}  // namespace lvk::point_cloud
