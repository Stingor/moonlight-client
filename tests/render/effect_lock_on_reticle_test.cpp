// LE RÉTICULE DE VISÉE, EF_LOCKON (60) : deux carrés couchés au sol, à zéro et
// quarante-cinq degrés — une étoile à huit branches —, dont la teinte TOURNE.

#include <cstdint>
#include <span>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pattern_table.h"

using ro::render::AdvanceEffectNode;
using ro::render::EffectNode;
using ro::render::EffectNodeFromPattern;
using ro::render::EffectNodePattern;
using ro::render::EffectNodePatternsFor;

namespace {

constexpr std::uint32_t kLockOn = 60;
constexpr std::int32_t kRetenue = 20;
constexpr float kPeak = 254.0F;
constexpr float kStep = -15.0F;
constexpr std::size_t kVert = 1;
constexpr std::size_t kBlue = 2;
// De 150 à zéro par pas de quinze : dix tics.
constexpr std::int32_t kTicksToZero = 10;
// Une vie qui ne borne pas l'essai.
constexpr std::int32_t kLongLife = 1000;

void Advance(EffectNode& node, std::int32_t ticks) {
  for (std::int32_t tick = 0; tick < ticks; ++tick) {
    static_cast<void>(AdvanceEffectNode(node));
  }
}

}  // namespace

TEST_CASE("le reticule pose deux carres, a zero et quarante-cinq degres", "[render][table]") {
  const std::span<const EffectNodePattern> carres = EffectNodePatternsFor(kLockOn);
  REQUIRE(carres.size() == 2);
  CHECK(carres[0].launch_yaw_low == 0.0F);
  CHECK(carres[1].launch_yaw_low == 45.0F);
  for (const EffectNodePattern& quad : carres) {
    CHECK(quad.shape == ro::render::EffectNodeShape::kOrientedQuad);
    CHECK(quad.ring.color_hold_ticks == kRetenue);
    CHECK((quad.ring.flags & ro::render::kEffectNodeCyclesColor) != 0U);
  }
}

TEST_CASE("sous le bit 0x800, la teinte du reticule boucle et se retient au sommet",
          "[render][effect_node]") {
  EffectNode node = EffectNodeFromPattern(EffectNodePatternsFor(kLockOn).front());
  node.life_ticks = kLongLife;
  REQUIRE(node.cycles_color);

  // LE VERT ET LE BLEU DESCENDENT À ZÉRO : le carré vire au rouge.
  Advance(node, kTicksToZero);
  CHECK(node.color[kVert] == 0.0F);
  CHECK(node.color[kBlue] == 0.0F);

  // ⚠ SOUS ZÉRO ILS BOUCLENT À 254, là où un nœud ordinaire se borne à zéro.
  Advance(node, 1);
  CHECK(node.color[kVert] == kPeak);
  CHECK(node.color[kBlue] == kPeak);

  // ET ILS Y TIENNENT : les pas n'avancent qu'au vingtième tic.
  Advance(node, kRetenue - 1);
  CHECK(node.color[kVert] == kPeak);
  Advance(node, 1);
  CHECK(node.color[kVert] == kPeak + kStep);
  CHECK(node.color_hold_count == 0);

  // Hors du sommet, ils reprennent un pas par tic.
  Advance(node, 1);
  CHECK(node.color[kVert] == kPeak + 2.0F * kStep);
}

TEST_CASE("sans le bit, la teinte d'un quad garde son asymetrie", "[render][effect_node]") {
  EffectNodePattern pattern = EffectNodePatternsFor(kLockOn).front();
  pattern.ring.flags = 0U;
  EffectNode node = EffectNodeFromPattern(pattern);
  node.life_ticks = kLongLife;
  CHECK_FALSE(node.cycles_color);
  Advance(node, kTicksToZero + 1);
  // Le vert se BORNE à zéro au lieu de boucler.
  CHECK(node.color[kVert] == 0.0F);
}
