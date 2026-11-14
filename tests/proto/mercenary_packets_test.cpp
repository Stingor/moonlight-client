#include "ro/proto/mercenary_packets.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/proto/manual_packets.h"

namespace {

void PutU16(std::vector<std::uint8_t>& bytes, std::size_t at, std::uint16_t value) {
  bytes[at] = static_cast<std::uint8_t>(value & 0xFF);
  bytes[at + 1] = static_cast<std::uint8_t>(value >> 8);
}

void PutU32(std::vector<std::uint8_t>& bytes, std::size_t at, std::uint32_t value) {
  for (std::size_t i = 0; i < 4; ++i) {
    bytes[at + i] = static_cast<std::uint8_t>((value >> (8 * i)) & 0xFF);
  }
}

}  // namespace

TEST_CASE("MercenaryPackets : la fiche se lit aux décalages du serveur", "[mercenarypackets]") {
  std::vector<std::uint8_t> frame(ro::proto::MercenaryInfo::kWireSize, 0);
  PutU16(frame, 0, 0x029B);
  PutU32(frame, 2, 110000123);
  PutU16(frame, 6, 321);      // ATK
  PutU16(frame, 8, 45);       // MATK
  PutU16(frame, 10, 150);     // HIT
  PutU16(frame, 12, 12);      // CRI
  PutU16(frame, 14, 0xFFFE);  // DEF, négative
  PutU16(frame, 16, 9);       // MDEF
  PutU16(frame, 18, 140);     // FLEE
  PutU16(frame, 20, 700);     // amotion
  frame[22] = 'L';
  frame[23] = 'e';
  frame[24] = 'o';
  PutU16(frame, 46, 7);
  PutU32(frame, 48, 1500);
  PutU32(frame, 52, 2000);
  PutU32(frame, 56, 40);
  PutU32(frame, 60, 80);
  PutU32(frame, 64, 1800000000);
  PutU16(frame, 68, 33);
  PutU32(frame, 70, 5);
  PutU32(frame, 74, 1234);
  PutU16(frame, 78, 9);

  const auto info = ro::proto::DecodeMercenaryInfo(frame);
  REQUIRE(info);
  CHECK(info->actor_id == 110000123);
  CHECK(info->atk == 321);
  CHECK(info->matk == 45);
  CHECK(info->hit == 150);
  CHECK(info->crit == 12);
  CHECK(info->def == -2);
  CHECK(info->mdef == 9);
  CHECK(info->flee == 140);
  CHECK(info->amotion_ms == 700);
  CHECK(info->name == "Leo");
  CHECK(info->level == 7);
  CHECK(info->hp == 1500);
  CHECK(info->max_hp == 2000);
  CHECK(info->sp == 40);
  CHECK(info->max_sp == 80);
  CHECK(info->expire_unix_s == 1800000000U);
  CHECK(info->faith == 33);
  CHECK(info->calls == 5);
  CHECK(info->kills == 1234);
  CHECK(info->range_cells == 9);

  // L'encodeur d'essai rend les mêmes octets.
  CHECK(ro::proto::EncodeMercenaryInfo(*info) == frame);
}

TEST_CASE("MercenaryPackets : une fiche tronquée ou étrangère est refusée", "[mercenarypackets]") {
  std::vector<std::uint8_t> frame = ro::proto::EncodeMercenaryInfo({});
  frame.pop_back();
  CHECK_FALSE(ro::proto::DecodeMercenaryInfo(frame));
  std::vector<std::uint8_t> other = ro::proto::EncodeMercenaryInfo({});
  PutU16(other, 0, 0x029C);
  CHECK_FALSE(ro::proto::DecodeMercenaryInfo(other));
}

TEST_CASE("MercenaryPackets : une valeur isolée", "[mercenarypackets]") {
  const std::vector<std::uint8_t> frame = {0xA2, 0x02, 0xBE, 0x00, 0x2A, 0x00, 0x00, 0x00};
  const auto status = ro::proto::DecodeMercenaryStatus(frame);
  REQUIRE(status);
  CHECK(status->variable == ro::proto::kMercenaryVarFaith);
  CHECK(status->value == 42);
  CHECK_FALSE(
      ro::proto::DecodeMercenaryStatus(std::vector<std::uint8_t>(frame.begin(), frame.end() - 1)));
}

TEST_CASE("MercenaryPackets : la liste des compétences, 37 octets par entrée",
          "[mercenarypackets]") {
  ro::proto::MercenarySkillList list;
  list.skills.push_back({.skill_id = 8201,
                         .targeting = 1,
                         .level = 5,
                         .sp_cost = 15,
                         .range_cells = 1,
                         .name = "MS_BASH"});
  list.skills.push_back({.skill_id = 8223,
                         .targeting = 4,
                         .level = 1,
                         .sp_cost = 14,
                         .range_cells = 0,
                         .name = "MER_QUICKEN"});
  const std::vector<std::uint8_t> frame = ro::proto::EncodeMercenarySkillList(list);
  REQUIRE(frame.size() == 4 + (2 * 37));
  CHECK(frame[2] == frame.size());
  // Le second identifiant, à l'en-tête plus une entrée.
  CHECK(frame[4 + 37] == (8223 & 0xFF));
  const auto decoded = ro::proto::DecodeMercenarySkillList(frame);
  REQUIRE(decoded);
  REQUIRE(decoded->skills.size() == 2);
  CHECK(decoded->skills[0].skill_id == 8201);
  CHECK(decoded->skills[0].targeting == 1);
  CHECK(decoded->skills[0].level == 5);
  CHECK(decoded->skills[0].sp_cost == 15);
  CHECK(decoded->skills[0].range_cells == 1);
  CHECK(decoded->skills[0].name == "MS_BASH");
  CHECK(decoded->skills[1].name == "MER_QUICKEN");
  CHECK_FALSE(decoded->skills[1].upgradable);

  std::vector<std::uint8_t> ragged = frame;
  ragged.pop_back();
  PutU16(ragged, 2, static_cast<std::uint16_t>(ragged.size()));
  CHECK_FALSE(ro::proto::DecodeMercenarySkillList(ragged));
}

TEST_CASE("MercenaryPackets : l'ordre du joueur tient sur trois octets", "[mercenarypackets]") {
  const auto dismiss =
      ro::proto::EncodeMercenaryCommand({.command = ro::proto::kMercenaryCommandDismiss});
  REQUIRE(dismiss.has_value());
  CHECK(*dismiss == std::vector<std::uint8_t>{0x9F, 0x02, 0x02});
  const auto info =
      ro::proto::EncodeMercenaryCommand({.command = ro::proto::kMercenaryCommandInfo});
  REQUIRE(info.has_value());
  CHECK(*info == std::vector<std::uint8_t>{0x9F, 0x02, 0x01});
}

TEST_CASE("MercenaryPackets : le serveur lit l'ordre là où le client l'écrit",
          "[mercenarypackets]") {
  // La commande au troisième octet, et nulle part ailleurs.
  constexpr std::array<std::uint16_t, 1> kCommandOffsets = {2};
  CHECK(ro::proto::MatchesServerLayout(ro::proto::MercenaryCommand::kOpcode,
                                       "clif_parse_mercenary_action", kCommandOffsets));
  constexpr std::array<std::uint16_t, 1> kWrongOffsets = {3};
  CHECK_FALSE(ro::proto::MatchesServerLayout(ro::proto::MercenaryCommand::kOpcode,
                                             "clif_parse_mercenary_action", kWrongOffsets));
  CHECK_FALSE(ro::proto::MatchesServerLayout(ro::proto::MercenaryCommand::kOpcode,
                                             "clif_parse_HomMenu", kCommandOffsets));
}
