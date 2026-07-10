#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "bounding_box.h"

namespace lvk::point_cloud {
using PotreeNodeIndex = std::size_t;

enum class PotreeNodeType : std::uint8_t { Normal = 0, Leaf = 1, Proxy = 2 };

struct Vector3d {
  double x{};
  double y{};
  double z{};
};

struct PotreeAttributeInfo {
  std::string name{};
  std::string type{};
  std::size_t byte_offset{};
  std::size_t size_bytes{};
  std::size_t element_count{};
  std::size_t element_size_bytes{};
};

struct PotreeMetadataInfo {
  std::string version{};
  std::string name{};
  std::string encoding{};
  std::uint64_t point_count{};
  std::uint64_t first_hierarchy_chunk_size_bytes{};
  double spacing{};
  Vector3d offset{};
  Vector3d scale{};
  BoundingBox bounds{};
  std::vector<PotreeAttributeInfo> attributes{};
  std::size_t point_record_size_bytes{};
};

struct PotreeNodeInfo {
  static constexpr std::size_t k_child_count{8};

  std::string name{};
  PotreeNodeType type{PotreeNodeType::Leaf};
  std::uint8_t child_mask{};
  std::size_t level{};
  std::uint32_t point_count{};
  std::uint64_t byte_offset{};
  std::uint64_t byte_size{};
  BoundingBox bounds{};
  std::optional<PotreeNodeIndex> parent_index{};
  std::array<std::optional<PotreeNodeIndex>, k_child_count> child_indices{};
  std::vector<std::byte> point_bytes{};
  bool is_data_loaded{false};
};

/// @brief 管理一个 PotreeConverter 2.x 点云数据集。
/// @details
/// - 构造时只读取元数据和完整层级。
/// - 点数据可按节点加载和卸载，便于后续实现 LOD 流式管理。
/// - 加载和卸载接口会修改内部节点状态，同一个对象不能被多个线程并发修改。
class PotreePointCloud final {
public:
  /// @brief 打开指定的 PotreeConverter 输出目录。
  /// @param directory 包含 metadata.json、hierarchy.bin 和 octree.bin 的目录。
  /// @throws std::exception 文件缺失、格式错误或层级无效时抛出异常。
  explicit PotreePointCloud(std::filesystem::path directory);
  ~PotreePointCloud() = default;

  PotreePointCloud(const PotreePointCloud&) = delete;
  PotreePointCloud& operator=(const PotreePointCloud&) = delete;
  PotreePointCloud(PotreePointCloud&&) noexcept = default;
  PotreePointCloud& operator=(PotreePointCloud&&) noexcept = default;

  [[nodiscard]] const std::filesystem::path& GetDirectory() const noexcept;
  [[nodiscard]] const PotreeMetadataInfo& GetMetadata() const noexcept;

  /// @brief 返回全部实际节点，不包含用于跳转层级块的代理记录。
  /// @return 只读节点视图；其生命周期不超过当前点云对象。
  [[nodiscard]] std::span<const PotreeNodeInfo> GetNodes() const noexcept;

  /// @brief 按稳定索引取得节点。
  /// @throws std::out_of_range `node_index` 超出节点范围时抛出异常。
  [[nodiscard]] const PotreeNodeInfo& GetNode(PotreeNodeIndex node_index) const;
  [[nodiscard]] PotreeNodeIndex GetRootNodeIndex() const noexcept;
  [[nodiscard]] std::optional<PotreeNodeIndex> TryFindNodeIndex(std::string_view node_name) const;

  /// @brief 从 octree.bin 加载一个节点的交错属性记录。
  /// @details 重复加载已经驻留的节点不会再次执行文件读取；失败时该节点保持未加载状态。
  void LoadNodeData(PotreeNodeIndex node_index);

  /// @brief 加载全部节点数据。
  /// @details 读取失败时，失败前已成功加载的节点仍保持加载状态。
  void LoadAllNodeData();
  void UnloadNodeData(PotreeNodeIndex node_index);
  void UnloadAllNodeData();

  [[nodiscard]] std::uint64_t GetLoadedPointCount() const noexcept;
  [[nodiscard]] std::uint64_t GetLoadedByteCount() const noexcept;

  /// @brief 将点云元数据、属性布局和节点信息写入指定输出流。
  /// @param output 接收文本信息的输出流，其生命周期必须覆盖本次调用。
  void PrintPointCloud(std::ostream& output) const;

private:
  void LoadMetadata();
  void LoadHierarchy();
  void BuildNodeLinks();

private:
  std::filesystem::path m_directory{};
  PotreeMetadataInfo m_metadata{};
  std::vector<PotreeNodeInfo> m_nodes{};
  std::unordered_map<std::string, PotreeNodeIndex> m_node_indices{};
  PotreeNodeIndex m_root_node_index{};
};
}  // namespace lvk::point_cloud
