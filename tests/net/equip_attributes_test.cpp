// Ce qu'apporte une pièce ou un ensemble, lu depuis les octets du serveur.

#include "ro/net/equip_attributes.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/byte_writer.h"
#include "ro/proto/map_packets.h"

using ro::net::ReadEquipAttributes;
using ro::sim::EquipAttributesKind;

namespace {

constexpr std::uint8_t kEquipped = 1;
constexpr std::uint8_t kComboKind = 1;
constexpr std::uint8_t kPetKind = 2;
constexpr std::uint32_t kPoringEgg = 9001;
constexpr std::uint16_t kLuk = 18;
constexpr std::int32_t kTwoLuk = 2;
constexpr std::uint8_t kUnknownKind = 7;
constexpr std::uint32_t kComboId = 42;
constexpr std::uint16_t kAddRace = 1008;
constexpr std::int32_t kDragonRace = 9;
constexpr std::int32_t kTenPercent = 10;
constexpr std::uint16_t kAtk1 = 41;
constexpr std::int32_t kWeaponAtk = 120;
constexpr std::size_t kLengthOffset = 2;
constexpr unsigned kBitsPerByte = 8;

struct Entry {
  std::uint8_t level = 0;
  std::uint16_t type = 0;
  std::int32_t first = 0;
  std::int32_t second = 0;
};

// Une trame écrite comme le serveur l'écrit : en-tête, puis cinq valeurs par
// entrée, les inutilisées à zéro.
ro::net::Frame FrameOf(std::uint8_t equipped, std::uint8_t kind, std::uint32_t id,
                       const std::vector<Entry>& entries) {
  ro::net::Frame frame;
  frame.opcode = ro::proto::ZcMoonlightEquipAttributes::kOpcode;
  ro::core::ByteWriter writer(frame.bytes);
  writer.U16(frame.opcode);
  writer.U16(0);
  writer.U8(equipped);
  writer.U8(kind);
  writer.U32(id);
  for (const Entry& entry : entries) {
    writer.U8(entry.level);
    writer.U16(entry.type);
    writer.U32(static_cast<std::uint32_t>(entry.first));
    writer.U32(static_cast<std::uint32_t>(entry.second));
    writer.U32(0);
    writer.U32(0);
    writer.U32(0);
  }
  frame.bytes[kLengthOffset] = static_cast<std::uint8_t>(frame.bytes.size() & 0xFFU);
  frame.bytes[kLengthOffset + 1] = static_cast<std::uint8_t>(frame.bytes.size() >> kBitsPerByte);
  return frame;
}

}  // namespace

TEST_CASE("Attributs d'équipement : l'en-tête et chaque entrée se lisent", "[net][equip]") {
  const ro::net::Frame frame =
      FrameOf(kEquipped, kComboKind, kComboId,
              {Entry{.level = 0, .type = kAtk1, .first = kWeaponAtk},
               Entry{.level = 2, .type = kAddRace, .first = kDragonRace, .second = kTenPercent}});
  const auto told = ReadEquipAttributes(frame);
  REQUIRE(told.has_value());
  CHECK(told->equipped);
  CHECK(told->kind == EquipAttributesKind::kCombo);
  CHECK(told->id == kComboId);
  REQUIRE(told->bonuses.size() == 2);
  CHECK(told->bonuses[0].level == 0);
  CHECK(told->bonuses[0].type == kAtk1);
  CHECK(told->bonuses[0].values[0] == kWeaponAtk);
  CHECK(told->bonuses[1].level == 2);
  CHECK(told->bonuses[1].type == kAddRace);
  CHECK(told->bonuses[1].values[0] == kDragonRace);
  CHECK(told->bonuses[1].values[1] == kTenPercent);
  CHECK(told->bonuses[1].values[2] == 0);
}

TEST_CASE("Attributs d'équipement : une pièce retirée sans bonus se lit aussi", "[net][equip]") {
  const auto told = ReadEquipAttributes(FrameOf(0, 0, kComboId, {}));
  REQUIRE(told.has_value());
  CHECK_FALSE(told->equipped);
  CHECK(told->kind == EquipAttributesKind::kPiece);
  CHECK(told->bonuses.empty());
}

TEST_CASE("Attributs d'équipement : le familier se lit, son œuf pour identifiant", "[net][equip]") {
  const auto told = ReadEquipAttributes(
      FrameOf(0, kPetKind, kPoringEgg, {Entry{.level = 1, .type = kLuk, .first = kTwoLuk}}));
  REQUIRE(told.has_value());
  CHECK_FALSE(told->equipped);
  CHECK(told->kind == EquipAttributesKind::kPet);
  CHECK(told->id == kPoringEgg);
  REQUIRE(told->bonuses.size() == 1);
  CHECK(told->bonuses[0].type == kLuk);
  CHECK(told->bonuses[0].values[0] == kTwoLuk);
}

TEST_CASE("Attributs d'équipement : ce qui ne se lit pas ne laisse rien", "[net][equip]") {
  // Une nature inconnue : l'écrire comme une pièce serait un mensonge.
  CHECK_FALSE(ReadEquipAttributes(FrameOf(kEquipped, kUnknownKind, kComboId, {})).has_value());

  // Une entrée tronquée : la longueur ne tombe pas sur un nombre entier d'entrées.
  ro::net::Frame cut =
      FrameOf(kEquipped, kComboKind, kComboId, {Entry{.level = 1, .type = kAtk1, .first = 1}});
  cut.bytes.pop_back();
  cut.bytes[kLengthOffset] = static_cast<std::uint8_t>(cut.bytes.size() & 0xFFU);
  CHECK_FALSE(ReadEquipAttributes(cut).has_value());

  // Un autre paquet.
  ro::net::Frame other = FrameOf(kEquipped, kComboKind, kComboId, {});
  other.opcode = ro::proto::ZcMoonlightStatBonus::kOpcode;
  CHECK_FALSE(ReadEquipAttributes(other).has_value());
}
