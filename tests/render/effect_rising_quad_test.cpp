#include "ro/render/effect_rising_quad.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <utility>

#include <catch2/catch_test_macros.hpp>
#include <glm/glm.hpp>

#include "ro/render/effect_node_pattern_table.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_tinted_quad.h"
#include "ro/render/effect_track.h"
#include "ro/render/effect_track_motion.h"

using ro::render::EffectRisingQuadPattern;
using ro::render::EffectTrack;

namespace {

constexpr std::uint32_t kLokisVeilMark = 283;
constexpr glm::vec3 kOwner{40.0F, 7.0F, -12.0F};
// Les marques que la figure et le mouvement lisent.
constexpr std::size_t kMarkOfMode = 1;
constexpr std::size_t kMarkOfQuadDrawing = 2;
constexpr std::size_t kSlotOfSpinDegrees = 8;

}  // namespace

TEST_CASE("Carrés qui montent : la marque de Loki's Veil sème deux pistes, en mode zéro",
          "[render][risingquad]") {
  const auto patterns = ro::render::EffectRisingQuadPatternsFor(kLokisVeilMark);
  REQUIRE(patterns.size() == 1);
  CHECK(patterns.front().texture == "data\\texture\\effect\\safeline.bmp");
  CHECK(patterns.front().mode == 0);
  CHECK(ro::render::KnowsProceduralEffect(kLokisVeilMark));

  for (std::uint32_t seed = 1; seed < 200; ++seed) {
    std::uint32_t generator = seed;
    const std::array<EffectTrack, ro::render::kEffectTracksPerNode> tracks =
        ro::render::SeedEffectRisingQuadTracks(patterns.front(), kOwner, generator);
    for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
      const EffectTrack& track = tracks[rang];
      REQUIRE(track.active == (rang < 2));
      if (!track.active) {
        continue;
      }
      CHECK(track.alpha == 0);
      CHECK(track.ticks >= 0);
      CHECK(track.ticks <= 10);
      // Un entier sous six, plus cinq.
      CHECK(track.radius_at_anchor >= 5.0F);
      CHECK(track.radius_at_anchor <= 10.0F);
      CHECK(track.radius_at_anchor == static_cast<float>(static_cast<int>(track.radius_at_anchor)));
      CHECK(track.degrees == ro::render::kRisingQuadStartDegrees);
      CHECK(track.mark[kMarkOfQuadDrawing] == 2);
      CHECK(track.mark[kMarkOfMode] == 0);
      // ⚠ LA HAUTEUR N'EST PAS POSÉE au semis : seule la reprise la pose.
      CHECK(track.anchor == glm::vec3{kOwner.x, 0.0F, kOwner.z});
    }
  }
}

TEST_CASE("Carrés qui montent : le mode tournant ne sème qu'une piste, plus étroite",
          "[render][risingquad]") {
  const EffectRisingQuadPattern turning{kLokisVeilMark, "x.bmp",
                                        ro::render::kRisingQuadTurningMode};
  for (std::uint32_t seed = 1; seed < 200; ++seed) {
    std::uint32_t generator = seed;
    const auto tracks = ro::render::SeedEffectRisingQuadTracks(turning, kOwner, generator);
    CHECK(tracks[0].active);
    CHECK_FALSE(tracks[1].active);
    CHECK(tracks[0].radius_at_anchor >= 3.0F);
    CHECK(tracks[0].radius_at_anchor < 5.0F);
  }
}

TEST_CASE("Carrés qui montent : dix tics d'allumage, puis l'extinction en montant, puis la reprise",
          "[render][risingquad]") {
  std::array<EffectTrack, ro::render::kEffectTracksPerNode> tracks;
  EffectTrack& track = tracks[0];
  track.active = true;
  track.ticks = 0;
  constexpr float kOwnerHeight = 7.0F;

  for (int advance = 1; advance <= 10; ++advance) {
    ro::render::AdvanceRisingQuadTracks(tracks, kOwnerHeight, 11, 22);
    CHECK(track.alpha == 25 * advance);
    CHECK(track.anchor.y == 0.0F);
  }
  CHECK(tracks[0].style == 11);
  CHECK(tracks[1].style == 22);

  // De deux cent cinquante, cinq par avance : quarante-neuf avances visibles.
  for (int advance = 1; advance <= 49; ++advance) {
    ro::render::AdvanceRisingQuadTracks(tracks, kOwnerHeight, 0, 0);
    CHECK(track.alpha == 250 - 5 * advance);
  }
  const float climbed = track.anchor.y;
  CHECK(climbed > 9.7F);
  CHECK(climbed < 9.9F);

  // La cinquantième l'éteint : elle repart du porteur, et monte déjà.
  ro::render::AdvanceRisingQuadTracks(tracks, kOwnerHeight, 0, 0);
  CHECK(track.alpha == 0);
  CHECK(track.ticks == 0);
  CHECK(track.anchor.y == kOwnerHeight + ro::render::kRisingQuadClimbPerTick);
  // Le cap du mode zéro ne tourne pas ; l'angle propre, si.
  CHECK(track.degrees == 0);
  CHECK(track.value[kSlotOfSpinDegrees] == 300.0F);
}

TEST_CASE("Carrés qui montent : le mode tournant reprend plus haut et fait tourner son cap",
          "[render][risingquad]") {
  std::array<EffectTrack, ro::render::kEffectTracksPerNode> tracks;
  EffectTrack& track = tracks[0];
  track.active = true;
  track.ticks = 10;
  track.degrees = 358;
  track.mark[kMarkOfMode] = ro::render::kRisingQuadTurningMode;
  ro::render::AdvanceRisingQuadTracks(tracks, 7.0F, 0, 0);
  // Onzième tic sur un alpha nul : la reprise, cinq plus haut.
  CHECK(track.ticks == 0);
  CHECK(track.anchor.y ==
        7.0F + ro::render::kRisingQuadTurningLift + ro::render::kRisingQuadClimbPerTick);
  CHECK(track.degrees == 3);
}

TEST_CASE("Carrés qui montent : le nœud semé suit la forme et la vie qu'on lui donne",
          "[render][risingquad]") {
  std::uint32_t generator = 7;
  const ro::render::PooledEffectNode pooled = ro::render::SowEffectRisingQuadNode(
      ro::render::EffectRisingQuadPatternsFor(kLokisVeilMark).front(), kOwner, 29999, generator);
  CHECK(pooled.shape == ro::render::PooledNodeShape::kRisingQuads);
  CHECK(pooled.node.life_ticks == 29999);
  CHECK(pooled.owner_position == kOwner);
  REQUIRE(pooled.resource_paths.size() == 1);
}

TEST_CASE("Carré teinté : le plafond d'alpha ne touche que les pistes allumées au-dessus de cent",
          "[render][risingquad]") {
  std::array<EffectTrack, ro::render::kEffectTracksPerNode> tracks;
  tracks[0].active = true;
  tracks[0].alpha = 250;
  tracks[1].active = true;
  tracks[1].alpha = 60;
  tracks[2].active = false;
  tracks[2].alpha = 250;
  ro::render::CapTintedQuadAlpha(tracks);
  CHECK(tracks[0].alpha == ro::render::kTintedQuadAlphaCapOnThatMap);
  CHECK(tracks[1].alpha == 60);
  CHECK(tracks[2].alpha == 250);
  CHECK(ro::render::DrawnAsTintedQuad(ro::render::PooledNodeShape::kRisingQuads));
  CHECK(ro::render::DrawnAsTintedQuad(ro::render::PooledNodeShape::kTintedQuads));
  CHECK_FALSE(ro::render::DrawnAsTintedQuad(ro::render::PooledNodeShape::kRingTracks));
  // ⚠ ET LES QUATRE QUE LE CLIENT MÈNE AU MÊME DESSINATEUR, ce que nos fichiers
  // ne disent pas : le champ dispersé (33), les étincelles qui jaillissent (45),
  // le carré qui monte (59) et l'objet qui tombe (87).
  CHECK(ro::render::DrawnAsTintedQuad(ro::render::PooledNodeShape::kScatterField));
  CHECK(ro::render::DrawnAsTintedQuad(ro::render::PooledNodeShape::kThunderScatter));
  CHECK(ro::render::DrawnAsTintedQuad(ro::render::PooledNodeShape::kRisingSparks));
  CHECK(ro::render::DrawnAsTintedQuad(ro::render::PooledNodeShape::kFallingItem));
}

TEST_CASE("Carré teinté : le plafond tombe une fois par image, après tous ses tics",
          "[render][risingquad]") {
  // Deux pistes au plafond, à un tic de la fin de leur montée.
  const auto pool_at_last_rise = [](std::int16_t alpha) {
    ro::render::EffectNodePool pool;
    ro::render::EffectScatterWorld world;
    world.caps_tinted_quad_alpha = true;
    pool.SetWorld(world);
    std::uint32_t generator = 3;
    ro::render::PooledEffectNode node = ro::render::SowEffectRisingQuadNode(
        ro::render::EffectRisingQuadPatternsFor(kLokisVeilMark).front(), kOwner, 29999, generator);
    for (EffectTrack& track : node.tracks) {
      track.ticks = ro::render::kRisingQuadLastTickOfRise - 1;
      track.alpha = alpha;
    }
    pool.Add(std::move(node));
    return pool;
  };
  const auto lowest_active_alpha = [](const ro::render::EffectNodePool& pool) {
    std::int16_t lowest = std::numeric_limits<std::int16_t>::max();
    for (const EffectTrack& track : pool.nodes().front().tracks) {
      if (track.active) {
        lowest = std::min(lowest, track.alpha);
      }
    }
    return lowest;
  };

  // 🔴 UNE IMAGE QUI RATTRAPE DEUX TICS : le dernier tic de montée porte la
  // piste à cent vingt-cinq, le premier de descente la ramène à cent vingt, et
  // le dessin la plafonne ENSUITE. Plafonner à chaque tic l'aurait fait
  // redescendre de cent, à quatre-vingt-quinze.
  ro::render::EffectNodePool caught_up =
      pool_at_last_rise(ro::render::kTintedQuadAlphaCapOnThatMap);
  static_cast<void>(caught_up.Advance(2 * static_cast<std::uint32_t>(ro::render::kEffectTickMs)));
  CHECK(lowest_active_alpha(caught_up) == ro::render::kTintedQuadAlphaCapOnThatMap);

  // ⚠ ET UNE IMAGE SANS TIC DÛ plafonne aussi : un nœud semé depuis la
  // précédente ne se montre pas au-dessus.
  constexpr std::int16_t kBirthAlphaAboveCap = 200;
  ro::render::EffectNodePool fresh = pool_at_last_rise(kBirthAlphaAboveCap);
  CHECK(fresh.Advance(0) == 0);
  CHECK(lowest_active_alpha(fresh) == ro::render::kTintedQuadAlphaCapOnThatMap);
}

TEST_CASE("Carrés qui montent : sur la carte qui plafonne, l'alpha ne passe jamais cent",
          "[render][risingquad]") {
  const auto highest_alpha = [](bool capped) {
    ro::render::EffectNodePool pool;
    ro::render::EffectScatterWorld world;
    world.caps_tinted_quad_alpha = capped;
    pool.SetWorld(world);
    std::uint32_t generator = 3;
    pool.Add(ro::render::SowEffectRisingQuadNode(
        ro::render::EffectRisingQuadPatternsFor(kLokisVeilMark).front(), kOwner, 29999, generator));
    std::int16_t highest = 0;
    for (int advance = 0; advance < 40; ++advance) {
      static_cast<void>(pool.Advance(static_cast<std::uint32_t>(ro::render::kEffectTickMs)));
      for (const EffectTrack& track : pool.nodes().front().tracks) {
        highest = std::max(highest, track.alpha);
      }
    }
    return highest;
  };
  CHECK(highest_alpha(true) == ro::render::kTintedQuadAlphaCapOnThatMap);
  CHECK(highest_alpha(false) > ro::render::kTintedQuadAlphaCapOnThatMap);
}

TEST_CASE("Carré teinté : le plafond ne touche pas un nœud que la porte de dessin refuse",
          "[render][risingquad]") {
  // Le client plafonne EN DESSINANT : ce qu'il ne dessine pas — l'effet d'un
  // porteur caché — garde son alpha.
  constexpr std::uint32_t kHiddenBearer = 1;
  constexpr std::uint32_t kSeenBearer = 2;
  constexpr std::int16_t kBirthAlphaAboveCap = 200;
  ro::render::EffectNodePool pool;
  ro::render::EffectScatterWorld world;
  world.caps_tinted_quad_alpha = true;
  pool.SetWorld(world);
  for (const std::uint32_t bearer : {kHiddenBearer, kSeenBearer}) {
    const ro::render::EffectNodePool::AnchoredSowing anchor{pool, bearer, kOwner, kLokisVeilMark};
    std::uint32_t generator = 3;
    ro::render::PooledEffectNode node = ro::render::SowEffectRisingQuadNode(
        ro::render::EffectRisingQuadPatternsFor(kLokisVeilMark).front(), kOwner, 29999, generator);
    for (EffectTrack& track : node.tracks) {
      track.alpha = kBirthAlphaAboveCap;
    }
    pool.Add(std::move(node));
  }
  pool.SetDrawGate(
      [](const ro::render::PooledEffectNode& node) { return node.anchor_id != kHiddenBearer; });
  // ⚠ UN PASSAGE ENTIER : semé par son effet, un nœud ne se dessine qu'après sa
  // première mise à jour — voir `EffectNodePool::IsShown`.
  static_cast<void>(pool.Advance(static_cast<std::uint32_t>(ro::render::kEffectTickMs)));
  for (const ro::render::PooledEffectNode& node : pool.nodes()) {
    for (const EffectTrack& track : node.tracks) {
      if (!track.active) {
        continue;
      }
      if (node.anchor_id == kHiddenBearer) {
        CHECK(track.alpha > ro::render::kTintedQuadAlphaCapOnThatMap);
      } else {
        CHECK(track.alpha == ro::render::kTintedQuadAlphaCapOnThatMap);
      }
    }
  }
}
