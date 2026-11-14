#include "ro/render/effect_swirl_trail.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <optional>
#include <string>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_node_seed.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_swirl_trail_seed.h"
#include "ro/render/effect_tinted_quad.h"
#include "ro/render/effect_track.h"

using Catch::Approx;
using ro::render::AdvanceSwirlTrailTracks;
using ro::render::ComposeTintedRotatedQuad;
using ro::render::EffectNodeScheduleLastSowTick;
using ro::render::EffectNodeScheduleSowsAt;
using ro::render::EffectSwirlTrailPattern;
using ro::render::EffectSwirlTrailPatternsFor;
using ro::render::EffectTrack;
using ro::render::kEffectTracksPerNode;
using ro::render::kSwirlTrailAlphaAtBirth;
using ro::render::kSwirlTrailBackStepCount;
using ro::render::kSwirlTrailLiftAboveOwner;
using ro::render::kSwirlTrailRadius;
using ro::render::kSwirlTrailRenderWord;
using ro::render::kSwirlTrailSchedule;
using ro::render::kSwirlTrailSlotOfSpeed;
using ro::render::kSwirlTrailSpeed;
using ro::render::SeedEffectSwirlTrailTracks;
using ro::render::SowEffectSwirlTrailNode;

namespace {

using Tracks = std::array<EffectTrack, kEffectTracksPerNode>;

// L'IDENTIFIANT UNIQUE du sous-type 116.
constexpr std::uint32_t kShieldChainEffectId = 520;

// La marque de teinte que le carré teinté lit sur la première piste.
constexpr std::size_t kMarkOfTint = 2;

// Deux angles de caméra quelconques, pour voir qu'ils sont rangés.
constexpr std::int16_t kTurn = 123;
constexpr std::int16_t kTilt = 45;

[[nodiscard]] float Radians(std::int32_t degrees) {
  return static_cast<float>(degrees) * std::numbers::pi_v<float> / 180.0F;
}

void Advance(Tracks& tracks, std::int32_t ticks) {
  for (std::int32_t tick = 0; tick < ticks; ++tick) {
    AdvanceSwirlTrailTracks(tracks, kTurn, kTilt);
  }
}

}  // namespace

TEST_CASE("l'effet 520 a son site, et son image est celle du bouclier", "[render][swirl_trail]") {
  const auto patterns = EffectSwirlTrailPatternsFor(kShieldChainEffectId);
  REQUIRE(patterns.size() == 1);
  CHECK(patterns[0].client_function == "EffectUpdate_CruShieldBoomerang");
  CHECK(patterns[0].texture == "data\\texture\\effect\\shield_boomerang.bmp");
  CHECK(EffectSwirlTrailPatternsFor(kShieldChainEffectId + 1).empty());
}

TEST_CASE("le bâtisseur sème aux tics zéro, trois, six, neuf et douze", "[render][swirl_trail]") {
  for (std::int32_t tick = 0; tick <= 20; ++tick) {
    const bool expected = tick <= 12 && tick % 3 == 0;
    CHECK(EffectNodeScheduleSowsAt(kSwirlTrailSchedule, tick) == expected);
  }
  CHECK(EffectNodeScheduleLastSowTick(kSwirlTrailSchedule) == 12);
}

TEST_CASE("la meneuse naît à vingt-cinq, reculée de quinze pas contre son cap",
          "[render][swirl_trail]") {
  const glm::vec3 bearer = {100.0F, 4.0F, -50.0F};
  std::uint32_t generator = 7;
  const Tracks tracks = SeedEffectSwirlTrailTracks(bearer, generator);
  for (const EffectTrack& track : tracks) {
    CHECK(track.active);
  }
  const EffectTrack& leader = tracks[0];
  CHECK(leader.alpha == kSwirlTrailAlphaAtBirth);
  CHECK(leader.radius_at_anchor == Approx(kSwirlTrailRadius));
  CHECK(leader.value[kSwirlTrailSlotOfSpeed] == Approx(kSwirlTrailSpeed));
  CHECK(leader.azimuth_degrees >= 0);
  CHECK(leader.azimuth_degrees < 360);
  CHECK(leader.mark[kMarkOfTint] == 0);
  const float recul = kSwirlTrailSpeed * kSwirlTrailBackStepCount;
  CHECK(recul == Approx(37.5F));
  CHECK(leader.anchor.x ==
        Approx(bearer.x - recul * std::cos(Radians(leader.azimuth_degrees))).margin(0.001F));
  CHECK(leader.anchor.z ==
        Approx(bearer.z - recul * std::sin(Radians(leader.azimuth_degrees))).margin(0.001F));
  // ⚠ LE CLIENT RETRANCHE HUIT À UNE ORDONNÉE QUI DESCEND : l'objet est AU-DESSUS.
  CHECK(leader.anchor.y == Approx(bearer.y + kSwirlTrailLiftAboveOwner));
  // Les échos naissent allumés et invisibles.
  for (std::size_t rang = 1; rang < tracks.size(); ++rang) {
    CHECK(tracks[rang].alpha == 0);
  }
}

TEST_CASE("la meneuse traverse la place de son porteur au quinzième tic", "[render][swirl_trail]") {
  const glm::vec3 bearer = {10.0F, 0.0F, 20.0F};
  std::uint32_t generator = 99;
  Tracks tracks = SeedEffectSwirlTrailTracks(bearer, generator);
  const float height = tracks[0].anchor.y;
  Advance(tracks, 15);
  CHECK(tracks[0].anchor.x == Approx(bearer.x).margin(0.001F));
  CHECK(tracks[0].anchor.z == Approx(bearer.z).margin(0.001F));
  // Elle file à plat.
  CHECK(tracks[0].anchor.y == Approx(height));
}

TEST_CASE("l'alpha monte cinq tics de quarante-cinq, tient, puis perd dix dès le quinzième",
          "[render][swirl_trail]") {
  std::uint32_t generator = 1;
  Tracks tracks = SeedEffectSwirlTrailTracks({}, generator);
  Advance(tracks, 5);
  CHECK(tracks[0].alpha == 250);
  Advance(tracks, 9);
  CHECK(tracks[0].alpha == 250);
  Advance(tracks, 1);
  CHECK(tracks[0].alpha == 240);
  // Planché à zéro, jamais négatif.
  Advance(tracks, 40);
  CHECK(tracks[0].alpha == 0);
}

TEST_CASE("le carré tourne de quinze degrés par tic, vers le bas", "[render][swirl_trail]") {
  std::uint32_t generator = 1;
  Tracks tracks = SeedEffectSwirlTrailTracks({}, generator);
  Advance(tracks, 1);
  CHECK(tracks[0].degrees == 345);
  Advance(tracks, 4);
  CHECK(tracks[0].degrees == 285);
}

TEST_CASE("les angles de caméra sont rangés dans le style des deux premières pistes",
          "[render][swirl_trail]") {
  Tracks tracks;
  // ⚠ MÊME MENEUSE ÉTEINTE : le client les range avant sa garde.
  AdvanceSwirlTrailTracks(tracks, kTurn, kTilt);
  CHECK(tracks[0].style == kTurn);
  CHECK(tracks[1].style == kTilt);
}

TEST_CASE("les échos reprennent leur voisine tous les trois tics, en partant du bout",
          "[render][swirl_trail]") {
  std::uint32_t generator = 3;
  Tracks tracks = SeedEffectSwirlTrailTracks({0.0F, 0.0F, 0.0F}, generator);
  Advance(tracks, 5);
  const EffectTrack leader_at_fifth = tracks[0];
  // AU SIXIÈME TIC, le premier écho reçoit la meneuse telle qu'elle était à la
  // fin du cinquième, moins cent cinquante d'alpha.
  Advance(tracks, 1);
  CHECK(tracks[1].alpha == 250 - 150);
  CHECK(tracks[1].degrees == leader_at_fifth.degrees);
  CHECK(tracks[1].anchor.x == Approx(leader_at_fifth.anchor.x));
  CHECK(tracks[1].anchor.z == Approx(leader_at_fifth.anchor.z));
  CHECK(tracks[1].radius_at_anchor == Approx(kSwirlTrailRadius));
  // Les deux autres ont reçu du vide.
  CHECK(tracks[2].alpha == 0);
  CHECK(tracks[3].alpha == 0);
  // Il ne bouge pas de lui-même entre deux recopies.
  const glm::vec3 fige = tracks[1].anchor;
  Advance(tracks, 2);
  CHECK(tracks[1].anchor.x == Approx(fige.x));
  // AU NEUVIÈME : le deuxième reçoit le premier moins vingt-cinq AVANT que le
  // premier ne soit récrit.
  Advance(tracks, 1);
  CHECK(tracks[2].alpha == 100 - 25);
  CHECK(tracks[2].anchor.x == Approx(fige.x));
  CHECK(tracks[1].alpha == 100);
  // AU DOUZIÈME : le dernier suit.
  Advance(tracks, 3);
  CHECK(tracks[3].alpha == 75 - 25);
}

TEST_CASE("le nœud semé se dessine en carré blanc, et porte son mot de rendu",
          "[render][swirl_trail]") {
  const EffectSwirlTrailPattern pattern = EffectSwirlTrailPatternsFor(kShieldChainEffectId)[0];
  std::uint32_t generator = 5;
  auto node = SowEffectSwirlTrailNode(pattern, {1.0F, 2.0F, 3.0F}, 487, generator);
  CHECK(node.shape == ro::render::PooledNodeShape::kSwirlTrail);
  CHECK(node.node.life_ticks == 487);
  REQUIRE(node.node.render_word.has_value());
  CHECK(*node.node.render_word == kSwirlTrailRenderWord);
  REQUIRE(node.resource_paths.size() == 1);
  CHECK(node.resource_paths[0] == std::string(pattern.texture));
  // LA MENEUSE SE VOIT DÈS LE SEMIS : la marque zéro dessine, à vingt-cinq.
  const std::optional<ro::render::EffectWorldQuad> quad =
      ComposeTintedRotatedQuad(node.tracks[0], node.tracks[0].mark[kMarkOfTint], 0, 0, 0);
  CHECK(quad.has_value());
  // Les échos, non.
  CHECK_FALSE(ComposeTintedRotatedQuad(node.tracks[1], node.tracks[0].mark[kMarkOfTint], 0, 0, 0)
                  .has_value());
}
