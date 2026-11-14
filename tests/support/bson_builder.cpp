#include "bson_builder.h"

#include <bit>
#include <cstring>

namespace ro::test_support {
namespace {

// BSON est petit-boutiste, comme les formats du jeu.
void AppendLittle(std::vector<std::uint8_t>& bytes, std::uint64_t value, std::size_t width) {
  for (std::size_t index = 0; index < width; ++index) {
    bytes.push_back(static_cast<std::uint8_t>((value >> (8 * index)) & 0xFF));
  }
}

void AppendCString(std::vector<std::uint8_t>& bytes, std::string_view text) {
  bytes.insert(bytes.end(), text.begin(), text.end());
  bytes.push_back(0);
}

constexpr std::uint8_t kDouble = 0x01;
constexpr std::uint8_t kString = 0x02;
constexpr std::uint8_t kDocument = 0x03;
constexpr std::uint8_t kArray = 0x04;
constexpr std::uint8_t kBoolean = 0x08;
constexpr std::uint8_t kInt32 = 0x10;

constexpr std::size_t kSizeFieldWidth = 4;

}  // namespace

BsonBuilder& BsonBuilder::Begin(std::uint8_t type, std::string_view name) {
  elements_.push_back(type);
  AppendCString(elements_, name);
  return *this;
}

BsonBuilder& BsonBuilder::Number(std::string_view name, double value) {
  Begin(kDouble, name);
  AppendLittle(elements_, std::bit_cast<std::uint64_t>(value), 8);
  return *this;
}

BsonBuilder& BsonBuilder::Text(std::string_view name, std::string_view value) {
  Begin(kString, name);
  // La longueur annoncée COMPTE le zéro final.
  AppendLittle(elements_, static_cast<std::uint64_t>(value.size() + 1), kSizeFieldWidth);
  AppendCString(elements_, value);
  return *this;
}

BsonBuilder& BsonBuilder::Document(std::string_view name, const BsonBuilder& nested) {
  Begin(kDocument, name);
  const std::vector<std::uint8_t> bytes = nested.Build();
  elements_.insert(elements_.end(), bytes.begin(), bytes.end());
  return *this;
}

BsonBuilder& BsonBuilder::Array(std::string_view name, const BsonBuilder& nested) {
  Begin(kArray, name);
  const std::vector<std::uint8_t> bytes = nested.Build();
  elements_.insert(elements_.end(), bytes.begin(), bytes.end());
  return *this;
}

BsonBuilder& BsonBuilder::Flag(std::string_view name, bool value) {
  Begin(kBoolean, name);
  elements_.push_back(value ? 1 : 0);
  return *this;
}

BsonBuilder& BsonBuilder::Integer(std::string_view name, std::int32_t value) {
  Begin(kInt32, name);
  AppendLittle(elements_, static_cast<std::uint32_t>(value), kSizeFieldWidth);
  return *this;
}

BsonBuilder& BsonBuilder::RawElement(std::uint8_t type, std::string_view name,
                                     std::span<const std::uint8_t> payload) {
  Begin(type, name);
  elements_.insert(elements_.end(), payload.begin(), payload.end());
  return *this;
}

std::vector<std::uint8_t> BsonBuilder::Build() const { return BuildWithSizeOffBy(0); }

std::vector<std::uint8_t> BsonBuilder::BuildWithSizeOffBy(std::int32_t delta) const {
  std::vector<std::uint8_t> bytes;
  const auto declared = static_cast<std::int32_t>(kSizeFieldWidth + elements_.size() + 1) + delta;
  AppendLittle(bytes, static_cast<std::uint32_t>(declared), kSizeFieldWidth);
  bytes.insert(bytes.end(), elements_.begin(), elements_.end());
  bytes.push_back(0);
  return bytes;
}

}  // namespace ro::test_support
