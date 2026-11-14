#include "ro/render/effect_double_ribbon.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_double_ribbon_seed.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_track_motion.h"
#include "ro/render/effect_trail_palette.h"

#include "support/effect_bearers.h"

using Catch::Approx;
using ro::render::AdvanceRadialFanTracks;
using ro::render::ComposeDoubleRibbonQuads;
using ro::render::EffectDoubleRibbonPattern;
using ro::render::EffectDoubleRibbonPatternsFor;
using ro::render::EffectDoubleRibbonPatternTable;
using ro::render::EffectTrack;
using ro::render::EffectTrailVariantFor;
using ro::render::EffectWorldQuad;
using ro::render::kDoubleRibbonLiftKey;
using ro::render::kDoubleRibbonOpeningPerTrack;
using ro::render::kDoubleRibbonRadius;
using ro::render::kDoubleRibbonSecondNodeDelayTicks;
using ro::render::kDoubleRibbonSegments;
using ro::render::kDoubleRibbonTrailVariant;
using ro::render::kDoubleRibbonTurnPerTrack;
using ro::render::kEffectTracksPerNode;
using ro::render::kSlotOfDoubleRibbonOpening;
using ro::render::PooledNodeShape;
using ro::render::SeedEffectDoubleRibbonTracks;
using ro::render::SowEffectDoubleRibbonNode;

namespace {

constexpr std::uint32_t kAttackEnergy2 = 438;
constexpr glm::vec3 kAtOrigin{0.0F, 0.0F, 0.0F};

[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> Advance(std::int32_t count,
                                                                    std::int32_t delay = 0) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks = SeedEffectDoubleRibbonTracks(delay);
  for (std::int32_t tick = 0; tick < count; ++tick) {
    AdvanceRadialFanTracks(tracks, 0.0F);
  }
  return tracks;
}

}  // namespace

TEST_CASE("l'aura naît eteinte et ne rend rien", "[render][double_ribbon]") {
  const std::array<EffectTrack, kEffectTracksPerNode> tracks = SeedEffectDoubleRibbonTracks(0);
  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    CHECK(tracks[rang].active);
    CHECK(tracks[rang].alpha == 0);
    CHECK(tracks[rang].radius_at_anchor == Approx(kDoubleRibbonRadius));
    CHECK(tracks[rang].lift_key == Approx(kDoubleRibbonLiftKey));
    CHECK(tracks[rang].degrees == 360);
    // LES DEUX ÉCHELONNEMENTS : le tour et l'ouverture.
    CHECK(tracks[rang].tilt_degrees ==
          static_cast<std::int16_t>(kDoubleRibbonTurnPerTrack * static_cast<std::int16_t>(rang)));
    CHECK(tracks[rang].value[kSlotOfDoubleRibbonOpening] ==
          Approx(kDoubleRibbonOpeningPerTrack * static_cast<float>(rang)));
  }
  // ⚠ UNE PISTE D'ALPHA NUL NE REND RIEN : rien ne se voit au premier tic.
  CHECK(ComposeDoubleRibbonQuads(tracks.front(), kAtOrigin, kDoubleRibbonTrailVariant).empty());
}

TEST_CASE("l'anneau s'ouvre puis recommence", "[render][double_ribbon]") {
  // La première piste part d'une ouverture nulle : elle monte d'un degré par tic.
  const std::array<EffectTrack, kEffectTracksPerNode> a_cinq = Advance(5);
  CHECK(a_cinq.front().value[kSlotOfDoubleRibbonOpening] == Approx(5.0F));
  CHECK(a_cinq.front().alpha == 50);

  // ⚠ L'ALPHA PLAFONNE À SOIXANTE, et bien avant que l'anneau soit ouvert.
  const std::array<EffectTrack, kEffectTracksPerNode> a_vingt = Advance(20);
  CHECK(a_vingt.front().value[kSlotOfDoubleRibbonOpening] == Approx(20.0F));
  CHECK(a_vingt.front().alpha == 60);

  // ⚠ AU QUART DE TOUR, L'ANNEAU REPART DE ZÉRO et l'alpha de dix. Le
  // quatre-vingt-dixième tic pose l'ouverture à quatre-vingt-dix ; le
  // quatre-vingt-onzième la remet à zéro.
  const std::array<EffectTrack, kEffectTracksPerNode> au_quart = Advance(90);
  CHECK(au_quart.front().value[kSlotOfDoubleRibbonOpening] == Approx(90.0F));
  const std::array<EffectTrack, kEffectTracksPerNode> after = Advance(91);
  CHECK(after.front().value[kSlotOfDoubleRibbonOpening] == Approx(0.0F));
}

TEST_CASE("le plafond descend au lieu de soustraire", "[render][double_ribbon]") {
  // Au trente et unième tic, le plafond vaut (45 - 31) * 4 = 56 : l'alpha, qui
  // stagnait à soixante, y tombe.
  const std::array<EffectTrack, kEffectTracksPerNode> tracks = Advance(31);
  CHECK(tracks.front().alpha == 56);
  // ⚠ ET AU QUARANTE-CINQUIÈME LE PLAFOND EST NUL : la piste reste VIVE mais ne
  // rend plus rien, faute d'alpha. Ce mouvement n'éteint jamais une piste.
  const std::array<EffectTrack, kEffectTracksPerNode> a_la_fin = Advance(45);
  CHECK(a_la_fin.front().alpha == 0);
  CHECK(a_la_fin.front().active);
  CHECK(ComposeDoubleRibbonQuads(a_la_fin.front(), kAtOrigin, kDoubleRibbonTrailVariant).empty());
}

TEST_CASE("le retard du second nœud le décale de quatre tics", "[render][double_ribbon]") {
  const std::array<EffectTrack, kEffectTracksPerNode> a_l_heure = Advance(10);
  const std::array<EffectTrack, kEffectTracksPerNode> delayed =
      Advance(10, kDoubleRibbonSecondNodeDelayTicks);
  CHECK(a_l_heure.front().ticks == 10);
  CHECK(delayed.front().ticks == 6);
  CHECK(delayed.front().value[kSlotOfDoubleRibbonOpening] == Approx(6.0F));
}

TEST_CASE("l'anneau tourne au-dessus du sol", "[render][double_ribbon]") {
  // Son centre est élevé de la clé : le client la retranche dans son repère
  // qui descend. La retrancher dans le nôtre enterrait l'anneau.
  const std::array<EffectTrack, kEffectTracksPerNode> tracks = Advance(10);
  const std::vector<EffectWorldQuad> ribbon =
      ComposeDoubleRibbonQuads(tracks.front(), kAtOrigin, kDoubleRibbonTrailVariant);
  REQUIRE_FALSE(ribbon.empty());
  bool au_dessus = false;
  for (const EffectWorldQuad& quad : ribbon) {
    for (const glm::vec3& corner : quad.corners) {
      CHECK(corner.y >= -0.001F);
      au_dessus = au_dessus || corner.y > 1.0F;
    }
  }
  CHECK(au_dessus);
}

TEST_CASE("chaque segment est peint deux fois", "[render][double_ribbon]") {
  const std::array<EffectTrack, kEffectTracksPerNode> tracks = Advance(10);
  const std::vector<EffectWorldQuad> ribbon =
      ComposeDoubleRibbonQuads(tracks.front(), kAtOrigin, kDoubleRibbonTrailVariant);
  // Vingt segments, deux quads chacun.
  REQUIRE(ribbon.size() == static_cast<std::size_t>(2 * kDoubleRibbonSegments));
  // ⚠ LES DEUX SONT IDENTIQUES : c'est le mélange additif qui en fait un double.
  for (std::size_t rang = 0; rang < ribbon.size(); rang += 2) {
    CHECK(ribbon[rang].corners[0] == ribbon[rang + 1].corners[0]);
    CHECK(ribbon[rang].corners[2] == ribbon[rang + 1].corners[2]);
    CHECK(ribbon[rang].tint == ribbon[rang + 1].tint);
  }
  // LA TEINTE VIENT DE LA PALETTE DE LA VOIE B, pas de la figure.
  const auto tint = EffectTrailVariantFor(kDoubleRibbonTrailVariant);
  CHECK(ribbon.front().tint[0] == tint.color[0]);
  CHECK(ribbon.front().tint[1] == tint.color[1]);
  CHECK(ribbon.front().tint[2] == tint.color[2]);
  CHECK(ribbon.front().tint[3] == static_cast<std::uint8_t>(tracks.front().alpha));
  CHECK(ribbon.front().blend_destination == tint.blend_destination);
}

TEST_CASE("le centre glisse pendant que l'anneau s'ouvre", "[render][double_ribbon]") {
  // À une ouverture d'un degré l'anneau est presque un point, et son centre est
  // porté de trois plus la clé entière sur l'axe. À quatre-vingt-dix degrés il
  // est plein, et le glissement a disparu.
  //
  // ⚠ LES DEUX ÉTATS VIENNENT DE PISTES DIFFÉRENTES, et il le faut : la première
  // met quatre-vingt-dix tics à s'ouvrir, et son alpha est nul bien avant. La
  // dernière part d'une ouverture de soixante-six et y arrive en vingt-quatre.
  const std::array<EffectTrack, kEffectTracksPerNode> tot = Advance(1);
  const std::array<EffectTrack, kEffectTracksPerNode> late = Advance(24);
  CHECK(late.back().value[kSlotOfDoubleRibbonOpening] == Approx(90.0F));
  const std::vector<EffectWorldQuad> narrow =
      ComposeDoubleRibbonQuads(tot.front(), kAtOrigin, kDoubleRibbonTrailVariant);
  const std::vector<EffectWorldQuad> large =
      ComposeDoubleRibbonQuads(late.back(), kAtOrigin, kDoubleRibbonTrailVariant);
  REQUIRE_FALSE(narrow.empty());
  REQUIRE_FALSE(large.empty());
  // L'azimut vaut cent quatre-vingts, donc l'axe perpendiculaire est la cote.
  CHECK(tot.front().azimuth_degrees == 180);
  CHECK(std::abs(narrow.front().corners[0].z) > std::abs(large.front().corners[0].z));
}

TEST_CASE("un balayage trop court ne fige pas", "[render][double_ribbon]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks = Advance(10);
  tracks.front().degrees = 19;
  CHECK(ComposeDoubleRibbonQuads(tracks.front(), kAtOrigin, kDoubleRibbonTrailVariant).empty());
  tracks.front().degrees = -1;
  CHECK(ComposeDoubleRibbonQuads(tracks.front(), kAtOrigin, kDoubleRibbonTrailVariant).empty());
}

TEST_CASE("la table des auras porte son unique effet", "[render][double_ribbon]") {
  const std::span<const EffectDoubleRibbonPattern> table = EffectDoubleRibbonPatternTable();
  CHECK(table.size() == 1);
  CHECK(EffectDoubleRibbonPatternsFor(kAttackEnergy2).size() == 1);
  CHECK(EffectDoubleRibbonPatternsFor(1U).empty());

  const ro::render::PooledEffectNode node =
      SowEffectDoubleRibbonNode(table.front(), glm::vec3{1.0F, 2.0F, 3.0F}, 0, 60);
  CHECK(node.shape == PooledNodeShape::kDoubleRibbon);
  CHECK(node.trail_variant == kDoubleRibbonTrailVariant);
  CHECK(node.node.life_ticks == 60);
  CHECK(node.resource_paths.size() == 1);
}

TEST_CASE("l'aura relit l'angle de son porteur a chaque tic", "[render][double_ribbon]") {
  // ⚠ LE MOUVEMENT REFAIT L'AZIMUT DEPUIS LE CORPS DU PORTEUR À CHAQUE TIC : un
  // porteur qui se tourne emporte l'anneau, au lieu de le laisser à l'angle de
  // sa naissance.
  constexpr std::uint32_t kBearer = 77;
  constexpr float kFirstBodyDegrees = 30.0F;
  constexpr float kSecondBodyDegrees = 120.0F;
  ro::render::EffectNodePool pool;
  pool.AnchorNextTo(kBearer, kAtOrigin);
  pool.Add(SowEffectDoubleRibbonNode(EffectDoubleRibbonPatternTable().front(), kAtOrigin, 0, 60));
  ro::test_support::MoveBearer(pool, kBearer, kAtOrigin, kFirstBodyDegrees);
  REQUIRE(pool.nodes()[0].owner_facing_degrees == Approx(kFirstBodyDegrees));
  ro::test_support::MoveBearer(pool, kBearer, kAtOrigin, kSecondBodyDegrees);
  REQUIRE(pool.nodes()[0].owner_facing_degrees == Approx(kSecondBodyDegrees));
}
