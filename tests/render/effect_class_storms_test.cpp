// LES DEUX TEMPÊTES que l'emploi d'une compétence sème par numéro : Gale Storm
// sur la cible, Jupitel Thunder Storm du lanceur vers la cible.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_class.h"

using Catch::Approx;

namespace {

constexpr std::uint32_t kGaleStorm = 1973;
constexpr std::uint32_t kJupitelThunderStorm = 2366;
constexpr std::int32_t kLongStr = 200;
constexpr std::int32_t kSingleImageStr = 0;
constexpr float kMargin = 1e-3F;
constexpr std::string_view kGaleStormPath = "galestorm\\galestorm\\galestorm.str";
constexpr std::string_view kGaleStormArrowPath = "galestorm\\galestorm_arrow\\galestorm_arrow.str";
constexpr std::string_view kJupitelBallPath =
    "HYPER_NOVICE\\HN_JUPITEL_THUNDER_STORM\\jupitel_thunder_storm_ball\\"
    "jupitel_thunder_storm_ball.str";
constexpr std::string_view kJupitelStrikePrefix =
    "HYPER_NOVICE\\HN_JUPITEL_THUNDER_STORM\\jupitel_thunder_storm\\jupitel_thunder_storm_";
constexpr std::string_view kJupitelSound = "effect\\HYPER_NOVICE\\hu_jupitel_thunder_storm.wav";

// Un tic dont le `.str` a `last_frame` images, le porteur à `owner`.
ro::render::EffectClassTick TickWith(const ro::render::EffectClassView& view, glm::vec3 owner,
                                     std::int32_t last_frame) {
  ro::render::EffectClassTick tick;
  tick.view = &view;
  tick.owner_position = owner;
  tick.str_last_frame = [last_frame](std::string_view) {
    return std::optional<std::int32_t>(last_frame);
  };
  return tick;
}

std::size_t Count(const ro::render::EffectClassOutput& drawn, std::string_view path) {
  return static_cast<std::size_t>(std::ranges::count_if(
      drawn.strs, [path](const ro::render::EffectClassStrDraw& str) { return str.path == path; }));
}

std::unique_ptr<ro::render::EffectClassInstance> GaleStormAt(
    std::int32_t level, glm::vec3 place, const ro::render::EffectClassView& view) {
  ro::render::EffectClassBirth birth;
  birth.effect_id = kGaleStorm;
  birth.position = place;
  birth.level = level;
  return ro::render::SpawnEffectClass(birth, view);
}

}  // namespace

TEST_CASE(
    "Gale Storm : des rafales tous les 100/n tics et des flèches tous les 30/n, dans le "
    "carré de 5n autour de la cible",
    "[render][effect_class]") {
  CHECK(ro::render::IsPortedEffectClass(kGaleStorm));
  CHECK(ro::render::EffectClassReceivesSkillLevel(kGaleStorm));
  const ro::render::EffectClassView view;
  constexpr glm::vec3 kTarget{40.0F, -2.0F, 60.0F};
  constexpr float kSpreadHalfAtFive = 12.5F;
  ro::render::EffectClassTick tick = TickWith(view, kTarget, kLongStr);
  ro::render::EffectClassOutput events;
  // Au niveau un, n vaut cinq : une rafale et une flèche dès le premier tic,
  // chacune dans le carré de vingt-cinq unités de côté, à la hauteur du sol.
  auto storm = GaleStormAt(1, kTarget, view);
  REQUIRE(storm != nullptr);
  storm->Advance(tick, events);
  ro::render::EffectClassOutput drawn;
  storm->Draw(view, drawn);
  REQUIRE(drawn.strs.size() == 2);
  CHECK(Count(drawn, kGaleStormPath) == 1);
  CHECK(Count(drawn, kGaleStormArrowPath) == 1);
  for (const ro::render::EffectClassStrDraw& str : drawn.strs) {
    CHECK_FALSE(str.behind_actors);
    CHECK(std::abs(str.ground_position.x - kTarget.x) <= kSpreadHalfAtFive + kMargin);
    CHECK(std::abs(str.ground_position.z + kTarget.z) <= kSpreadHalfAtFive + kMargin);
    CHECK(str.ground_position.y == Approx(-kTarget.y));
    CHECK(str.frame == 0);
  }
  // Rien jusqu'au sixième tic, où part la flèche suivante ; la rafale suivante
  // au vingtième. Au vingt et unième tic : deux rafales, quatre flèches.
  for (std::int32_t age = 1; age <= 20; ++age) {
    storm->Advance(tick, events);
  }
  drawn = {};
  storm->Draw(view, drawn);
  CHECK(Count(drawn, kGaleStormPath) == 2);
  CHECK(Count(drawn, kGaleStormArrowPath) == 4);
  for (const ro::render::EffectClassSound& sound : events.sounds) {
    CHECK(sound.file == "effect\\WH_GALESTORM.wav");
  }
  // Au dixième niveau, n vaut onze : une rafale tous les neuf tics, une flèche
  // tous les deux — et un niveau absent prend le même.
  for (const std::int32_t level : {10, -1}) {
    auto dense = GaleStormAt(level, kTarget, view);
    REQUIRE(dense != nullptr);
    for (std::int32_t age = 0; age <= 9; ++age) {
      dense->Advance(tick, events);
    }
    drawn = {};
    dense->Draw(view, drawn);
    CHECK(Count(drawn, kGaleStormPath) == 2);
    CHECK(Count(drawn, kGaleStormArrowPath) == 5);
  }
  // Au niveau zéro, rien ne naît.
  auto none = GaleStormAt(0, kTarget, view);
  REQUIRE(none != nullptr);
  none->Advance(tick, events);
  drawn = {};
  none->Draw(view, drawn);
  CHECK(drawn.strs.empty());
}

TEST_CASE(
    "Gale Storm : les hôtes finis s'en vont, et l'instance meurt après ses semis ou avec sa "
    "cible",
    "[render][effect_class]") {
  const ro::render::EffectClassView view;
  constexpr glm::vec3 kTarget{0.0F, 0.0F, 0.0F};
  constexpr std::int32_t kSowUntilAge = 60;
  ro::render::EffectClassTick tick = TickWith(view, kTarget, kSingleImageStr);
  ro::render::EffectClassOutput events;
  auto storm = GaleStormAt(3, kTarget, view);
  REQUIRE(storm != nullptr);
  // Une image par hôte : montrée un tic, l'hôte est retiré au suivant.
  storm->Advance(tick, events);
  ro::render::EffectClassOutput drawn;
  storm->Draw(view, drawn);
  CHECK(drawn.strs.size() == 2);
  storm->Advance(tick, events);
  storm->Advance(tick, events);
  drawn = {};
  storm->Draw(view, drawn);
  CHECK(drawn.strs.empty());
  CHECK(storm->Alive(tick));
  // Passé le soixantième tic, plus rien ne naît : l'instance meurt quand le
  // dernier hôte a fini.
  for (std::int32_t age = 3; age <= kSowUntilAge + 3; ++age) {
    storm->Advance(tick, events);
  }
  CHECK_FALSE(storm->Alive(tick));
  // Et la cible partie l'emporte, quel que soit son âge.
  auto orphan = GaleStormAt(3, kTarget, view);
  REQUIRE(orphan != nullptr);
  orphan->Advance(tick, events);
  tick.owner_present = false;
  CHECK_FALSE(orphan->Alive(tick));
}

TEST_CASE(
    "Jupitel Thunder Storm : la boule file en dix tics vers la place de la cible, les "
    "éclairs tombent autour toutes les quatre-vingts millisecondes, quinze unités plus haut",
    "[render][effect_class]") {
  CHECK(ro::render::IsPortedEffectClass(kJupitelThunderStorm));
  CHECK(ro::render::EffectClassReceivesSkillLevel(kJupitelThunderStorm));
  const ro::render::EffectClassView view;
  constexpr glm::vec3 kCaster{0.0F, 0.0F, 0.0F};
  constexpr glm::vec3 kTarget{100.0F, 0.0F, 0.0F};
  constexpr float kStepX = 10.0F;
  constexpr float kStrikeUp = 15.0F;
  constexpr float kSpreadAtLevelOne = 12.0F;
  constexpr std::int32_t kTravelTicks = 10;
  constexpr std::size_t kStrikesAtLevelOne = 11;
  constexpr std::int32_t kFirstStrikeShownAge = 5;
  constexpr std::int32_t kFirstSoundAge = 6;
  ro::render::EffectClassTick tick = TickWith(view, kCaster, kLongStr);
  tick.bound_position = kTarget;
  ro::render::EffectClassBirth birth;
  birth.effect_id = kJupitelThunderStorm;
  birth.position = kCaster;
  birth.bound_position = kTarget;
  birth.level = 1;
  auto storm = ro::render::SpawnEffectClass(birth, view);
  REQUIRE(storm != nullptr);
  ro::render::EffectClassOutput events;
  // Premier tic : la boule a fait un dixième du chemin ; aucun éclair encore.
  storm->Advance(tick, events);
  ro::render::EffectClassOutput drawn;
  storm->Draw(view, drawn);
  REQUIRE(drawn.strs.size() == 1);
  CHECK(drawn.strs.front().path == kJupitelBallPath);
  CHECK(drawn.strs.front().ground_position.x == Approx(kStepX));
  CHECK(drawn.strs.front().spin_degrees == Approx(0.0F));
  // Le premier éclair attend quatre-vingts millisecondes, cinq tics : il se
  // montre au sixième, dans le quart de l'abscisse négative et de la
  // profondeur positive du client, à moins de douze unités par côté.
  for (std::int32_t age = 1; age <= kFirstStrikeShownAge; ++age) {
    storm->Advance(tick, events);
  }
  drawn = {};
  storm->Draw(view, drawn);
  REQUIRE(drawn.strs.size() == 2);
  const ro::render::EffectClassStrDraw& strike = drawn.strs.back();
  CHECK(strike.path.starts_with(kJupitelStrikePrefix));
  CHECK(strike.path.ends_with(".str"));
  CHECK(strike.ground_position.x < kTarget.x);
  CHECK(strike.ground_position.x >= kTarget.x - kSpreadAtLevelOne);
  CHECK(strike.ground_position.z < 0.0F);
  CHECK(strike.ground_position.z >= -kSpreadAtLevelOne);
  CHECK(strike.ground_position.y == Approx(kStrikeUp));
  CHECK(events.sounds.empty());
  // Le tonnerre gronde au sixième tic, avec le premier éclair.
  storm->Advance(tick, events);
  CHECK(storm->age() == kFirstSoundAge + 1);
  REQUIRE(events.sounds.size() == 1);
  CHECK(events.sounds.front().file == kJupitelSound);
  // Passé dix tics, la boule se tient sur la place de la cible — même si la
  // cible a bougé depuis.
  tick.bound_position = glm::vec3{200.0F, 0.0F, 0.0F};
  for (std::int32_t age = 7; age <= kTravelTicks + 1; ++age) {
    storm->Advance(tick, events);
  }
  drawn = {};
  storm->Draw(view, drawn);
  REQUIRE(!drawn.strs.empty());
  CHECK(drawn.strs.front().ground_position.x == Approx(kTarget.x));
  // Onze éclairs au niveau un : 150 × 6 millisecondes, un tous les quatre-vingts.
  for (std::int32_t age = 12; age <= 80; ++age) {
    storm->Advance(tick, events);
  }
  drawn = {};
  storm->Draw(view, drawn);
  CHECK(drawn.strs.size() == 1 + kStrikesAtLevelOne);
  // Quinze au niveau cinq, dix-huit au dixième.
  for (const auto& [level, strikes] : {std::pair{5, 15}, std::pair{10, 18}}) {
    birth.level = level;
    auto stronger = ro::render::SpawnEffectClass(birth, view);
    REQUIRE(stronger != nullptr);
    for (std::int32_t age = 0; age <= 100; ++age) {
      stronger->Advance(tick, events);
    }
    drawn = {};
    stronger->Draw(view, drawn);
    CHECK(drawn.strs.size() == 1 + static_cast<std::size_t>(strikes));
  }
}

TEST_CASE(
    "Jupitel Thunder Storm : la boule est tournée à l'écran vers la cible, et l'instance "
    "meurt quand tout a fini, le lanceur parti ou non",
    "[render][effect_class]") {
  ro::render::EffectClassView view;
  // Une projection à plat : l'abscisse à droite, la profondeur vers le bas.
  view.project = [](glm::vec3 point) {
    return std::optional<glm::vec2>(glm::vec2{point.x, point.z});
  };
  constexpr glm::vec3 kCaster{0.0F, 0.0F, 0.0F};
  constexpr glm::vec3 kTargetAhead{0.0F, 0.0F, 100.0F};
  constexpr float kQuarterTurnUp = 270.0F;
  ro::render::EffectClassBirth birth;
  birth.effect_id = kJupitelThunderStorm;
  birth.position = kCaster;
  birth.bound_position = kTargetAhead;
  birth.level = 1;
  auto storm = ro::render::SpawnEffectClass(birth, view);
  REQUIRE(storm != nullptr);
  ro::render::EffectClassTick tick = TickWith(view, kCaster, kSingleImageStr);
  tick.owner_present = false;
  ro::render::EffectClassOutput events;
  storm->Advance(tick, events);
  ro::render::EffectClassOutput drawn;
  storm->Draw(view, drawn);
  REQUIRE(drawn.strs.size() == 1);
  CHECK(drawn.strs.front().spin_degrees == Approx(kQuarterTurnUp));
  // Le lanceur parti ne l'emporte pas ; la fin de la boule et des éclairs, si.
  CHECK(storm->Alive(tick));
  for (std::int32_t age = 1; age <= 70; ++age) {
    storm->Advance(tick, events);
  }
  CHECK_FALSE(storm->Alive(tick));
}
