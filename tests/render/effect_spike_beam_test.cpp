#include "ro/render/effect_spike_beam.h"

#include <set>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_spike_beam_seed.h"
#include "ro/render/effect_track_motion.h"

using Catch::Approx;
using ro::render::AdvanceFourLayerTimerTracks;
using ro::render::ComposeSpikeBeamQuads;
using ro::render::EffectSpikeBeamPattern;
using ro::render::EffectSpikeBeamPatternsFor;
using ro::render::EffectSpikeBeamPatternTable;
using ro::render::EffectTrack;
using ro::render::EffectWorldQuad;
using ro::render::kEffectTrackFullTurnDegrees;
using ro::render::kEffectTracksPerNode;
using ro::render::kPieWedgeDegreesPerStep;
using ro::render::kPieWedgeSowingTicks;
using ro::render::kSlotOfSpikeBeamImage;
using ro::render::kSpikeBeamFirstReach;
using ro::render::kSpikeBeamMaxSegments;
using ro::render::kSpikeBeamStep;
using ro::render::PooledNodeShape;
using ro::render::SeedEffectSpikeBeamTracks;
using ro::render::SowEffectSpikeBeamNode;

namespace {

constexpr std::uint32_t kCharge3 = 897;
constexpr glm::vec3 kAtOrigin{0.0F, 0.0F, 0.0F};

}  // namespace

TEST_CASE("douze faisceaux ferment le tour en trois tics", "[render][spike_beam]") {
  std::set<std::int16_t> inclinaisons;
  for (std::int32_t tick = 0; tick < kPieWedgeSowingTicks; ++tick) {
    for (const EffectTrack& track : SeedEffectSpikeBeamTracks(tick)) {
      CHECK(track.active);
      inclinaisons.insert(track.tilt_degrees);
    }
  }
  // LA MÊME DISPERSION QUE LES QUARTIERS DE TARTE : les deux bâtisseurs
  // partagent leur horaire.
  CHECK(inclinaisons.size() == 12);
  CHECK(*inclinaisons.rbegin() == kEffectTrackFullTurnDegrees - kPieWedgeDegreesPerStep);
}

TEST_CASE("au premier tic, un seul segment tient", "[render][spike_beam]") {
  const EffectTrack track = SeedEffectSpikeBeamTracks(0).front();
  // ⚠ LE RAYON DE DÉPART VAUT EXACTEMENT LA PORTÉE MAXIMALE : le premier segment
  // y tient tout juste, le second la dépasse.
  REQUIRE(track.radius_at_anchor == Approx(kSpikeBeamFirstReach));
  CHECK(ComposeSpikeBeamQuads(track, kAtOrigin).size() == 1);
}

TEST_CASE("le faisceau file cinq unites AU-DESSUS du sol", "[render][spike_beam]") {
  // La section est deux paires opposees : son milieu est a la hauteur du
  // semis, que le client ecrit NEGATIVE dans son repere qui descend. Recopiee,
  // elle faisait filer le faisceau sous le sol.
  const EffectTrack track = SeedEffectSpikeBeamTracks(0).front();
  const std::vector<EffectWorldQuad> quads = ComposeSpikeBeamQuads(track, kAtOrigin);
  REQUIRE_FALSE(quads.empty());
  float somme = 0.0F;
  for (const glm::vec3& corner : quads.front().corners) {
    somme += corner.y;
  }
  CHECK(somme / 4.0F == Approx(5.0F).margin(0.01F));
}

TEST_CASE("le faisceau grandit en reculant", "[render][spike_beam]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks = SeedEffectSpikeBeamTracks(0);
  std::size_t before = ComposeSpikeBeamQuads(tracks.front(), kAtOrigin).size();
  // ⚠ IL GAGNE UN SEGMENT CHAQUE FOIS QUE SON DÉPART DESCEND D'UN PAS, et le pas
  // est la clé — quinze. Quinze tics après, il en a deux.
  for (std::int32_t tick = 0; tick < static_cast<std::int32_t>(kSpikeBeamStep); ++tick) {
    AdvanceFourLayerTimerTracks(tracks, 1);
  }
  const std::size_t after = ComposeSpikeBeamQuads(tracks.front(), kAtOrigin).size();
  CHECK(after == before + 1);

  // ET IL PLAFONNE À SIX, jamais plus : c'est la borne de la boucle du client.
  for (std::int32_t tick = 0; tick < 200; ++tick) {
    AdvanceFourLayerTimerTracks(tracks, 1);
    before = std::max(before, ComposeSpikeBeamQuads(tracks.front(), kAtOrigin).size());
  }
  CHECK(before <= kSpikeBeamMaxSegments);
}

TEST_CASE("le depart ne file jamais sous zero", "[render][spike_beam]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks = SeedEffectSpikeBeamTracks(0);
  for (std::int32_t tick = 0; tick < 500; ++tick) {
    AdvanceFourLayerTimerTracks(tracks, 1);
    // ⚠ LE RECUL SE REMONTE D'UN PAS QUAND IL PASSE SOUS ZÉRO : sans cela le
    // faisceau partirait derrière son ancre et n'en reviendrait pas.
    CHECK(tracks.front().radius_at_anchor >= 0.0F);
    CHECK(tracks.front().radius_at_anchor <= kSpikeBeamFirstReach);
  }
}

TEST_CASE("l'indice d'image tourne avec le compte du noeud", "[render][spike_beam]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks = SeedEffectSpikeBeamTracks(0);
  // ⚠ UN NŒUD À UNE SEULE IMAGE GARDE L'INDICE ZÉRO : le client compare par
  // « supérieur ou égal », si bien qu'un indice de un revient aussitôt.
  for (std::int32_t tick = 0; tick < 10; ++tick) {
    AdvanceFourLayerTimerTracks(tracks, 1);
    CHECK(tracks.front().value[kSlotOfSpikeBeamImage] == Approx(0.0F));
  }

  std::array<EffectTrack, kEffectTracksPerNode> three = SeedEffectSpikeBeamTracks(0);
  std::set<int> seen;
  for (std::int32_t tick = 0; tick < 9; ++tick) {
    AdvanceFourLayerTimerTracks(three, 3);
    seen.insert(static_cast<int>(three.front().value[kSlotOfSpikeBeamImage]));
  }
  CHECK(seen == std::set<int>{0, 1, 2});
}

TEST_CASE("la section est deux paires diametralement opposees", "[render][spike_beam]") {
  EffectTrack track = SeedEffectSpikeBeamTracks(0).front();
  track.tilt_degrees = 0;
  const std::vector<EffectWorldQuad> faisceau = ComposeSpikeBeamQuads(track, kAtOrigin);
  REQUIRE(faisceau.size() == 1);
  const std::array<glm::vec3, 4>& corners = faisceau.front().corners;

  // ⚠ LES COINS ZÉRO ET DEUX SONT OPPOSÉS, comme les coins un et trois : deux
  // cents contre vingt, moins vingt contre cent soixante. Leur milieu tombe donc
  // au même point, et c'est la seule facon de le verifier sans refaire le calcul.
  const glm::vec3 middle_a = (corners[0] + corners[2]) * 0.5F;
  const glm::vec3 middle_b = (corners[1] + corners[3]) * 0.5F;
  CHECK(middle_a.x == Approx(middle_b.x).margin(0.001F));
  CHECK(middle_a.z == Approx(middle_b.z).margin(0.001F));

  // ⚠ ET LE RUBAN EST PLAT : les quatre coins ne partagent pas leur ordonnée —
  // elle dépend de l'angle — mais aucune ne dépend du SEGMENT.
  track.radius_at_anchor = 10.0F;
  const std::vector<EffectWorldQuad> long_faisceau = ComposeSpikeBeamQuads(track, kAtOrigin);
  REQUIRE(long_faisceau.size() > 1);
  for (std::size_t corner = 0; corner < 4; ++corner) {
    CHECK(long_faisceau.front().corners[corner].y ==
          Approx(long_faisceau.back().corners[corner].y));
  }
}

TEST_CASE("une piste eteinte ou pale ne rend rien", "[render][spike_beam]") {
  EffectTrack faded = SeedEffectSpikeBeamTracks(0).front();
  faded.active = false;
  CHECK(ComposeSpikeBeamQuads(faded, kAtOrigin).empty());
  EffectTrack pale = SeedEffectSpikeBeamTracks(0).front();
  pale.alpha = 0;
  CHECK(ComposeSpikeBeamQuads(pale, kAtOrigin).empty());
}

TEST_CASE("le noeud seme porte sa forme et son image", "[render][spike_beam]") {
  const std::span<const EffectSpikeBeamPattern> table = EffectSpikeBeamPatternTable();
  REQUIRE_FALSE(table.empty());
  CHECK(EffectSpikeBeamPatternsFor(kCharge3).size() == 1);

  const ro::render::PooledEffectNode node =
      SowEffectSpikeBeamNode(table.front(), glm::vec3{1.0F, 2.0F, 3.0F}, 2, 60);
  CHECK(node.shape == PooledNodeShape::kSpikeBeam);
  CHECK(node.tracks.back().tilt_degrees == 330);
  REQUIRE(node.resource_paths.size() == 1);
}
