#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_heart_ring_seed.h"
#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"
#include "ro/render/effect_track_figures.h"

using Catch::Approx;
using ro::render::EffectHeartRingPattern;
using ro::render::EffectHeartRingPatternsFor;
using ro::render::EffectHeartRingPatternTable;
using ro::render::EffectHeartRingSeeds;
using ro::render::EffectTrack;
using ro::render::EffectWorldQuad;
using ro::render::kEffectTracksPerNode;
using ro::render::kHeartRingCameraMark;
using ro::render::kHeartRingStyleBeforeEnd;
using ro::render::kHeartRingSweepDegrees;
using ro::render::SeedEffectHeartRingTracks;
using ro::render::SowEffectHeartRingNode;

namespace {

constexpr std::uint32_t kHeartCasting = 343;
constexpr std::int32_t kLife = 200;

}  // namespace

TEST_CASE("les vingt noeuds dessinent un coeur", "[render][heart_ring]") {
  const std::span<const EffectHeartRingPattern> patterns =
      EffectHeartRingPatternsFor(kHeartCasting);
  REQUIRE(patterns.size() == 20);
  CHECK(EffectHeartRingPatternTable().size() == 20);
  // ⚠ CE SONT DES DÉCALAGES, PAS DES POSITIONS, et leur forme se vérifie : la
  // pointe descend plus bas que les lobes ne montent, et les deux côtés se
  // répondent. Un point recopié de travers casserait l'un des deux.
  float lowest = patterns.front().offset_z;
  float highest = patterns.front().offset_z;
  float somme_x = 0.0F;
  for (const EffectHeartRingPattern& pattern : patterns) {
    lowest = std::min(lowest, pattern.offset_z);
    highest = std::max(highest, pattern.offset_z);
    somme_x += pattern.offset_x;
  }
  CHECK(lowest < -highest);
  // ⚠ ET LE CŒUR N'EST PAS TOUT À FAIT SYMÉTRIQUE : chaque point de droite est
  // décalé d'un cran de 0,27 par rapport à son vis-à-vis de gauche — sept paires
  // sur neuf d'exactement un cran, une de deux, une d'aucun. C'est le dessin du
  // client, et la régularité de l'écart prouve que le relevé ne l'a pas faussé.
  // Exiger une symétrie parfaite ferait échouer ce contrôle sur une mesure juste.
  CHECK(somme_x > 0.0F);
  CHECK(somme_x == Approx(0.27F * 9.0F).margin(0.3F));
  CHECK(EffectHeartRingPatternsFor(1).empty());
}

TEST_CASE("un noeud porte DEUX anneaux, et les deux autres pistes dorment",
          "[render][heart_ring]") {
  std::uint32_t seed = 7U;
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectHeartRingTracks(EffectHeartRingPatternsFor(kHeartCasting).front(), kLife, seed);
  const std::span<const ro::render::EffectHeartRingSeed> seeds = EffectHeartRingSeeds();
  REQUIRE(seeds.size() == 2);
  for (std::size_t rang = 0; rang < seeds.size(); ++rang) {
    CHECK(tracks[rang].active);
    // ⚠ L'ALPHA NAÎT NUL : c'est le mouvement radial qui l'allume.
    CHECK(tracks[rang].alpha == 0);
    CHECK(tracks[rang].degrees == kHeartRingSweepDegrees);
    CHECK(tracks[rang].lift_key == Approx(seeds[rang].lift_key));
    CHECK(tracks[rang].tilt_degrees == seeds[rang].tilt_degrees);
    // ⚠ LE STYLE EST UNE DURÉE, et le mouvement l'emploie comme telle.
    CHECK(tracks[rang].style == kLife - kHeartRingStyleBeforeEnd);
    // ET LA MARQUE DIT AU MOUVEMENT DE TOURNER LE NŒUD VERS LA CAMÉRA.
    CHECK(tracks[rang].mark[0] == kHeartRingCameraMark);
  }
  CHECK_FALSE(tracks[2].active);
  CHECK_FALSE(tracks[3].active);
  // LES DEUX ANNEAUX NE SE CONFONDENT PAS : ni le même rayon, ni la même
  // inclinaison.
  CHECK(tracks[0].lift_key != Approx(tracks[1].lift_key));
  CHECK(tracks[0].tilt_degrees != tracks[1].tilt_degrees);
}

TEST_CASE("l'ancre porte la sentinelle, et non une hauteur", "[render][heart_ring]") {
  std::uint32_t seed = 3U;
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectHeartRingTracks(EffectHeartRingPatternsFor(kHeartCasting)[5], kLife, seed);
  // ⚠ À CETTE VALEUR EXACTE, la figure cesse de lire l'ancre comme un point et
  // la TOURNE du cap de la caméra. La lire comme une hauteur poserait le cœur
  // mille deux cents unités au-dessus du sol.
  CHECK(tracks[0].anchor.y == Approx(1234.5F));
  CHECK(tracks[0].anchor.x == Approx(EffectHeartRingPatternsFor(kHeartCasting)[5].offset_x));
  CHECK(tracks[0].anchor.z == Approx(EffectHeartRingPatternsFor(kHeartCasting)[5].offset_z));
}

TEST_CASE("deux coeurs joues a la suite ne presentent pas le meme angle", "[render][heart_ring]") {
  std::uint32_t first = 11U;
  std::uint32_t second = 29U;
  const std::array<EffectTrack, kEffectTracksPerNode> un =
      SeedEffectHeartRingTracks(EffectHeartRingPatternsFor(kHeartCasting).front(), kLife, first);
  const std::array<EffectTrack, kEffectTracksPerNode> two =
      SeedEffectHeartRingTracks(EffectHeartRingPatternsFor(kHeartCasting).front(), kLife, second);
  CHECK(un[0].azimuth_degrees != two[0].azimuth_degrees);
  // Et l'azimut reste dans un tour.
  CHECK(un[0].azimuth_degrees >= 0);
  CHECK(un[0].azimuth_degrees < kHeartRingSweepDegrees);
}

TEST_CASE("le noeud seme porte sa forme et son image, et rend une aire", "[render][heart_ring]") {
  std::uint32_t seed = 5U;
  const EffectHeartRingPattern& pattern = EffectHeartRingPatternsFor(kHeartCasting).front();
  ro::render::PooledEffectNode node =
      SowEffectHeartRingNode(pattern, {4.0F, 1.0F, 6.0F}, kLife, seed);
  CHECK(node.shape == ro::render::PooledNodeShape::kHeartRings);
  REQUIRE(node.resource_paths.size() == 1);
  CHECK(node.resource_paths.front() == std::string(pattern.texture));

  // LA FIGURE EST CELLE DU RUBAN, et elle rend une aire dès que le mouvement a
  // donné aux pistes un alpha et une longueur.
  for (EffectTrack& track : node.tracks) {
    track.alpha = 120;
    std::ranges::fill(track.value, 4.0F);
  }
  const std::vector<EffectWorldQuad> quads = ro::render::ComposeEffectTrackRibbon(
      node.tracks.front(), 0, node.ground_position, ro::render::EffectRibbonTurnOf(node.tracks));
  CHECK_FALSE(quads.empty());
}
