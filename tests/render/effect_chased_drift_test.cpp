// LA DÉRIVE POURSUIVIE DES PÉTALES DE CERISIER (effet 618).

#include "ro/render/effect_chased_drift.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <set>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pattern_table.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_node_sprite_seed.h"

#include "support/effect_bearers.h"

using Catch::Approx;
using ro::render::AdvanceEffectChasedDrift;
using ro::render::EffectChasedDrift;
using ro::render::EffectChasedDriftPattern;
using ro::render::EffectChasedDriftPatternFor;
using ro::render::SowEffectChasedDrift;

namespace {

constexpr std::uint32_t kCherryPetals = 618;
// LA MÉTÉO QUI TOMBE : @sakura et @leaves.
constexpr std::uint32_t kSakuraWeather = 163;
constexpr std::uint32_t kLeavesWeather = 333;

}  // namespace

TEST_CASE("Pétales : ils naissent sur un CERCLE, pas dans un disque", "[render][chased_drift]") {
  const EffectChasedDriftPattern* pattern = EffectChasedDriftPatternFor(kCherryPetals);
  REQUIRE(pattern != nullptr);
  std::uint32_t generator = 5;
  for (int attempt = 0; attempt < 32; ++attempt) {
    EffectChasedDrift derive;
    const glm::vec3 offset = SowEffectChasedDrift(*pattern, derive, generator).birth_offset;
    CHECK(derive.active);
    CHECK(offset.y == 0.0F);
    CHECK(std::hypot(offset.x, offset.z) == Approx(pattern->birth_ring_radius));
  }
}

TEST_CASE("Pétales : la montée prend TROIS valeurs, et non une plage", "[render][chased_drift]") {
  const EffectChasedDriftPattern* pattern = EffectChasedDriftPatternFor(kCherryPetals);
  REQUIRE(pattern != nullptr);
  std::uint32_t generator = 9;
  std::set<int> montees;
  for (int attempt = 0; attempt < 64; ++attempt) {
    EffectChasedDrift derive;
    (void)SowEffectChasedDrift(*pattern, derive, generator);
    montees.insert(static_cast<int>(std::lround(derive.rise_per_tick * 100.0F)));
  }
  CHECK(montees == std::set<int>{30, 45, 60});
}

TEST_CASE("Pétales : chaque tic monte du pas tiré et dérive d'au plus quinze centièmes",
          "[render][chased_drift]") {
  const EffectChasedDriftPattern* pattern = EffectChasedDriftPatternFor(kCherryPetals);
  REQUIRE(pattern != nullptr);
  std::uint32_t generator = 3;
  EffectChasedDrift derive;
  (void)SowEffectChasedDrift(*pattern, derive, generator);
  for (int tick = 0; tick < 100; ++tick) {
    const glm::vec3 step = AdvanceEffectChasedDrift(derive, generator);
    CHECK(step.y == Approx(derive.rise_per_tick));
    CHECK(std::abs(step.x) <= pattern->drift_step + 1e-6F);
    CHECK(std::abs(step.z) <= pattern->drift_step + 1e-6F);
    // Les angles restent sur le tour, bornes comprises.
    for (std::size_t case_ = 0; case_ < 4; ++case_) {
      CHECK(derive.angles.value[case_] >= 0.0F);
      CHECK(derive.angles.value[case_] <= 359.0F);
    }
  }
}

TEST_CASE("Pétales : un nœud sans dérive ne bouge pas et ne tire rien", "[render][chased_drift]") {
  EffectChasedDrift faded;
  std::uint32_t generator = 42;
  const glm::vec3 step = AdvanceEffectChasedDrift(faded, generator);
  CHECK(step == glm::vec3{0.0F, 0.0F, 0.0F});
  CHECK(generator == 42U);
}

TEST_CASE("Pétales : la graine du 618 porte sa dérive, et seulement la sienne",
          "[render][chased_drift]") {
  const auto petales = ro::render::EffectNodePatternsFor(kCherryPetals);
  REQUIRE(petales.size() == 1);
  const ro::render::EffectNodeSeed seed =
      ro::render::EffectNodeSeedFromPattern(petales.front(), 300, 0, 1);
  CHECK(seed.chased_drift == EffectChasedDriftPatternFor(kCherryPetals));
  // Un autre sprite n'en porte aucune.
  const auto torche = ro::render::EffectNodePatternsFor(47);
  REQUIRE(!torche.empty());
  CHECK(ro::render::EffectNodeSeedFromPattern(torche.front(), 250, 0, 1).chased_drift == nullptr);
}

TEST_CASE("Pétales : les tirages suivent l'ordre du bâtisseur, entiers", "[render][chased_drift]") {
  // Montée, échelle, cercle, quatre angles, action : sept restes entiers, pris
  // dans cet ordre au même générateur.
  const EffectChasedDriftPattern* pattern = EffectChasedDriftPatternFor(kCherryPetals);
  REQUIRE(pattern != nullptr);
  constexpr std::uint32_t kSeed = 77;
  std::uint32_t witness = kSeed;
  const auto remainder = [&witness](std::uint32_t etendue) {
    return static_cast<std::int32_t>(ro::render::NextEffectNodeDraw(witness) % etendue);
  };
  const std::int32_t rise = remainder(3);
  const std::int32_t scale = remainder(6);
  static_cast<void>(remainder(360));
  std::array<std::int32_t, 4> angles{};
  for (std::int32_t& angle : angles) {
    angle = remainder(360);
  }
  const std::int32_t action = remainder(3);

  std::uint32_t generator = kSeed;
  EffectChasedDrift derive;
  const ro::render::EffectChasedDriftSowing sowing =
      SowEffectChasedDrift(*pattern, derive, generator);
  CHECK(derive.rise_per_tick == Approx(static_cast<float>(rise + 2) * 0.15F));
  CHECK(sowing.sprite_scale == Approx(static_cast<float>(scale + 22) / 70.0F));
  for (std::size_t case_ = 0; case_ < angles.size(); ++case_) {
    CHECK(derive.angles.value[case_] == static_cast<float>(angles[case_]));
  }
  CHECK(sowing.action == action);
  CHECK(generator == witness);
}

TEST_CASE("Pétales : six échelles et trois actions, pas une plage", "[render][chased_drift]") {
  const EffectChasedDriftPattern* pattern = EffectChasedDriftPatternFor(kCherryPetals);
  REQUIRE(pattern != nullptr);
  std::uint32_t generator = 11;
  std::set<int> echelles;
  std::set<int> actions;
  for (int attempt = 0; attempt < 256; ++attempt) {
    EffectChasedDrift derive;
    const ro::render::EffectChasedDriftSowing sowing =
        SowEffectChasedDrift(*pattern, derive, generator);
    echelles.insert(static_cast<int>(std::lround(sowing.sprite_scale * 70.0F)));
    actions.insert(sowing.action);
  }
  CHECK(echelles == std::set<int>{22, 23, 24, 25, 26, 27});
  CHECK(actions == std::set<int>{0, 1, 2});
}

TEST_CASE("Pétales : l'alpha tombe à zéro dès qu'il passe sous trois", "[render][chased_drift]") {
  ro::render::EffectNode node;
  node.life_ticks = 300;
  node.alpha = 5.0F;
  node.alpha_max = 254.0F;
  node.alpha_step_per_tick = -3.0F;
  node.alpha_zero_below = 3.0F;
  // Pas de sortie en fondu, qui recalculerait le pas.
  node.fade_out_tick = -1;
  REQUIRE(ro::render::AdvanceEffectNode(node));
  // Cinq moins trois font deux, sous le seuil : éteint, sans un tic à deux.
  CHECK(node.alpha == 0.0F);
}

TEST_CASE("Pétales : le patron sème vingt fois, s'éteint de trois par tic, vit trois cents tics",
          "[render][chased_drift]") {
  const auto patterns = ro::render::EffectNodePatternsFor(kCherryPetals);
  REQUIRE(patterns.size() == 1);
  const ro::render::EffectNodePattern& p = patterns.front();
  CHECK(p.first_tick == 0);
  CHECK(p.last_tick == 19);
  CHECK(p.period_ticks == 1);
  CHECK(p.alpha_start == 250.0F);
  CHECK(p.alpha_step == -3.0F);
  CHECK(p.life_from_effect);
  CHECK(p.life_ticks == 300);
  // L'action et l'échelle se tirent avec la dérive, pas dans le patron.
  CHECK(p.action == 0);
  CHECK(p.action_high == 0);
  CHECK(p.sprite_scale_high == 0.0F);
}

TEST_CASE("Météo : pétales et feuilles naissent dans le carré, deux cents au-dessus",
          "[render][chased_drift]") {
  // 🔴 SEMÉS AUX PIEDS, ILS COLLAIENT AU PERSONNAGE au lieu de pleuvoir sur
  // l'écran.
  for (const std::uint32_t effect : {kSakuraWeather, kLeavesWeather}) {
    const EffectChasedDriftPattern* pattern = EffectChasedDriftPatternFor(effect);
    REQUIRE(pattern != nullptr);
    std::uint32_t generator = 11;
    for (int attempt = 0; attempt < 64; ++attempt) {
      EffectChasedDrift derive;
      const glm::vec3 offset = SowEffectChasedDrift(*pattern, derive, generator).birth_offset;
      CHECK(offset.y == 200.0F);
      CHECK(std::abs(offset.x) <= 150.0F);
      CHECK(std::abs(offset.z) <= 150.0F);
      // ET ILS TOMBENT.
      CHECK(derive.rise_per_tick < 0.0F);
    }
  }
}

TEST_CASE("Météo : passé sous les pieds du porteur, un pétale renaît au-dessus de lui",
          "[render][chased_drift]") {
  const EffectChasedDriftPattern* pattern = EffectChasedDriftPatternFor(kSakuraWeather);
  REQUIRE(pattern != nullptr);
  std::uint32_t generator = 3;
  EffectChasedDrift derive;
  static_cast<void>(SowEffectChasedDrift(*pattern, derive, generator));
  const glm::vec3 owner{1000.0F, 50.0F, -400.0F};
  glm::vec3 place{1010.0F, 60.0F, -390.0F};
  CHECK_FALSE(ro::render::RebirthEffectChasedDrift(derive, place, owner, generator));
  place.y = 49.0F;
  REQUIRE(ro::render::RebirthEffectChasedDrift(derive, place, owner, generator));
  CHECK(place.y == owner.y + 200.0F);
  CHECK(std::abs(place.x - owner.x) <= 150.0F);
  CHECK(std::abs(place.z - owner.z) <= 150.0F);
  // Un pétale du 618, qui ne renaît pas, passe sous le porteur sans rien tirer.
  EffectChasedDrift rising;
  static_cast<void>(
      SowEffectChasedDrift(*EffectChasedDriftPatternFor(kCherryPetals), rising, generator));
  glm::vec3 below{0.0F, -10.0F, 0.0F};
  CHECK_FALSE(ro::render::RebirthEffectChasedDrift(rising, below, owner, generator));
}

TEST_CASE("Météo : cent cinquante pétales et cent feuilles, un tous les deux tics",
          "[render][chased_drift]") {
  const auto sakura = ro::render::EffectNodePatternsFor(kSakuraWeather);
  REQUIRE(sakura.size() == 1);
  CHECK(sakura.front().period_ticks == 2);
  CHECK(sakura.front().last_tick == 298);
  const auto leaves = ro::render::EffectNodePatternsFor(kLeavesWeather);
  REQUIRE(leaves.size() == 1);
  CHECK(leaves.front().period_ticks == 2);
  CHECK(leaves.front().last_tick == 198);
}

TEST_CASE("Météo : la pluie ne suit pas son porteur pas à pas", "[render][chased_drift]") {
  // 🔴 SA PLACE EST ABSOLUE : l'emmener avec le personnage collerait la pluie à
  // lui. Seule sa renaissance relit le porteur.
  ro::render::EffectNodePool pool;
  ro::render::PooledEffectNode falling;
  falling.ground_position = {0.0F, 200.0F, 0.0F};
  std::uint32_t generator = 1;
  static_cast<void>(SowEffectChasedDrift(*EffectChasedDriftPatternFor(kLeavesWeather),
                                         falling.chased_drift, generator));
  pool.AnchorNextTo(7, glm::vec3{0.0F, 0.0F, 0.0F});
  pool.Add(falling);
  ro::test_support::MoveBearer(pool, 7, glm::vec3{30.0F, 0.0F, 0.0F});
  REQUIRE(pool.nodes().size() == 1);
  CHECK(pool.nodes().front().ground_position.x == 0.0F);
  CHECK(pool.nodes().front().owner_position.x == 30.0F);
}
