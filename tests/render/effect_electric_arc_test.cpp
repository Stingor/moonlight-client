#include "ro/render/effect_electric_arc.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_electric_arc_seed.h"
#include "ro/render/effect_launch.h"
#include "ro/render/effect_lightning_fan.h"
#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

using Catch::Approx;
using ro::render::AdvanceElectricArcTracks;
using ro::render::ComposeElectricArcQuads;
using ro::render::EffectElectricArcPattern;
using ro::render::EffectElectricArcPatternsFor;
using ro::render::EffectElectricArcPatternTable;
using ro::render::EffectOwnerFacingDegrees;
using ro::render::EffectTrack;
using ro::render::EffectWorldQuad;
using ro::render::ElectricArcStartTiltDegrees;
using ro::render::kEffectTracksPerNode;
using ro::render::kElectricArcAnchorLift;
using ro::render::kElectricArcGradientPerSegment;
using ro::render::kElectricArcLeadAlpha;
using ro::render::kElectricArcRandomBlueFloor;
using ro::render::kElectricArcRandomBlueSpread;
using ro::render::kElectricArcRandomColorSweep;
using ro::render::kElectricArcRandomGreenSpread;
using ro::render::kElectricArcReach;
using ro::render::kElectricArcRed;
using ro::render::kElectricArcSegments;
using ro::render::kElectricArcTubeRadius;
using ro::render::kLightningFanFaces;
using ro::render::kLightningFanFirstDrawnSegment;
using ro::render::kLightningFanLastDrawnSegment;
using ro::render::kLightningFanTailOfBow;
using ro::render::kLightningFanTailOfSegments;
using ro::render::SeedEffectElectricArcTracks;
using ro::render::SowEffectElectricArcNode;

namespace {

constexpr std::uint32_t kInfraredScan = 794;
constexpr std::size_t kDrawnSegments =
    kLightningFanLastDrawnSegment - kLightningFanFirstDrawnSegment + 1;

[[nodiscard]] const EffectElectricArcPattern& PatternForStyle(std::int16_t style) {
  for (const EffectElectricArcPattern& pattern : EffectElectricArcPatternsFor(kInfraredScan)) {
    if (pattern.spin_style == style) {
      return pattern;
    }
  }
  return EffectElectricArcPatternsFor(kInfraredScan).front();
}

// LES QUATRE PISTES APRÈS UN PREMIER TIC : l'historique est écrit, l'ancre
// recopiée du porteur.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> WrittenTracks(
    glm::vec3 bearer, float cap_de_base = EffectOwnerFacingDegrees(0)) {
  std::uint32_t seed = 5U;
  std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectElectricArcTracks(PatternForStyle(0), bearer, cap_de_base, seed);
  AdvanceElectricArcTracks(tracks, bearer);
  return tracks;
}

}  // namespace

TEST_CASE("la table porte DEUX arcs pour l'effet 794, un par sens", "[render][electric_arc]") {
  const std::span<const EffectElectricArcPattern> table = EffectElectricArcPatternTable();
  REQUIRE(table.size() == 2);
  const std::span<const EffectElectricArcPattern> own_entries =
      EffectElectricArcPatternsFor(kInfraredScan);
  REQUIRE(own_entries.size() == 2);
  // ⚠ L'OCTET DU SITE EST LE SENS DE ROTATION : zéro puis un, dans l'ordre des
  // deux appels.
  CHECK(own_entries[0].spin_style == 0);
  CHECK(own_entries[1].spin_style == 1);
  for (const EffectElectricArcPattern& pattern : own_entries) {
    CHECK(pattern.texture == "data\\texture\\effect\\elec1.tga");
  }
  CHECK(EffectElectricArcPatternsFor(1U).empty());
}

TEST_CASE("le cap de depart vient du corps du porteur, moins un quart de tour",
          "[render][electric_arc]") {
  // Direction zéro : l'angle de base est nul, le corps vaut cent quatre-vingts.
  CHECK(ElectricArcStartTiltDegrees(EffectOwnerFacingDegrees(0)) == 90);
  CHECK(ElectricArcStartTiltDegrees(EffectOwnerFacingDegrees(2)) == 180);
  // ⚠ LE CORPS EST RANGÉ DANS LE TOUR avant la soustraction : la direction
  // quatre donne un corps de zéro, et le cap négatif reçoit un tour.
  CHECK(ElectricArcStartTiltDegrees(EffectOwnerFacingDegrees(4)) == 270);
  CHECK(ElectricArcStartTiltDegrees(EffectOwnerFacingDegrees(6)) == 0);
  // ⚠ LE CLIENT TRONQUE VERS ZÉRO : moins quatre-vingt-neuf et demi donne moins
  // quatre-vingt-neuf, soit deux cent soixante et onze.
  CHECK(ElectricArcStartTiltDegrees(180.0F - 0.5F) == 271);
}

TEST_CASE("le semis pose quatre pistes vives, la tete plus opaque", "[render][electric_arc]") {
  std::uint32_t seed = 17U;
  const glm::vec3 bearer{1.0F, 2.0F, 3.0F};
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectElectricArcTracks(PatternForStyle(1), bearer, EffectOwnerFacingDegrees(2), seed);
  // ⚠ UN SEUL TIRAGE PAR PISTE, le bombement.
  std::uint32_t witness = 17U;
  for (std::size_t draw = 0; draw < kEffectTracksPerNode; ++draw) {
    static_cast<void>(ro::render::NextEffectNodeDraw(witness));
  }
  CHECK(seed == witness);
  // DEUX CENT CINQUANTE, puis cent quarante moins trente par rang.
  constexpr std::array<std::int16_t, kEffectTracksPerNode> kAlphas = {250, 110, 80, 50};
  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    const EffectTrack& track = tracks[rang];
    CHECK(track.active);
    CHECK(track.ticks == 0);
    CHECK(track.alpha == kAlphas[rang]);
    CHECK(track.tilt_degrees == 180);
    CHECK(track.style == 1);
    CHECK(track.degrees == 0);
    CHECK(track.lift_key == Approx(kElectricArcTubeRadius));
    CHECK(track.radius_at_anchor == Approx(kElectricArcReach));
    CHECK(track.tail[kLightningFanTailOfSegments] == Approx(kElectricArcSegments));
    CHECK(track.tail[kLightningFanTailOfBow] >= 0.0F);
    CHECK(track.tail[kLightningFanTailOfBow] <= 0.1F + 1e-6F);
    // ⚠ DIX PLUS HAUT : le client retranche dix à une ordonnée qui descend.
    CHECK(track.anchor.y == Approx(bearer.y + kElectricArcAnchorLift));
    for (const float phase : track.value) {
      CHECK(phase == Approx(0.0F));
    }
  }
  CHECK(tracks[0].alpha == kElectricArcLeadAlpha);
}

TEST_CASE("le noeud seme porte sa forme, son image et la vie de l'effet",
          "[render][electric_arc]") {
  std::uint32_t seed = 3U;
  const ro::render::PooledEffectNode node =
      SowEffectElectricArcNode(PatternForStyle(0), {2.0F, 0.0F, 7.0F}, 0.0F, 300, seed);
  CHECK(node.shape == ro::render::PooledNodeShape::kElectricArc);
  CHECK(node.node.life_ticks == 300);
  REQUIRE(node.resource_paths.size() == 1);
  CHECK(node.resource_paths.front() == std::string(PatternForStyle(0).texture));
}

TEST_CASE("l'historique s'ecrit au premier tic, comme celui de l'eclair",
          "[render][electric_arc]") {
  const std::array<EffectTrack, kEffectTracksPerNode> tracks = WrittenTracks({0.0F, 0.0F, 0.0F});
  CHECK(tracks[0].value[0] == Approx(180.0F));
  CHECK(tracks[0].value[1] == Approx(180.0F));
  CHECK(tracks[0].value[2] == Approx(171.0F));
  CHECK(tracks[0].value[20] == Approx(9.0F));
}

TEST_CASE("l'ancre suit le porteur a chaque tic, dix unites plus haut", "[render][electric_arc]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks = WrittenTracks({0.0F, 0.0F, 0.0F});
  const glm::vec3 elsewhere{30.0F, 4.0F, -12.0F};
  AdvanceElectricArcTracks(tracks, elsewhere);
  for (const EffectTrack& track : tracks) {
    CHECK(track.anchor.x == Approx(elsewhere.x));
    CHECK(track.anchor.y == Approx(elsewhere.y + kElectricArcAnchorLift));
    CHECK(track.anchor.z == Approx(elsewhere.z));
  }
}

TEST_CASE("le cap tourne de deux degres par tic, dix tics de retard par rang",
          "[render][electric_arc]") {
  std::uint32_t seed = 5U;
  std::array<EffectTrack, kEffectTracksPerNode> croissant = SeedEffectElectricArcTracks(
      PatternForStyle(1), {0.0F, 0.0F, 0.0F}, EffectOwnerFacingDegrees(0), seed);
  std::array<EffectTrack, kEffectTracksPerNode> decroissant = SeedEffectElectricArcTracks(
      PatternForStyle(0), {0.0F, 0.0F, 0.0F}, EffectOwnerFacingDegrees(0), seed);
  for (std::int32_t tick = 0; tick < 10; ++tick) {
    AdvanceElectricArcTracks(croissant, {0.0F, 0.0F, 0.0F});
    AdvanceElectricArcTracks(decroissant, {0.0F, 0.0F, 0.0F});
  }
  // La tête tourne dès le premier tic ; la piste un attend le onzième.
  CHECK(croissant[0].tilt_degrees == 90 + 20);
  CHECK(decroissant[0].tilt_degrees == 90 - 20);
  CHECK(croissant[1].tilt_degrees == 90);
  CHECK(croissant[3].tilt_degrees == 90);
  AdvanceElectricArcTracks(croissant, {0.0F, 0.0F, 0.0F});
  CHECK(croissant[1].tilt_degrees == 92);
  CHECK(croissant[2].tilt_degrees == 90);
}

TEST_CASE("le cap reboucle dans le tour, dans les deux sens", "[render][electric_arc]") {
  std::uint32_t seed = 5U;
  std::array<EffectTrack, kEffectTracksPerNode> tracks = SeedEffectElectricArcTracks(
      PatternForStyle(0), {0.0F, 0.0F, 0.0F}, EffectOwnerFacingDegrees(0), seed);
  tracks[0].tilt_degrees = 0;
  AdvanceElectricArcTracks(tracks, {0.0F, 0.0F, 0.0F});
  CHECK(tracks[0].tilt_degrees == 358);
  tracks[0].style = 1;
  tracks[0].tilt_degrees = 359;
  AdvanceElectricArcTracks(tracks, {0.0F, 0.0F, 0.0F});
  CHECK(tracks[0].tilt_degrees == 1);
}

TEST_CASE("l'alpha tient soixante tics puis perd six par tic", "[render][electric_arc]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks = WrittenTracks({0.0F, 0.0F, 0.0F});
  for (std::int32_t tick = 1; tick < 60; ++tick) {
    AdvanceElectricArcTracks(tracks, {0.0F, 0.0F, 0.0F});
  }
  CHECK(tracks[0].ticks == 60);
  CHECK(tracks[0].alpha == 250);
  AdvanceElectricArcTracks(tracks, {0.0F, 0.0F, 0.0F});
  CHECK(tracks[0].alpha == 244);
  CHECK(tracks[3].alpha == 44);
  for (std::int32_t tick = 0; tick < 100; ++tick) {
    AdvanceElectricArcTracks(tracks, {0.0F, 0.0F, 0.0F});
  }
  CHECK(tracks[0].alpha == 0);
  // ⚠ ET LA PISTE RESTE VIVE : c'est la vie du nœud qui l'efface.
  CHECK(tracks[0].active);
}

TEST_CASE("un seul tube, et trois nappes tendues vers lui", "[render][electric_arc]") {
  const std::array<EffectTrack, kEffectTracksPerNode> tracks = WrittenTracks({0.0F, 0.0F, 0.0F});
  std::uint32_t rng = 3U;
  const std::vector<EffectWorldQuad> quads = ComposeElectricArcQuads(tracks, rng);
  // ⚠ LE TUBE DE LA SEULE PISTE ZÉRO — trois faces par segment dessiné —, puis
  // UN quad par segment pour chacune des trois autres. Quatre tubes, comme
  // l'éclair du 101, en rendraient quatre fois trois par segment.
  const std::size_t tube = kDrawnSegments * kLightningFanFaces;
  const std::size_t nappes = kDrawnSegments * (kEffectTracksPerNode - 1);
  REQUIRE(quads.size() == tube + nappes);

  // LE TUBE EST ROUGE, le vert et le bleu montant de trois par segment ; son
  // alpha tombe de cinq par segment.
  const EffectWorldQuad& first = quads.front();
  CHECK(first.tint[0] == kElectricArcRed);
  CHECK(first.tint[1] == kElectricArcGradientPerSegment);
  CHECK(first.tint[2] == kElectricArcGradientPerSegment);
  CHECK(first.tint[3] == 250 - 5);

  // LES NAPPES SONT ROUGE PUR, à l'alpha de leur piste divisé par trois, par
  // deux, puis entier : cent dix, quatre-vingts et cinquante.
  constexpr std::array<std::uint8_t, 3> kAlphasDesNappes = {110 / 3, 80 / 2, 50};
  for (std::size_t track = 0; track < kAlphasDesNappes.size(); ++track) {
    const EffectWorldQuad& nappe = quads[tube + track * kDrawnSegments];
    CHECK(nappe.tint[0] == kElectricArcRed);
    CHECK(nappe.tint[1] == 0);
    CHECK(nappe.tint[2] == 0);
    CHECK(nappe.tint[3] == kAlphasDesNappes[track]);
  }
}

TEST_CASE("chaque nappe s'accroche au bord que le tube vient de poser", "[render][electric_arc]") {
  const std::array<EffectTrack, kEffectTracksPerNode> tracks = WrittenTracks({0.0F, 0.0F, 0.0F});
  std::uint32_t rng = 3U;
  const std::vector<EffectWorldQuad> quads = ComposeElectricArcQuads(tracks, rng);
  const std::size_t tube = kDrawnSegments * kLightningFanFaces;
  REQUIRE(quads.size() > tube);
  for (std::size_t segment = 0; segment < kDrawnSegments; ++segment) {
    // LA DERNIÈRE FACE du segment, celle qui ferme le tube à trois cent
    // soixante degrés : ses deux premiers coins sont le bord.
    const EffectWorldQuad& face = quads[(segment * kLightningFanFaces) + kLightningFanFaces - 1];
    const EffectWorldQuad& nappe = quads[tube + segment];
    CHECK(nappe.corners[2] == face.corners[1]);
    CHECK(nappe.corners[3] == face.corners[0]);
  }
}

TEST_CASE("sans tete, les autres pistes ne tendent rien", "[render][electric_arc]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks = WrittenTracks({0.0F, 0.0F, 0.0F});
  tracks[0].alpha = 0;
  std::uint32_t rng = 3U;
  CHECK(ComposeElectricArcQuads(tracks, rng).empty());
}

TEST_CASE("chaque piste consomme son tremblement, vive ou non", "[render][electric_arc]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks = WrittenTracks({0.0F, 0.0F, 0.0F});
  for (EffectTrack& track : tracks) {
    track.active = false;
  }
  std::uint32_t rng = 42U;
  CHECK(ComposeElectricArcQuads(tracks, rng).empty());
  std::uint32_t witness = 42U;
  const std::size_t tirages =
      kEffectTracksPerNode * 3U * static_cast<std::size_t>(kElectricArcSegments);
  for (std::size_t draw = 0; draw < tirages; ++draw) {
    static_cast<void>(ro::render::NextEffectNodeDraw(witness));
  }
  CHECK(rng == witness);
}

TEST_CASE("l'arc se tient a la hauteur de son ancre, pas en miroir sous le sol",
          "[render][electric_arc]") {
  // ⚠ L'ANCRE EST DANS NOTRE REPÈRE et le joint dans celui du client : ne
  // retourner que leur somme enverrait l'arc à moins soixante.
  const glm::vec3 bearer{0.0F, 50.0F, 0.0F};
  const std::array<EffectTrack, kEffectTracksPerNode> tracks = WrittenTracks(bearer);
  std::uint32_t rng = 3U;
  const std::vector<EffectWorldQuad> quads = ComposeElectricArcQuads(tracks, rng);
  REQUIRE_FALSE(quads.empty());
  const float height = bearer.y + kElectricArcAnchorLift;
  // Le joint descend de deux et bombe d'au plus deux, le tremblement ajoute
  // trois dixièmes : l'arc reste à quelques unités de son ancre.
  constexpr float kMaxSpread = 5.0F;
  for (const EffectWorldQuad& quad : quads) {
    for (const glm::vec3& corner : quad.corners) {
      CHECK(std::abs(corner.y - height) < kMaxSpread);
    }
  }
}

TEST_CASE("le cap du client se retourne en profondeur", "[render][electric_arc]") {
  // CAP QUATRE-VINGT-DIX CHEZ LE CLIENT : l'arc part vers sa profondeur
  // croissante, qui est NOTRE profondeur décroissante.
  std::array<EffectTrack, kEffectTracksPerNode> tracks = WrittenTracks({0.0F, 0.0F, 0.0F});
  // ⚠ LE PREMIER TIC A DÉJÀ TOURNÉ LA TÊTE DE DEUX DEGRÉS : on la remet au
  // quart de tour pour lire l'axe sans le cosinus d'un reste.
  tracks[0].tilt_degrees = 90;
  std::uint32_t rng = 3U;
  const std::vector<EffectWorldQuad> quads = ComposeElectricArcQuads(tracks, rng);
  REQUIRE(quads.size() >= kLightningFanFaces);
  // LE BOUT PROCHE DU PREMIER SEGMENT DESSINÉ : sa phase est au plafond, cent
  // quatre-vingts, et il se tient à deux portées de l'ancre.
  const EffectWorldQuad& tip = quads.front();
  CHECK(tip.corners[0].z < -kElectricArcReach);
  CHECK(std::abs(tip.corners[0].x) < 1.0F);
}

TEST_CASE("le balayage trois tire la teinte du tube, le bleu d'abord", "[render][electric_arc]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks = WrittenTracks({0.0F, 0.0F, 0.0F});
  tracks[0].degrees = kElectricArcRandomColorSweep;
  std::uint32_t rng = 11U;
  const std::vector<EffectWorldQuad> quads = ComposeElectricArcQuads(tracks, rng);
  REQUIRE_FALSE(quads.empty());
  for (std::size_t face = 0; face < kDrawnSegments * kLightningFanFaces; ++face) {
    const EffectWorldQuad& quad = quads[face];
    CHECK(quad.tint[0] == kElectricArcRed);
    CHECK(quad.tint[1] < kElectricArcRandomGreenSpread);
    CHECK(quad.tint[2] >= kElectricArcRandomBlueFloor);
    CHECK(quad.tint[2] < kElectricArcRandomBlueFloor + kElectricArcRandomBlueSpread);
  }
}
