#include "ro/render/effect_spinning_round.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_spinning_round_seed.h"
#include "ro/render/effect_track.h"

using Catch::Approx;
using ro::render::AdvanceSpinningRoundTracks;
using ro::render::ComposeSpinningRoundQuads;
using ro::render::EffectSpinningRoundAlpha;
using ro::render::EffectSpinningRoundColor;
using ro::render::EffectSpinningRoundOffsetOf;
using ro::render::EffectSpinningRoundPattern;
using ro::render::EffectSpinningRoundPatternsFor;
using ro::render::EffectSpinningRoundPatternTable;
using ro::render::EffectTrack;
using ro::render::EffectWorldQuad;
using ro::render::kEffectTracksPerNode;
using ro::render::kSpinningRoundDrawsPerTrack;
using ro::render::kSpinningRoundGreen;
using ro::render::kSpinningRoundGreenAlpha;
using ro::render::kSpinningRoundGreenMark;
using ro::render::kSpinningRoundHeightPerByte;
using ro::render::kSpinningRoundJitter;
using ro::render::kSpinningRoundMarkOfColor;
using ro::render::kSpinningRoundOrbitRadius;
using ro::render::kSpinningRoundParticles;
using ro::render::kSpinningRoundSlotOfFirstSpin;
using ro::render::kSpinningRoundSquareRadius;
using ro::render::kSpinningRoundWhite;
using ro::render::kSpinningRoundWhiteAlpha;
using ro::render::SeedEffectSpinningRoundTracks;
using ro::render::SetEffectSpinningRoundOffset;
using ro::render::SowEffectSpinningRoundNode;

namespace {

constexpr std::array<std::uint32_t, 3> kRoundEffectIds = {651, 695, 2402};

[[nodiscard]] const EffectSpinningRoundPattern& PatternOf(std::uint32_t effect_id) {
  return EffectSpinningRoundPatternsFor(effect_id).front();
}

}  // namespace

TEST_CASE("la ronde rend quatre carres", "[render][spinning_round]") {
  std::uint32_t seed = 4U;
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectSpinningRoundTracks(PatternOf(651), seed);
  const std::vector<EffectWorldQuad> carres =
      ComposeSpinningRoundQuads(tracks[0], 0, 0, {0.0F, 0.0F, 0.0F});
  CHECK(carres.size() == kSpinningRoundParticles);
}

TEST_CASE("une piste eteinte ne rend rien", "[render][spinning_round]") {
  EffectTrack faded;
  CHECK(ComposeSpinningRoundQuads(faded, 0, 0, {0.0F, 0.0F, 0.0F}).empty());
}

TEST_CASE("une particule SOUS le noeud n'est pas dessinee", "[render][spinning_round]") {
  std::uint32_t seed = 4U;
  std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectSpinningRoundTracks(PatternOf(651), seed);
  // ⚠ LA HAUTEUR EST DANS LE REPÈRE DU CLIENT : un nombre POSITIF met la
  // particule sous le nœud, et le client ne la peint pas.
  SetEffectSpinningRoundOffset(tracks[0], 0, {0.0F, 50.0F, 0.0F});
  CHECK(ComposeSpinningRoundQuads(tracks[0], 0, 0, {0.0F, 0.0F, 0.0F}).size() ==
        kSpinningRoundParticles - 1);
}

TEST_CASE("la ronde flotte AU-DESSUS du noeud", "[render][spinning_round]") {
  std::uint32_t seed = 4U;
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectSpinningRoundTracks(PatternOf(651), seed);
  const std::vector<EffectWorldQuad> carres =
      ComposeSpinningRoundQuads(tracks[0], 0, 0, {0.0F, 0.0F, 0.0F});
  REQUIRE_FALSE(carres.empty());
  // ⚠ NOTRE REPÈRE MONTE : le client écrit moins deux fois l'octet, nous
  // montons d'autant.
  const float attendue =
      kSpinningRoundHeightPerByte * static_cast<float>(PatternOf(651).height_byte);
  CHECK(carres.front().corners[0].y == Approx(attendue).margin(0.01F));
}

TEST_CASE("la teinte et l'alpha suivent la marque", "[render][spinning_round]") {
  CHECK(EffectSpinningRoundColor(0) == kSpinningRoundWhite);
  CHECK(EffectSpinningRoundColor(kSpinningRoundGreenMark) == kSpinningRoundGreen);
  // À PLEIN PIED, la verte est plus opaque que la blanche.
  CHECK(EffectSpinningRoundAlpha(kSpinningRoundGreenMark, 0.0F) == kSpinningRoundGreenAlpha);
  CHECK(EffectSpinningRoundAlpha(0, 0.0F) == kSpinningRoundWhiteAlpha);
  // ⚠ ET EN HAUTEUR LA RAMPE REMPLACE LES DEUX : la marque ne compte plus.
  CHECK(EffectSpinningRoundAlpha(kSpinningRoundGreenMark, -5.0F) ==
        EffectSpinningRoundAlpha(0, -5.0F));
  CHECK(EffectSpinningRoundAlpha(0, -5.0F) == 114);
  // Et elle ne descend jamais sous zéro.
  CHECK(EffectSpinningRoundAlpha(0, -1000.0F) == 0);
}

TEST_CASE("les quatre particules tournent sur un cercle de vingt", "[render][spinning_round]") {
  std::uint32_t seed = 4U;
  std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectSpinningRoundTracks(PatternOf(651), seed);
  // ⚠ ELLES NAISSENT TOUTES SUR L'AXE : c'est le premier tic qui les écarte.
  for (std::size_t particle = 0; particle < kSpinningRoundParticles; ++particle) {
    const glm::vec3 start = EffectSpinningRoundOffsetOf(tracks[0], particle);
    CHECK(start.x == Approx(0.0F));
    CHECK(start.z == Approx(0.0F));
  }
  std::uint32_t rng = 9U;
  AdvanceSpinningRoundTracks(tracks, 0, 0, rng);
  for (std::size_t particle = 0; particle < kSpinningRoundParticles; ++particle) {
    const glm::vec3 place = EffectSpinningRoundOffsetOf(tracks[0], particle);
    const float radius = std::sqrt(place.x * place.x + place.z * place.z);
    // Le cercle de vingt, plus ou moins le tremblement de deux et demi sur
    // chaque axe.
    CHECK(radius > kSpinningRoundOrbitRadius - 2.0F * kSpinningRoundJitter);
    CHECK(radius < kSpinningRoundOrbitRadius + 2.0F * kSpinningRoundJitter);
  }
}

TEST_CASE("la hauteur n'est JAMAIS ecrite par le mouvement", "[render][spinning_round]") {
  std::uint32_t seed = 4U;
  std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectSpinningRoundTracks(PatternOf(651), seed);
  const float start = EffectSpinningRoundOffsetOf(tracks[0], 0).y;
  std::uint32_t rng = 9U;
  for (std::int32_t tick = 0; tick < 50; ++tick) {
    AdvanceSpinningRoundTracks(tracks, 0, 0, rng);
  }
  for (std::size_t particle = 0; particle < kSpinningRoundParticles; ++particle) {
    CHECK(EffectSpinningRoundOffsetOf(tracks[0], particle).y == Approx(start));
  }
}

TEST_CASE("l'angle de ronde DESCEND et rattrape sous zero", "[render][spinning_round]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  for (EffectTrack& track : tracks) {
    track.active = true;
  }
  tracks[0].value[kSpinningRoundSlotOfFirstSpin] = 0.0F;
  std::uint32_t rng = 9U;
  AdvanceSpinningRoundTracks(tracks, 0, 0, rng);
  CHECK(tracks[0].value[kSpinningRoundSlotOfFirstSpin] == Approx(359.0F));
}

TEST_CASE("les deux angles de camera se rangent dans les styles", "[render][spinning_round]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  std::uint32_t rng = 9U;
  // 🔴 TELS QU'ILS ARRIVENT : ils sortent déjà de `EffectBillboard*Degrees`, qui
  // fait le calcul du client. Voir `StoreEffectBillboardAngles`.
  AdvanceSpinningRoundTracks(tracks, 30, 100, rng);
  CHECK(tracks[0].style == 30);
  CHECK(tracks[1].style == 100);
  // ⚠ ET ILS SE POSENT MÊME QUAND LES PISTES SONT ÉTEINTES : le client les
  // écrit avant sa boucle.
  CHECK_FALSE(tracks[0].active);
}

TEST_CASE("le tremblement partage ses deux courants entre quatre quadrants",
          "[render][spinning_round]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  for (EffectTrack& track : tracks) {
    track.active = true;
  }
  // Deux courants à quatre-vingt-dix degrés : le tremblement vaut sa pleine
  // amplitude sur les deux axes.
  tracks[0].value[0] = 90.0F;
  tracks[0].value[1] = 90.0F;
  tracks[0].value[2] = 90.0F;
  tracks[0].value[3] = 90.0F;
  for (std::size_t particle = 0; particle < kSpinningRoundParticles; ++particle) {
    tracks[0].value[kSpinningRoundSlotOfFirstSpin + particle] = 1.0F;
  }
  std::uint32_t rng = 9U;
  AdvanceSpinningRoundTracks(tracks, 0, 0, rng);
  // ⚠ QUATRE SIGNES, UN PAR QUADRANT : les deux premières particules sont
  // opposées sur les deux axes, et les deux dernières sur un seul.
  const glm::vec3 a = EffectSpinningRoundOffsetOf(tracks[0], 0);
  const glm::vec3 b = EffectSpinningRoundOffsetOf(tracks[0], 1);
  const glm::vec3 c = EffectSpinningRoundOffsetOf(tracks[0], 2);
  const glm::vec3 d = EffectSpinningRoundOffsetOf(tracks[0], 3);
  CHECK(a.x > b.x);
  CHECK(a.z > b.z);
  CHECK(c.x == Approx(a.x));
  CHECK(c.z == Approx(b.z));
  CHECK(d.x == Approx(b.x));
  CHECK(d.z == Approx(a.z));
}

TEST_CASE("le semis tire HUIT fois par piste", "[render][spinning_round]") {
  std::uint32_t seed = 21U;
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectSpinningRoundTracks(PatternOf(695), seed);
  std::uint32_t witness = 21U;
  for (std::size_t draw = 0; draw < kSpinningRoundDrawsPerTrack * kEffectTracksPerNode; ++draw) {
    static_cast<void>(ro::render::NextEffectNodeDraw(witness));
  }
  CHECK(seed == witness);
  for (const EffectTrack& track : tracks) {
    CHECK(track.active);
    CHECK(track.radius_at_anchor == Approx(kSpinningRoundSquareRadius));
    CHECK(track.mark[0] == 1);
    CHECK(track.mark[kSpinningRoundMarkOfColor] == kSpinningRoundGreenMark);
    for (std::size_t rang = 0; rang < kSpinningRoundDrawsPerTrack; ++rang) {
      CHECK(track.value[rang] >= 0.0F);
      CHECK(track.value[rang] < 360.0F);
    }
  }
}

TEST_CASE("la table porte les trois identifiants du sous-type 27", "[render][spinning_round]") {
  const std::span<const EffectSpinningRoundPattern> table = EffectSpinningRoundPatternTable();
  REQUIRE(table.size() == kRoundEffectIds.size());
  for (std::size_t rang = 0; rang < kRoundEffectIds.size(); ++rang) {
    CHECK(table[rang].effect_id == kRoundEffectIds[rang]);
    CHECK(table[rang].height_byte == 2);
    CHECK_FALSE(table[rang].texture.empty());
  }
  CHECK_FALSE(PatternOf(651).green);
  CHECK(PatternOf(695).green);
  CHECK(PatternOf(2402).green);
  CHECK(EffectSpinningRoundPatternsFor(1U).empty());
}

TEST_CASE("le noeud seme porte sa forme et son image", "[render][spinning_round]") {
  std::uint32_t seed = 3U;
  const ro::render::PooledEffectNode node =
      SowEffectSpinningRoundNode(PatternOf(2402), {2.0F, 0.0F, 7.0F}, 600, seed);
  CHECK(node.shape == ro::render::PooledNodeShape::kSpinningRound);
  CHECK(node.node.life_ticks == 600);
  REQUIRE(node.resource_paths.size() == 1);
  CHECK(node.resource_paths.front() == std::string(PatternOf(2402).texture));
}
