#include "ro/render/effect_teleport_rings.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <string>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_teleport_rings_seed.h"
#include "ro/render/effect_track.h"
#include "ro/render/effect_track_figures.h"
#include "ro/render/effect_track_motion.h"

using Catch::Approx;
using ro::render::AdvanceTeleportRingTracks;
using ro::render::ComposeEffectTrackSkirt;
using ro::render::EffectTeleportRingPattern;
using ro::render::EffectTeleportRingPatternsFor;
using ro::render::EffectTeleportRingPatternTable;
using ro::render::EffectTeleportRingSeeds;
using ro::render::EffectTrack;
using ro::render::EffectWorldQuad;
using ro::render::kEffectTrackSlots;
using ro::render::kEffectTracksPerNode;
using ro::render::kTeleportRingMarkOfAmplitude;
using ro::render::kTeleportRingMarkOfSwitch;
using ro::render::SeedEffectTeleportRingTracks;
using ro::render::SowEffectTeleportRingNode;

namespace {

constexpr std::uint32_t kTeleportation2 = 304;

[[nodiscard]] const EffectTeleportRingPattern& PatternOf(std::uint32_t effect_id) {
  const std::span<const EffectTeleportRingPattern> patterns =
      EffectTeleportRingPatternsFor(effect_id);
  REQUIRE(patterns.size() == 1);
  return patterns.front();
}

// COMBIEN DE TICS il faut pour atteindre un double-tic donné.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> AfterTicks(std::int32_t count) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectTeleportRingTracks(PatternOf(kTeleportation2));
  for (std::int32_t tick = 0; tick < count; ++tick) {
    AdvanceTeleportRingTracks(tracks);
  }
  return tracks;
}

}  // namespace

TEST_CASE("la table porte l'identifiant du sous-type 57", "[render][teleport_rings]") {
  const std::span<const EffectTeleportRingPattern> table = EffectTeleportRingPatternTable();
  CHECK(table.size() == 1);
  for (std::size_t rang = 1; rang < table.size(); ++rang) {
    CHECK(table[rang - 1].effect_id < table[rang].effect_id);
  }
  CHECK(PatternOf(kTeleportation2).scale == 0);
  CHECK(EffectTeleportRingPatternsFor(1).empty());
}

TEST_CASE("le semis allume quatre anneaux qui ne se ressemblent pas", "[render][teleport_rings]") {
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectTeleportRingTracks(PatternOf(kTeleportation2));
  const std::span<const ro::render::EffectTeleportRingSeed> seeds = EffectTeleportRingSeeds();
  REQUIRE(seeds.size() == kEffectTracksPerNode);
  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    CHECK(tracks[rang].active);
    CHECK(tracks[rang].degrees == 360);
    CHECK(tracks[rang].ticks == 0);
    CHECK(tracks[rang].alpha == seeds[rang].alpha);
    CHECK(tracks[rang].azimuth_degrees == seeds[rang].azimuth_degrees);
    CHECK(tracks[rang].tilt_degrees == seeds[rang].tilt_degrees);
    // ⚠ LES VINGT ET UNE CASES NAISSENT À ZÉRO : un anneau sans longueur n'a pas
    // d'aire, et c'est le mouvement qui lui en donne une.
    CHECK(std::ranges::all_of(tracks[rang].value, [](float length) { return length == 0.0F; }));
    CHECK(tracks[rang].mark[kTeleportRingMarkOfSwitch] == 0);
  }
  // L'ALPHA DESCEND ET LE RAYON MONTE : le plus intérieur est le plus vif.
  CHECK(tracks[0].alpha > tracks[3].alpha);
  CHECK(tracks[0].radius_at_anchor < tracks[3].radius_at_anchor);
  // L'AMPLITUDE EST LA PARTIE ENTIÈRE DE LA CLÉ, rangée dans un octet.
  CHECK(static_cast<std::int8_t>(tracks[0].mark[kTeleportRingMarkOfAmplitude]) == 100);
}

TEST_CASE("chaque anneau tourne a sa vitesse", "[render][teleport_rings]") {
  const std::array<EffectTrack, kEffectTracksPerNode> start =
      SeedEffectTeleportRingTracks(PatternOf(kTeleportation2));
  const std::array<EffectTrack, kEffectTracksPerNode> after = AfterTicks(1);
  for (std::size_t rang = 0; rang < after.size(); ++rang) {
    const std::int32_t expected = 5 - static_cast<std::int32_t>(rang);
    const std::int32_t rotated =
        (after[rang].azimuth_degrees - start[rang].azimuth_degrees + 360) % 360;
    CHECK(rotated == expected);
  }
}

TEST_CASE("l'azimut reste dans un tour", "[render][teleport_rings]") {
  // La première piste part de 180 et tourne de cinq : elle repasse par zéro au
  // trente-septième tic.
  const std::array<EffectTrack, kEffectTracksPerNode> tracks = AfterTicks(200);
  for (const EffectTrack& track : tracks) {
    CHECK(track.azimuth_degrees >= 0);
    CHECK(track.azimuth_degrees < 360);
  }
}

TEST_CASE("le rayon respire au lieu de croitre", "[render][teleport_rings]") {
  // ⚠ CE N'EST PAS UNE CROISSANCE : à quarante-cinq tics, le double-tic vaut
  // quatre-vingt-dix et le sinus est à son sommet ; à quatre-vingt-dix, il est
  // revenu à zéro. Un rayon qui croîtrait ne redescendrait jamais.
  const float at_peak = AfterTicks(45)[0].lift_key;
  const float au_retour = AfterTicks(90)[0].lift_key;
  CHECK(at_peak == Approx(100.0F).margin(0.01F));
  CHECK(au_retour == Approx(0.0F).margin(0.01F));
}

TEST_CASE("l'alpha descend d'un cran puis de dix", "[render][teleport_rings]") {
  // Avant le soixante-huitième tic — cent trente-cinq en double-tics —, l'alpha
  // ne bouge pas.
  CHECK(AfterTicks(60)[0].alpha == 100);
  // Entre cent trente-cinq et cent quatre-vingts, il descend d'un par tic.
  const std::array<EffectTrack, kEffectTracksPerNode> before = AfterTicks(80);
  CHECK(before[0].alpha < 100);
  CHECK(before[0].mark[kTeleportRingMarkOfSwitch] == 0);
  // Passé cent quatre-vingts, le drapeau se lève et la chute s'accélère.
  const std::array<EffectTrack, kEffectTracksPerNode> after = AfterTicks(92);
  CHECK(after[0].mark[kTeleportRingMarkOfSwitch] == 1);
  CHECK(after[0].alpha < before[0].alpha);
  // Et il ne descend jamais sous zéro.
  CHECK(AfterTicks(300)[0].alpha == 0);
}

TEST_CASE("les segments ondulent autour du rayon", "[render][teleport_rings]") {
  const std::array<EffectTrack, kEffectTracksPerNode> tracks = AfterTicks(20);
  const EffectTrack& track = tracks[0];
  // ⚠ LE PREMIER ET LE DERNIER SEGMENT NE SONT PAS ONDULÉS : le sinus du long du
  // ruban s'annule à zéro et à cent quatre-vingts degrés, ce qui empêche l'onde
  // de casser la jointure de l'anneau.
  CHECK(track.value[0] == Approx(track.lift_key));
  CHECK(track.value[kEffectTrackSlots - 1] == Approx(track.lift_key).margin(0.01F));
  const auto vertex = std::ranges::max_element(track.value);
  CHECK(*vertex != Approx(track.lift_key));
}

TEST_CASE("le noeud seme porte sa forme, son image et une aire", "[render][teleport_rings]") {
  const ro::render::PooledEffectNode node = SowEffectTeleportRingNode(
      PatternOf(kTeleportation2), {4.0F, 1.0F, 6.0F}, ro::render::kEffectNodeDefaultLifeTicks);
  CHECK(node.shape == ro::render::PooledNodeShape::kTeleportRings);
  REQUIRE(node.resource_paths.size() == 1);
  CHECK(node.resource_paths.front() == std::string(PatternOf(kTeleportation2).texture));
  CHECK(node.ground_position.y == Approx(1.0F));

  // LA FIGURE EST CELLE DE L'ANNEAU, et elle doit rendre une aire une fois que
  // le mouvement a donné une longueur aux segments.
  std::array<EffectTrack, kEffectTracksPerNode> tracks = node.tracks;
  AdvanceTeleportRingTracks(tracks);
  const std::vector<EffectWorldQuad> quads =
      ComposeEffectTrackSkirt(tracks[0], 0, node.ground_position);
  CHECK_FALSE(quads.empty());
}
