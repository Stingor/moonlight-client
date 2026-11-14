// CE QUE LE SERVEUR SEUL SAIT D'UNE COMPÉTENCE, décodé.
//
// 🔴 CE BANC PORTE QUATRE CHOSES QU'AUCUNE SESSION NE MONTRERAIT VITE :
//
//   · un RANG fait seize octets, dans l'ordre que le serveur écrit —
//     `[variable:4][fixe:4][attente:4][délai:4]`. Un champ interverti donnerait
//     des temps plausibles à la mauvaise colonne, ce qui se lit comme un serveur
//     mal réglé plutôt que comme un défaut de lecture ;
//   · une trame TRONQUÉE annonce plus de rangs qu'elle n'en porte : on lit ce
//     qui tient ;
//   · la réponse d'un OBJET passe par le MÊME opcode, et ses entrées sont des
//     monstres. La lire comme des rangs donnerait n'importe quoi ;
//   · le NOM du monstre est FACULTATIF — le mannequin neutre n'en a pas — et
//     son absence n'est pas une trame tronquée.

#include "ro/net/skill_facts.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/proto/map_packets.h"
#include "ro/sim/skill_facts.h"
#include "ro/sim/world.h"

#include "support/wire_bytes.h"

using ro::net::ApplySkillFactsFrame;
using ro::net::Frame;
using ro::net::FrameOutcome;
using ro::sim::SkillCastFacts;
using ro::sim::World;
using ro::test_support::PutU16;
using ro::test_support::PutU32;
using ro::test_support::PutU64;

namespace {

constexpr std::uint32_t kHeal = 28;

void SealLength(std::vector<std::uint8_t>& bytes) {
  bytes[2] = static_cast<std::uint8_t>(bytes.size() & 0xFF);
  bytes[3] = static_cast<std::uint8_t>(bytes.size() >> 8);
}

[[nodiscard]] Frame CastFrame(const std::vector<SkillCastFacts>& levels, int announced = -1,
                              std::uint8_t is_skill = 1) {
  std::vector<std::uint8_t> bytes;
  PutU16(bytes, ro::proto::ZcMoonlightTechdata::kOpcode);
  PutU16(bytes, 0);  // la longueur, réécrite plus bas
  PutU32(bytes, kHeal);
  bytes.push_back(is_skill);
  bytes.push_back(0);  // la portée, renvoyée en écho
  bytes.push_back(static_cast<std::uint8_t>(announced < 0 ? levels.size()
                                                          : static_cast<std::size_t>(announced)));
  for (const SkillCastFacts& level : levels) {
    PutU32(bytes, static_cast<std::uint32_t>(level.cast_variable_ms));
    PutU32(bytes, static_cast<std::uint32_t>(level.cast_fixed_ms));
    PutU32(bytes, static_cast<std::uint32_t>(level.cooldown_ms));
    PutU32(bytes, static_cast<std::uint32_t>(level.after_delay_ms));
  }
  SealLength(bytes);
  return Frame{.opcode = ro::proto::ZcMoonlightTechdata::kOpcode, .bytes = bytes};
}

[[nodiscard]] Frame DamageFrame(std::uint32_t target, std::string_view name,
                                std::uint8_t status = 0) {
  std::vector<std::uint8_t> bytes;
  PutU16(bytes, ro::proto::ZcMoonlightDamage::kOpcode);
  PutU16(bytes, 0);
  PutU32(bytes, kHeal);
  PutU16(bytes, 7);  // le rang
  PutU32(bytes, target);
  bytes.push_back(status);
  bytes.push_back(2);  // magique
  PutU16(bytes, 3);    // trois coups
  PutU64(bytes, 100);
  PutU64(bytes, 300);
  PutU64(bytes, 200);
  if (!name.empty()) {
    bytes.push_back(static_cast<std::uint8_t>(name.size()));
    for (const char letter : name) {
      bytes.push_back(static_cast<std::uint8_t>(letter));
    }
  }
  SealLength(bytes);
  return Frame{.opcode = ro::proto::ZcMoonlightDamage::kOpcode, .bytes = bytes};
}

}  // namespace

TEST_CASE("Cast d'une compétence : les quatre temps d'un rang tombent à leur place",
          "[net][grimoire]") {
  // 🔴 LES DEUX INCANTATIONS NE SE RÉDUISENT PAS PAREIL : le variable tombe avec
  // la DEX et l'INT et se coupe à l'interruption, le fixe non. Les intervertir
  // donnerait deux chiffres plausibles et faux.
  const std::vector<SkillCastFacts> levels = {
      SkillCastFacts{.cast_variable_ms = 800,
                     .cast_fixed_ms = 200,
                     .cooldown_ms = 1500,
                     .after_delay_ms = 300},
      SkillCastFacts{.cast_variable_ms = 900,
                     .cast_fixed_ms = 250,
                     .cooldown_ms = 1600,
                     .after_delay_ms = 350},
  };
  World world;
  REQUIRE(ApplySkillFactsFrame(CastFrame(levels), world) == FrameOutcome::kApplied);

  const auto told = world.skill_facts().CastOf(kHeal);
  REQUIRE(told.size() == 2);
  CHECK(told[0].cast_variable_ms == 800);
  CHECK(told[0].cast_fixed_ms == 200);
  CHECK(told[0].cooldown_ms == 1500);
  CHECK(told[0].after_delay_ms == 300);
  CHECK(told[1].cast_variable_ms == 900);
}

TEST_CASE("Cast d'une compétence : une trame tronquée ne lit que ce qui tient", "[net][grimoire]") {
  // ⚠ LE COMPTE EST CRU, MAIS PAS SUR PAROLE : une trame coupée en annoncerait
  // plus qu'elle n'en porte, et le lecteur sortirait de ses octets.
  const std::vector<SkillCastFacts> levels = {SkillCastFacts{.cast_variable_ms = 800}};
  World world;
  REQUIRE(ApplySkillFactsFrame(CastFrame(levels, /*announced=*/9), world) ==
          FrameOutcome::kApplied);
  CHECK(world.skill_facts().CastOf(kHeal).size() == 1);
}

TEST_CASE("Cast d'une compétence : la réponse d'un OBJET n'est pas pour nous", "[net][grimoire]") {
  // 🔴 LE MÊME OPCODE SERT AUX DEUX, et c'est l'octet `is_skill` qui tranche :
  // pour un objet, la queue porte des MONSTRES. La lire comme des rangs
  // donnerait des temps d'incantation tirés d'identifiants de monstres.
  const std::vector<SkillCastFacts> levels = {SkillCastFacts{.cast_variable_ms = 800}};
  World world;
  REQUIRE(ApplySkillFactsFrame(CastFrame(levels, -1, /*is_skill=*/0), world) ==
          FrameOutcome::kApplied);
  CHECK(world.skill_facts().CastOf(kHeal).empty());
}

TEST_CASE("Dégâts : le nom du monstre est facultatif", "[net][grimoire]") {
  World world;
  // Contre un vrai monstre : le nom vient avec.
  REQUIRE(ApplySkillFactsFrame(DamageFrame(1002, "Poring"), world) == FrameOutcome::kApplied);
  const ro::sim::SkillDamageFacts* told = world.skill_facts().DamageOf(kHeal);
  REQUIRE(told != nullptr);
  CHECK(told->target_name == "Poring");
  CHECK(told->target_mob_id == 1002);
  CHECK(told->least == 100);
  CHECK(told->most == 300);
  CHECK(told->average == 200);
  CHECK(told->hits == 3);
  CHECK(told->level == 7);

  // ⚠ CONTRE LE MANNEQUIN, IL N'Y EN A PAS — et ce n'est pas une trame tronquée.
  REQUIRE(ApplySkillFactsFrame(DamageFrame(0, ""), world) == FrameOutcome::kApplied);
  told = world.skill_facts().DamageOf(kHeal);
  REQUIRE(told != nullptr);
  CHECK(told->target_name.empty());
  CHECK(told->target_mob_id == 0);
}

TEST_CASE("Dégâts : un refus « trop rapproché » ne remplace pas l'estimation", "[net][grimoire]") {
  World world;
  REQUIRE(ApplySkillFactsFrame(DamageFrame(1002, "Poring"), world) == FrameOutcome::kApplied);
  REQUIRE(world.skill_facts().damage_too_soon_count() == 0);

  REQUIRE(ApplySkillFactsFrame(DamageFrame(1031, "", ro::net::kSkillDamageTooSoon), world) ==
          FrameOutcome::kApplied);
  // L'estimation d'avant reste, et rien n'écrira « indisponible ».
  const ro::sim::SkillDamageFacts* const told = world.skill_facts().DamageOf(kHeal);
  REQUIRE(told != nullptr);
  CHECK(told->status == 0);
  CHECK(told->target_mob_id == 1002);
  // Le refus se compte, avec la question refusée.
  CHECK(world.skill_facts().damage_too_soon_count() == 1);
  CHECK(world.skill_facts().last_damage_too_soon() ==
        ro::sim::SkillDamageQuestion{.skill_id = kHeal, .target_mob_id = 1031});
}

TEST_CASE("Dégâts : une seule estimation par compétence", "[net][grimoire]") {
  // 🔴 CHANGER DE CIBLE, C'EST UNE AUTRE QUESTION. Garder les deux ferait lire
  // une estimation faite contre autre chose.
  World world;
  REQUIRE(ApplySkillFactsFrame(DamageFrame(1002, "Poring"), world) == FrameOutcome::kApplied);
  REQUIRE(ApplySkillFactsFrame(DamageFrame(0, ""), world) == FrameOutcome::kApplied);
  const ro::sim::SkillDamageFacts* const told = world.skill_facts().DamageOf(kHeal);
  REQUIRE(told != nullptr);
  CHECK(told->target_mob_id == 0);
  CHECK(told->target_name.empty());
}

TEST_CASE("Dégâts : le verdict du serveur passe tel quel", "[net][grimoire]") {
  // ⚠ ZÉRO EST « L'ESTIMATION VAUT » : c'est le seul cas où les chiffres se
  // lisent, et les autres ont chacun leur phrase à l'écran.
  World world;
  REQUIRE(ApplySkillFactsFrame(DamageFrame(1002, "Poring", /*status=*/1), world) ==
          FrameOutcome::kApplied);
  const ro::sim::SkillDamageFacts* const told = world.skill_facts().DamageOf(kHeal);
  REQUIRE(told != nullptr);
  CHECK(told->status == 1);
}
