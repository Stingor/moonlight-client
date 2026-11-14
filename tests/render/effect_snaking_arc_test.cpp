#include "ro/render/effect_snaking_arc.h"

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
#include "ro/render/effect_snaking_arc_seed.h"
#include "ro/render/effect_track.h"

using Catch::Approx;
using ro::render::AdvanceSnakingArcTracks;
using ro::render::ComposeSnakingArcQuads;
using ro::render::EffectSnakingArcColor;
using ro::render::EffectSnakingArcJoint;
using ro::render::EffectSnakingArcPattern;
using ro::render::EffectSnakingArcPatternsFor;
using ro::render::EffectSnakingArcPatternTable;
using ro::render::EffectTrack;
using ro::render::EffectWorldQuad;
using ro::render::kEffectTrackSlots;
using ro::render::kEffectTracksPerNode;
using ro::render::kSnakingArcAlphaFloor;
using ro::render::kSnakingArcAlphaPerSegment;
using ro::render::kSnakingArcAlphaSpread;
using ro::render::kSnakingArcBlueStyle;
using ro::render::kSnakingArcDecay;
using ro::render::kSnakingArcDelaySpread;
using ro::render::kSnakingArcFaces;
using ro::render::kSnakingArcLastSowTick;
using ro::render::kSnakingArcLengthFloor;
using ro::render::kSnakingArcLengthSpread;
using ro::render::kSnakingArcPhaseFullDegrees;
using ro::render::kSnakingArcPinkStyle;
using ro::render::kSnakingArcSegments;
using ro::render::kSnakingArcTubeRadius;
using ro::render::SeedEffectSnakingArcTracks;
using ro::render::SowEffectSnakingArcNode;

namespace {

constexpr std::array<std::uint32_t, 3> kArcEffectIds = {330, 376, 887};

[[nodiscard]] const EffectSnakingArcPattern& PatternOf(std::uint32_t effect_id) {
  return EffectSnakingArcPatternsFor(effect_id).front();
}

// UNE PISTE DÉJÀ DÉPLIÉE : vingt et un crans, un rang par cran.
[[nodiscard]] EffectTrack UnfoldedTrack() {
  EffectTrack track;
  track.active = true;
  track.alpha = 150;
  track.ticks = 30;
  track.tilt_degrees = 0;
  track.radius_at_anchor = 10.0F;
  track.lift_key = kSnakingArcTubeRadius;
  track.anchor = {1.0F, 2.0F, 3.0F};
  for (std::size_t rang = 0; rang < kEffectTrackSlots; ++rang) {
    track.mark[rang] = static_cast<std::uint8_t>(kEffectTrackSlots - 1 - rang);
    track.value[rang] = 40.0F;
  }
  return track;
}

}  // namespace

TEST_CASE("l'arc rend trois faces par segment", "[render][snaking_arc]") {
  const std::vector<EffectWorldQuad> faces = ComposeSnakingArcQuads(UnfoldedTrack());
  // ⚠ L'ALPHA TOMBE DE CINQ PAR SEGMENT : à cent cinquante, trente segments
  // seraient permis, mais il n'y en a que vingt.
  CHECK(faces.size() == kSnakingArcSegments * kSnakingArcFaces);
}

TEST_CASE("un alpha faible coupe l'arc court", "[render][snaking_arc]") {
  EffectTrack pale = UnfoldedTrack();
  pale.alpha = 3 * kSnakingArcAlphaPerSegment;
  // ⚠ TROIS SEGMENTS SEULEMENT : le quatrième aurait un alpha nul, et le client
  // s'arrête là. Les dessiner quand même ferait un trait plus long qu'il n'est.
  CHECK(ComposeSnakingArcQuads(pale).size() == 3 * kSnakingArcFaces);
}

TEST_CASE("une piste eteinte, muette ou en retard ne rend rien", "[render][snaking_arc]") {
  EffectTrack faded = UnfoldedTrack();
  faded.active = false;
  CHECK(ComposeSnakingArcQuads(faded).empty());
  EffectTrack silent = UnfoldedTrack();
  silent.alpha = 0;
  CHECK(ComposeSnakingArcQuads(silent).empty());
  // ⚠ LA TROISIÈME GARDE EST UN COMPTEUR, et elle est facile à oublier : une
  // piste en retard est allumée ET opaque, et ne se dessine pas.
  EffectTrack delayed = UnfoldedTrack();
  delayed.ticks = 0;
  CHECK(ComposeSnakingArcQuads(delayed).empty());
}

TEST_CASE("les segments raccourcissent de sept dixiemes par rang", "[render][snaking_arc]") {
  EffectTrack track;
  track.radius_at_anchor = 10.0F;
  track.tilt_degrees = 0;
  for (std::size_t rang = 0; rang < kEffectTrackSlots; ++rang) {
    track.mark[rang] = static_cast<std::uint8_t>(rang);
    track.value[rang] = 0.0F;
  }
  // Rang zéro : rien de parcouru. Rang un : une longueur pleine. Rang deux :
  // une pleine plus sept dixièmes.
  CHECK(EffectSnakingArcJoint(track, 0).x == Approx(0.0F).margin(0.001F));
  CHECK(EffectSnakingArcJoint(track, 1).x == Approx(20.0F).margin(0.001F));
  CHECK(EffectSnakingArcJoint(track, 2).x ==
        Approx(20.0F + 20.0F * kSnakingArcDecay).margin(0.01F));
}

TEST_CASE("la phase bombe le segment et avance dans sa longueur", "[render][snaking_arc]") {
  EffectTrack track;
  track.radius_at_anchor = 10.0F;
  track.tilt_degrees = 0;
  // ⚠ À QUATRE-VINGT-DIX DEGRÉS, LE BOMBEMENT EST AU PLUS HAUT et la moitié du
  // segment est franchie.
  track.value[0] = 90.0F;
  const glm::vec3 middle = EffectSnakingArcJoint(track, 0);
  CHECK(middle.x == Approx(10.0F).margin(0.001F));
  CHECK(middle.y == Approx(15.0F).margin(0.01F));
  // ⚠ ET NOTRE REPÈRE MONTE : l'arc bombe VERS LE HAUT là où le client écrit un
  // nombre négatif.
  CHECK(middle.y > 0.0F);
  track.value[0] = kSnakingArcPhaseFullDegrees;
  const glm::vec3 tip = EffectSnakingArcJoint(track, 0);
  CHECK(tip.x == Approx(20.0F).margin(0.001F));
  CHECK(tip.y == Approx(0.0F).margin(0.01F));
}

TEST_CASE("le cap couche l'arc dans son plan", "[render][snaking_arc]") {
  EffectTrack track;
  track.radius_at_anchor = 10.0F;
  track.mark[3] = 2;
  track.tilt_degrees = 90;
  const glm::vec3 joint = EffectSnakingArcJoint(track, 3);
  CHECK(joint.x == Approx(0.0F).margin(0.001F));
  // ⚠ LE CAP EST CELUI DU CLIENT, dont la profondeur positive devient négative
  // chez nous : garder son signe tracerait l'arc en miroir.
  CHECK(joint.z < 0.0F);
}

TEST_CASE("chaque style a sa teinte, et le defaut sert", "[render][snaking_arc]") {
  CHECK(EffectSnakingArcColor(kSnakingArcPinkStyle, 0)[0] == 255);
  CHECK(EffectSnakingArcColor(kSnakingArcPinkStyle, 0)[1] == 60);
  CHECK(EffectSnakingArcColor(kSnakingArcBlueStyle, 0)[2] == 255);
  CHECK(EffectSnakingArcColor(kSnakingArcBlueStyle, 0)[0] == 10);
  // ⚠ LE STYLE ZÉRO N'EST PAS NOMMÉ PAR L'AIGUILLAGE et il retombe sur l'ambre
  // — c'est le premier des trois sites.
  CHECK(EffectSnakingArcColor(0, 0)[1] == 200);
  // LA TEINTE MONTE DE SEPT PAR SEGMENT.
  CHECK(EffectSnakingArcColor(kSnakingArcBlueStyle, 3)[0] == 31);
}

TEST_CASE("l'historique recule d'un cran par tic", "[render][snaking_arc]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  tracks[0].active = true;
  tracks[0].ticks = 0;
  tracks[0].alpha = 150;
  AdvanceSnakingArcTracks(tracks);
  CHECK(tracks[0].ticks == 1);
  CHECK(tracks[0].value[0] == Approx(8.0F));
  // ⚠ LA TÊTE ET LE PREMIER CRAN PORTENT LA MÊME PHASE : le client fait avancer
  // la tête AVANT de décaler.
  CHECK(tracks[0].value[1] == Approx(8.0F));
  CHECK(tracks[0].value[2] == Approx(0.0F));
  AdvanceSnakingArcTracks(tracks);
  CHECK(tracks[0].value[0] == Approx(16.0F));
  CHECK(tracks[0].value[1] == Approx(16.0F));
  CHECK(tracks[0].value[2] == Approx(8.0F));
}

TEST_CASE("la phase reboucle et pousse un rang", "[render][snaking_arc]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  tracks[0].active = true;
  tracks[0].ticks = 0;
  tracks[0].alpha = 150;
  // Vingt-trois tics de huit degrés dépassent le demi-tour.
  for (std::int32_t tick = 0; tick < 23; ++tick) {
    AdvanceSnakingArcTracks(tracks);
  }
  CHECK(tracks[0].mark[0] == 1);
  CHECK(tracks[0].value[0] < kSnakingArcPhaseFullDegrees);
  // ET LE RANG DESCEND L'HISTORIQUE AVEC LA PHASE.
  CHECK(tracks[0].mark[1] == 1);
  CHECK(tracks[0].mark[kEffectTrackSlots - 1] == 0);
}

TEST_CASE("une piste en retard n'avance pas son historique", "[render][snaking_arc]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  tracks[0].active = true;
  tracks[0].ticks = -3;
  tracks[0].alpha = 150;
  for (std::int32_t tick = 0; tick < 3; ++tick) {
    AdvanceSnakingArcTracks(tracks);
    CHECK(tracks[0].value[0] == Approx(0.0F));
  }
  AdvanceSnakingArcTracks(tracks);
  CHECK(tracks[0].value[0] == Approx(8.0F));
}

TEST_CASE("l'alpha ne tombe qu'apres le trente-cinquieme tic", "[render][snaking_arc]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  tracks[0].active = true;
  tracks[0].ticks = 0;
  tracks[0].alpha = 150;
  for (std::int32_t tick = 0; tick < 35; ++tick) {
    AdvanceSnakingArcTracks(tracks);
  }
  CHECK(tracks[0].alpha == 150);
  AdvanceSnakingArcTracks(tracks);
  CHECK(tracks[0].alpha == 145);
}

TEST_CASE("le semis tire QUATRE fois par piste, dans son ordre", "[render][snaking_arc]") {
  std::uint32_t seed = 17U;
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectSnakingArcTracks(PatternOf(330), {1.0F, 2.0F, 3.0F}, seed);
  std::uint32_t witness = 17U;
  for (std::size_t draw = 0; draw < 4 * kEffectTracksPerNode; ++draw) {
    static_cast<void>(ro::render::NextEffectNodeDraw(witness));
  }
  CHECK(seed == witness);
  for (const EffectTrack& track : tracks) {
    CHECK(track.active);
    CHECK(track.ticks <= 0);
    CHECK(track.ticks > -kSnakingArcDelaySpread);
    CHECK(track.alpha >= kSnakingArcAlphaFloor);
    CHECK(track.alpha < kSnakingArcAlphaFloor + kSnakingArcAlphaSpread);
    CHECK(track.radius_at_anchor >= kSnakingArcLengthFloor);
    CHECK(track.radius_at_anchor <
          kSnakingArcLengthFloor + static_cast<float>(kSnakingArcLengthSpread));
    CHECK(track.lift_key == Approx(kSnakingArcTubeRadius));
    CHECK(track.anchor.x == Approx(1.0F));
    // ⚠ L'HISTORIQUE NAÎT VIDE : les vingt segments se superposent à l'ancre, et
    // c'est le mouvement qui déplie l'arc.
    for (std::size_t rang = 0; rang < kEffectTrackSlots; ++rang) {
      CHECK(track.value[rang] == Approx(0.0F));
      CHECK(track.mark[rang] == 0);
    }
  }
}

TEST_CASE("la table porte les trois identifiants du sous-type 70", "[render][snaking_arc]") {
  const std::span<const EffectSnakingArcPattern> table = EffectSnakingArcPatternTable();
  REQUIRE(table.size() == kArcEffectIds.size());
  for (std::size_t rang = 0; rang < kArcEffectIds.size(); ++rang) {
    CHECK(table[rang].effect_id == kArcEffectIds[rang]);
    CHECK_FALSE(table[rang].texture.empty());
    // ⚠ QUATRE SEMIS, AUX QUATRE PREMIERS TICS.
    CHECK(table[rang].schedule.first_tick == 0);
    CHECK(table[rang].schedule.last_tick == kSnakingArcLastSowTick);
    CHECK(table[rang].schedule.period_ticks == 1);
  }
  // LES TROIS STYLES DU DESSINATEUR SONT TOUS SERVIS.
  CHECK(PatternOf(330).style == 0);
  CHECK(PatternOf(376).style == kSnakingArcPinkStyle);
  CHECK(PatternOf(887).style == kSnakingArcBlueStyle);
  CHECK(EffectSnakingArcPatternsFor(1U).empty());
}

TEST_CASE("le noeud seme porte sa forme et son image", "[render][snaking_arc]") {
  std::uint32_t seed = 3U;
  const ro::render::PooledEffectNode node =
      SowEffectSnakingArcNode(PatternOf(887), {2.0F, 0.0F, 7.0F}, 600, seed);
  CHECK(node.shape == ro::render::PooledNodeShape::kSnakingArc);
  CHECK(node.node.life_ticks == 600);
  REQUIRE(node.resource_paths.size() == 1);
  CHECK(node.resource_paths.front() == std::string(PatternOf(887).texture));
  CHECK(node.tracks[0].style == kSnakingArcBlueStyle);
}
