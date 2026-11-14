// LA FICHE D'UN MONSTRE, décodée.
//
// 🔴 CE BANC PORTE CE QU'AUCUNE SESSION NE MONTRERAIT VITE :
//
//   · le bloc fixe fait QUATRE-VINGT-HUIT octets, dans l'ordre que le serveur
//     écrit. Un champ interverti donnerait des statistiques plausibles à la
//     mauvaise ligne ;
//   · les résistances sont SIGNÉES : un monstre qui absorbe un élément s'en
//     soigne, et le lire non signé l'afficherait à plus de soixante mille ;
//   · une liste TRONQUÉE garde ce qui tient entier, et les deux queues
//     facultatives — identité, EXP estimée — s'absentent sans rien casser ;
//   · un bloc fixe incomplet se lit comme un monstre INCONNU, pas comme une
//     trame à redemander.

#include "ro/net/monster_sheet.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/connection.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/bank.h"
#include "ro/sim/monster_sheet.h"

#include "support/loopback_server.h"
#include "support/wire_bytes.h"

using ro::net::ApplyMonsterSheetFrame;
using ro::net::DecodeMonsterSheet;
using ro::net::Frame;
using ro::net::FrameOutcome;
using ro::sim::MonsterSheetBook;
using ro::test_support::PutU16;
using ro::test_support::PutU32;

namespace {

constexpr std::uint32_t kPoring = 1002;
constexpr std::uint32_t kPoringSprite = 1002;
constexpr std::uint32_t kJellopy = 909;
constexpr std::uint32_t kJellopyRate = 7000;
constexpr std::uint32_t kPoringCard = 4001;
constexpr std::uint32_t kPoringCardRate = 1;
constexpr std::uint16_t kPrtFieldCount = 60;
constexpr std::uint16_t kEmotion = 197;
constexpr std::int16_t kHealedByHoly = -25;
constexpr std::uint32_t kEstimatedBase = 270;
constexpr std::uint32_t kNextBase = 5400;
// L'indicateur « niveau de job au maximum ».
constexpr std::uint8_t kJobMaxed = 0x02;

void PutText(std::vector<std::uint8_t>& into, std::string_view text) {
  into.push_back(static_cast<std::uint8_t>(text.size()));
  for (const char letter : text) {
    into.push_back(static_cast<std::uint8_t>(letter));
  }
}

// Ce que la fiche porte, queue par queue.
struct Tails {
  bool lists = true;
  bool identity = true;
  bool exp_gain = true;
  // Le nom de la fiche, tel que la trame le porte.
  std::string_view name = "Poring";
};

[[nodiscard]] std::vector<std::uint8_t> SheetBytes(const Tails& tails = {}) {
  std::vector<std::uint8_t> bytes;
  PutU16(bytes, ro::proto::ZcMoonlightMobinfo::kOpcode);
  PutU16(bytes, 0);  // la longueur, réécrite plus bas
  PutU32(bytes, kPoring);
  bytes.push_back(0);  // trouvé

  PutU32(bytes, kPoringSprite);
  PutU16(bytes, 1);   // niveau
  PutU32(bytes, 50);  // PV
  PutU32(bytes, 7);   // SP
  PutU32(bytes, 2);   // EXP de base
  PutU32(bytes, 1);   // EXP de job
  PutU32(bytes, 0);   // EXP de MVP
  PutU16(bytes, 7);   // ATK
  PutU16(bytes, 10);
  PutU16(bytes, 3);  // MATK
  PutU16(bytes, 4);
  PutU16(bytes, 5);  // DEF
  PutU16(bytes, 6);
  PutU16(bytes, 8);  // MDEF
  PutU16(bytes, 9);
  PutU16(bytes, 11);   // STR
  PutU16(bytes, 12);   // AGI
  PutU16(bytes, 13);   // VIT
  PutU16(bytes, 14);   // INT
  PutU16(bytes, 15);   // DEX
  PutU16(bytes, 16);   // LUK
  PutU16(bytes, 1);    // portée
  bytes.push_back(1);  // taille : moyenne
  bytes.push_back(3);  // race : plante
  bytes.push_back(1);  // élément : eau
  bytes.push_back(2);  // niveau élémentaire
  bytes.push_back(0);  // rang de boss
  bytes.push_back(0);  // classe
  PutU32(bytes, 0x81);
  PutU16(bytes, 400);  // une case en quatre cents millisecondes
  for (int element = 0; element < 10; ++element) {
    PutU16(bytes, static_cast<std::uint16_t>(element == 6 ? kHealedByHoly : 100));
  }
  PutText(bytes, tails.name);

  if (tails.lists) {
    bytes.push_back(2);
    PutU32(bytes, kJellopy);
    PutU32(bytes, kJellopyRate);
    bytes.push_back(0);
    PutText(bytes, "Jellopy");
    PutU32(bytes, kPoringCard);
    PutU32(bytes, kPoringCardRate);
    bytes.push_back(1);
    PutText(bytes, "Poring Card");

    bytes.push_back(1);
    PutU16(bytes, kPrtFieldCount);
    PutText(bytes, "prt_fild08");

    bytes.push_back(1);
    PutU16(bytes, kEmotion);
    PutU16(bytes, 1);
    PutText(bytes, "Emotion");
  }
  if (tails.lists && tails.identity) {
    PutText(bytes, "PORING");
    bytes.push_back(0);  // invoqué
    bytes.push_back(3);  // homonymes
    PutU32(bytes, kPoring);
  }
  if (tails.lists && tails.identity && tails.exp_gain) {
    PutU32(bytes, kEstimatedBase);
    PutU32(bytes, 135);
    PutU32(bytes, kNextBase);
    PutU32(bytes, 0);
    bytes.push_back(kJobMaxed);
  }
  bytes[2] = static_cast<std::uint8_t>(bytes.size() & 0xFF);
  bytes[3] = static_cast<std::uint8_t>(bytes.size() >> 8);
  return bytes;
}

[[nodiscard]] Frame SheetFrame(std::vector<std::uint8_t> bytes) {
  return Frame{.opcode = ro::proto::ZcMoonlightMobinfo::kOpcode, .bytes = std::move(bytes)};
}

}  // namespace

TEST_CASE("Fiche de monstre : le bloc fixe tombe champ par champ à sa place",
          "[net][monster_sheet]") {
  const auto reply = DecodeMonsterSheet(SheetBytes());
  REQUIRE(reply.has_value());
  CHECK(reply->asked_id == kPoring);
  REQUIRE(reply->sheet.has_value());
  const ro::sim::MonsterSheet& sheet = *reply->sheet;
  CHECK(sheet.sprite_class == kPoringSprite);
  CHECK(sheet.hp == 50);
  CHECK(sheet.sp == 7);
  CHECK(sheet.atk_min == 7);
  CHECK(sheet.atk_max == 10);
  CHECK(sheet.mdef_bonus == 9);
  CHECK(sheet.luk == 16);
  CHECK(sheet.size == 1);
  CHECK(sheet.race == 3);
  CHECK(sheet.element == 1);
  CHECK(sheet.element_level == 2);
  CHECK(sheet.mode == 0x81);
  CHECK(sheet.walk_ms_per_cell == 400);
  CHECK(sheet.name == "Poring");
}

TEST_CASE("Fiche de monstre : un nom rembourré de NUL s'arrête au premier",
          "[net][monster_sheet]") {
  // ⚠ LE MÊME REMPLISSAGE QUE SUR LES SOURCES D'UN OBJET : le nom de la base du
  // serveur peut porter ses NUL dans la longueur annoncée.
  constexpr std::size_t kServerNameBytes = 24;
  std::string padded = "Poring";
  padded.resize(kServerNameBytes, '\0');
  const auto reply = DecodeMonsterSheet(SheetBytes(Tails{.name = padded}));
  REQUIRE(reply.has_value());
  REQUIRE(reply->sheet.has_value());
  CHECK(reply->sheet->name == "Poring");
  // La suite de la trame se lit toujours : la longueur a été consommée entière.
  CHECK(reply->sheet->drops.size() == 2);
  CHECK(reply->sheet->aegis_name == "PORING");
}

TEST_CASE("Fiche de monstre : une résistance négative reste négative", "[net][monster_sheet]") {
  const auto reply = DecodeMonsterSheet(SheetBytes());
  REQUIRE(reply.has_value());
  REQUIRE(reply->sheet.has_value());
  CHECK(reply->sheet->resist_percent[6] == kHealedByHoly);
  CHECK(reply->sheet->resist_percent[0] == 100);
}

TEST_CASE("Fiche de monstre : butin, apparitions, compétences, identité et EXP estimée",
          "[net][monster_sheet]") {
  const auto reply = DecodeMonsterSheet(SheetBytes());
  REQUIRE(reply.has_value());
  REQUIRE(reply->sheet.has_value());
  const ro::sim::MonsterSheet& sheet = *reply->sheet;
  REQUIRE(sheet.drops.size() == 2);
  CHECK(sheet.drops[0].item_id == kJellopy);
  CHECK(sheet.drops[0].rate_hundredths == kJellopyRate);
  CHECK_FALSE(sheet.drops[0].mvp_reward);
  CHECK(sheet.drops[1].mvp_reward);
  CHECK(sheet.drops[1].name == "Poring Card");
  REQUIRE(sheet.spawns.size() == 1);
  CHECK(sheet.spawns[0].count == kPrtFieldCount);
  CHECK(sheet.spawns[0].map_name == "prt_fild08");
  REQUIRE(sheet.skills.size() == 1);
  CHECK(sheet.skills[0].skill_id == kEmotion);
  CHECK(sheet.skills[0].server_name == "Emotion");
  CHECK(sheet.aegis_name == "PORING");
  CHECK(sheet.namesake_count == 3);
  CHECK(sheet.namesake_origin == kPoring);
  REQUIRE(sheet.exp_gain.has_value());
  CHECK(sheet.exp_gain->base_exp == kEstimatedBase);
  CHECK(sheet.exp_gain->next_base_exp == kNextBase);
  CHECK_FALSE(sheet.exp_gain->base_level_maxed);
  CHECK(sheet.exp_gain->job_level_maxed);
}

TEST_CASE("Fiche de monstre : les queues absentes laissent leurs défauts", "[net][monster_sheet]") {
  const auto without_identity = DecodeMonsterSheet(SheetBytes(Tails{.identity = false}));
  REQUIRE(without_identity.has_value());
  REQUIRE(without_identity->sheet.has_value());
  CHECK(without_identity->sheet->aegis_name.empty());
  CHECK(without_identity->sheet->namesake_count == 1);
  CHECK_FALSE(without_identity->sheet->exp_gain.has_value());

  const auto without_gain = DecodeMonsterSheet(SheetBytes(Tails{.exp_gain = false}));
  REQUIRE(without_gain.has_value());
  REQUIRE(without_gain->sheet.has_value());
  CHECK(without_gain->sheet->aegis_name == "PORING");
  CHECK_FALSE(without_gain->sheet->exp_gain.has_value());
}

TEST_CASE("Fiche de monstre : une liste tronquée garde ce qui tient entier",
          "[net][monster_sheet]") {
  std::vector<std::uint8_t> bytes = SheetBytes(Tails{.identity = false});
  // Le dernier nom de compétence perd deux octets, le reste est intact.
  bytes.resize(bytes.size() - 2);
  const auto reply = DecodeMonsterSheet(bytes);
  REQUIRE(reply.has_value());
  REQUIRE(reply->sheet.has_value());
  CHECK(reply->sheet->drops.size() == 2);
  CHECK(reply->sheet->spawns.size() == 1);
  CHECK(reply->sheet->skills.empty());
}

TEST_CASE("Fiche de monstre : un monstre inconnu, et un bloc fixe incomplet, se lisent pareil",
          "[net][monster_sheet]") {
  std::vector<std::uint8_t> unknown;
  PutU16(unknown, ro::proto::ZcMoonlightMobinfo::kOpcode);
  PutU16(unknown, 9);
  PutU32(unknown, kPoring);
  unknown.push_back(1);
  MonsterSheetBook book;
  REQUIRE(ApplyMonsterSheetFrame(SheetFrame(unknown), book) == FrameOutcome::kApplied);
  CHECK(book.IsUnknown(kPoring));
  CHECK(book.Find(kPoring) == nullptr);

  std::vector<std::uint8_t> cut = SheetBytes();
  cut.resize(ro::net::kMonsterSheetHeaderSize + ro::net::kMonsterSheetFixedSize - 1);
  const auto reply = DecodeMonsterSheet(cut);
  REQUIRE(reply.has_value());
  CHECK_FALSE(reply->sheet.has_value());
}

TEST_CASE("Fiche de monstre : la réponse se range sous l'identifiant DEMANDÉ",
          "[net][monster_sheet]") {
  MonsterSheetBook book;
  REQUIRE(ApplyMonsterSheetFrame(SheetFrame(SheetBytes()), book) == FrameOutcome::kApplied);
  REQUIRE(book.Find(kPoring) != nullptr);
  const std::uint32_t first = book.ArrivalOf(kPoring);
  CHECK(first != 0);
  // La même réponse, reçue à nouveau : c'est une arrivée neuve.
  REQUIRE(ApplyMonsterSheetFrame(SheetFrame(SheetBytes()), book) == FrameOutcome::kApplied);
  CHECK(book.ArrivalOf(kPoring) != first);
}

TEST_CASE("Fiche de monstre : le relevé de Sense se range à part", "[net][monster_sheet]") {
  ro::proto::ZcMonsterInfo packet;
  packet.class_ = static_cast<std::uint16_t>(kPoringSprite);
  packet.level = 3;
  packet.hp = 60;
  packet.def = 2;
  packet.mdef = 5;
  packet.element = 1;
  packet.water = 25;
  packet.undead = 100;
  MonsterSheetBook book;
  REQUIRE(ApplyMonsterSheetFrame(Frame{.opcode = ro::proto::ZcMonsterInfo::kOpcode,
                                       .bytes = ro::proto::Encode(packet)},
                                 book) == FrameOutcome::kApplied);
  CHECK(book.sense_readings() == 1);
  REQUIRE(book.sense().has_value());
  CHECK(book.sense()->sprite_class == kPoringSprite);
  CHECK(book.sense()->hp == 60);
  CHECK(book.sense()->resist_percent[0] == 25);
  CHECK(book.sense()->resist_percent[8] == 100);
  // ⚠ LE CHANGEMENT DE CARTE EFFACE LE RELEVÉ, pas son compte : qui l'a déjà lu
  // ne doit pas le relire.
  book.Clear();
  CHECK_FALSE(book.sense().has_value());
  CHECK(book.sense_readings() == 1);
}

TEST_CASE("Fiche de monstre : la demande part en 0x0F1F, taille et nature comprises",
          "[net][monster_sheet]") {
  ro::test::LoopbackServer server;
  REQUIRE(server.listening());
  auto link = ro::net::Connection::Open("127.0.0.1", server.port());
  REQUIRE(link.has_value());
  REQUIRE(server.Accept());
  REQUIRE(ro::net::RequestMonsterSheet(*link, kPoringSprite, /*by_view=*/true).has_value());
  const std::vector<std::uint8_t> sent =
      server.Receive(ro::proto::CzMoonlightReqMobinfo::kWireSize);
  ro::proto::CzMoonlightReqMobinfo told;
  REQUIRE(ro::proto::Decode(sent, told));
  // ⚠ LE SERVEUR LIT LA TAILLE : une taille nulle ferait jeter la demande.
  CHECK(told.packetLength ==
        static_cast<std::int16_t>(ro::proto::CzMoonlightReqMobinfo::kWireSize));
  CHECK(told.mob_id == kPoringSprite);
  CHECK(told.by_view == 1);
}
