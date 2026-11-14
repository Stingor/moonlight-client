#include "ro/render/effect_camera_fan.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_camera_fan_seed.h"
#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_spiral_fan.h"
#include "ro/render/effect_track.h"
#include "ro/render/effect_track_motion.h"

using Catch::Approx;
using ro::render::AdvanceCameraFanTracks;
using ro::render::CameraFanFollowsItsOwner;
using ro::render::ComposeSpiralFanQuads;
using ro::render::EffectCameraFanPattern;
using ro::render::EffectCameraFanPatternsFor;
using ro::render::EffectCameraFanPatternTable;
using ro::render::EffectTrack;
using ro::render::EffectWorldQuad;
using ro::render::kCameraFanBladeCount;
using ro::render::kCameraFanCeilingStep;
using ro::render::kCameraFanFirstCeiling;
using ro::render::kCameraFanMarkOfTexture;
using ro::render::kCameraFanMostTrailsPerNode;
using ro::render::kCameraFanSlotOfFirstBlade;
using ro::render::kCameraFanSlotOfSideways;
using ro::render::kEffectTracksPerNode;
using ro::render::SeedEffectCameraFanTracks;
using ro::render::SowEffectCameraFanNode;

namespace {

constexpr std::uint32_t kBeginAsura = 328;
constexpr std::int16_t kTour = 45;
constexpr std::int16_t kTilt = 60;

// LES DEUX NŒUDS d'un effet, dans l'ordre de leur tic.
[[nodiscard]] std::span<const EffectCameraFanPattern> PatternsOf(std::uint32_t effect_id) {
  const std::span<const EffectCameraFanPattern> patterns = EffectCameraFanPatternsFor(effect_id);
  REQUIRE(patterns.size() == 2);
  return patterns;
}

[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> AfterTicks(
    const EffectCameraFanPattern& pattern, std::int32_t count) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks = SeedEffectCameraFanTracks(pattern);
  for (std::int32_t tick = 0; tick < count; ++tick) {
    AdvanceCameraFanTracks(tracks, kTour, kTilt);
  }
  return tracks;
}

}  // namespace

TEST_CASE("la table porte deux noeuds par identifiant", "[render][camera_fan]") {
  const std::span<const EffectCameraFanPattern> table = EffectCameraFanPatternTable();
  CHECK(table.size() == 6);
  for (std::size_t rang = 1; rang < table.size(); ++rang) {
    CHECK(table[rang - 1].effect_id <= table[rang].effect_id);
  }
  const std::span<const EffectCameraFanPattern> asura = PatternsOf(kBeginAsura);
  // ⚠ LES DEUX NŒUDS NAISSENT À VINGT TICS D'ÉCART, et AUCUN au tic zéro.
  CHECK(asura[0].schedule.first_tick == 1);
  CHECK(asura[1].schedule.first_tick == 21);
  // ET LEURS TRAÎNÉES PARTENT DE DEUX CÔTÉS OPPOSÉS.
  CHECK(asura[0].seed.first_sideways < 0.0F);
  CHECK(asura[1].seed.first_sideways > 0.0F);
  CHECK(EffectCameraFanPatternsFor(1).empty());
}

TEST_CASE("le semis allume trois trainees et laisse la quatrieme eteinte", "[render][camera_fan]") {
  const EffectCameraFanPattern& pattern = PatternsOf(kBeginAsura).front();
  const std::array<EffectTrack, kEffectTracksPerNode> tracks = SeedEffectCameraFanTracks(pattern);
  REQUIRE(pattern.trail_count == 3);
  for (std::size_t trail = 0; trail < pattern.trail_count; ++trail) {
    CHECK(tracks[trail].active);
    // ⚠ L'ALPHA ET LES DIX LAMES NAISSENT NULS : l'éventail s'ouvre sous le
    // mouvement, et le semer brillant lui retirerait son ouverture.
    CHECK(tracks[trail].alpha == 0);
    for (std::size_t lame = 0; lame < kCameraFanBladeCount; ++lame) {
      CHECK(tracks[trail].value[kCameraFanSlotOfFirstBlade + lame] == 0.0F);
    }
    // CHAQUE TRAÎNÉE A SON IMAGE, et c'est son rang qui la désigne.
    CHECK(tracks[trail].mark[kCameraFanMarkOfTexture] == trail);
    // ET SON RETARD : le premier nœud rattrape le sien, de moins quarante à zéro.
    CHECK(tracks[trail].ticks == -40 + 20 * static_cast<std::int32_t>(trail));
  }
  CHECK_FALSE(tracks[pattern.trail_count].active);
  // LES TROIS S'ÉTALENT DU MÊME CÔTÉ, de plus en plus loin.
  CHECK(tracks[0].value[kCameraFanSlotOfSideways] > tracks[2].value[kCameraFanSlotOfSideways]);
  // ET AUCUNE NE SUIT SON PORTEUR : la marque de style vaut zéro.
  CHECK_FALSE(CameraFanFollowsItsOwner(tracks));
}

TEST_CASE("le second noeud s'echelonne au lieu de rattraper", "[render][camera_fan]") {
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectCameraFanTracks(PatternsOf(kBeginAsura)[1]);
  CHECK(tracks[0].ticks == -40);
  CHECK(tracks[1].ticks == -60);
  CHECK(tracks[2].ticks == -80);
  // Et ses traînées s'étalent de l'autre côté.
  CHECK(tracks[0].value[kCameraFanSlotOfSideways] < tracks[2].value[kCameraFanSlotOfSideways]);
}

TEST_CASE("les deux angles de camera se rangent dans les styles", "[render][camera_fan]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectCameraFanTracks(PatternsOf(kBeginAsura).front());
  AdvanceCameraFanTracks(tracks, kTour, kTilt);
  CHECK(tracks[0].style == kTour);
  CHECK(tracks[1].style == kTilt);
}

TEST_CASE("une trainee retardee attend sans bouger", "[render][camera_fan]") {
  // ⚠ C'EST LA PREMIÈRE QUI ATTEND LE PLUS, et non la dernière : le nœud du tic
  // un RATTRAPE son retard, de moins quarante à zéro. La troisième traînée part
  // donc tout de suite, et la première quarante tics plus tard.
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      AfterTicks(PatternsOf(kBeginAsura).front(), 20);
  CHECK(tracks[2].value[kCameraFanSlotOfFirstBlade] > 0.0F);
  CHECK(tracks[0].value[kCameraFanSlotOfFirstBlade] == 0.0F);
}

// LE DERNIER TIC DE MONTÉE de la première traînée : elle part de moins quarante
// et monte tant que son compte reste sous vingt.
constexpr std::int32_t kTicksToPeak = 59;

TEST_CASE("chaque lame monte a son propre plafond", "[render][camera_fan]") {
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      AfterTicks(PatternsOf(kBeginAsura).front(), kTicksToPeak);
  const EffectTrack& track = tracks[0];
  // ⚠ LE DÉGRADÉ COURT SUR LES PREMIÈRES LAMES ET S'APLATIT SUR LES DERNIÈRES :
  // dix-neuf pas de treize font deux cent quarante-sept, et seuls les plafonds
  // sous ce compte sont atteints. C'est la FENÊTRE de montée qui les rend
  // inatteignables, pas eux.
  CHECK(track.value[kCameraFanSlotOfFirstBlade] == Approx(kCameraFanFirstCeiling));
  CHECK(track.value[kCameraFanSlotOfFirstBlade + 1] ==
        Approx(kCameraFanFirstCeiling + kCameraFanCeilingStep));
  const float last = track.value[kCameraFanSlotOfFirstBlade + kCameraFanBladeCount - 1];
  CHECK(last > 0.0F);
  CHECK(last < kCameraFanFirstCeiling + kCameraFanCeilingStep * 9.0F);
}

TEST_CASE("passe la montee, neuf lames retombent et la dixieme tient", "[render][camera_fan]") {
  const std::array<EffectTrack, kEffectTracksPerNode> rise =
      AfterTicks(PatternsOf(kBeginAsura).front(), kTicksToPeak);
  const std::array<EffectTrack, kEffectTracksPerNode> after =
      AfterTicks(PatternsOf(kBeginAsura).front(), kTicksToPeak + 6);
  CHECK(after[0].value[kCameraFanSlotOfFirstBlade] < rise[0].value[kCameraFanSlotOfFirstBlade]);
  const std::size_t last = kCameraFanSlotOfFirstBlade + kCameraFanBladeCount - 1;
  // ⚠ LA DIXIÈME NE TOMBE PAS AVEC LES AUTRES : elle attend le cent quarantième
  // tic de SA piste. C'est elle qui tient l'éventail ouvert.
  CHECK(after[0].value[last] == Approx(rise[0].value[last]));
}

TEST_CASE("le noeud seme porte sa forme, ses trois images et une aire", "[render][camera_fan]") {
  const EffectCameraFanPattern& pattern = PatternsOf(kBeginAsura).front();
  const ro::render::PooledEffectNode node =
      SowEffectCameraFanNode(pattern, {2.0F, 0.5F, 3.0F}, ro::render::kEffectNodeDefaultLifeTicks);
  CHECK(node.shape == ro::render::PooledNodeShape::kCameraFan);
  REQUIRE(node.resource_paths.size() == pattern.trail_count);
  CHECK(node.resource_paths.front() == std::string(pattern.textures.front()));
  // ⚠ LES TROIS IMAGES SONT DISTINCTES : les peindre toutes de la première
  // perdrait la moitié de la figure.
  CHECK(node.resource_paths[0] != node.resource_paths[1]);
  CHECK(node.resource_paths[1] != node.resource_paths[2]);

  // LA FIGURE EST CELLE DE L'ÉVENTAIL, et elle rend une aire une fois les lames
  // ouvertes.
  const std::array<EffectTrack, kEffectTracksPerNode> tracks = AfterTicks(pattern, 50);
  const std::vector<EffectWorldQuad> quads =
      ComposeSpiralFanQuads(tracks[0], kTour, kTilt, node.ground_position);
  CHECK_FALSE(quads.empty());
}

namespace {

constexpr std::uint32_t kSoulLink = 503;
constexpr float kSoulLinkRadius = 5.0F;
constexpr std::uint8_t kSoulLinkMark = 2;
// LA TRAÎNÉE QUI PART TOUT DE SUITE : la quatrième du premier nœud, retard nul.
constexpr std::size_t kTrailWithoutWait = 3;

}  // namespace

TEST_CASE("le soul link allume ses quatre trainees, et les deux noeuds rattrapent",
          "[render][camera_fan]") {
  const std::span<const EffectCameraFanPattern> soul = PatternsOf(kSoulLink);
  const std::array<std::int32_t, kCameraFanMostTrailsPerNode> first_node_delays = {-60, -40, -20,
                                                                                   0};
  const std::array<std::int32_t, kCameraFanMostTrailsPerNode> retards_du_second = {-120, -100, -80,
                                                                                   -60};
  const std::array<float, kCameraFanMostTrailsPerNode> first_node_elevations = {-4.0F, -10.0F,
                                                                                -16.0F, -22.0F};
  const std::array<float, kCameraFanMostTrailsPerNode> cotes_du_second = {22.0F, 16.0F, 10.0F,
                                                                          4.0F};
  CHECK(soul[0].schedule.first_tick == 1);
  CHECK(soul[1].schedule.first_tick == 21);
  const std::array<EffectTrack, kEffectTracksPerNode> first = SeedEffectCameraFanTracks(soul[0]);
  const std::array<EffectTrack, kEffectTracksPerNode> second = SeedEffectCameraFanTracks(soul[1]);
  for (std::size_t trail = 0; trail < kCameraFanMostTrailsPerNode; ++trail) {
    CHECK(first[trail].active);
    CHECK(second[trail].active);
    CHECK(first[trail].ticks == first_node_delays[trail]);
    CHECK(second[trail].ticks == retards_du_second[trail]);
    CHECK(first[trail].value[kCameraFanSlotOfSideways] == first_node_elevations[trail]);
    CHECK(second[trail].value[kCameraFanSlotOfSideways] == cotes_du_second[trail]);
    CHECK(first[trail].radius_at_anchor == kSoulLinkRadius);
    CHECK(first[trail].mark[ro::render::kCameraFanMarkOfStyle] == kSoulLinkMark);
    CHECK(first[trail].mark[kCameraFanMarkOfTexture] == trail);
  }
  CHECK_FALSE(CameraFanFollowsItsOwner(first));

  const ro::render::PooledEffectNode node =
      SowEffectCameraFanNode(soul[1], {}, ro::render::kEffectNodeDefaultLifeTicks);
  REQUIRE(node.resource_paths.size() == kCameraFanMostTrailsPerNode);
  CHECK(node.resource_paths.front().ends_with("soul_k.tga"));
  CHECK(node.resource_paths.back().ends_with("soul_l.tga"));
}

TEST_CASE("la marque deux monte a d'autres plafonds et ne se resserre pas",
          "[render][camera_fan]") {
  const EffectCameraFanPattern& pattern = PatternsOf(kSoulLink).front();
  // DIX-NEUF TICS DE MONTÉE : 247, au-dessus de tous les plafonds.
  const EffectTrack vertex = AfterTicks(pattern, 19)[kTrailWithoutWait];
  for (std::size_t lame = 0; lame < kCameraFanBladeCount; ++lame) {
    CHECK(vertex.value[kCameraFanSlotOfFirstBlade + lame] ==
          Approx(ro::render::kCameraFanTwinFirstCeiling +
                 ro::render::kCameraFanTwinCeilingStep * static_cast<float>(lame)));
  }
  // ⚠ LE RAYON NE SE RESSERRE PAS, là où l'Asura perd un dixième par tic.
  CHECK(vertex.radius_at_anchor == kSoulLinkRadius);

  // PASSÉ LE VINGTIÈME TIC, HUIT LAMES RETOMBENT DE DEUX, et les deux dernières
  // tiennent.
  const EffectTrack chute = AfterTicks(pattern, 20)[kTrailWithoutWait];
  const std::size_t neuvieme =
      kCameraFanSlotOfFirstBlade + ro::render::kCameraFanTwinBladesThatFall;
  CHECK(chute.value[kCameraFanSlotOfFirstBlade] ==
        Approx(vertex.value[kCameraFanSlotOfFirstBlade] - ro::render::kCameraFanTwinFallPerTick));
  CHECK(chute.value[neuvieme] == Approx(vertex.value[neuvieme]));
  CHECK(chute.value[neuvieme + 1] == Approx(vertex.value[neuvieme + 1]));
}

TEST_CASE("apres cent tics, les deux dernieres lames se derobent et le rayon enfle deux fois",
          "[render][camera_fan]") {
  const EffectCameraFanPattern& pattern = PatternsOf(kSoulLink).front();
  const EffectTrack before = AfterTicks(pattern, 100)[kTrailWithoutWait];
  const EffectTrack after = AfterTicks(pattern, 101)[kTrailWithoutWait];
  const std::size_t neuvieme =
      kCameraFanSlotOfFirstBlade + ro::render::kCameraFanTwinBladesThatFall;
  CHECK(before.radius_at_anchor == kSoulLinkRadius);
  CHECK(after.value[neuvieme] ==
        Approx(before.value[neuvieme] - ro::render::kCameraFanTwinLastBladesFall));
  CHECK(after.value[neuvieme + 1] ==
        Approx(before.value[neuvieme + 1] - ro::render::kCameraFanTwinLastBladesFall));
  CHECK(after.radius_at_anchor == Approx(kSoulLinkRadius * ro::render::kCameraFanTwinRadiusGrowth *
                                         ro::render::kCameraFanTwinRadiusGrowth));
  // Les huit autres sont éteintes depuis longtemps.
  CHECK(after.value[kCameraFanSlotOfFirstBlade] == 0.0F);
}
