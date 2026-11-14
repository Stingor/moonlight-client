#include "ro/render/effect_update_quake.h"

#include <algorithm>
#include <cstdint>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/actor_effect_table.h"
#include "ro/render/effect_class.h"
#include "ro/render/effect_duration.h"
#include "ro/render/effect_frame.h"
#include "ro/render/skill_unit_table.h"
#include "ro/render/str_effect_setup.h"

using ro::render::EffectUpdateQuake;
using ro::render::EffectUpdateQuakeFor;
using ro::render::EffectUpdateQuakeTable;

namespace {

constexpr std::uint32_t kLordOfVermilionEffect = 90;
constexpr std::uint32_t kMeteorStormEffect = 92;
constexpr std::uint32_t kExplosionEffect = 183;
constexpr std::uint32_t kGravitationFieldEffect = 522;
// Le tic où le champ de Gravitation commence à s'effacer.
constexpr std::uint32_t kGravitationFadeTick = 240;
constexpr std::uint16_t kIceWallUnit = 141;
// La commande du tremblement, au mode de jeu.
constexpr std::uint16_t kScreenQuakeCommand = 0x7A;

std::vector<std::uint16_t> TicksOf(const EffectUpdateQuake& quake) {
  return {quake.ticks.begin(), quake.ticks.end()};
}

}  // namespace

TEST_CASE("Tremblements d'âge : Meteor Storm tremble au quarantième tic", "[effect_update_quake]") {
  const EffectUpdateQuake* meteor = EffectUpdateQuakeFor(kMeteorStormEffect);
  REQUIRE(meteor != nullptr);
  CHECK(TicksOf(*meteor) == std::vector<std::uint16_t>{40});
  CHECK(meteor->quake_ms == ro::render::kEffectQueuedQuakeMs);
}

TEST_CASE("Tremblements d'âge : Lord of Vermilion tremble cinq fois, de cinquante en cinquante",
          "[effect_update_quake]") {
  const EffectUpdateQuake* lord = EffectUpdateQuakeFor(kLordOfVermilionEffect);
  REQUIRE(lord != nullptr);
  CHECK(TicksOf(*lord) == std::vector<std::uint16_t>{50, 100, 150, 200, 250});
}

TEST_CASE("Tremblements d'âge : l'argument un donne la secousse courte", "[effect_update_quake]") {
  const EffectUpdateQuake* explosion = EffectUpdateQuakeFor(kExplosionEffect);
  REQUIRE(explosion != nullptr);
  CHECK(TicksOf(*explosion) == std::vector<std::uint16_t>{3, 7, 9});
  CHECK(explosion->quake_ms == ro::render::kEffectShortQuakeMs);
  CHECK(EffectUpdateQuakeFor(kGravitationFieldEffect) == nullptr);
}

TEST_CASE("Tremblements d'âge : aucun autre chemin ne fait déjà trembler ces effets",
          "[effect_update_quake]") {
  // Une classe d'effet a sa propre mise à jour ; un effet d'acteur qui envoie la
  // commande tremble déjà par la caméra. Un identifiant dans les deux
  // tremblerait deux fois.
  const std::span<const EffectUpdateQuake> table = EffectUpdateQuakeTable();
  REQUIRE_FALSE(table.empty());
  CHECK(std::ranges::is_sorted(table, {}, &EffectUpdateQuake::effect_id));
  for (const EffectUpdateQuake& quake : table) {
    INFO("effet " << quake.effect_id);
    CHECK_FALSE(ro::render::IsPortedEffectClass(quake.effect_id));
    const ro::render::ActorEffectChange* change = ro::render::ActorEffectChangeFor(quake.effect_id);
    CHECK((change == nullptr || change->command != kScreenQuakeCommand));
    const ro::render::StrEffectSetup* setup = ro::render::StrEffectSetupFor(quake.effect_id);
    CHECK((setup == nullptr || setup->quake_ticks.empty()));
  }
}

TEST_CASE("Gravitation : son champ se relance quand il commence à s'effacer",
          "[effect_update_quake]") {
  // ⚠ ÉCART VOULU au client d'origine, qui le laisse s'éteindre avant la zone.
  CHECK(ro::render::SkillUnitVisualLastsWithUnit(ro::render::kGravitationFieldUnit));
  CHECK_FALSE(ro::render::SkillUnitVisualLastsWithUnit(kIceWallUnit));
  CHECK(ro::render::SustainedUnitVisualPeriodMs(kGravitationFieldEffect) ==
        kGravitationFadeTick * static_cast<std::uint32_t>(ro::render::kEffectTickMs));
  const ro::render::SkillUnitVisualEntry* field =
      ro::render::SkillUnitVisual(ro::render::kGravitationFieldUnit);
  REQUIRE(field != nullptr);
  CHECK(field->effect_id == kGravitationFieldEffect);
  CHECK(ro::render::SkillUnitVisualPeriodMs(*field) ==
        ro::render::SustainedUnitVisualPeriodMs(kGravitationFieldEffect));
  // Les autres unités gardent la relance du client.
  const ro::render::SkillUnitVisualEntry* wall = ro::render::SkillUnitVisual(kIceWallUnit);
  REQUIRE(wall != nullptr);
  CHECK(ro::render::SkillUnitVisualPeriodMs(*wall) ==
        ro::render::SkillUnitRespawnMs(wall->effect_id, wall->withhold_ticks));
}
