#include "ro/render/effect_homing_gust.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <span>
#include <string>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_ground_gust.h"
#include "ro/render/effect_homing_gust_seed.h"
#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

using Catch::Approx;
using ro::render::AdvanceHomingGustSpreadTracks;
using ro::render::AdvanceHomingGustSteeredTracks;
using ro::render::EffectHomingGustKind;
using ro::render::EffectHomingGustPattern;
using ro::render::EffectHomingGustPatternsFor;
using ro::render::EffectHomingGustPatternTable;
using ro::render::EffectTrack;
using ro::render::HomingGustSteeredStyleOf;
using ro::render::kEffectTracksPerNode;
using ro::render::kGroundGustMarkOfColor;
using ro::render::kGroundGustSlotOfSpeed;
using ro::render::kHomingGustChasing;
using ro::render::kHomingGustFading;
using ro::render::kHomingGustLiftAboveNode;
using ro::render::kHomingGustMarkOfEarlyFade;
using ro::render::kHomingGustMarkOfSide;
using ro::render::kHomingGustMarkOfState;
using ro::render::kHomingGustReturning;
using ro::render::kHomingGustSteeredSpeed;
using ro::render::SeedEffectHomingGustSpreadTracks;
using ro::render::SeedEffectHomingGustSteeredTracks;
using ro::render::SowEffectHomingGustNode;

namespace {

using Tracks = std::array<EffectTrack, kEffectTracksPerNode>;

// LES DEUX IDENTIFIANTS de la famille : Esma (122) et Wind Blade (125).
constexpr std::uint32_t kEsmaEffectId = 553;
constexpr std::uint32_t kWindBladeEffectId = 621;

// LE NŒUD À L'ORIGINE, sa cible à l'EST — le long de notre x, que le client
// partage avec nous.
constexpr glm::vec3 kNode{0.0F, 0.0F, 0.0F};
constexpr glm::vec3 kTargetToEast{200.0F, 0.0F, 0.0F};

// LA VARIANTE GRISE d'Esma, posée par sa propre compétence.
constexpr std::uint32_t kGreyEsmaEffectId = 1206;

// Les deux côtés d'Esma, tels que le site les passe.
constexpr std::uint8_t kLeft = 0;
constexpr std::uint8_t kRight = 1;
// Ceux de la variante grise, et la teinte qu'ils choisissent.
constexpr std::uint8_t kGreyLeft = 4;
constexpr std::uint8_t kGreyRight = 5;
constexpr std::uint8_t kGreyTint = 3;

// Le tic au-delà duquel la poursuite commence, et celui où l'alpha cesse de
// monter.
constexpr std::int32_t kStraightAhead = 5;
constexpr std::int32_t kEndOfClimb = 20;

void Advance(Tracks& tracks, const std::optional<glm::vec3>& target, std::int32_t ticks) {
  for (std::int32_t tick = 0; tick < ticks; ++tick) {
    AdvanceHomingGustSteeredTracks(tracks, kNode, target);
  }
}

void AdvanceInFan(Tracks& tracks, const std::optional<glm::vec3>& target, std::int32_t ticks,
                  std::uint32_t& seed) {
  for (std::int32_t tick = 0; tick < ticks; ++tick) {
    AdvanceHomingGustSpreadTracks(tracks, kNode, target, seed);
  }
}

}  // namespace

TEST_CASE("la table porte Esma deux fois et Wind Blade une fois", "[render][homing_gust]") {
  const std::span<const EffectHomingGustPattern> esma = EffectHomingGustPatternsFor(kEsmaEffectId);
  REQUIRE(esma.size() == 2);
  CHECK(esma[0].kind == EffectHomingGustKind::kSteered);
  CHECK(esma[0].octet == kLeft);
  CHECK(esma[1].octet == kRight);
  CHECK_FALSE(esma[0].one_node_per_hit);
  CHECK(esma[0].client_function == "Effect_Build_Scatter7aFromParam");

  const std::span<const EffectHomingGustPattern> lame =
      EffectHomingGustPatternsFor(kWindBladeEffectId);
  REQUIRE(lame.size() == 1);
  CHECK(lame[0].kind == EffectHomingGustKind::kRandomSpread);
  CHECK(lame[0].one_node_per_hit);
  CHECK(lame[0].client_function == "Effect_Build_Scatter7dFromParam");
  CHECK(EffectHomingGustPatternTable().size() == 5);
  CHECK(EffectHomingGustPatternsFor(1U).empty());
}

TEST_CASE("l'effet 1206 pose les deux bourrasques grises d'Esma, une de chaque cote",
          "[render][homing_gust]") {
  const std::span<const EffectHomingGustPattern> grises =
      EffectHomingGustPatternsFor(kGreyEsmaEffectId);
  REQUIRE(grises.size() == 2);
  for (const EffectHomingGustPattern& pattern : grises) {
    CHECK(pattern.kind == EffectHomingGustKind::kSteered);
    CHECK_FALSE(pattern.one_node_per_hit);
    CHECK(pattern.client_function == "Effect_Build_Scatter7aFromParam");
  }
  CHECK(grises[0].octet == kGreyLeft);
  CHECK(grises[1].octet == kGreyRight);

  // CE QUE LES DEUX OCTETS DONNENT AUX PISTES : le rayon large d'Esma, la teinte
  // grise, un côté chacun, et aucune extinction hâtive.
  const Tracks left = SeedEffectHomingGustSteeredTracks(kNode, grises[0].octet);
  const Tracks right = SeedEffectHomingGustSteeredTracks(kNode, grises[1].octet);
  for (std::size_t rang = 0; rang < left.size(); ++rang) {
    CHECK(left[rang].radius_at_anchor == Approx(25.0F));
    CHECK(left[rang].mark[kGroundGustMarkOfColor] == kGreyTint);
    CHECK(right[rang].mark[kGroundGustMarkOfColor] == kGreyTint);
    CHECK(left[rang].mark[kHomingGustMarkOfSide] == kLeft);
    CHECK(right[rang].mark[kHomingGustMarkOfSide] == kRight);
    CHECK(left[rang].mark[kHomingGustMarkOfEarlyFade] == 0);
    CHECK(right[rang].mark[kHomingGustMarkOfEarlyFade] == 0);
  }
}

TEST_CASE("l'octet du 122 decide du rayon, de la teinte, du cote et de la hate",
          "[render][homing_gust]") {
  CHECK(HomingGustSteeredStyleOf(0).radius == Approx(25.0F));
  CHECK(HomingGustSteeredStyleOf(0).color == 0);
  CHECK(HomingGustSteeredStyleOf(0).side == std::optional<std::uint8_t>{kLeft});
  CHECK(HomingGustSteeredStyleOf(1).side == std::optional<std::uint8_t>{kRight});
  CHECK(HomingGustSteeredStyleOf(1).early_fade == 0);
  // LES DEUX OCTETS DU MILIEU : étroits, blancs, hâtifs.
  CHECK(HomingGustSteeredStyleOf(2).radius == Approx(6.0F));
  CHECK(HomingGustSteeredStyleOf(2).color == 2);
  CHECK(HomingGustSteeredStyleOf(3).early_fade == 1);
  // LES DEUX DERNIERS : larges et gris.
  CHECK(HomingGustSteeredStyleOf(5).radius == Approx(25.0F));
  CHECK(HomingGustSteeredStyleOf(5).color == 3);
  // ⚠ AU-DELÀ, LE CÔTÉ N'EST PAS ÉCRIT.
  CHECK_FALSE(HomingGustSteeredStyleOf(6).side.has_value());
}

TEST_CASE("le 122 seme quatre pistes vives et transparentes au-dessus du noeud",
          "[render][homing_gust]") {
  const Tracks tracks = SeedEffectHomingGustSteeredTracks({3.0F, 1.0F, -2.0F}, kRight);
  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    CHECK(tracks[rang].active);
    CHECK(tracks[rang].alpha == 0);
    CHECK(tracks[rang].ticks == 0);
    CHECK(tracks[rang].tilt_degrees == static_cast<std::int16_t>(rang));
    CHECK(tracks[rang].value[kGroundGustSlotOfSpeed] == Approx(kHomingGustSteeredSpeed));
    // ⚠ LE CLIENT RETRANCHE DIX À SON ORDONNÉE, QUI DESCEND : chez nous, elle MONTE.
    CHECK(tracks[rang].anchor.y == Approx(1.0F + kHomingGustLiftAboveNode));
    CHECK(tracks[rang].mark[kHomingGustMarkOfSide] == kRight);
    CHECK(tracks[rang].mark[kHomingGustMarkOfState] == kHomingGustChasing);
    CHECK(tracks[rang].mark[kGroundGustMarkOfColor] == 0);
  }
}

TEST_CASE("le 125 tire le rayon puis la vitesse de chaque piste", "[render][homing_gust]") {
  std::uint32_t seed = 7U;
  std::uint32_t witness = 7U;
  const Tracks tracks = SeedEffectHomingGustSpreadTracks(kNode, kRight, seed);
  for (const EffectTrack& track : tracks) {
    const std::uint32_t radius = ro::render::NextEffectNodeDraw(witness) % 11U;
    const std::uint32_t speed = ro::render::NextEffectNodeDraw(witness) % 21U;
    CHECK(track.radius_at_anchor == Approx(10.0F + 0.5F * static_cast<float>(radius)));
    CHECK(track.value[kGroundGustSlotOfSpeed] == Approx(1.0F + 0.1F * static_cast<float>(speed)));
    CHECK(track.mark[kGroundGustMarkOfColor] == 1);
    CHECK(track.mark[kHomingGustMarkOfSide] == kRight);
  }
  // DEUX TIRAGES PAR PISTE, et pas un de plus.
  CHECK(seed == witness);
}

TEST_CASE("Esma part a angle droit de sa cible, d'un cote puis de l'autre",
          "[render][homing_gust]") {
  Tracks left = SeedEffectHomingGustSteeredTracks(kNode, kLeft);
  Tracks right = SeedEffectHomingGustSteeredTracks(kNode, kRight);
  Advance(left, kTargetToEast, 1);
  Advance(right, kTargetToEast, 1);
  // ⚠ LE PREMIER TIC FILE DÉJÀ : ni l'une ni l'autre n'a avancé vers l'est,
  // elles partent en travers, en sens opposés.
  CHECK(left[0].anchor.x == Approx(0.0F).margin(0.001F));
  CHECK(right[0].anchor.x == Approx(0.0F).margin(0.001F));
  CHECK(std::abs(left[0].anchor.z) == Approx(kHomingGustSteeredSpeed));
  CHECK(left[0].anchor.z == Approx(-right[0].anchor.z));
  // 🔴 LE CÔTÉ ZÉRO VA VERS LE Z POSITIF DU CLIENT — le NÉGATIF chez nous.
  CHECK(left[0].anchor.z < 0.0F);
}

TEST_CASE("Esma file droit cinq tics, puis vire de cinq degres vers sa cible",
          "[render][homing_gust]") {
  Tracks tracks = SeedEffectHomingGustSteeredTracks(kNode, kLeft);
  Advance(tracks, kTargetToEast, kStraightAhead);
  const std::int16_t straight_heading = tracks[0].azimuth_degrees;
  Advance(tracks, kTargetToEast, 1);
  // Le client retranche cinq à son cap ; nié, cela AJOUTE cinq au nôtre.
  CHECK(tracks[0].azimuth_degrees == straight_heading + 5);
  // Et à la longue, elle file vers l'est.
  Advance(tracks, kTargetToEast, 40);
  CHECK(tracks[0].anchor.x > 20.0F);
}

TEST_CASE("l'alpha d'Esma monte de quinze jusqu'au vingtieme tic", "[render][homing_gust]") {
  Tracks tracks = SeedEffectHomingGustSteeredTracks(kNode, kLeft);
  Advance(tracks, kTargetToEast, 2);
  CHECK(tracks[0].alpha == 15);
  Advance(tracks, kTargetToEast, kEndOfClimb - 2);
  CHECK(tracks[0].alpha == 250);
}

TEST_CASE("une Esma hative s'eteint des le quatre-vingt-onzieme tic", "[render][homing_gust]") {
  Tracks hative = SeedEffectHomingGustSteeredTracks(kNode, 2);
  Tracks patiente = SeedEffectHomingGustSteeredTracks(kNode, kLeft);
  REQUIRE(hative[0].mark[kHomingGustMarkOfEarlyFade] == 1);
  Advance(hative, kTargetToEast, 90);
  Advance(patiente, kTargetToEast, 90);
  CHECK(hative[0].mark[kHomingGustMarkOfState] != kHomingGustFading);
  Advance(hative, kTargetToEast, 1);
  Advance(patiente, kTargetToEast, 1);
  CHECK(hative[0].mark[kHomingGustMarkOfState] == kHomingGustFading);
  CHECK(patiente[0].mark[kHomingGustMarkOfState] != kHomingGustFading);
  Advance(patiente, kTargetToEast, 50);
  CHECK(patiente[0].mark[kHomingGustMarkOfState] == kHomingGustFading);
}

TEST_CASE("sans cible, Esma ne fait rien au premier tic, puis rentre", "[render][homing_gust]") {
  Tracks tracks = SeedEffectHomingGustSteeredTracks(kNode, kLeft);
  Advance(tracks, std::nullopt, 1);
  CHECK(tracks[0].ticks == 1);
  CHECK(tracks[0].tilt_degrees == 0);
  CHECK(tracks[0].anchor.x == Approx(0.0F));
  CHECK(tracks[0].mark[kHomingGustMarkOfState] == kHomingGustReturning);
  // ⚠ ELLE TOURNE ET S'ALLUME ENSUITE, et renonce au cinquante et unième tic.
  Advance(tracks, std::nullopt, 1);
  CHECK(tracks[0].tilt_degrees == 30);
  CHECK(tracks[0].alpha == 15);
  Advance(tracks, std::nullopt, 48);
  CHECK(tracks[0].mark[kHomingGustMarkOfState] == kHomingGustReturning);
  Advance(tracks, std::nullopt, 1);
  CHECK(tracks[0].mark[kHomingGustMarkOfState] == kHomingGustFading);
}

TEST_CASE("en s'eteignant, les QUATRE pistes palissent ensemble", "[render][homing_gust]") {
  Tracks tracks = SeedEffectHomingGustSteeredTracks(kNode, kLeft);
  for (EffectTrack& track : tracks) {
    track.alpha = 100;
  }
  // ⚠ UN TIC QUI N'EST PAS UN MULTIPLE DE TROIS : l'écho recopierait sinon
  // l'alpha de la meneuse sur les trois autres.
  tracks[0].ticks = 199;
  tracks[0].mark[kHomingGustMarkOfState] = kHomingGustFading;
  const std::int16_t cap = tracks[0].azimuth_degrees;
  Advance(tracks, kTargetToEast, 1);
  for (const EffectTrack& track : tracks) {
    CHECK(track.alpha == 95);
  }
  // Et le cap tourne d'un degré, du côté de la marque — nié chez nous.
  CHECK(tracks[0].azimuth_degrees == (cap + 1) % 360);
}

TEST_CASE("Wind Blade part d'un ecart tire, et tire une seule fois", "[render][homing_gust]") {
  std::uint32_t seed = 11U;
  Tracks left = SeedEffectHomingGustSpreadTracks(kNode, kLeft, seed);
  Tracks right = SeedEffectHomingGustSpreadTracks(kNode, kRight, seed);
  const std::uint32_t before = seed;
  AdvanceInFan(left, kTargetToEast, 1, seed);
  std::uint32_t witness = before;
  static_cast<void>(ro::render::NextEffectNodeDraw(witness));
  CHECK(seed == witness);
  AdvanceInFan(right, kTargetToEast, 1, seed);
  // LE CÔTÉ ZÉRO AJOUTE vingt-cinq à quarante-cinq degrés chez le client : il
  // les RETRANCHE chez nous.
  CHECK(left[0].azimuth_degrees >= 360 - 45);
  CHECK(left[0].azimuth_degrees <= 360 - 25);
  CHECK(right[0].azimuth_degrees >= 25);
  CHECK(right[0].azimuth_degrees <= 45);
}

TEST_CASE("Wind Blade s'eteint au vingt et unieme tic, ou sans cible", "[render][homing_gust]") {
  std::uint32_t seed = 5U;
  Tracks tracks = SeedEffectHomingGustSpreadTracks(kNode, kLeft, seed);
  AdvanceInFan(tracks, kTargetToEast, 2, seed);
  CHECK(tracks[0].alpha == 8);
  AdvanceInFan(tracks, kTargetToEast, kStraightAhead - 2, seed);
  CHECK(tracks[0].alpha == 32);
  // LE PLAFOND ET LE VINGT ET UNIÈME TIC, cap tourné à l'opposé de la cible :
  // sans quoi elle la trouverait avant, et s'éteindrait pour cette raison-là.
  constexpr std::int16_t kHalfTurn = 180;
  tracks[0].azimuth_degrees = kHalfTurn;
  tracks[0].alpha = 118;
  AdvanceInFan(tracks, kTargetToEast, 1, seed);
  CHECK(tracks[0].alpha == 120);
  CHECK(tracks[0].mark[kHomingGustMarkOfState] == kHomingGustChasing);
  tracks[0].azimuth_degrees = kHalfTurn;
  tracks[0].ticks = kEndOfClimb;
  AdvanceInFan(tracks, kTargetToEast, 1, seed);
  CHECK(tracks[0].mark[kHomingGustMarkOfState] == kHomingGustFading);

  Tracks perdue = SeedEffectHomingGustSpreadTracks(kNode, kLeft, seed);
  AdvanceInFan(perdue, std::nullopt, 1, seed);
  CHECK(perdue[0].mark[kHomingGustMarkOfState] == kHomingGustFading);
  CHECK(perdue[0].tilt_degrees == 0);
}

TEST_CASE("Wind Blade s'eteint des qu'elle vise sa cible", "[render][homing_gust]") {
  std::uint32_t seed = 5U;
  Tracks tracks = SeedEffectHomingGustSpreadTracks(kNode, kLeft, seed);
  AdvanceInFan(tracks, kTargetToEast, kStraightAhead, seed);
  // ELLE VISE DÉJÀ : le cap est posé plein est, à la place de l'écart tiré.
  tracks[0].azimuth_degrees = 0;
  tracks[0].anchor = kNode;
  AdvanceInFan(tracks, kTargetToEast, 1, seed);
  CHECK(tracks[0].mark[kHomingGustMarkOfState] == kHomingGustFading);
  CHECK(tracks[0].azimuth_degrees == 0);
}

TEST_CASE("le noeud seme porte sa forme, sa cible et son image", "[render][homing_gust]") {
  std::uint32_t seed = 3U;
  const EffectHomingGustPattern& esma = EffectHomingGustPatternsFor(kEsmaEffectId).front();
  const ro::render::PooledEffectNode node =
      SowEffectHomingGustNode(esma, {2.0F, 0.0F, 7.0F}, kTargetToEast, kRight, 200, seed);
  CHECK(node.shape == ro::render::PooledNodeShape::kHomingGustSteered);
  CHECK(node.node.life_ticks == 200);
  REQUIRE(node.target_position.has_value());
  CHECK(node.target_position->x == Approx(kTargetToEast.x));
  REQUIRE(node.resource_paths.size() == 1);
  CHECK(node.resource_paths.front() == std::string(esma.texture));
  CHECK(node.tracks[0].mark[kHomingGustMarkOfSide] == kRight);

  const EffectHomingGustPattern& lame = EffectHomingGustPatternsFor(kWindBladeEffectId).front();
  CHECK(SowEffectHomingGustNode(lame, kNode, std::nullopt, kLeft, 200, seed).shape ==
        ro::render::PooledNodeShape::kHomingGustSpread);
}
