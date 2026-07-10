#include "potree_vertex_decoder.h"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>

namespace lvk::point_cloud {
namespace {
constexpr std::uint32_t k_fallback_point_color{0xffffffffU};

[[nodiscard]] const PotreeAttributeInfo& GetPositionAttribute(const PotreeMetadataInfo& metadata) {
  for (const PotreeAttributeInfo& attribute : metadata.attributes) {
    if (attribute.name == "position") {
      return attribute;
    }
  }
  throw std::runtime_error{"Potree metadata does not contain a position attribute"};
}

[[nodiscard]] bool IsColorAttributeName(std::string_view attribute_name) noexcept {
  return attribute_name == "rgb" || attribute_name == "rgba" || attribute_name == "RGB" || attribute_name == "RGBA";
}

[[nodiscard]] const PotreeAttributeInfo* TryGetColorAttribute(const PotreeMetadataInfo& metadata) noexcept {
  for (const PotreeAttributeInfo& attribute : metadata.attributes) {
    if (IsColorAttributeName(attribute.name)) {
      return &attribute;
    }
  }
  return nullptr;
}

[[nodiscard]] std::int32_t ReadInt32LittleEndian(std::span<const std::byte> bytes, std::size_t byte_offset) {
  if (byte_offset > bytes.size() || sizeof(std::uint32_t) > bytes.size() - byte_offset) {
    throw std::runtime_error{"Potree position record is truncated"};
  }

  std::uint32_t encoded_value{};
  for (std::size_t byte_index{}; byte_index < sizeof(encoded_value); ++byte_index) {
    const std::uint32_t byte_value{
      static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[byte_offset + byte_index]))};
    encoded_value |= byte_value << (byte_index * 8U);
  }
  return std::bit_cast<std::int32_t>(encoded_value);
}

[[nodiscard]] std::uint16_t ReadUint16LittleEndian(std::span<const std::byte> bytes, std::size_t byte_offset) {
  if (byte_offset > bytes.size() || sizeof(std::uint16_t) > bytes.size() - byte_offset) {
    throw std::runtime_error{"Potree color record is truncated"};
  }

  const std::uint16_t low_byte{std::to_integer<std::uint8_t>(bytes[byte_offset])};
  const std::uint16_t high_byte{std::to_integer<std::uint8_t>(bytes[byte_offset + 1U])};
  return static_cast<std::uint16_t>(low_byte | static_cast<std::uint16_t>(high_byte << 8U));
}

void ValidateColorAttribute(const PotreeAttributeInfo& attribute) {
  const bool is_uint8 = attribute.type == "uint8" && attribute.element_size_bytes == sizeof(std::uint8_t);
  const bool is_uint16 = attribute.type == "uint16" && attribute.element_size_bytes == sizeof(std::uint16_t);
  const bool has_supported_elements = attribute.element_count == 3 || attribute.element_count == 4;
  const bool has_valid_size =
    attribute.element_size_bytes != 0 &&
    attribute.element_count <= std::numeric_limits<std::size_t>::max() / attribute.element_size_bytes &&
    attribute.size_bytes >= attribute.element_count * attribute.element_size_bytes;
  if ((!is_uint8 && !is_uint16) || !has_supported_elements || !has_valid_size) {
    throw std::runtime_error{"Potree color attribute must contain three or four uint8/uint16 elements"};
  }
}

[[nodiscard]] std::uint8_t ReadColorChannel(std::span<const std::byte> bytes, std::size_t byte_offset,
                                            std::size_t element_size_bytes) {
  if (element_size_bytes == sizeof(std::uint8_t)) {
    if (byte_offset >= bytes.size()) {
      throw std::runtime_error{"Potree color record is truncated"};
    }
    return std::to_integer<std::uint8_t>(bytes[byte_offset]);
  }

  const std::uint16_t value{ReadUint16LittleEndian(bytes, byte_offset)};
  return static_cast<std::uint8_t>(value > 255U ? value / 256U : value);
}

[[nodiscard]] std::uint32_t DecodePointColor(std::span<const std::byte> point_bytes, std::size_t point_offset,
                                             const PotreeAttributeInfo& color_attribute) {
  const std::size_t color_offset{point_offset + color_attribute.byte_offset};
  const std::size_t element_size{color_attribute.element_size_bytes};
  const std::uint8_t red{ReadColorChannel(point_bytes, color_offset, element_size)};
  const std::uint8_t green{ReadColorChannel(point_bytes, color_offset + element_size, element_size)};
  const std::uint8_t blue{ReadColorChannel(point_bytes, color_offset + (element_size * 2U), element_size)};
  const std::uint8_t alpha{color_attribute.element_count == 4
                             ? ReadColorChannel(point_bytes, color_offset + (element_size * 3U), element_size)
                             : std::numeric_limits<std::uint8_t>::max()};

  return static_cast<std::uint32_t>(red) | (static_cast<std::uint32_t>(green) << 8U) |
         (static_cast<std::uint32_t>(blue) << 16U) | (static_cast<std::uint32_t>(alpha) << 24U);
}
}  // namespace

std::vector<PointCloudVertex> LoadAndDecodePotreeNodeVertices(PotreePointCloud& point_cloud,
                                                              PotreeNodeIndex node_index) {
  point_cloud.LoadNodeData(node_index);
  const PotreeMetadataInfo& metadata{point_cloud.GetMetadata()};
  const PotreeNodeInfo& node{point_cloud.GetNode(node_index)};
  if (!node.is_data_loaded) {
    throw std::runtime_error{"Potree node data must be loaded before vertex decoding: " + node.name};
  }

  const PotreeAttributeInfo& position_attribute{GetPositionAttribute(metadata)};
  const bool is_supported_position = position_attribute.type == "int32" && position_attribute.element_count == 3 &&
                                     position_attribute.size_bytes == sizeof(std::int32_t) * 3U;
  if (!is_supported_position) {
    throw std::runtime_error{"Potree position attribute must contain three int32 elements"};
  }

  const PotreeAttributeInfo* color_attribute{TryGetColorAttribute(metadata)};
  if (nullptr != color_attribute) {
    ValidateColorAttribute(*color_attribute);
  }

  const std::uint64_t point_record_size{static_cast<std::uint64_t>(metadata.point_record_size_bytes)};
  const std::uint64_t expected_byte_size{static_cast<std::uint64_t>(node.point_count) * point_record_size};
  if (expected_byte_size != static_cast<std::uint64_t>(node.point_bytes.size())) {
    throw std::runtime_error{"Loaded Potree node byte size does not match its point count: " + node.name};
  }

  const std::span<const std::byte> point_bytes{node.point_bytes};
  const std::size_t point_count{static_cast<std::size_t>(node.point_count)};
  std::vector<PointCloudVertex> vertices{};
  vertices.reserve(point_count);

  for (std::size_t point_index{}; point_index < point_count; ++point_index) {
    const std::size_t point_offset{point_index * metadata.point_record_size_bytes};
    const std::size_t position_offset{point_offset + position_attribute.byte_offset};
    const std::int32_t encoded_x{ReadInt32LittleEndian(point_bytes, position_offset)};
    const std::int32_t encoded_y{ReadInt32LittleEndian(point_bytes, position_offset + sizeof(std::int32_t))};
    const std::int32_t encoded_z{ReadInt32LittleEndian(point_bytes, position_offset + (sizeof(std::int32_t) * 2U))};

    PointCloudVertex vertex{};
    vertex.position.x = static_cast<float>(encoded_x * metadata.scale.x + metadata.offset.x);
    vertex.position.y = static_cast<float>(encoded_y * metadata.scale.y + metadata.offset.y);
    vertex.position.z = static_cast<float>(encoded_z * metadata.scale.z + metadata.offset.z);
    vertex.color = nullptr != color_attribute ? DecodePointColor(point_bytes, point_offset, *color_attribute)
                                              : k_fallback_point_color;

    vertices.push_back(vertex);
  }

  return vertices;
}
}  // namespace lvk::point_cloud
