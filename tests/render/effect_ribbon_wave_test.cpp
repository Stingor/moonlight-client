#include <array>
#include <cmath>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_ribbon_wave_seed.h"
#include "ro/render/effect_track.h"
#include "ro/render/effect_track_figures.h"
#include "ro/render/effect_track_motion.h"

using Catch::Approx;
using ro::render::AdvanceRibbonWaveTracks;
using ro::render::ComposeEffectTrackRibbon;
using ro::render::EffectRibbonWavePattern;
using ro::render::EffectRibbonWavePatternsFor;
using ro::render::EffectRibbonWavePatternTable;
using ro::render::EffectTrack;
using ro::render::EffectWorldQuad;
using ro::render::kEffectTracksPerNode;
using ro::render::PooledNodeShape;
using ro::render::SeedEffectRibbonWaveTracks;
using ro::render::SowEffectRibbonWaveNode;

namespace {

// Les quatre identifiants que le bâtisseur sert, et rien d'autre.
constexpr std::array<std::uint32_t, 4> kServis = {239, 240, 241, 620};

constexpr std::uint32_t kSeed = 20250716U;

[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> OneWave() {
  std::uint32_t generator = kSeed;
  return SeedEffectRibbonWaveTracks(generator);
}

}  // namespace

TEST_CASE("l'onde de ruban sert quatre identifiants, un patron chacun", "[render][table]") {
  const std::span<const EffectRibbonWavePattern> table = EffectRibbonWavePatternTable();
  REQUIRE(table.size() == kServis.size());
  std::uint32_t previous = 0;
  for (const EffectRibbonWavePattern& pattern : table) {
    // La recherche est une `equal_range` : un tri perdu rendrait des spans
    // vides sans rien casser d'autre.
    REQUIRE(pattern.effect_id > previous);
    previous = pattern.effect_id;
    REQUIRE(pattern.texture.starts_with("data\\texture\\"));
    REQUIRE(pattern.client_function == std::string_view{"Effect_Build_SpinRingNode36"});
  }
  for (const std::uint32_t effect_id : kServis) {
    REQUIRE(EffectRibbonWavePatternsFor(effect_id).size() == 1);
  }
  REQUIRE(EffectRibbonWavePatternsFor(242).empty());

  // ⚠ LE PARAMÈTRE NE CHOISIT QUE LA TEINTE, et trois des quatre prennent la
  // même. Le 620 est le seul à la changer : lui donner celle des autres ferait
  // quatre anneaux de la même couleur.
  CHECK(EffectRibbonWavePatternsFor(620).front().trail_variant == 14);
  for (const std::uint32_t effect_id : {239U, 240U, 241U}) {
    CHECK(EffectRibbonWavePatternsFor(effect_id).front().trail_variant == 0);
  }
}

TEST_CASE("l'onde ne sème qu'UNE piste sur quatre", "[render][pool]") {
  const std::array<EffectTrack, kEffectTracksPerNode> tracks = OneWave();
  CHECK(tracks[0].active);
  for (std::size_t rang = 1; rang < tracks.size(); ++rang) {
    INFO("piste " << rang);
    CHECK_FALSE(tracks[rang].active);
  }
  // Le balayage fait le tour, et la clé vaut quinze.
  CHECK(tracks[0].degrees == 360);
  CHECK(tracks[0].lift_key == Approx(15.0F));
  CHECK(tracks[0].radius_at_anchor == Approx(2.6F));
  CHECK(tracks[0].tilt_degrees == 80);

  // ⚠ LES LONGUEURS NAISSENT NULLES : c'est le mouvement qui les écrit, dès son
  // premier tic. Un ruban semé avec des longueurs inventées montrerait une
  // forme que le client ne montre jamais.
  for (const float length : tracks[0].value) {
    CHECK(length == Approx(0.0F));
  }
}

TEST_CASE("l'ALPHA DE DÉPART se tire, et décale le cycle", "[render][pool]") {
  // ⚠ DEUX ANNEAUX SEMÉS ENSEMBLE NE RESPIRENT PAS EN PHASE. Le mouvement fait
  // descendre l'alpha jusqu'à zéro, puis tout renaît : leur donner le même
  // alpha les ferait battre d'un seul cœur.
  std::uint32_t un = 1U;
  std::uint32_t two = 2U;
  const std::array<EffectTrack, kEffectTracksPerNode> a = SeedEffectRibbonWaveTracks(un);
  const std::array<EffectTrack, kEffectTracksPerNode> b = SeedEffectRibbonWaveTracks(two);
  CHECK(a[0].alpha != b[0].alpha);

  // Les deux tirages tiennent dans leurs bornes.
  for (std::uint32_t seed = 1; seed <= 64U; ++seed) {
    std::uint32_t generator = seed;
    const std::array<EffectTrack, kEffectTracksPerNode> tracks =
        SeedEffectRibbonWaveTracks(generator);
    REQUIRE(tracks[0].alpha >= 0);
    REQUIRE(tracks[0].alpha < 100);
    REQUIRE(tracks[0].azimuth_degrees >= 0);
    REQUIRE(tracks[0].azimuth_degrees < 360);
  }
}

TEST_CASE("l'ONDE est une DEMI-sinusoïde le long du ruban", "[render][pool]") {
  // ⚠ NULLE AUX DEUX BOUTS, MAXIMALE AU MILIEU. Le client parcourt sa table de
  // sinus par pas de neuf degrés sur vingt et une cases : 20 × 9 = 180. Un tour
  // entier aurait fait battre l'onde deux fois le long du ruban.
  std::array<EffectTrack, kEffectTracksPerNode> tracks = OneWave();
  // Un tic où le sinus du temps n'est pas nul, sans quoi l'onde serait plate.
  for (std::int32_t tick = 0; tick < 30; ++tick) {
    AdvanceRibbonWaveTracks(tracks);
  }
  const EffectTrack& track = tracks[0];

  // Les deux bouts valent la clé : l'onde y est nulle.
  CHECK(track.value[0] == Approx(track.lift_key));
  CHECK(track.value[20] == Approx(track.lift_key).margin(0.01F));
  // Le milieu s'en écarte le plus.
  const float offset_from_middle = std::abs(track.value[10] - track.lift_key);
  CHECK(offset_from_middle > 0.0F);
  for (std::size_t creneau = 0; creneau < track.value.size(); ++creneau) {
    INFO("case " << creneau);
    CHECK(std::abs(track.value[creneau] - track.lift_key) <=
          Approx(offset_from_middle).margin(0.01F));
  }
  // ⚠ ET L'AMPLITUDE NE DÉPASSE PAS TROIS DIXIÈMES DE LA CLÉ. Le client
  // multiplie par 0,3 ; l'oublier aurait fait battre le ruban du simple au
  // double.
  CHECK(offset_from_middle <= Approx(0.3F * track.lift_key).margin(0.01F));
}

TEST_CASE("l'anneau RESPIRE : l'alpha tombe, puis tout renaît", "[render][pool]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks = OneWave();
  // Le rayon part au-dessus du seuil : l'alpha descend dès le premier tic.
  const std::int16_t start = tracks[0].alpha;
  AdvanceRibbonWaveTracks(tracks);
  CHECK(tracks[0].alpha == start - 2);
  CHECK(tracks[0].radius_at_anchor == Approx(2.64F));
  CHECK(tracks[0].tilt_degrees == 79);

  // ⚠ ET LA CHUTE À ZÉRO FAIT TOUT RENAÎTRE : le rayon retombe à 2,14 et
  // l'inclinaison remonte à 74. Sans cette renaissance l'anneau s'éteindrait
  // une fois pour toutes, là où le client le fait respirer sans fin.
  bool respawn_seen = false;
  for (std::int32_t tick = 0; tick < 200 && !respawn_seen; ++tick) {
    AdvanceRibbonWaveTracks(tracks);
    if (tracks[0].radius_at_anchor == Approx(2.14F)) {
      respawn_seen = true;
      CHECK(tracks[0].alpha == 0);
      CHECK(tracks[0].tilt_degrees == 74);
    }
  }
  REQUIRE(respawn_seen);

  // Puis l'alpha REMONTE, le rayon étant repassé sous le seuil.
  AdvanceRibbonWaveTracks(tracks);
  CHECK(tracks[0].alpha == 20);
}

TEST_CASE("une piste ÉTEINTE ne bouge pas", "[render][pool]") {
  // Les trois pistes mortes du semis doivent rester intactes : le client saute
  // celles dont le drapeau n'est pas un.
  std::array<EffectTrack, kEffectTracksPerNode> tracks = OneWave();
  for (std::int32_t tick = 0; tick < 10; ++tick) {
    AdvanceRibbonWaveTracks(tracks);
  }
  for (std::size_t rang = 1; rang < tracks.size(); ++rang) {
    INFO("piste " << rang);
    CHECK(tracks[rang].ticks == 0);
    CHECK(tracks[rang].radius_at_anchor == Approx(0.0F));
  }
}

TEST_CASE("un nœud d'onde prend sa forme, et la figure du ruban a une aire", "[render][pool]") {
  const EffectRibbonWavePattern& pattern = EffectRibbonWavePatternsFor(239).front();
  std::uint32_t generator = kSeed;
  ro::render::PooledEffectNode node =
      SowEffectRibbonWaveNode(pattern, glm::vec3{2.0F, 0.0F, -3.0F}, generator);
  CHECK(node.shape == PooledNodeShape::kRibbonWave);
  CHECK(node.trail_variant == pattern.trail_variant);
  // ⚠ IL SUIT SON PORTEUR : la troupe recopie `owner_position` dans
  // `ground_position` à chaque tic, et le semis doit donc poser les deux.
  CHECK(node.owner_position.x == Approx(2.0F));
  CHECK(node.resource_paths.size() == 1);

  // ⚠ AU SEMIS LE RUBAN SORT QUAND MÊME SES QUADS, d'aire NULLE : ses deux
  // bords coïncident tant que les longueurs le sont. Ce n'est pas une absence
  // mais un ruban replié sur son cercle — et c'est pourquoi le mouvement doit
  // l'ouvrir pour qu'il se voie. Attendre une liste VIDE était une supposition,
  // pas une mesure.
  const std::vector<EffectWorldQuad> replie = ComposeEffectTrackRibbon(
      node.tracks[0], node.trail_variant, node.ground_position, node.tracks[0].azimuth_degrees);
  REQUIRE_FALSE(replie.empty());
  CHECK(replie[0].corners[0].x == Approx(replie[0].corners[3].x));
  CHECK(replie[0].corners[0].z == Approx(replie[0].corners[3].z));
  AdvanceRibbonWaveTracks(node.tracks);
  const std::vector<EffectWorldQuad> ribbon = ComposeEffectTrackRibbon(
      node.tracks[0], node.trail_variant, node.ground_position, node.tracks[0].azimuth_degrees);
  CHECK_FALSE(ribbon.empty());
}
