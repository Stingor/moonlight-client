#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/core/ascii.h"

#include "gr2_file.h"

namespace ro::formats::gr2 {
namespace {

// La taille d'un élément de chaque genre, en octets ; celle d'un membre en
// ligne est celle de son type.
constexpr std::array<std::uint32_t, 21> kKindBytes = {0, 0, 4, 8, 8, 8, 8, 12, 4, 68, 4,
                                                      1, 1, 1, 1, 2, 2, 2, 2,  4, 4};
// Une définition de membre : son genre, son nom, son type, sa largeur, puis
// quatre mots qu'on n'emploie pas.
constexpr std::uint32_t kMemberDefinitionBytes = 32;
constexpr std::uint32_t kNameField = 4;
constexpr std::uint32_t kTypeField = 8;
constexpr std::uint32_t kWidthField = 12;
// Des bornes contre un fichier qui boucle sur lui-même.
constexpr std::size_t kLargestMemberCount = 4096;
constexpr std::uint32_t kDeepestNesting = 64;

// Un tableau : son compte, puis l'adresse de ses éléments ; un tableau de
// type propre porte d'abord ce type.
constexpr std::uint32_t kArrayPointerField = 4;
constexpr std::uint32_t kVariantCountField = 4;
constexpr std::uint32_t kVariantPointerField = 8;

// Une transformée : ses drapeaux, puis ses seize réels.
constexpr std::uint32_t kTransformFlagsBytes = 4;
constexpr std::uint32_t kRealBytes = 4;

// Les entiers normalisés : un entier signé sur 8 ou 16 bits vaut son quotient
// par 127 ou 32767 ; un non signé, par 255 ou 65535.
constexpr double kBinormal8 = 127.0;
constexpr double kNormal8 = 255.0;
constexpr double kBinormal16 = 32767.0;
constexpr double kNormal16 = 65535.0;

bool SameName(std::string_view left, std::string_view right) {
  return left.size() == right.size() &&
         std::equal(left.begin(), left.end(), right.begin(), [](char a, char b) {
           return core::AsciiLower(static_cast<std::uint8_t>(a)) ==
                  core::AsciiLower(static_cast<std::uint8_t>(b));
         });
}

bool IsNumeric(Gr2Kind kind) { return kind >= Gr2Kind::kReal32 && kind <= Gr2Kind::kUInt32; }

}  // namespace

const std::vector<Gr2Member>& Gr2File::Members(std::uint32_t type) {
  if (const auto found = members_.find(type); found != members_.end()) {
    return found->second;
  }
  std::vector<Gr2Member> members;
  if (type == 0 || depth_ >= kDeepestNesting) {
    memory_.Invalidate();
    return members_[type];
  }
  ++depth_;
  {
    // La place est retenue avant de descendre : un type qui se contient lui-même
    // s'y retrouve vide au lieu de boucler.
    members_[type] = {};
    std::uint32_t offset = 0;
    for (std::uint32_t at = type; members.size() < kLargestMemberCount;
         at += kMemberDefinitionBytes) {
      const std::uint32_t kind = memory_.U32(at);
      if (kind == 0 || !memory_.ok()) {
        break;
      }
      if (kind >= kKindBytes.size()) {
        memory_.Invalidate();
        break;
      }
      Gr2Member member;
      member.kind = static_cast<Gr2Kind>(kind);
      member.name = memory_.String(memory_.U32(at + kNameField));
      member.type = memory_.U32(at + kTypeField);
      member.width = std::max<std::uint32_t>(memory_.U32(at + kWidthField), 1);
      member.offset = offset;
      const std::uint64_t element =
          member.kind == Gr2Kind::kInline ? SizeOf(member.type) : kKindBytes[kind];
      offset = static_cast<std::uint32_t>(
          std::min<std::uint64_t>(offset + element * member.width, UINT32_MAX));
      members.push_back(std::move(member));
    }
  }
  --depth_;
  return members_[type] = std::move(members);
}

std::uint32_t Gr2File::SizeOf(std::uint32_t type) {
  if (const auto found = sizes_.find(type); found != sizes_.end()) {
    return found->second;
  }
  const std::vector<Gr2Member>& members = Members(type);
  std::uint64_t size = 0;
  if (!members.empty()) {
    const Gr2Member& last = members.back();
    const std::uint64_t element = last.kind == Gr2Kind::kInline
                                      ? SizeOf(last.type)
                                      : kKindBytes[static_cast<std::size_t>(last.kind)];
    size = last.offset + element * last.width;
  }
  const auto clamped = static_cast<std::uint32_t>(std::min<std::uint64_t>(size, UINT32_MAX));
  sizes_[type] = clamped;
  return clamped;
}

std::optional<Gr2Member> Gr2File::MemberNamed(std::uint32_t type, std::string_view name) {
  if (type == 0) {
    return std::nullopt;
  }
  for (const Gr2Member& member : Members(type)) {
    if (SameName(member.name, name)) {
      return member;
    }
  }
  return std::nullopt;
}

std::optional<Gr2Member> Gr2File::Field(const Gr2Object& object, std::string_view name) {
  return object.present() ? MemberNamed(object.type, name) : std::nullopt;
}

double Gr2File::Scalar(const Gr2Member& member, std::uint32_t address, std::uint32_t index) {
  const std::uint32_t at =
      address + member.offset + index * kKindBytes[static_cast<std::size_t>(member.kind)];
  switch (member.kind) {
    case Gr2Kind::kReal32:
      return memory_.F32(at);
    case Gr2Kind::kInt8:
    case Gr2Kind::kBinormalInt8:
      return static_cast<std::int8_t>(memory_.U8(at));
    case Gr2Kind::kUInt8:
    case Gr2Kind::kNormalUInt8:
      return memory_.U8(at);
    case Gr2Kind::kInt16:
    case Gr2Kind::kBinormalInt16:
      return static_cast<std::int16_t>(memory_.U16(at));
    case Gr2Kind::kUInt16:
    case Gr2Kind::kNormalUInt16:
      return memory_.U16(at);
    case Gr2Kind::kInt32:
      return memory_.I32(at);
    case Gr2Kind::kUInt32:
      return memory_.U32(at);
    default:
      return 0.0;
  }
}

double Gr2File::Normalized(const Gr2Member& member, std::uint32_t address, std::uint32_t index) {
  const double raw = Scalar(member, address, index);
  switch (member.kind) {
    case Gr2Kind::kBinormalInt8:
      return raw / kBinormal8;
    case Gr2Kind::kNormalUInt8:
      return raw / kNormal8;
    case Gr2Kind::kBinormalInt16:
      return raw / kBinormal16;
    case Gr2Kind::kNormalUInt16:
      return raw / kNormal16;
    default:
      return raw;
  }
}

std::string Gr2File::String(const Gr2Object& object, std::string_view name) {
  const std::optional<Gr2Member> member = Field(object, name);
  if (!member || member->kind != Gr2Kind::kString) {
    return {};
  }
  return memory_.String(memory_.U32(object.address + member->offset));
}

std::int32_t Gr2File::Int(const Gr2Object& object, std::string_view name, std::int32_t fallback) {
  const std::optional<Gr2Member> member = Field(object, name);
  if (!member || !IsNumeric(member->kind)) {
    return fallback;
  }
  return static_cast<std::int32_t>(Scalar(*member, object.address, 0));
}

float Gr2File::Real(const Gr2Object& object, std::string_view name, float fallback) {
  const std::optional<Gr2Member> member = Field(object, name);
  if (!member || !IsNumeric(member->kind)) {
    return fallback;
  }
  return static_cast<float>(Normalized(*member, object.address, 0));
}

void Gr2File::Reals(const Gr2Object& object, std::string_view name, std::span<float> out) {
  const std::optional<Gr2Member> member = Field(object, name);
  if (!member || !IsNumeric(member->kind)) {
    return;
  }
  const std::uint32_t count =
      std::min<std::uint32_t>(member->width, static_cast<std::uint32_t>(out.size()));
  for (std::uint32_t index = 0; index < count; ++index) {
    out[index] = static_cast<float>(Normalized(*member, object.address, index));
  }
}

GrannyTransform Gr2File::Transform(const Gr2Object& object, std::string_view name) {
  GrannyTransform transform;
  const std::optional<Gr2Member> member = Field(object, name);
  if (!member || member->kind != Gr2Kind::kTransform) {
    return transform;
  }
  std::uint32_t at = object.address + member->offset;
  transform.flags = memory_.U32(at);
  at += kTransformFlagsBytes;
  for (float& lane : transform.position) {
    lane = memory_.F32(at);
    at += kRealBytes;
  }
  for (float& lane : transform.orientation) {
    lane = memory_.F32(at);
    at += kRealBytes;
  }
  for (float& lane : transform.scale_shear) {
    lane = memory_.F32(at);
    at += kRealBytes;
  }
  return transform;
}

Gr2Object Gr2File::Reference(const Gr2Object& object, std::string_view name) {
  const std::optional<Gr2Member> member = Field(object, name);
  if (!member || member->kind != Gr2Kind::kReference) {
    return {};
  }
  const std::uint32_t address = memory_.U32(object.address + member->offset);
  return address == 0 ? Gr2Object{} : Gr2Object{member->type, address};
}

std::vector<Gr2Object> Gr2File::Array(const Gr2Object& object, std::string_view name) {
  std::vector<Gr2Object> items;
  const std::optional<Gr2Member> member = Field(object, name);
  if (!member ||
      (member->kind != Gr2Kind::kReferenceToArray && member->kind != Gr2Kind::kArrayOfReferences)) {
    return items;
  }
  const std::uint32_t at = object.address + member->offset;
  const std::int32_t count = memory_.I32(at);
  const std::uint32_t first = memory_.U32(at + kArrayPointerField);
  if (count <= 0 || first == 0) {
    return items;
  }
  const bool references = member->kind == Gr2Kind::kArrayOfReferences;
  const std::uint32_t stride = references ? kRealBytes : SizeOf(member->type);
  if (!memory_.Holds(first, std::uint64_t{stride} * static_cast<std::uint32_t>(count))) {
    memory_.Invalidate();
    return items;
  }
  items.reserve(static_cast<std::size_t>(count));
  for (std::uint32_t index = 0; index < static_cast<std::uint32_t>(count); ++index) {
    const std::uint32_t element = first + index * stride;
    if (references) {
      const std::uint32_t pointer = memory_.U32(element);
      items.push_back(pointer == 0 ? Gr2Object{} : Gr2Object{member->type, pointer});
    } else {
      items.push_back(Gr2Object{member->type, element});
    }
  }
  return items;
}

Gr2VariantArray Gr2File::VariantArray(const Gr2Object& object, std::string_view name) {
  const std::optional<Gr2Member> member = Field(object, name);
  if (!member || member->kind != Gr2Kind::kReferenceToVariantArray) {
    return {};
  }
  const std::uint32_t at = object.address + member->offset;
  Gr2VariantArray array{memory_.U32(at), memory_.U32(at + kVariantCountField),
                        memory_.U32(at + kVariantPointerField)};
  if (array.type == 0 || array.address == 0 ||
      !memory_.Holds(array.address, std::uint64_t{SizeOf(array.type)} * array.count)) {
    return {};
  }
  return array;
}

std::vector<std::int64_t> Gr2File::Numbers(const Gr2Object& object, std::string_view name,
                                           std::vector<float>* reals) {
  std::vector<std::int64_t> integers;
  for (const Gr2Object& item : Array(object, name)) {
    const std::vector<Gr2Member>& members = Members(item.type);
    if (members.empty() || !IsNumeric(members.front().kind) || !item.present()) {
      break;
    }
    if (reals != nullptr) {
      reals->push_back(static_cast<float>(Normalized(members.front(), item.address, 0)));
    } else {
      integers.push_back(static_cast<std::int64_t>(Scalar(members.front(), item.address, 0)));
    }
  }
  return integers;
}

std::vector<float> Gr2File::RealArray(const Gr2Object& object, std::string_view name) {
  std::vector<float> reals;
  static_cast<void>(Numbers(object, name, &reals));
  return reals;
}

std::vector<std::int64_t> Gr2File::IntegerArray(const Gr2Object& object, std::string_view name) {
  return Numbers(object, name, nullptr);
}

}  // namespace ro::formats::gr2
