// LES TIRAGES DU CLIENT SONT DISCRETS : `rand() % n`, que des constantes
// décalent ou mettent à l'échelle, ne rend que `n` valeurs.

#include <cmath>
#include <cstdint>
#include <set>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_node_pattern_table.h"
#include "ro/render/effect_node_seed.h"

using Catch::Approx;
using ro::render::EffectDrawnRange;
using ro::render::EffectNodeRange;

namespace {

std::uint16_t ValuesOf(const ro::render::EffectNodePattern& pattern, EffectDrawnRange which) {
  return pattern.draw_values[static_cast<std::size_t>(which)];
}

}  // namespace

TEST_CASE("une plage qui porte son nombre de valeurs ne rend que celles-là", "[render][draws]") {
  // `(rand() % 60 + 30) / 10` : soixante valeurs, de dixième en dixième.
  const EffectNodeRange half_width{3.0F, 8.9F, 60};
  std::uint32_t generator = 7;
  std::set<int> seen;
  for (int attempt = 0; attempt < 6000; ++attempt) {
    const float tiree = ro::render::DrawEffectNodeRange(half_width, generator);
    const float dixiemes = tiree * 10.0F;
    CHECK(std::abs(dixiemes - std::round(dixiemes)) < 1e-3F);
    CHECK(tiree >= 3.0F - 1e-4F);
    CHECK(tiree <= 8.9F + 1e-4F);
    seen.insert(static_cast<int>(std::lround(dixiemes)));
  }
  CHECK(seen.size() == 60);
}

TEST_CASE("une borne haute nulle dit « pas de tirage », même sous une basse négative",
          "[render][draws]") {
  std::uint32_t generator = 3;
  const std::uint32_t before = generator;
  CHECK(ro::render::DrawEffectNodeRange(EffectNodeRange{-20.0F, 0.0F, 0}, generator) ==
        Approx(-20.0F));
  CHECK(generator == before);
}

TEST_CASE("un entier du client se tire bornes comprises, à chances égales", "[render][draws]") {
  // `rand() % 24 + 6` : la vie des étincelles d'impact.
  std::uint32_t generator = 11;
  std::set<std::int32_t> seen;
  for (int attempt = 0; attempt < 4000; ++attempt) {
    const std::int32_t life = ro::render::DrawEffectNodeInteger(6, 29, generator);
    CHECK(life >= 6);
    CHECK(life <= 29);
    seen.insert(life);
  }
  CHECK(seen.size() == 24);
}

TEST_CASE("les patrons portent le nombre de valeurs de leurs plages tirées", "[render][draws]") {
  // La gerbe d'impact : `face − 40 + rand() % 80` et `rand() % 4 + 4`.
  bool spray = false;
  for (const ro::render::EffectNodePattern& node_pattern : ro::render::EffectNodePatternsFor(0)) {
    if (node_pattern.launch_yaw_high > node_pattern.launch_yaw_low) {
      CHECK(ValuesOf(node_pattern, EffectDrawnRange::kLaunchYaw) == 80);
      CHECK(ValuesOf(node_pattern, EffectDrawnRange::kLaunchDistance) == 4);
      spray = true;
    }
  }
  CHECK(spray);
  // CoinShower : `30 · (rand() % 12)` — douze caps, de trente en trente.
  bool pluie = false;
  for (const ro::render::EffectNodePattern& node_pattern : ro::render::EffectNodePatternsFor(10)) {
    if (node_pattern.launch_yaw_high > node_pattern.launch_yaw_low) {
      CHECK(ValuesOf(node_pattern, EffectDrawnRange::kLaunchYaw) == 12);
      pluie = true;
    }
  }
  CHECK(pluie);
  // ET AUCUNE PLAGE TIRÉE N'EST SANS SON NOMBRE : le générateur refuse.
  for (const ro::render::EffectNodePattern& node_pattern : ro::render::EffectNodePatternTable()) {
    CAPTURE(node_pattern.effect_id);
    if (node_pattern.sprite_scale_high > node_pattern.sprite_scale &&
        node_pattern.sprite_scale_high != 0.0F && node_pattern.sprite_scale_steps == 0) {
      CHECK(ValuesOf(node_pattern, EffectDrawnRange::kSpriteScale) > 1);
    }
    if (node_pattern.step_high > node_pattern.step_low && node_pattern.step_high != 0.0F &&
        !node_pattern.aims_at_target) {
      CHECK(ValuesOf(node_pattern, EffectDrawnRange::kStep) > 1);
    }
  }
}

TEST_CASE("un cap tiré sur le tour entier se passe de la face du porteur", "[render][draws]") {
  // CoinShower : `30 · (rand() % 12)`, ABSOLU. La gerbe d'impact, elle, écrit
  // `face − 40 + rand() % 80`.
  for (const ro::render::EffectNodePattern& node_pattern : ro::render::EffectNodePatternsFor(10)) {
    if (node_pattern.launch_yaw_high > node_pattern.launch_yaw_low) {
      CHECK(node_pattern.launch_yaw_is_absolute);
    }
  }
  for (const ro::render::EffectNodePattern& node_pattern : ro::render::EffectNodePatternsFor(0)) {
    CHECK_FALSE(node_pattern.launch_yaw_is_absolute);
  }
}
