// QUELLE BARRE D'INCANTATION UN ACTEUR MONTRE, ET SOUS QUEL FILTRE.
//
// 🔴 UNE SEULE BARRE PAR ACTEUR : l'incantation et la barre d'une compétence
// annoncée sont la même jauge chez le client, que la dernière pose reprend. Et
// une incantation échue n'en montre plus — le serveur n'annonce pas qu'un sort
// aboutit.

#include "world/cast_bar_view.h"

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/cast_bar.h"
#include "ro/sim/actor.h"
#include "ro/sim/world.h"

using moonclient::OwnCastGauge;
using moonclient::ShownCast;
using moonclient::ShownCastOf;
using ro::sim::Actor;
using ro::sim::ActorKind;

namespace {

constexpr std::uint16_t kStormGustSkill = 89;

[[nodiscard]] Actor Caster(std::uint16_t skill_id, std::uint32_t started_ms,
                           std::uint32_t duration_ms) {
  Actor actor;
  actor.id = 2'000;
  actor.kind = ActorKind::kPlayer;
  ro::sim::Cast cast;
  cast.skill_id = skill_id;
  cast.started_ms = started_ms;
  cast.duration_ms = duration_ms;
  actor.cast = cast;
  return actor;
}

}  // namespace

TEST_CASE("Barre d'incantation : une incantation en cours se montre et se nomme",
          "[app][incantation]") {
  const Actor actor = Caster(kStormGustSkill, 1'000, 3'000);
  ShownCast shown;
  REQUIRE(ShownCastOf(actor, 2'000, &shown));
  CHECK(shown.started_ms == 1'000);
  CHECK(shown.duration_ms == 3'000);
  CHECK(shown.skill_id == kStormGustSkill);
}

TEST_CASE("Barre d'incantation : échue ou instantanée, elle ne se montre pas",
          "[app][incantation]") {
  ShownCast shown;
  CHECK_FALSE(ShownCastOf(Caster(kStormGustSkill, 1'000, 3'000), 4'000, &shown));
  CHECK_FALSE(ShownCastOf(Caster(kStormGustSkill, 1'000, 0), 1'000, &shown));
  // Avant son début : une horloge qui recule ne fait pas une barre pleine.
  CHECK_FALSE(ShownCastOf(Caster(kStormGustSkill, 1'000, 3'000), 500, &shown));
}

TEST_CASE("Barre d'incantation : Brandish Spear et Bowling Bash n'en ont pas",
          "[app][incantation]") {
  ShownCast shown;
  CHECK_FALSE(ShownCastOf(Caster(ro::render::kBrandishSpearSkill, 1'000, 700), 1'200, &shown));
  CHECK_FALSE(ShownCastOf(Caster(ro::render::kBowlingBashSkill, 1'000, 700), 1'200, &shown));
}

TEST_CASE("Barre d'incantation : la barre d'une annonce plus récente reprend la jauge",
          "[app][incantation]") {
  Actor actor = Caster(kStormGustSkill, 1'000, 3'000);
  actor.gauge = ro::sim::SkillGauge{.started_ms = 1'500, .duration_ms = 2'000};
  ShownCast shown;
  REQUIRE(ShownCastOf(actor, 2'000, &shown));
  CHECK(shown.started_ms == 1'500);
  CHECK(shown.duration_ms == 2'000);
  // ⚠ ELLE N'A PAS DE SORT À NOMMER : la nommer « Storm Gust » mentirait.
  CHECK(shown.skill_id == 0);

  // Plus ancienne que l'incantation, elle lui cède la place.
  actor.gauge = ro::sim::SkillGauge{.started_ms = 500, .duration_ms = 5'000};
  REQUIRE(ShownCastOf(actor, 2'000, &shown));
  CHECK(shown.skill_id == kStormGustSkill);
}

TEST_CASE("Barre d'incantation : ma jauge du HUD dit le temps restant", "[app][incantation]") {
  ro::sim::World world;
  world.set_player_id(2'000);
  CHECK_FALSE(OwnCastGauge(world, nullptr, 1'000).known);

  world.Spawn(Caster(kStormGustSkill, 1'000, 3'000));
  const ro::ui::StatBarValue gauge = OwnCastGauge(world, nullptr, 2'600);
  REQUIRE(gauge.known);
  CHECK(gauge.current == 1'600);
  CHECK(gauge.most == 3'000);
  // Sans table des compétences, le temps seul.
  CHECK(gauge.composed == ro::ui::CastSecondsText(1'400));
  // Échue, elle disparaît.
  CHECK_FALSE(OwnCastGauge(world, nullptr, 4'000).known);
}
