#include "ro/render/effect_tri_plane.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"
#include "ro/render/effect_tri_plane_seed.h"

using Catch::Approx;
using ro::render::AdvanceDriftingTriPlaneTracks;
using ro::render::AdvanceFlyingTriPlaneTracks;
using ro::render::ComposeTriPlaneQuads;
using ro::render::EffectTrack;
using ro::render::EffectTriPlaneBuilder;
using ro::render::EffectTriPlaneColor;
using ro::render::EffectTriPlaneDraws;
using ro::render::EffectTriPlaneOffsetOf;
using ro::render::EffectTriPlanePattern;
using ro::render::EffectTriPlanePatternsFor;
using ro::render::EffectTriPlanePatternTable;
using ro::render::EffectWorldQuad;
using ro::render::kDriftingTriPlaneStep;
using ro::render::kDriftingTriPlaneTracks;
using ro::render::kEffectTracksPerNode;
using ro::render::kFlyingTriPlaneAlpha;
using ro::render::kFlyingTriPlaneSowTick;
using ro::render::kFlyingTriPlaneStep;
using ro::render::kTriPlaneBlue;
using ro::render::kTriPlaneBlueColor;
using ro::render::kTriPlaneDeepPinkColor;
using ro::render::kTriPlanePerPlane;
using ro::render::kTriPlanePerPlaneColor;
using ro::render::kTriPlanePlanes;
using ro::render::kTriPlaneSlotOfColor;
using ro::render::kTriPlaneSlotOfFirstAlpha;
using ro::render::SeedEffectTriPlaneTracks;
using ro::render::SetEffectTriPlaneOffset;
using ro::render::SowEffectTriPlaneNode;

namespace {

// UNE PISTE VIVE, caméra à plat : les quatre coins du carré restent à hauteur
// nulle, si bien que la garde ne dépend que du décalage.
[[nodiscard]] EffectTrack Plans(float tint = kTriPlaneBlueColor) {
  EffectTrack track;
  track.active = true;
  track.radius_at_anchor = 2.0F;
  track.lift_key = kDriftingTriPlaneStep;
  track.value[kTriPlaneSlotOfColor] = tint;
  for (std::size_t plan = 0; plan < kTriPlanePlanes; ++plan) {
    track.value[kTriPlaneSlotOfFirstAlpha + plan] = 100.0F;
    SetEffectTriPlaneOffset(track, plan, glm::vec3{0.0F, 0.0F, 0.0F});
  }
  return track;
}

[[nodiscard]] const EffectTriPlanePattern& PatternOf(std::uint32_t effect_id) {
  const std::span<const EffectTriPlanePattern> patterns = EffectTriPlanePatternsFor(effect_id);
  REQUIRE(patterns.size() == 1);
  return patterns.front();
}

}  // namespace

TEST_CASE("une piste rend TROIS plans", "[render][tri_plane]") {
  const std::vector<EffectWorldQuad> quads = ComposeTriPlaneQuads(Plans(), 0, 0, {});
  CHECK(quads.size() == kTriPlanePlanes);
}

TEST_CASE("un plan passe sous le noeud se tait", "[render][tri_plane]") {
  // ⚠ LA GARDE PORTE SUR LE PREMIER COIN plus le décalage : un plan dont le
  // décalage est POSITIF dans le repère du client est passé sous le nœud.
  EffectTrack track = Plans();
  SetEffectTriPlaneOffset(track, 1, glm::vec3{0.0F, 1.0F, 0.0F});
  CHECK(ComposeTriPlaneQuads(track, 0, 0, {}).size() == kTriPlanePlanes - 1);
}

TEST_CASE("une piste eteinte ou d'une teinte inconnue ne rend rien", "[render][tri_plane]") {
  EffectTrack faded = Plans();
  faded.active = false;
  CHECK(ComposeTriPlaneQuads(faded, 0, 0, {}).empty());
  // ⚠ LE CLIENT COMPARE À CINQ VALEURS EXACTES : une teinte ENTRE DEUX ne
  // dessine rien non plus.
  CHECK(ComposeTriPlaneQuads(Plans(2.5F), 0, 0, {}).empty());
  CHECK(ComposeTriPlaneQuads(Plans(5.0F), 0, 0, {}).empty());
  CHECK(EffectTriPlaneDraws(kTriPlaneDeepPinkColor));
  CHECK_FALSE(EffectTriPlaneDraws(2.5F));
}

TEST_CASE("la cinquieme teinte change avec le plan", "[render][tri_plane]") {
  const std::vector<EffectWorldQuad> quads =
      ComposeTriPlaneQuads(Plans(kTriPlanePerPlaneColor), 0, 0, {});
  REQUIRE(quads.size() == kTriPlanePlanes);
  for (std::size_t plan = 0; plan < kTriPlanePlanes; ++plan) {
    CHECK(quads[plan].tint[0] == kTriPlanePerPlane[plan][0]);
  }
  // ⚠ ET LES QUATRE AUTRES NE CHANGENT PAS : c'est la seule du lot à le faire.
  const std::vector<EffectWorldQuad> unies = ComposeTriPlaneQuads(Plans(), 0, 0, {});
  for (const EffectWorldQuad& quad : unies) {
    CHECK(quad.tint[0] == kTriPlaneBlue[0]);
  }
  CHECK(EffectTriPlaneColor(kTriPlanePerPlaneColor, 2) == kTriPlanePerPlane[2]);
}

TEST_CASE("l'alpha est par PLAN et non par piste", "[render][tri_plane]") {
  EffectTrack track = Plans();
  track.value[kTriPlaneSlotOfFirstAlpha] = 30.0F;
  track.value[kTriPlaneSlotOfFirstAlpha + 1] = 200.0F;
  track.value[kTriPlaneSlotOfFirstAlpha + 2] = -5.0F;
  const std::vector<EffectWorldQuad> quads = ComposeTriPlaneQuads(track, 0, 0, {});
  REQUIRE(quads.size() == kTriPlanePlanes);
  CHECK(quads[0].tint[3] == 30);
  CHECK(quads[1].tint[3] == 200);
  // Un alpha négatif se borne à zéro, comme le client le borne.
  CHECK(quads[2].tint[3] == 0);
}

TEST_CASE("le deuxieme plan range son decalage dans l'ANCRE", "[render][tri_plane]") {
  EffectTrack track = Plans();
  SetEffectTriPlaneOffset(track, 1, glm::vec3{3.0F, -1.0F, 4.0F});
  CHECK(track.anchor.x == Approx(3.0F));
  CHECK(EffectTriPlaneOffsetOf(track, 1).z == Approx(4.0F));
  // Les deux autres vivent dans la queue, aux cases trois et six.
  SetEffectTriPlaneOffset(track, 0, glm::vec3{1.0F, 0.0F, 0.0F});
  SetEffectTriPlaneOffset(track, 2, glm::vec3{2.0F, 0.0F, 0.0F});
  CHECK(track.tail[3] == Approx(1.0F));
  CHECK(track.tail[6] == Approx(2.0F));
}

TEST_CASE("la derive ne fait monter QUE le premier plan", "[render][tri_plane]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  tracks[0] = Plans();
  std::uint32_t seed = 11U;
  for (std::int32_t tick = 0; tick < 3; ++tick) {
    AdvanceDriftingTriPlaneTracks(tracks, 0, 0, seed);
  }
  // ⚠ LE PREMIER MONTE, LE DEUXIÈME DESCEND, ET LE TROISIÈME NE BOUGE PAS.
  CHECK(EffectTriPlaneOffsetOf(tracks[0], 0).y == Approx(-3.0F * kDriftingTriPlaneStep));
  CHECK(EffectTriPlaneOffsetOf(tracks[0], 1).y == Approx(3.0F * kDriftingTriPlaneStep));
  CHECK(EffectTriPlaneOffsetOf(tracks[0], 2).y == Approx(0.0F));
  // ⚠ ET SEUL LE PREMIER DÉRIVE DE CÔTÉ : la garde des deux autres n'est
  // jamais vraie.
  CHECK(EffectTriPlaneOffsetOf(tracks[0], 1).x == Approx(0.0F));
  CHECK(EffectTriPlaneOffsetOf(tracks[0], 2).x == Approx(0.0F));
}

TEST_CASE("le premier plan derive dans le sens de ses angles", "[render][tri_plane]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  tracks[0] = Plans();
  // Déjà parti : sa garde est vraie dès le premier tic.
  SetEffectTriPlaneOffset(tracks[0], 0, glm::vec3{0.0F, -1.0F, 0.0F});
  // Un quart de tour : la poursuite n'en retire que deux degrés, et le sinus
  // reste presque plein.
  tracks[0].value[0] = 90.0F;
  tracks[0].value[1] = 90.0F;
  tracks[0].value[2] = 90.0F;
  tracks[0].value[3] = 90.0F;
  std::uint32_t seed = 5U;
  AdvanceDriftingTriPlaneTracks(tracks, 0, 0, seed);
  CHECK(EffectTriPlaneOffsetOf(tracks[0], 0).x > 0.0F);
  CHECK(EffectTriPlaneOffsetOf(tracks[0], 0).z > 0.0F);
}

TEST_CASE("l'alpha de la derive monte dix tics puis tombe apres trente", "[render][tri_plane]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  tracks[0] = Plans();
  for (std::size_t plan = 0; plan < kTriPlanePlanes; ++plan) {
    tracks[0].value[kTriPlaneSlotOfFirstAlpha + plan] = 0.0F;
  }
  std::uint32_t seed = 3U;
  for (std::int32_t tick = 0; tick < 9; ++tick) {
    AdvanceDriftingTriPlaneTracks(tracks, 0, 0, seed);
  }
  CHECK(tracks[0].value[kTriPlaneSlotOfFirstAlpha] == Approx(180.0F));
  // Du dixième au trentième, plateau.
  for (std::int32_t tick = 9; tick < 30; ++tick) {
    AdvanceDriftingTriPlaneTracks(tracks, 0, 0, seed);
  }
  CHECK(tracks[0].value[kTriPlaneSlotOfFirstAlpha] == Approx(180.0F));
  AdvanceDriftingTriPlaneTracks(tracks, 0, 0, seed);
  CHECK(tracks[0].value[kTriPlaneSlotOfFirstAlpha] == Approx(177.0F));
}

TEST_CASE("les angles de camera se rangent dans les styles zero et un", "[render][tri_plane]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  tracks[0] = Plans();
  std::uint32_t seed = 1U;
  // 🔴 TELS QU'ILS ARRIVENT : ils sortent déjà de `EffectBillboard*Degrees`, qui
  // fait le calcul du client. Le refaire tournait la figure à l'envers de la
  // caméra. Voir `StoreEffectBillboardAngles`.
  AdvanceDriftingTriPlaneTracks(tracks, 30, 60, seed);
  CHECK(tracks[0].style == 30);
  CHECK(tracks[1].style == 60);
}

TEST_CASE("le vol suit un cap et une pente fixes", "[render][tri_plane]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  tracks[0] = Plans();
  tracks[0].lift_key = kFlyingTriPlaneStep;
  // Cap plein est, pente nulle : le plan file sur l'axe des z.
  tracks[0].value[0] = 0.0F;
  tracks[0].value[2] = 0.0F;
  // Pente d'un quart de tour : le deuxième monte tout droit.
  tracks[0].value[4] = 0.0F;
  tracks[0].value[6] = 90.0F;
  AdvanceFlyingTriPlaneTracks(tracks, 0, 0);
  CHECK(EffectTriPlaneOffsetOf(tracks[0], 0).z == Approx(kFlyingTriPlaneStep));
  CHECK(EffectTriPlaneOffsetOf(tracks[0], 0).y == Approx(0.0F).margin(1e-5));
  CHECK(EffectTriPlaneOffsetOf(tracks[0], 1).y == Approx(-kFlyingTriPlaneStep));
  // ⚠ LES ANGLES NE BOUGENT PAS : ce mouvement ne poursuit rien.
  CHECK(tracks[0].value[0] == Approx(0.0F));
  CHECK(tracks[0].value[6] == Approx(90.0F));
}

TEST_CASE("le vol ralentit et s'eteint apres le vingt-cinquieme tic", "[render][tri_plane]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  tracks[0] = Plans();
  tracks[0].lift_key = kFlyingTriPlaneStep;
  for (std::size_t plan = 0; plan < kTriPlanePlanes; ++plan) {
    tracks[0].value[kTriPlaneSlotOfFirstAlpha + plan] = kFlyingTriPlaneAlpha;
  }
  for (std::int32_t tick = 0; tick < 25; ++tick) {
    AdvanceFlyingTriPlaneTracks(tracks, 0, 0);
  }
  CHECK(tracks[0].lift_key == Approx(kFlyingTriPlaneStep));
  CHECK(tracks[0].value[kTriPlaneSlotOfFirstAlpha] == Approx(kFlyingTriPlaneAlpha));
  AdvanceFlyingTriPlaneTracks(tracks, 0, 0);
  CHECK(tracks[0].lift_key == Approx(kFlyingTriPlaneStep * 0.9F));
  CHECK(tracks[0].value[kTriPlaneSlotOfFirstAlpha] == Approx(kFlyingTriPlaneAlpha - 6.0F));
}

TEST_CASE("la table porte les quatre identifiants des deux sous-types", "[render][tri_plane]") {
  const std::span<const EffectTriPlanePattern> table = EffectTriPlanePatternTable();
  CHECK(table.size() == 4);
  CHECK(std::ranges::is_sorted(table, {}, &EffectTriPlanePattern::effect_id));
  CHECK(PatternOf(506).builder == EffectTriPlaneBuilder::kDrifting);
  CHECK(PatternOf(850).builder == EffectTriPlaneBuilder::kDrifting);
  CHECK(PatternOf(932).builder == EffectTriPlaneBuilder::kDrifting);
  CHECK(PatternOf(518).builder == EffectTriPlaneBuilder::kFlying);
  // ⚠ ET LE VOL ATTEND LE TRENTIÈME TIC.
  CHECK(PatternOf(518).schedule.first_tick == kFlyingTriPlaneSowTick);
  CHECK(PatternOf(506).schedule.first_tick == 0);
  CHECK(EffectTriPlanePatternsFor(1).empty());
}

TEST_CASE("la derive n'allume que DEUX pistes, le vol QUATRE", "[render][tri_plane]") {
  std::uint32_t seed = 7U;
  const std::array<EffectTrack, kEffectTracksPerNode> derive =
      SeedEffectTriPlaneTracks(PatternOf(506), seed);
  CHECK(derive[0].active);
  CHECK(derive[kDriftingTriPlaneTracks - 1].active);
  CHECK_FALSE(derive[kDriftingTriPlaneTracks].active);
  CHECK_FALSE(derive[kEffectTracksPerNode - 1].active);

  std::uint32_t other = 7U;
  const std::array<EffectTrack, kEffectTracksPerNode> vol =
      SeedEffectTriPlaneTracks(PatternOf(518), other);
  for (const EffectTrack& track : vol) {
    CHECK(track.active);
  }
}

TEST_CASE("le semis pose les decalages a ZERO et les teintes mesurees", "[render][tri_plane]") {
  std::uint32_t seed = 13U;
  const std::array<EffectTrack, kEffectTracksPerNode> bleue =
      SeedEffectTriPlaneTracks(PatternOf(506), seed);
  // ⚠ LES TROIS DÉCALAGES PARTENT DE ZÉRO, et ce zéro est celui du
  // constructeur du client — pas un arbitrage.
  for (std::size_t plan = 0; plan < kTriPlanePlanes; ++plan) {
    CHECK(EffectTriPlaneOffsetOf(bleue[0], plan) == glm::vec3{0.0F, 0.0F, 0.0F});
    CHECK(bleue[0].value[kTriPlaneSlotOfFirstAlpha + plan] == Approx(0.0F));
  }
  CHECK(bleue[0].value[kTriPlaneSlotOfColor] == Approx(kTriPlaneBlueColor));

  std::uint32_t two = 13U;
  CHECK(SeedEffectTriPlaneTracks(PatternOf(850), two)[0].value[kTriPlaneSlotOfColor] ==
        Approx(kTriPlanePerPlaneColor));

  // ⚠ LE TROISIÈME OCTET TIRE SA TEINTE entre un et trois.
  std::uint32_t three = 13U;
  const float tiree =
      SeedEffectTriPlaneTracks(PatternOf(932), three)[0].value[kTriPlaneSlotOfColor];
  CHECK(tiree >= 1.0F);
  CHECK(tiree <= 3.0F);

  // Et le vol naît déjà lumineux.
  std::uint32_t four = 13U;
  const std::array<EffectTrack, kEffectTracksPerNode> vol =
      SeedEffectTriPlaneTracks(PatternOf(518), four);
  CHECK(vol[0].value[kTriPlaneSlotOfFirstAlpha] == Approx(kFlyingTriPlaneAlpha));
  CHECK(vol[0].lift_key == Approx(kFlyingTriPlaneStep));
}

TEST_CASE("le noeud seme porte sa forme et son image", "[render][tri_plane]") {
  std::uint32_t seed = 2U;
  const ro::render::PooledEffectNode derive =
      SowEffectTriPlaneNode(PatternOf(506), {1.0F, 0.0F, 1.0F}, 300, seed);
  CHECK(derive.shape == ro::render::PooledNodeShape::kDriftingTriPlane);
  REQUIRE(derive.resource_paths.size() == 1);
  CHECK(derive.resource_paths.front() == std::string(PatternOf(506).texture));

  std::uint32_t other = 2U;
  const ro::render::PooledEffectNode vol =
      SowEffectTriPlaneNode(PatternOf(518), {1.0F, 0.0F, 1.0F}, 300, other);
  CHECK(vol.shape == ro::render::PooledNodeShape::kFlyingTriPlane);
  CHECK(vol.node.life_ticks == 300);
}
