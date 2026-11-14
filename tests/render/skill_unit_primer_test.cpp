// L'AMORCE D'UN PIÈGE ET LE FICHIER QUI REPART À SA FIN : ce qu'un piège du
// chasseur joue en se déclenchant, et l'éclair d'un Shockwave Trap qui reste
// sur le piège tant que le piège est là.

#include "ro/render/skill_unit_primer.h"

#include <algorithm>
#include <cstdint>
#include <span>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_duration.h"
#include "ro/render/primitive_str_loop.h"

namespace {

constexpr std::uint8_t kLevel = 5;
constexpr std::uint32_t kBlastMine = 143;
constexpr std::uint32_t kSkidTrap = 144;
constexpr std::uint32_t kAnkleSnare = 145;
constexpr std::uint32_t kLandMine = 147;
constexpr std::uint32_t kShockwaveTrap = 148;
constexpr std::uint32_t kSandman = 149;
constexpr std::uint32_t kFlasher = 150;
constexpr std::uint32_t kFreezingTrap = 151;
constexpr std::uint32_t kClaymoreTrap = 152;
constexpr std::uint32_t kShockwaveEffect = 145;
constexpr std::uint32_t kShockwaveHitEffect = 146;

}  // namespace

TEST_CASE("Amorce : la table est triée comme la recherche le suppose", "[render][unitprimer]") {
  const std::span<const ro::render::SkillUnitPrimerEntry> table =
      ro::render::SkillUnitPrimerTable();
  REQUIRE_FALSE(table.empty());
  CHECK(std::ranges::is_sorted(table, {}, &ro::render::SkillUnitPrimerEntry::unit_id));
}

TEST_CASE("Amorce : un piège du chasseur joue la sienne en devenant le piège usé",
          "[render][unitprimer]") {
  using ro::render::kUsedTrapUnit;
  using ro::render::SkillUnitPrimerOnLook;
  // Rien ne jouait au déclenchement : l'amorce retenue à la pose n'était lue
  // nulle part.
  CHECK(SkillUnitPrimerOnLook(kBlastMine, kLevel, kUsedTrapUnit) == 106);
  CHECK(SkillUnitPrimerOnLook(kSandman, kLevel, kUsedTrapUnit) == 139);
  CHECK(SkillUnitPrimerOnLook(kFlasher, kLevel, kUsedTrapUnit) == 99);
  CHECK(SkillUnitPrimerOnLook(kFreezingTrap, kLevel, kUsedTrapUnit) == 108);
  CHECK(SkillUnitPrimerOnLook(kClaymoreTrap, kLevel, kUsedTrapUnit) == 107);
  // Ceux qui n'en retiennent aucune restent muets.
  CHECK(SkillUnitPrimerOnLook(kSkidTrap, kLevel, kUsedTrapUnit) == 0);
  CHECK(SkillUnitPrimerOnLook(kAnkleSnare, kLevel, kUsedTrapUnit) == 0);
}

TEST_CASE("Amorce : ce client en ajoute une à Land Mine et à Shockwave Trap",
          "[render][unitprimer]") {
  using ro::render::kUsedTrapUnit;
  using ro::render::SkillUnitPrimerOnLook;
  // Déclenchés, ils ne montraient que leur modèle.
  CHECK(SkillUnitPrimerOnLook(kLandMine, kLevel, kUsedTrapUnit) == 97);
  CHECK(SkillUnitPrimerOnLook(kShockwaveTrap, kLevel, kUsedTrapUnit) == 146);
  // L'ajout reste hors de la table relevée, qui ne dit que le client d'origine.
  for (const ro::render::SkillUnitPrimerEntry& entry : ro::render::SkillUnitPrimerTable()) {
    CHECK(entry.unit_id != kLandMine);
    CHECK(entry.unit_id != kShockwaveTrap);
  }
  CHECK(std::ranges::is_sorted(ro::render::kClientAddedPrimers, {},
                               &ro::render::SkillUnitPrimerEntry::unit_id));
}

TEST_CASE("Amorce : une autre apparence ne la fait pas jouer", "[render][unitprimer]") {
  CHECK(ro::render::SkillUnitPrimerOnLook(kFreezingTrap, kLevel, kFreezingTrap) == 0);
}

TEST_CASE("Amorce : un tic qui la réécrit donne celle du déclenchement", "[render][unitprimer]") {
  // L'unité 268 sème elle-même l'amorce de sa pose au premier tic, puis en
  // retient une autre.
  constexpr std::uint32_t kSelfSowingUnit = 268;
  constexpr std::uint16_t kRewrittenPrimer = 1312;
  CHECK(ro::render::SkillUnitPrimer(kSelfSowingUnit, 1) == kRewrittenPrimer);
  CHECK(ro::render::SkillUnitPrimer(kSelfSowingUnit, kLevel) == kRewrittenPrimer);
}

TEST_CASE("Shockwave Trap : son éclair repart à sa fin, pour toute sa vie de table",
          "[render][unitprimer]") {
  // Il ne jouait son fichier qu'une fois, puis laissait le piège nu.
  CHECK(ro::render::PrimitiveStrLoopsForLife(kShockwaveEffect));
  REQUIRE(ro::render::EffectDurationIsKnown(kShockwaveEffect));
  CHECK_FALSE(ro::render::PrimitiveStrLoopsForLife(kShockwaveHitEffect));
  CHECK(std::ranges::is_sorted(ro::render::PrimitiveStrLoopTable()));
}
