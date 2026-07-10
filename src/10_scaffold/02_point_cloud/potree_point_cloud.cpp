#include "potree_point_cloud.h"

#include <algorithm>
#include <deque>
#include <fstream>
#include <glm/vec3.hpp>
#include <iomanip>
#include <limits>
#include <ostream>
#include <span>
#include <stdexcept>
#include <tiny_gltf/json.hpp>
#include <type_traits>
#include <unordered_set>
#include <utility>

namespace lvk::point_cloud {
namespace {
using Json = nlohmann::json;

constexpr std::size_t k_hierarchy_record_size_bytes{22};

struct HierarchyChunkInfo {
  std::string root_node_name{};
  std::uint64_t byte_offset{};
  std::uint64_t byte_size{};
};

[[nodiscard]] std::string_view GetNodeTypeName(PotreeNodeType node_type) noexcept {
  switch (node_type) {
    case PotreeNodeType::Normal:
      return "normal";
    case PotreeNodeType::Leaf:
      return "leaf";
    case PotreeNodeType::Proxy:
      return "proxy";
  }
  return "unknown";
}

void PrintVector3(std::ostream& output, Vector3d value) {
  output << '(' << value.x << ", " << value.y << ", " << value.z << ')';
}

void PrintVector3(std::ostream& output, glm::dvec3 value) {
  output << '(' << value.x << ", " << value.y << ", " << value.z << ')';
}

[[nodiscard]] Vector3d ParseVector3(const Json& json) {
  if (!json.is_array() || json.size() != 3) {
    throw std::runtime_error{"Expected a JSON array containing three numbers"};
  }

  return {
    json.at(0).get<double>(),
    json.at(1).get<double>(),
    json.at(2).get<double>(),
  };
}

[[nodiscard]] glm::dvec3 ToGlmVector(Vector3d value) { return {value.x, value.y, value.z}; }

[[nodiscard]] PotreeMetadataInfo ParseMetadata(const std::filesystem::path& metadata_path) {
  std::ifstream metadata_file{metadata_path};
  if (!metadata_file) {
    throw std::runtime_error{"Failed to open metadata file: " + metadata_path.string()};
  }

  Json json{};
  metadata_file >> json;

  PotreeMetadataInfo metadata{};
  metadata.version = json.at("version").get<std::string>();
  metadata.name = json.at("name").get<std::string>();
  metadata.encoding = json.at("encoding").get<std::string>();
  metadata.point_count = json.at("points").get<std::uint64_t>();
  metadata.first_hierarchy_chunk_size_bytes = json.at("hierarchy").at("firstChunkSize").get<std::uint64_t>();
  metadata.spacing = json.at("spacing").get<double>();
  metadata.offset = ParseVector3(json.at("offset"));
  metadata.scale = ParseVector3(json.at("scale"));
  metadata.bounds = BoundingBox{ToGlmVector(ParseVector3(json.at("boundingBox").at("min"))),
                                ToGlmVector(ParseVector3(json.at("boundingBox").at("max")))};

  for (const Json& attribute_json : json.at("attributes")) {
    PotreeAttributeInfo attribute{};
    attribute.name = attribute_json.at("name").get<std::string>();
    attribute.type = attribute_json.at("type").get<std::string>();
    attribute.byte_offset = metadata.point_record_size_bytes;
    attribute.size_bytes = attribute_json.at("size").get<std::size_t>();
    attribute.element_count = attribute_json.at("numElements").get<std::size_t>();
    attribute.element_size_bytes = attribute_json.at("elementSize").get<std::size_t>();

    const bool has_point_size_overflow =
      attribute.size_bytes > std::numeric_limits<std::size_t>::max() - metadata.point_record_size_bytes;
    if (has_point_size_overflow) {
      throw std::runtime_error{"Point record size exceeds the supported range"};
    }

    metadata.point_record_size_bytes += attribute.size_bytes;
    metadata.attributes.push_back(std::move(attribute));
  }

  return metadata;
}

template <typename T>
[[nodiscard]] T ReadLittleEndian(std::span<const std::byte> bytes, std::size_t byte_offset) {
  static_assert(std::is_unsigned_v<T>);

  if (byte_offset > bytes.size() || sizeof(T) > bytes.size() - byte_offset) {
    throw std::runtime_error{"Binary record is truncated"};
  }

  T value{};
  for (std::size_t byte_index{}; byte_index < sizeof(T); ++byte_index) {
    const T byte_value{static_cast<T>(std::to_integer<std::uint8_t>(bytes[byte_offset + byte_index]))};
    value |= byte_value << (byte_index * 8);
  }
  return value;
}

[[nodiscard]] std::vector<std::byte> ReadBinaryRange(std::ifstream& input, std::uint64_t file_size,
                                                     std::uint64_t byte_offset, std::uint64_t byte_size,
                                                     std::string_view file_description) {
  const bool is_range_valid = byte_offset <= file_size && byte_size <= file_size - byte_offset;
  if (!is_range_valid) {
    throw std::runtime_error{std::string{file_description} + " range is outside the file"};
  }

  const std::uint64_t maximum_buffer_size{std::min<std::uint64_t>(
    std::numeric_limits<std::size_t>::max(), static_cast<std::uint64_t>(std::numeric_limits<std::streamsize>::max()))};
  const std::uint64_t maximum_file_offset{static_cast<std::uint64_t>(std::numeric_limits<std::streamoff>::max())};
  if (byte_size > maximum_buffer_size || byte_offset > maximum_file_offset) {
    throw std::runtime_error{std::string{file_description} + " range exceeds the supported stream size"};
  }

  std::vector<std::byte> bytes(static_cast<std::size_t>(byte_size));
  if (bytes.empty()) {
    return bytes;
  }

  input.clear();
  input.seekg(static_cast<std::streamoff>(byte_offset), std::ios::beg);
  if (!input) {
    throw std::runtime_error{"Failed to seek in " + std::string{file_description}};
  }

  input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  if (!input || input.gcount() != static_cast<std::streamsize>(bytes.size())) {
    throw std::runtime_error{"Failed to read the requested range from " + std::string{file_description}};
  }

  return bytes;
}

[[nodiscard]] PotreeNodeType ParseNodeType(std::uint8_t raw_type) {
  switch (raw_type) {
    case 0:
      return PotreeNodeType::Normal;
    case 1:
      return PotreeNodeType::Leaf;
    case 2:
      return PotreeNodeType::Proxy;
    default:
      throw std::runtime_error{"Hierarchy contains an unknown node type: " + std::to_string(raw_type)};
  }
}

[[nodiscard]] BoundingBox ComputeNodeBounds(const BoundingBox& root_bounds, std::string_view node_name) {
  if (node_name.empty() || node_name.front() != 'r') {
    throw std::runtime_error{"Invalid Potree node name: " + std::string{node_name}};
  }

  glm::dvec3 min{root_bounds.GetMin()};
  glm::dvec3 max{root_bounds.GetMax()};
  for (const char child_character : node_name.substr(1)) {
    if (child_character < '0' || child_character > '7') {
      throw std::runtime_error{"Invalid Potree child index in node name: " + std::string{node_name}};
    }

    const std::uint8_t child_index{static_cast<std::uint8_t>(child_character - '0')};
    const glm::dvec3 center{(min + max) * 0.5};

    if ((child_index & 0b100U) != 0U) {
      min.x = center.x;
    } else {
      max.x = center.x;
    }
    if ((child_index & 0b010U) != 0U) {
      min.y = center.y;
    } else {
      max.y = center.y;
    }
    if ((child_index & 0b001U) != 0U) {
      min.z = center.z;
    } else {
      max.z = center.z;
    }
  }

  return {min, max};
}

[[nodiscard]] std::vector<PotreeNodeInfo> ParseHierarchy(const std::filesystem::path& hierarchy_path,
                                                         const PotreeMetadataInfo& metadata) {
  const std::uint64_t hierarchy_file_size{std::filesystem::file_size(hierarchy_path)};
  std::ifstream hierarchy_file{hierarchy_path, std::ios::binary};
  if (!hierarchy_file) {
    throw std::runtime_error{"Failed to open hierarchy file: " + hierarchy_path.string()};
  }

  std::deque<HierarchyChunkInfo> pending_chunks{HierarchyChunkInfo{"r", 0, metadata.first_hierarchy_chunk_size_bytes}};
  std::unordered_set<std::string> visited_chunk_roots{};
  std::unordered_map<std::string, PotreeNodeInfo> node_map{};

  while (!pending_chunks.empty()) {
    HierarchyChunkInfo chunk{std::move(pending_chunks.front())};
    pending_chunks.pop_front();

    if (!visited_chunk_roots.insert(chunk.root_node_name).second) {
      throw std::runtime_error{"Hierarchy references the same chunk more than once: " + chunk.root_node_name};
    }
    if (chunk.byte_size == 0 || chunk.byte_size % k_hierarchy_record_size_bytes != 0) {
      throw std::runtime_error{"Hierarchy chunk has an invalid byte size for node: " + chunk.root_node_name};
    }

    const std::vector<std::byte> chunk_bytes{
      ReadBinaryRange(hierarchy_file, hierarchy_file_size, chunk.byte_offset, chunk.byte_size, "hierarchy.bin")};
    const std::size_t record_count{chunk_bytes.size() / k_hierarchy_record_size_bytes};
    std::vector<std::string> record_node_names(record_count);
    record_node_names.front() = chunk.root_node_name;
    std::size_t next_child_record{1};

    for (std::size_t record_index{}; record_index < record_count; ++record_index) {
      if (record_node_names[record_index].empty()) {
        throw std::runtime_error{"Hierarchy records are not in the expected breadth-first order"};
      }

      const std::size_t record_offset{record_index * k_hierarchy_record_size_bytes};
      const PotreeNodeType node_type{ParseNodeType(ReadLittleEndian<std::uint8_t>(chunk_bytes, record_offset))};
      const std::uint8_t child_mask{ReadLittleEndian<std::uint8_t>(chunk_bytes, record_offset + 1)};
      const std::uint32_t point_count{ReadLittleEndian<std::uint32_t>(chunk_bytes, record_offset + 2)};
      const std::uint64_t byte_offset{ReadLittleEndian<std::uint64_t>(chunk_bytes, record_offset + 6)};
      const std::uint64_t byte_size{ReadLittleEndian<std::uint64_t>(chunk_bytes, record_offset + 14)};
      const std::string& node_name{record_node_names[record_index]};

      if (node_type == PotreeNodeType::Proxy) {
        pending_chunks.push_back({node_name, byte_offset, byte_size});
        continue;
      }

      PotreeNodeInfo node{};
      node.name = node_name;
      node.type = node_type;
      node.child_mask = child_mask;
      node.level = node_name.size() - 1;
      node.point_count = point_count;
      node.byte_offset = byte_offset;
      node.byte_size = byte_size;
      node.bounds = ComputeNodeBounds(metadata.bounds, node_name);
      node_map.insert_or_assign(node_name, std::move(node));

      for (std::size_t child_index{}; child_index < PotreeNodeInfo::k_child_count; ++child_index) {
        const bool has_child = (child_mask & static_cast<std::uint8_t>(1U << child_index)) != 0;
        if (!has_child) {
          continue;
        }
        if (next_child_record >= record_count) {
          throw std::runtime_error{"Hierarchy child mask references a missing record"};
        }

        record_node_names[next_child_record] = node_name + static_cast<char>('0' + static_cast<int>(child_index));
        ++next_child_record;
      }
    }

    if (next_child_record != record_count) {
      throw std::runtime_error{"Hierarchy chunk contains records that are not referenced by a parent"};
    }
  }

  std::vector<PotreeNodeInfo> nodes{};
  nodes.reserve(node_map.size());
  for (auto& entry : node_map) {
    nodes.push_back(std::move(entry.second));
  }
  std::ranges::sort(nodes, [](const PotreeNodeInfo& left, const PotreeNodeInfo& right) {
    if (left.name.size() != right.name.size()) {
      return left.name.size() < right.name.size();
    }
    return left.name < right.name;
  });

  return nodes;
}

void LoadNodeBytes(std::ifstream& octree_file, std::uint64_t octree_file_size, const PotreeMetadataInfo& metadata,
                   PotreeNodeInfo& node) {
  if (node.is_data_loaded) {
    return;
  }

  const std::uint64_t record_size{static_cast<std::uint64_t>(metadata.point_record_size_bytes)};
  const bool has_point_byte_size_overflow =
    record_size != 0 && node.point_count > std::numeric_limits<std::uint64_t>::max() / record_size;
  if (has_point_byte_size_overflow) {
    throw std::runtime_error{"Point byte size overflows for node: " + node.name};
  }

  const std::uint64_t expected_byte_size{static_cast<std::uint64_t>(node.point_count) * record_size};
  if (node.byte_size != expected_byte_size) {
    throw std::runtime_error{"Unexpected byte size for node " + node.name + ": expected " +
                             std::to_string(expected_byte_size) + ", got " + std::to_string(node.byte_size)};
  }

  node.point_bytes = ReadBinaryRange(octree_file, octree_file_size, node.byte_offset, node.byte_size, "octree.bin");
  node.is_data_loaded = true;
}
}  // namespace

PotreePointCloud::PotreePointCloud(std::filesystem::path directory) : m_directory{std::move(directory)} {
  LoadMetadata();
  LoadHierarchy();
  BuildNodeLinks();
}

const std::filesystem::path& PotreePointCloud::GetDirectory() const noexcept { return m_directory; }

const PotreeMetadataInfo& PotreePointCloud::GetMetadata() const noexcept { return m_metadata; }

std::span<const PotreeNodeInfo> PotreePointCloud::GetNodes() const noexcept { return m_nodes; }

const PotreeNodeInfo& PotreePointCloud::GetNode(PotreeNodeIndex node_index) const { return m_nodes.at(node_index); }

PotreeNodeIndex PotreePointCloud::GetRootNodeIndex() const noexcept { return m_root_node_index; }

std::optional<PotreeNodeIndex> PotreePointCloud::TryFindNodeIndex(std::string_view node_name) const {
  const auto node_iter{m_node_indices.find(std::string{node_name})};
  if (node_iter == m_node_indices.end()) {
    return std::nullopt;
  }
  return node_iter->second;
}

void PotreePointCloud::LoadNodeData(PotreeNodeIndex node_index) {
  if (m_metadata.encoding != "DEFAULT") {
    throw std::runtime_error{"This loader only supports Potree DEFAULT encoding, but metadata uses " +
                             m_metadata.encoding};
  }

  PotreeNodeInfo& node{m_nodes.at(node_index)};
  if (node.is_data_loaded) {
    return;
  }

  const std::filesystem::path octree_path{m_directory / "octree.bin"};
  const std::uint64_t octree_file_size{std::filesystem::file_size(octree_path)};
  std::ifstream octree_file{octree_path, std::ios::binary};
  if (!octree_file) {
    throw std::runtime_error{"Failed to open octree file: " + octree_path.string()};
  }

  LoadNodeBytes(octree_file, octree_file_size, m_metadata, node);
}

void PotreePointCloud::LoadAllNodeData() {
  if (m_metadata.encoding != "DEFAULT") {
    throw std::runtime_error{"This loader only supports Potree DEFAULT encoding, but metadata uses " +
                             m_metadata.encoding};
  }

  const std::filesystem::path octree_path{m_directory / "octree.bin"};
  const std::uint64_t octree_file_size{std::filesystem::file_size(octree_path)};
  std::ifstream octree_file{octree_path, std::ios::binary};
  if (!octree_file) {
    throw std::runtime_error{"Failed to open octree file: " + octree_path.string()};
  }

  for (PotreeNodeInfo& node : m_nodes) {
    LoadNodeBytes(octree_file, octree_file_size, m_metadata, node);
  }
}

void PotreePointCloud::UnloadNodeData(PotreeNodeIndex node_index) {
  PotreeNodeInfo& node{m_nodes.at(node_index)};
  node.point_bytes = {};
  node.is_data_loaded = false;
}

void PotreePointCloud::UnloadAllNodeData() {
  for (PotreeNodeInfo& node : m_nodes) {
    node.point_bytes = {};
    node.is_data_loaded = false;
  }
}

std::uint64_t PotreePointCloud::GetLoadedPointCount() const noexcept {
  std::uint64_t loaded_point_count{};
  for (const PotreeNodeInfo& node : m_nodes) {
    if (node.is_data_loaded) {
      loaded_point_count += node.point_count;
    }
  }
  return loaded_point_count;
}

std::uint64_t PotreePointCloud::GetLoadedByteCount() const noexcept {
  std::uint64_t loaded_byte_count{};
  for (const PotreeNodeInfo& node : m_nodes) {
    loaded_byte_count += static_cast<std::uint64_t>(node.point_bytes.size());
  }
  return loaded_byte_count;
}

void PotreePointCloud::PrintPointCloud(std::ostream& output) const {
  const std::ios_base::fmtflags original_flags{output.flags()};
  const std::streamsize original_precision{output.precision()};
  const char original_fill{output.fill()};

  output << std::fixed << std::setprecision(3);
  output << "Potree directory: " << m_directory << '\n';
  output << "Name: " << m_metadata.name << '\n';
  output << "Version: " << m_metadata.version << ", encoding: " << m_metadata.encoding << '\n';
  output << "Metadata points: " << m_metadata.point_count << '\n';
  output << "Nodes: " << m_nodes.size() << ", loaded points: " << GetLoadedPointCount()
         << ", loaded bytes: " << GetLoadedByteCount() << '\n';
  output << "Point record size: " << m_metadata.point_record_size_bytes << " bytes\n";
  output << "Spacing: " << m_metadata.spacing << "\nOffset: ";
  PrintVector3(output, m_metadata.offset);
  output << "\nScale: ";
  PrintVector3(output, m_metadata.scale);
  output << "\nBounds: ";
  PrintVector3(output, m_metadata.bounds.GetMin());
  output << " - ";
  PrintVector3(output, m_metadata.bounds.GetMax());
  output << "\n\nAttributes:\n";
  for (const PotreeAttributeInfo& attribute : m_metadata.attributes) {
    output << "  " << attribute.name << " (" << attribute.type << ", offset=" << attribute.byte_offset
           << ", size=" << attribute.size_bytes << " bytes)\n";
  }

  output << "\nNodes:\n";
  for (const PotreeNodeInfo& node : m_nodes) {
    output << "  " << node.name << " | level=" << node.level << " | type=" << GetNodeTypeName(node.type)
           << " | children=0x" << std::hex << std::setw(2) << std::setfill('0')
           << static_cast<unsigned int>(node.child_mask) << std::dec << std::setfill(' ')
           << " | points=" << node.point_count << " | octree=[" << node.byte_offset << ", "
           << node.byte_offset + node.byte_size << ") | loaded=" << std::boolalpha << node.is_data_loaded
           << " | bounds=";
    PrintVector3(output, node.bounds.GetMin());
    output << " - ";
    PrintVector3(output, node.bounds.GetMax());
    output << '\n';
  }

  output.flags(original_flags);
  output.precision(original_precision);
  output.fill(original_fill);
}

void PotreePointCloud::LoadMetadata() { m_metadata = ParseMetadata(m_directory / "metadata.json"); }

void PotreePointCloud::LoadHierarchy() {
  m_nodes = ParseHierarchy(m_directory / "hierarchy.bin", m_metadata);
  if (m_nodes.empty()) {
    throw std::runtime_error{"Potree hierarchy does not contain any nodes"};
  }
}

void PotreePointCloud::BuildNodeLinks() {
  m_node_indices.clear();
  m_node_indices.reserve(m_nodes.size());

  for (PotreeNodeIndex node_index{}; node_index < m_nodes.size(); ++node_index) {
    const bool is_inserted{m_node_indices.emplace(m_nodes[node_index].name, node_index).second};
    if (!is_inserted) {
      throw std::runtime_error{"Potree hierarchy contains a duplicate node: " + m_nodes[node_index].name};
    }
  }

  const auto root_iter{m_node_indices.find("r")};
  if (root_iter == m_node_indices.end()) {
    throw std::runtime_error{"Potree hierarchy does not contain the root node"};
  }
  m_root_node_index = root_iter->second;

  for (PotreeNodeIndex node_index{}; node_index < m_nodes.size(); ++node_index) {
    PotreeNodeInfo& node{m_nodes[node_index]};
    if (node.name == "r") {
      continue;
    }

    const std::string parent_name{node.name.substr(0, node.name.size() - 1)};
    const auto parent_iter{m_node_indices.find(parent_name)};
    if (parent_iter == m_node_indices.end()) {
      throw std::runtime_error{"Potree node does not have a loaded parent: " + node.name};
    }

    const std::size_t child_index{static_cast<std::size_t>(node.name.back() - '0')};
    PotreeNodeInfo& parent{m_nodes[parent_iter->second]};
    const bool is_child_declared = (parent.child_mask & static_cast<std::uint8_t>(1U << child_index)) != 0;
    if (!is_child_declared || parent.child_indices[child_index].has_value()) {
      throw std::runtime_error{"Potree hierarchy contains an invalid child link: " + node.name};
    }

    node.parent_index = parent_iter->second;
    parent.child_indices[child_index] = node_index;
  }

  for (const PotreeNodeInfo& node : m_nodes) {
    for (std::size_t child_index{}; child_index < PotreeNodeInfo::k_child_count; ++child_index) {
      const bool is_child_declared = (node.child_mask & static_cast<std::uint8_t>(1U << child_index)) != 0;
      if (is_child_declared != node.child_indices[child_index].has_value()) {
        throw std::runtime_error{"Potree hierarchy child mask does not match loaded nodes: " + node.name};
      }
    }
  }
}
}  // namespace lvk::point_cloud
