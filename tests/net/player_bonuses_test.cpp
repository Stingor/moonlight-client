// Le détail de ce que l'équipement apporte, lu depuis les octets du serveur.
//
// 🔴 CE QUI SE PROUVE ICI EST L'ORDRE. Quarante-neuf entiers de même taille se
// suivent dans une trame : une permutation ne fait pas échouer le décodage, elle
// écrit « Vol SP » là où le serveur disait « Splash ». La seule façon de la voir
// est de donner à chaque champ une valeur DIFFÉRENTE — son rang — et de relire
// quelques champs nommés.

#include "ro/net/player_bonuses.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/byte_writer.h"
#include "ro/net/world_update.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/player_bonuses.h"
#include "ro/sim/world.h"

#include "support/net_frames.h"
#include "support/wire_bytes.h"

using ro::net::ApplyFrame;
using ro::net::FrameOutcome;
using ro::sim::World;
using ro::test_support::FrameOf;
using ro::test_support::PutU16;
using ro::test_support::PutU32;

namespace {

// Les six, et les quarante-neuf. Voir `ZC_MOONLIGHT_STAT_BONUS`.
constexpr std::size_t kAttributes = 6;
constexpr std::size_t kWideFields = 49;

// Ce qu'on écrit dans chaque champ pour que sa PLACE se lise dans sa valeur.
constexpr std::int16_t kEquipBase = 100;
constexpr std::int16_t kCardBase = 200;

// Le bloc fixe, écrit comme le serveur l'écrit : les six et les six, puis les
// quarante-neuf entiers, chacun portant son rang.
std::vector<std::uint8_t> FixedBlock() {
  std::vector<std::uint8_t> bytes;
  ro::core::ByteWriter writer(bytes);
  writer.U16(ro::proto::ZcMoonlightStatBonus::kOpcode);
  writer.U16(0);  // la longueur, remplie par l'appelant
  for (std::size_t rank = 0; rank < kAttributes; ++rank) {
    writer.U16(static_cast<std::uint16_t>(kEquipBase + static_cast<std::int16_t>(rank)));
  }
  for (std::size_t rank = 0; rank < kAttributes; ++rank) {
    writer.U16(static_cast<std::uint16_t>(kCardBase + static_cast<std::int16_t>(rank)));
  }
  for (std::size_t rank = 1; rank <= kWideFields; ++rank) {
    writer.U32(static_cast<std::uint32_t>(rank));
  }
  return bytes;
}

void PutLength(std::vector<std::uint8_t>& bytes) {
  bytes[2] = static_cast<std::uint8_t>(bytes.size() & 0xFF);
  bytes[3] = static_cast<std::uint8_t>(bytes.size() >> 8);
}

}  // namespace

TEST_CASE("Traduction : les quarante-neuf apports se rangent dans l'ordre du serveur",
          "[net][bonus]") {
  World world;
  CHECK_FALSE(world.player_bonuses().told);

  std::vector<std::uint8_t> bytes = FixedBlock();
  REQUIRE(bytes.size() == ro::net::kStatBonusFixedSize);
  PutLength(bytes);

  CHECK(ApplyFrame(FrameOf(bytes), world, 0) == FrameOutcome::kApplied);
  const ro::sim::PlayerBonuses& bonuses = world.player_bonuses();

  // ⚠ « DIT » ET NON « NON NUL » : c'est ce qui sépare un personnage nu d'un
  // personnage dont le serveur n'a pas parlé.
  CHECK(bonuses.told);

  CHECK(bonuses.param_equip[0] == kEquipBase);
  CHECK(bonuses.param_equip[5] == kEquipBase + 5);
  CHECK(bonuses.param_bonus[0] == kCardBase);
  CHECK(bonuses.param_bonus[5] == kCardBase + 5);

  // Un champ par groupe du serveur : le premier, le dernier, et les jointures.
  CHECK(bonuses.eatk == 1);
  CHECK(bonuses.ematk == 2);
  CHECK(bonuses.fcast_pct == 10);
  CHECK(bonuses.atk_pct == 11);
  CHECK(bonuses.perfect_hit == 17);
  CHECK(bonuses.hp_pct == 18);
  CHECK(bonuses.unbreak_pct == 25);
  CHECK(bonuses.pot_hp_pct == 26);
  CHECK(bonuses.steal_pct == 32);
  CHECK(bonuses.def_melee_pct == 33);
  CHECK(bonuses.splash_add == 38);
  CHECK(bonuses.hp_drain_pct == 39);
  CHECK(bonuses.sp_drain_pct == 40);
  CHECK(bonuses.break_weapon_pct == 41);
  CHECK(bonuses.magic_sp_gain == 47);
  CHECK(bonuses.refine_atk == 48);
  CHECK(bonuses.refine_def == 49);

  // Sans queue, les trois listes sont vides — et la trame reste bonne.
  CHECK(bonuses.conditional.empty());
  CHECK(bonuses.skills.empty());
  CHECK(bonuses.items.empty());
}

TEST_CASE("Traduction : les trois listes de queue se lisent chacune à sa taille", "[net][bonus]") {
  World world;
  std::vector<std::uint8_t> bytes = FixedBlock();

  // Deux conditionnels, de huit octets chacun.
  PutU16(bytes, 2);
  PutU16(bytes, static_cast<std::uint16_t>(ro::sim::ConditionalBonusKind::kSubRace));
  PutU16(bytes, 7);  // l'index, sur seize bits signés
  PutU32(bytes, 25);
  PutU16(bytes, static_cast<std::uint16_t>(ro::sim::ConditionalBonusKind::kAddSize));
  PutU16(bytes, 1);
  PutU32(bytes, static_cast<std::uint32_t>(-5));

  // 🔴 UNE ENTRÉE DE COMPÉTENCE FAIT DOUZE OCTETS, PAS SEIZE : le serveur l'écrit
  // sous `pack(1)`. L'aligner naturellement décalerait la liste des objets.
  PutU16(bytes, 1);
  PutU16(bytes, static_cast<std::uint16_t>(ro::sim::SkillBonusKind::kAutoSpellOnSkill));
  PutU16(bytes, 20);  // la compétence lancée
  PutU16(bytes, 3);   // son niveau
  PutU32(bytes, 500);
  PutU16(bytes, 45);  // celle qui la déclenche

  // Un objet, dont l'identifiant tient sur trente-deux bits.
  PutU16(bytes, 1);
  PutU16(bytes, static_cast<std::uint16_t>(ro::sim::ItemBonusKind::kAddDrop));
  PutU32(bytes, 501);
  PutU32(bytes, 1000);

  PutLength(bytes);
  CHECK(ApplyFrame(FrameOf(bytes), world, 0) == FrameOutcome::kApplied);
  const ro::sim::PlayerBonuses& bonuses = world.player_bonuses();

  REQUIRE(bonuses.conditional.size() == 2);
  CHECK(bonuses.conditional[0].code ==
        static_cast<std::uint16_t>(ro::sim::ConditionalBonusKind::kSubRace));
  CHECK(bonuses.conditional[0].index == 7);
  CHECK(bonuses.conditional[0].value == 25);
  // ⚠ UNE MAGNITUDE PEUT ÊTRE NÉGATIVE : « moins cinq pour cent » est un bonus
  // comme un autre, et le lire non signé en ferait quatre milliards.
  CHECK(bonuses.conditional[1].value == -5);

  REQUIRE(bonuses.skills.size() == 1);
  CHECK(bonuses.skills[0].skill_id == 20);
  CHECK(bonuses.skills[0].level == 3);
  CHECK(bonuses.skills[0].value == 500);
  CHECK(bonuses.skills[0].triggered_by_skill == 45);

  REQUIRE(bonuses.items.size() == 1);
  CHECK(bonuses.items[0].item_id == 501);
  CHECK(bonuses.items[0].rate == 1000);
}

TEST_CASE("Traduction : un compte qui ment ne lit que ce qui est là", "[net][bonus]") {
  World world;
  std::vector<std::uint8_t> bytes = FixedBlock();

  // La trame annonce cinq conditionnels et n'en porte qu'un. C'est le cas d'une
  // trame coupée, et il ne doit ni lire au-delà ni jeter ce qui est lisible.
  PutU16(bytes, 5);
  PutU16(bytes, static_cast<std::uint16_t>(ro::sim::ConditionalBonusKind::kSubElement));
  PutU16(bytes, 3);
  PutU32(bytes, 10);
  PutLength(bytes);

  CHECK(ApplyFrame(FrameOf(bytes), world, 0) == FrameOutcome::kApplied);
  CHECK(world.player_bonuses().conditional.size() == 1);
  CHECK(world.player_bonuses().skills.empty());
}

TEST_CASE("Traduction : un relevé remplace l'ancien, il ne s'y ajoute pas", "[net][bonus]") {
  World world;
  std::vector<std::uint8_t> first = FixedBlock();
  PutU16(first, 1);
  PutU16(first, static_cast<std::uint16_t>(ro::sim::ConditionalBonusKind::kSubRace));
  PutU16(first, 2);
  PutU32(first, 30);
  PutLength(first);
  CHECK(ApplyFrame(FrameOf(first), world, 0) == FrameOutcome::kApplied);
  REQUIRE(world.player_bonuses().conditional.size() == 1);

  // 🔴 LE SERVEUR RECOMPOSE TOUT À CHAQUE RECALCUL et n'envoie jamais de
  // différence. Garder l'ancien par morceaux laisserait afficher un bonus que
  // l'équipement retiré ne donne plus.
  std::vector<std::uint8_t> second = FixedBlock();
  PutLength(second);
  CHECK(ApplyFrame(FrameOf(second), world, 0) == FrameOutcome::kApplied);
  CHECK(world.player_bonuses().conditional.empty());
}

TEST_CASE("Traduction : un bloc fixe tronqué ne se devine pas", "[net][bonus]") {
  World world;
  std::vector<std::uint8_t> bytes = FixedBlock();
  bytes.resize(ro::net::kStatBonusFixedSize - 4);
  PutLength(bytes);

  // Indéchiffrable, et non « appliquée à moitié » : un relevé à trous se lirait
  // comme un personnage sans bonus, ce qui est une information FAUSSE.
  CHECK(ApplyFrame(FrameOf(bytes), world, 0) == FrameOutcome::kUndecodable);
  CHECK_FALSE(world.player_bonuses().told);
}
