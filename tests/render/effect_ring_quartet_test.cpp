#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_ring_quartet_seed.h"
#include "ro/render/effect_track.h"
#include "ro/render/effect_track_figures.h"

using Catch::Approx;
using ro::render::AdvanceRingQuartetTracks;
using ro::render::ComposeEffectTrackSkirt;
using ro::render::EffectRingQuartetPattern;
using ro::render::EffectRingQuartetPatternsFor;
using ro::render::EffectRingQuartetPatternTable;
using ro::render::EffectTrack;
using ro::render::EffectWorldQuad;
using ro::render::kEffectTracksPerNode;
using ro::render::kRingQuartetClosesAfterTick;
using ro::render::kRingQuartetDegreesPerTrack;
using ro::render::kRingQuartetFadeAfterTick;
using ro::render::kRingQuartetFullSweep;
using ro::render::kRingQuartetOpenedAtTick;
using ro::render::kRingQuartetOpensAtTick;
using ro::render::kRingQuartetReach;
using ro::render::kRingQuartetStyleThatFades;
using ro::render::kRingQuartetTicksPerTrack;
using ro::render::SeedEffectRingQuartetTracks;
using ro::render::SowEffectRingQuartetNode;

namespace {

[[nodiscard]] EffectRingQuartetPattern TestPattern() {
  EffectRingQuartetPattern pattern;
  pattern.effect_id = 1;
  pattern.client_function = "test";
  pattern.texture = "essai.tga";
  return pattern;
}

[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> Advance(
    std::array<EffectTrack, kEffectTracksPerNode> tracks, int count) {
  for (int tick = 0; tick < count; ++tick) {
    AdvanceRingQuartetTracks(tracks);
  }
  return tracks;
}

}  // namespace

TEST_CASE("les quatre pistes ne partent pas ENSEMBLE", "[render][ring_quartet]") {
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectRingQuartetTracks(TestPattern());
  // ⚠ ZÉRO, TRENTE, SOIXANTE, QUATRE-VINGT-DIX : elles s'ouvrent l'une après
  // l'autre, et c'est ce décalage qui fait la vague.
  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    REQUIRE(tracks[rang].active);
    REQUIRE(tracks[rang].ticks == kRingQuartetTicksPerTrack * static_cast<std::int32_t>(rang));
    REQUIRE(tracks[rang].degrees == kRingQuartetFullSweep);
    REQUIRE(tracks[rang].azimuth_degrees ==
            kRingQuartetDegreesPerTrack * static_cast<std::int16_t>(rang));
    REQUIRE(tracks[rang].lift_key == Approx(kRingQuartetReach));
  }
}

TEST_CASE("le ruban NAIT sans longueur", "[render][ring_quartet]") {
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectRingQuartetTracks(TestPattern());
  REQUIRE(tracks[0].value[0] == Approx(0.0F));
  REQUIRE(tracks[0].value[20] == Approx(0.0F));
  // ⚠ ET LA JUPE SORT QUAND MÊME SES QUADS, d'aire NULLE : ses deux bords
  // coïncident tant que la longueur est nulle. Ce n'est pas une absence, c'est
  // un ruban replié sur son cercle — et c'est pourquoi le mouvement doit
  // l'ouvrir pour qu'il se voie.
  const std::vector<EffectWorldQuad> replie =
      ComposeEffectTrackSkirt(tracks[0], 5, glm::vec3{0.0F, 0.0F, 0.0F});
  REQUIRE_FALSE(replie.empty());
  REQUIRE(replie[0].corners[0].x == Approx(replie[0].corners[3].x));
  REQUIRE(replie[0].corners[0].z == Approx(replie[0].corners[3].z));
}

TEST_CASE("la jupe reste fermee jusqu'au deux-centieme tic", "[render][ring_quartet]") {
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      Advance(SeedEffectRingQuartetTracks(TestPattern()), kRingQuartetOpensAtTick - 1);
  REQUIRE(tracks[0].ticks == kRingQuartetOpensAtTick - 1);
  REQUIRE(tracks[0].value[0] == Approx(0.0F));
}

TEST_CASE("elle s'ouvre en SINUS, pas en rampe droite", "[render][ring_quartet]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks =
      Advance(SeedEffectRingQuartetTracks(TestPattern()), kRingQuartetOpensAtTick);
  const float at_start = tracks[0].value[0];
  tracks = Advance(tracks, 45);
  const float halfway = tracks[0].value[0];
  tracks = Advance(tracks, 45);
  const float a_la_fin = tracks[0].value[0];

  // ⚠ UN SINUS DE ZÉRO À QUATRE-VINGT-DIX DEGRÉS : la jupe s'ouvre vite puis
  // ralentit. Une rampe droite la ferait pousser d'un trait, et la moitié du
  // chemin tomberait à la moitié de la longueur.
  REQUIRE(at_start < halfway);
  REQUIRE(halfway < a_la_fin);
  REQUIRE(a_la_fin == Approx(kRingQuartetReach).epsilon(1e-3));
  REQUIRE(halfway > kRingQuartetReach * 0.5F);
}

TEST_CASE("les vingt et une longueurs sont EGALES", "[render][ring_quartet]") {
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      Advance(SeedEffectRingQuartetTracks(TestPattern()), kRingQuartetOpenedAtTick + 10);
  // ⚠ LE CLIENT RECALCULE LA SIENNE À CHAQUE CASE sans jamais lire l'index : la
  // jupe est UNIFORME. Lui donner un profil par segment inventerait une forme
  // qu'il n'a pas.
  for (std::size_t rang = 1; rang < tracks[0].value.size(); ++rang) {
    REQUIRE(tracks[0].value[rang] == Approx(tracks[0].value[0]));
  }
  REQUIRE(tracks[0].value[0] == Approx(kRingQuartetReach));
}

TEST_CASE("la quatrieme piste s'ouvre AVANT la premiere", "[render][ring_quartet]") {
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      Advance(SeedEffectRingQuartetTracks(TestPattern()), kRingQuartetOpensAtTick - 60);
  // La piste trois part de quatre-vingt-dix : elle a déjà franchi le seuil.
  REQUIRE(tracks[3].value[0] > 0.0F);
  REQUIRE(tracks[0].value[0] == Approx(0.0F));
}

TEST_CASE("la jupe SE REFERME passe le huit-centieme tic", "[render][ring_quartet]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks =
      Advance(SeedEffectRingQuartetTracks(TestPattern()), kRingQuartetClosesAfterTick);
  const float full = tracks[0].value[0];
  tracks = Advance(tracks, 20);
  // ⚠ C'EST LA SECONDE FIN DE CETTE FIGURE, et elle est indépendante de
  // l'alpha : la portée rentre d'une unité par tic.
  REQUIRE(tracks[0].value[0] < full);
  REQUIRE(tracks[0].value[0] == Approx(full - 20.0F).epsilon(1e-3));
}

TEST_CASE("seul le style UN voit son alpha tomber", "[render][ring_quartet]") {
  EffectRingQuartetPattern calme = TestPattern();
  EffectRingQuartetPattern falling = TestPattern();
  falling.style = kRingQuartetStyleThatFades;

  const std::array<EffectTrack, kEffectTracksPerNode> sans =
      Advance(SeedEffectRingQuartetTracks(calme), kRingQuartetFadeAfterTick + 30);
  const std::array<EffectTrack, kEffectTracksPerNode> with =
      Advance(SeedEffectRingQuartetTracks(falling), kRingQuartetFadeAfterTick + 30);
  REQUIRE(sans[0].alpha == calme.alpha);
  REQUIRE(with[0].alpha < falling.alpha);
}

TEST_CASE("l'azimut tourne d'un degre par tic et reboucle", "[render][ring_quartet]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks = SeedEffectRingQuartetTracks(TestPattern());
  tracks[0].azimuth_degrees = 359;
  AdvanceRingQuartetTracks(tracks);
  REQUIRE(tracks[0].azimuth_degrees == 0);
}

TEST_CASE("une piste eteinte ne bouge pas", "[render][ring_quartet]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks = SeedEffectRingQuartetTracks(TestPattern());
  tracks[1].active = false;
  const std::int32_t before = tracks[1].ticks;
  AdvanceRingQuartetTracks(tracks);
  REQUIRE(tracks[1].ticks == before);
}

TEST_CASE("la jupe ouverte rend des quads", "[render][ring_quartet]") {
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      Advance(SeedEffectRingQuartetTracks(TestPattern()), kRingQuartetOpenedAtTick + 5);
  const std::vector<EffectWorldQuad> skirt =
      ComposeEffectTrackSkirt(tracks[0], 5, glm::vec3{0.0F, 0.0F, 0.0F});
  REQUIRE_FALSE(skirt.empty());
}

TEST_CASE("le noeud seme porte la bonne forme", "[render][ring_quartet]") {
  const ro::render::PooledEffectNode node =
      SowEffectRingQuartetNode(TestPattern(), glm::vec3{0.0F, 0.0F, 0.0F}, 1000);
  REQUIRE(node.shape == ro::render::PooledNodeShape::kRingQuartet);
  REQUIRE(node.resource_paths.size() == 1);
}

TEST_CASE("la table des quatuors porte les cinq identifiants, triee", "[render][ring_quartet]") {
  const std::span<const EffectRingQuartetPattern> table = EffectRingQuartetPatternTable();
  REQUIRE(table.size() == 5);
  REQUIRE(std::ranges::is_sorted(table, {}, &EffectRingQuartetPattern::effect_id));
  for (const std::uint32_t effect_id : {231U, 247U, 248U, 259U, 260U}) {
    REQUIRE(EffectRingQuartetPatternsFor(effect_id).size() == 1);
  }
  REQUIRE(EffectRingQuartetPatternsFor(1U).empty());
}

TEST_CASE("le SECOND batisseur ne ressemble au premier que de loin", "[render][ring_quartet]") {
  const EffectRingQuartetPattern second = EffectRingQuartetPatternsFor(248U)[0];
  // ⚠ SES QUATRE PISTES PARTENT DÉJÀ OUVERTES, au deux-centième tic, et
  // ensemble : aucune vague.
  REQUIRE(second.first_tick == kRingQuartetOpensAtTick);
  REQUIRE(second.ticks_per_track == 0);
  // ⚠ ET SON INCLINAISON DÉCROÎT d'une piste à l'autre, là où le premier la
  // garde.
  REQUIRE(second.tilt_per_track == -1);
  REQUIRE(second.style == kRingQuartetStyleThatFades);

  const EffectRingQuartetPattern first = EffectRingQuartetPatternsFor(231U)[0];
  REQUIRE(first.first_tick == 0);
  REQUIRE(first.ticks_per_track == kRingQuartetTicksPerTrack);
  REQUIRE(first.tilt_per_track == 0);
  REQUIRE(first.style == 0);
}

TEST_CASE("l'octet UN est le seul a changer l'alpha ET le rayon", "[render][ring_quartet]") {
  // 247 est le seul site du premier bâtisseur à passer l'octet un.
  const EffectRingQuartetPattern un = EffectRingQuartetPatternsFor(247U)[0];
  REQUIRE(un.alpha == 70);
  REQUIRE(un.first_radius == Approx(11.0F));
  for (const std::uint32_t effect_id : {231U, 259U, 260U}) {
    const EffectRingQuartetPattern other = EffectRingQuartetPatternsFor(effect_id)[0];
    REQUIRE(other.alpha == 50);
    REQUIRE(other.first_radius == Approx(2.0F));
  }
}
