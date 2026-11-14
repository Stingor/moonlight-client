#include "ro/render/map_effect_maker.h"

#include <array>
#include <cstdint>
#include <set>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_arc_ribbon_seed.h"
#include "ro/render/effect_track.h"
#include "ro/render/map_effects.h"

using ro::render::MapEffectMaker;
using ro::render::MapEffects;
using ro::render::MapEffectTable;

namespace {

constexpr glm::vec3 kPlace{0.0F, 0.0F, 0.0F};
constexpr glm::vec3 kNear{0.0F, 0.0F, 0.0F};
// Hors de portée EN PLAN ; l'ordonnée ne compte pas.
constexpr glm::vec3 kFar{1000.0F, 0.0F, 0.0F};
// Un semeur ordinaire, assez lent pour que le retard de départ ne le fasse pas
// partir au premier pas.
constexpr float kSlowSpeed = 100.0F;
// Un objet qui ne déclare aucun paramètre : l'effet semé prend alors les
// constantes de son style.
constexpr std::array<float, 4> kNoParam{};

// Les pas où le semeur lance, sur une fenêtre donnée.
std::vector<int> SowingSteps(MapEffectMaker& maker, int steps, glm::vec3 player = kNear) {
  std::vector<int> sown;
  for (int step = 0; step < steps; ++step) {
    if (maker.Tick(player)) {
      sown.push_back(step);
    }
  }
  return sown;
}

}  // namespace

TEST_CASE("un semeur ordinaire lance à la cadence de la vitesse de son objet") {
  std::uint32_t draws = 1;
  MapEffectMaker maker(kPlace, 47, kSlowSpeed, kNoParam, draws);
  CHECK(maker.period_steps() == 100);
  // Le départ est tiré dans les vingt-quatre derniers pas : le premier lancer
  // tombe donc entre le pas zéro et le vingt-troisième.
  const std::vector<int> sown = SowingSteps(maker, 400);
  REQUIRE(sown.size() >= 3);
  CHECK(sown.front() < 24);
  // Puis EXACTEMENT une période entre deux lancers : le compteur repart de zéro
  // au lancer puis s'incrémente dans le même pas, si bien qu'il vaut le nombre
  // de pas écoulés quand la garde le relit.
  for (std::size_t rank = 1; rank < sown.size(); ++rank) {
    CHECK(sown[rank] - sown[rank - 1] == 100);
  }
}

TEST_CASE("une vitesse plus courte que le retard de départ lance au premier pas") {
  // 🔴 `vitesse - rand() % 24` passe sous zéro, et la conversion du client rend
  // alors un très grand compte, non pas zéro : le semeur part aussitôt.
  std::uint32_t draws = 1;
  MapEffectMaker maker(kPlace, 47, 1.0F, kNoParam, draws);
  CHECK(maker.counter_steps() > maker.period_steps());
  CHECK(maker.Tick(kNear));
}

TEST_CASE("les lucioles ont leur propre cadence, plus courte d'un vingtième") {
  std::uint32_t draws = 1;
  std::set<std::uint32_t> periods;
  std::set<std::uint32_t> starts;
  for (int attempt = 0; attempt < 40; ++attempt) {
    MapEffectMaker maker(kPlace, ro::render::kMapFireflyEffect, 200.0F, kNoParam, draws);
    // 200 - 200/20 = 190, plus un bruit tiré sous 200/10 = 20.
    CHECK(maker.period_steps() >= 190);
    CHECK(maker.period_steps() < 210);
    // Le départ est tiré dans TOUTE la période, pas dans ses derniers pas.
    CHECK(maker.counter_steps() <= 200);
    periods.insert(maker.period_steps());
    starts.insert(maker.counter_steps());
  }
  // Deux semeurs voisins ne battent pas ensemble : les deux tirages varient.
  CHECK(periods.size() >= 2);
  CHECK(starts.size() >= 2);
}

TEST_CASE("loin du joueur le compte avance, et le retour lance aussitôt") {
  std::uint32_t draws = 1;
  MapEffectMaker maker(kPlace, 47, kSlowSpeed, kNoParam, draws);
  // Rien ne part au loin, même très longtemps.
  CHECK(SowingSteps(maker, 500, kFar).empty());
  // Mais le compte a couru : de retour, le semeur part au premier pas.
  CHECK(maker.Tick(kNear));
}

TEST_CASE("sans borne, un semeur loin du joueur lance à sa cadence") {
  std::uint32_t draws = 1;
  MapEffectMaker maker(kPlace, 47, kSlowSpeed, kNoParam, draws);
  std::vector<int> sown;
  for (int step = 0; step < 400; ++step) {
    if (maker.Tick(kFar, ro::render::kUnboundedMapEffectReach)) {
      sown.push_back(step);
    }
  }
  REQUIRE(sown.size() >= 3);
  CHECK(sown[1] - sown[0] == 100);
}

TEST_CASE("la portée réglée sur les effets de carte vaut pour leurs semeurs") {
  MapEffectTable table;
  ro::formats::RswEffect object;
  object.number = 47;
  object.emit_speed = 1.0F;
  MapEffects effects;
  effects.Raise(table, {&object, 1}, 200, 200);
  // Le joueur à mille unités du semeur, en plan.
  const glm::vec3 far = ro::render::MapEffectFrame{200, 200}.ToWorld(kFar);
  effects.Advance(1000, far);
  effects.Advance(1016, far);
  CHECK(effects.sown().empty());

  effects.SetReachUnits(ro::render::kUnboundedMapEffectReach);
  effects.Advance(1032, far);
  CHECK(effects.sown().size() == 1);
}

TEST_CASE("la portée d'un semeur est PLANE : la hauteur ne compte pas") {
  std::uint32_t draws = 1;
  MapEffectMaker maker(kPlace, 47, kSlowSpeed, kNoParam, draws);
  SowingSteps(maker, 300, kFar);
  // Cent vingt-neuf unités en plan, mais mille en hauteur : à portée.
  CHECK(maker.Tick({129.0F, 1000.0F, 0.0F}));
}

TEST_CASE("tout objet d'effet qui n'est pas une classe suivie devient un semeur") {
  MapEffectTable table;
  std::vector<ro::formats::RswEffect> objects;
  const std::vector<std::int32_t> numbers = {
      47,                                     // un semeur
      ro::render::kMapEmitterEffect,          // une classe qui se dessine
      ro::render::kMapChimneySmokeEffect,     // une classe qui se dessine
      ro::render::kMapAnimatedEmitterEffect,  // une classe suivie, non portée
      ro::render::kMapMagicFloorEffect,       // idem
      45,                                     // un semeur
      99999};                                 // un numéro qu'aucune table ne connaît
  for (const std::int32_t number : numbers) {
    ro::formats::RswEffect object;
    object.number = number;
    object.emit_speed = kSlowSpeed;
    objects.push_back(object);
  }
  MapEffects effects;
  effects.Raise(table, objects, 200, 200);
  // Les deux familles suivies mais non portées ne sèment RIEN : le client les
  // confie à leur classe, pas à un semeur.
  REQUIRE(effects.makers().size() == 3);
  CHECK(effects.makers()[0].effect_id() == 47);
  CHECK(effects.makers()[1].effect_id() == 45);
  CHECK(effects.makers()[2].effect_id() == 99999);
}

TEST_CASE("le semis porte l'échelle d'instance, le premier paramètre de l'objet") {
  MapEffectTable table;
  ro::formats::RswEffect object;
  // Un halo de minerai de manuk : effet 693, rayon huit.
  object.number = 693;
  object.emit_speed = 1.0F;
  object.param = {8.0F, 135.0F, 0.0F, 0.0F};
  MapEffects effects;
  effects.Raise(table, {&object, 1}, 200, 200);
  REQUIRE(effects.makers().size() == 1);
  CHECK(effects.makers()[0].param()[0] == 8.0F);

  const glm::vec3 player = ro::render::MapEffectFrame{200, 200}.ToWorld({0.0F, 0.0F, 0.0F});
  effects.Advance(1000, player);
  effects.Advance(1016, player);
  REQUIRE(effects.sown().size() == 1);
  CHECK(effects.sown()[0].instance_scale == 8.0F);
  // Le second paramètre voyage avec le premier : l'eau de 838 y lit son cap.
  CHECK(effects.sown()[0].instance_second == 135.0F);
}

TEST_CASE("un style d'arc prend son rayon sur l'échelle, et le sien sans elle") {
  const ro::render::EffectArcRibbonStyleSeed* style = ro::render::EffectArcRibbonStyleSeedFor(14);
  REQUIRE(style != nullptr);
  // Le style 14 — celui des halos de minerai — porte un rayon constant de
  // trente et un facteur d'un : le client ne prend le trente que faute
  // d'échelle.
  REQUIRE(style->radius_at_anchor == 30.0F);
  REQUIRE(style->radius_per_instance_scale == 1.0F);

  std::uint32_t generator = 1;
  // La piste VIVE est la première ; le bâtisseur éteint les trois autres.
  const std::array<ro::render::EffectTrack, ro::render::kEffectTracksPerNode> sans =
      ro::render::SeedEffectArcRibbonTracks(*style, {0.0F, 0.0F, 0.0F}, generator, 0.0F);
  CHECK(sans[0].radius_at_anchor == 30.0F);

  generator = 1;
  const std::array<ro::render::EffectTrack, ro::render::kEffectTracksPerNode> with =
      ro::render::SeedEffectArcRibbonTracks(*style, {0.0F, 0.0F, 0.0F}, generator, 8.0F);
  CHECK(with[0].radius_at_anchor == 8.0F);

  // Un style dont le facteur est nul ne regarde jamais l'échelle : le 0 garde
  // ses cinq unités quoi qu'on lui passe.
  const ro::render::EffectArcRibbonStyleSeed* fixed = ro::render::EffectArcRibbonStyleSeedFor(0);
  REQUIRE(fixed != nullptr);
  REQUIRE(fixed->radius_per_instance_scale == 0.0F);
  generator = 1;
  const std::array<ro::render::EffectTrack, ro::render::kEffectTracksPerNode> immuable =
      ro::render::SeedEffectArcRibbonTracks(*fixed, {0.0F, 0.0F, 0.0F}, generator, 8.0F);
  CHECK(immuable[0].radius_at_anchor == fixed->radius_at_anchor);
}

TEST_CASE("les semis d'un pas sortent par la liste, dans notre repère") {
  MapEffectTable table;
  ro::formats::RswEffect object;
  object.number = 47;
  object.emit_speed = 1.0F;
  object.position = {5.0F, -20.0F, 7.0F};
  MapEffects effects;
  effects.Raise(table, {&object, 1}, 200, 200);

  const glm::vec3 player = ro::render::MapEffectFrame{200, 200}.ToWorld({5.0F, 0.0F, 7.0F});
  effects.Advance(1000, player);
  CHECK(effects.sown().empty());
  effects.Advance(1016, player);
  REQUIRE(effects.sown().size() == 1);
  CHECK(effects.sown()[0].effect_id == 47);
  CHECK(effects.sown()[0].world_position ==
        ro::render::MapEffectFrame{200, 200}.ToWorld({5.0F, -20.0F, 7.0F}));
  // La liste se vide à chaque pas : elle ne dit que ce que CE pas a semé.
  effects.Advance(1017, player);
  CHECK(effects.sown().empty());
}
