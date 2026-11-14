// LES SPHÈRES DES QUATRIÈMES MÉTIERS : les armes de servant (1694) et les sphères
// d'ombre (2109) naissent, partent une à une, et passent par leur boucle.

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_class.h"

using Catch::Approx;

namespace {

constexpr std::int32_t kStrLastFrame = 2;
// Assez de tics pour qu'un `.str` de trois images finisse.
constexpr std::int32_t kTicksToFinish = 4;
constexpr std::string_view kServantCast = "servantweapon_cast.str";
constexpr std::string_view kServantLoop = "servantweapon.str";
constexpr std::string_view kServantShoot = "servantweapon_shoot.str";
constexpr std::string_view kShadowCreate = "shadowball_create.str";
constexpr std::string_view kShadowLoop = "abyss_shadowball.str";
constexpr std::string_view kShadowDie = "shadowball_die.str";

struct Ticking {
  ro::render::EffectClassView view;
  ro::render::EffectClassTick tick;
  Ticking() {
    tick.view = &view;
    tick.owner_position = glm::vec3{10.0F, -3.0F, 20.0F};
    tick.str_last_frame = [](std::string_view) {
      return std::optional<std::int32_t>(kStrLastFrame);
    };
  }
};

std::unique_ptr<ro::render::EffectClassInstance> Spawn(std::uint32_t effect_id,
                                                       std::int32_t level) {
  ro::render::EffectClassBirth birth;
  birth.effect_id = effect_id;
  birth.position = {10.0F, -3.0F, 20.0F};
  birth.level = level;
  const ro::render::EffectClassView view;
  auto instance = ro::render::SpawnEffectClass(birth, view);
  REQUIRE(instance != nullptr);
  return instance;
}

ro::render::EffectClassOutput AdvanceAndDraw(ro::render::EffectClassInstance& instance,
                                             Ticking& ticking, std::int32_t ticks = 1) {
  ro::render::EffectClassOutput events;
  for (std::int32_t tick_index = 0; tick_index < ticks; ++tick_index) {
    instance.Advance(ticking.tick, events);
  }
  ro::render::EffectClassOutput drawn;
  instance.Draw(ticking.view, drawn);
  drawn.sounds = events.sounds;
  return drawn;
}

std::size_t Count(const ro::render::EffectClassOutput& output, std::string_view ending) {
  return static_cast<std::size_t>(std::ranges::count_if(
      output.strs,
      [ending](const ro::render::EffectClassStrDraw& str) { return str.path.ends_with(ending); }));
}

}  // namespace

TEST_CASE("Sphères : les deux classes sont portées et tiennent leur nombre de la valeur",
          "[render][effect_class]") {
  CHECK(ro::render::IsPortedEffectClass(ro::render::kServantWeaponsEffect));
  CHECK(ro::render::IsPortedEffectClass(ro::render::kShadowSpheresEffect));
  CHECK(ro::render::EffectClassCountsFromValue(ro::render::kServantWeaponsEffect));
  CHECK_FALSE(ro::render::EffectClassCountsFromValue(228));
}

TEST_CASE("Armes de servant : chacune apparaît, boucle, et la dernière venue part en tirant",
          "[render][effect_class]") {
  Ticking ticking;
  auto weapons = Spawn(ro::render::kServantWeaponsEffect, 3);
  CHECK(weapons->reused_on_same_owner());
  ro::render::EffectClassOutput drawn = AdvanceAndDraw(*weapons, ticking);
  CHECK(Count(drawn, kServantCast) == 3);
  // Chaque arme sonne à sa venue.
  CHECK(drawn.sounds.size() == 3);
  // La première à quatre-vingt-dix degrés : droit devant, à treize unités et dix
  // au-dessus du porteur.
  CHECK(drawn.strs[0].ground_position.x == Approx(10.0F).margin(1.0e-3F));
  CHECK(drawn.strs[0].ground_position.y == Approx(13.0F));
  CHECK(drawn.strs[0].ground_position.z == Approx(-33.0F));
  CHECK(drawn.strs[0].scale == Approx(0.12F));

  drawn = AdvanceAndDraw(*weapons, ticking, kTicksToFinish);
  CHECK(Count(drawn, kServantLoop) == 3);

  weapons->Retell(1);
  drawn = AdvanceAndDraw(*weapons, ticking);
  CHECK(Count(drawn, kServantLoop) == 1);
  CHECK(Count(drawn, kServantShoot) == 2);
  drawn = AdvanceAndDraw(*weapons, ticking, kTicksToFinish);
  CHECK(Count(drawn, kServantShoot) == 0);
  // ZÉRO NE TUE PAS L'INSTANCE : seul le départ du porteur l'éteint.
  weapons->Retell(0);
  CHECK(weapons->Alive(ticking.tick));
}

TEST_CASE("Sphères d'ombre : chaque place a sa taille, et la disparition se joue",
          "[render][effect_class]") {
  Ticking ticking;
  auto spheres = Spawn(ro::render::kShadowSpheresEffect, 2);
  ro::render::EffectClassOutput drawn = AdvanceAndDraw(*spheres, ticking);
  REQUIRE(Count(drawn, kShadowCreate) == 2);
  CHECK(drawn.strs[0].scale == Approx(0.8F));
  CHECK(drawn.strs[1].scale == Approx(0.6F));

  drawn = AdvanceAndDraw(*spheres, ticking, kTicksToFinish);
  CHECK(Count(drawn, kShadowLoop) == 2);

  // Au-delà de cinq, le nombre n'est pas tenu.
  spheres->Retell(6);
  drawn = AdvanceAndDraw(*spheres, ticking);
  CHECK(Count(drawn, kShadowLoop) == 2);

  spheres->Retell(1);
  drawn = AdvanceAndDraw(*spheres, ticking);
  CHECK(Count(drawn, kShadowLoop) == 1);
  CHECK(Count(drawn, kShadowDie) == 1);
  drawn = AdvanceAndDraw(*spheres, ticking, kTicksToFinish);
  CHECK(Count(drawn, kShadowDie) == 0);
  CHECK(drawn.strs.size() == 1);
}
