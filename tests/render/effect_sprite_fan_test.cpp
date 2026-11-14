#include "ro/render/effect_sprite_fan.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_sprite_fan_seed.h"

using Catch::Approx;
using ro::render::AdvanceSpriteFanTracks;
using ro::render::ComposeSpriteFanPoses;
using ro::render::EffectSpriteFanPattern;
using ro::render::EffectSpriteFanPatternsFor;
using ro::render::EffectSpriteFanPatternTable;
using ro::render::EffectSpriteFanTint;
using ro::render::EffectTrack;
using ro::render::EffectTrailStep;
using ro::render::kEffectTracksPerNode;
using ro::render::kSpriteFanAlphaShare;
using ro::render::kSpriteFanBlue;
using ro::render::kSpriteFanBlueMark;
using ro::render::kSpriteFanDegreesPerTrack;
using ro::render::kSpriteFanLastSowTick;
using ro::render::kSpriteFanLayerMark;
using ro::render::kSpriteFanSpawnHeight;
using ro::render::kSpriteFanSpawnRadius;
using ro::render::kSpriteFanSpreadStep;
using ro::render::kSpriteFanUntintedMark;
using ro::render::SeedEffectSpriteFanTracks;
using ro::render::SowEffectSpriteFanNode;

namespace {

[[nodiscard]] EffectSpriteFanPattern TestPattern(std::uint8_t mark) {
  EffectSpriteFanPattern pattern;
  pattern.effect_id = 1;
  pattern.client_function = "test";
  pattern.sprite_base = "essai";
  pattern.color_mark = mark;
  return pattern;
}

[[nodiscard]] float FlatRadiusOf(glm::vec3 point) {
  return std::sqrt(point.x * point.x + point.z * point.z);
}

}  // namespace

TEST_CASE("un noeud rend QUATRE poses, une par piste", "[render][sprite_fan]") {
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectSpriteFanTracks(TestPattern(kSpriteFanLayerMark), 0);
  const std::vector<EffectTrailStep> poses =
      ComposeSpriteFanPoses(tracks, {}, glm::vec3{0.0F, 0.0F, 0.0F});
  // ⚠ QUATRE, ET NON SEIZE : le constructeur du client ne déclare QU'UN cran de
  // traînée, et le bâtisseur ne le change pas.
  REQUIRE(poses.size() == kEffectTracksPerNode);
}

TEST_CASE("l'eventail croise ses crans avec ses pistes", "[render][sprite_fan]") {
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectSpriteFanTracks(TestPattern(kSpriteFanLayerMark), 0);
  std::vector<EffectTrailStep> crans(3);
  crans[0].ground_position = {0.0F, 0.0F, 0.0F};
  crans[1].ground_position = {10.0F, 0.0F, 0.0F};
  crans[2].ground_position = {20.0F, 0.0F, 0.0F};
  const std::vector<EffectTrailStep> poses =
      ComposeSpriteFanPoses(tracks, crans, glm::vec3{0.0F, 0.0F, 0.0F});
  // ⚠ C'EST CE CROISEMENT QUI FAIT L'ÉVENTAIL, et il est seul de la famille :
  // le sprite éclatant rend une pose par cran, celui-ci une par cran ET par
  // piste. Le traiter comme l'autre n'en montrerait qu'un quart.
  REQUIRE(poses.size() == kEffectTracksPerNode * crans.size());
  // La position est la SOMME des deux points.
  REQUIRE(poses[1].ground_position.x == Approx(tracks[0].anchor.x + 10.0F).epsilon(1e-4));
}

TEST_CASE("une piste eteinte ne rend aucune pose", "[render][sprite_fan]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectSpriteFanTracks(TestPattern(kSpriteFanLayerMark), 0);
  tracks[0].active = false;
  tracks[2].active = false;
  REQUIRE(ComposeSpriteFanPoses(tracks, {}, glm::vec3{0.0F, 0.0F, 0.0F}).size() == 2);
}

TEST_CASE("les quatre pistes naissent aux quatre coins d'un cercle", "[render][sprite_fan]") {
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectSpriteFanTracks(TestPattern(kSpriteFanLayerMark), 0);
  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    REQUIRE(tracks[rang].azimuth_degrees ==
            kSpriteFanDegreesPerTrack * static_cast<std::int16_t>(rang));
    REQUIRE(FlatRadiusOf(tracks[rang].anchor) == Approx(kSpriteFanSpawnRadius).epsilon(1e-4));
    // L'ancre est tenue dans le repère du client : la hauteur y est telle quelle.
    REQUIRE(tracks[rang].anchor.y == Approx(kSpriteFanSpawnHeight));
  }
}

TEST_CASE("l'eventail nait AU-DESSUS du noeud, branches tournees sans miroir",
          "[render][sprite_fan]") {
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectSpriteFanTracks(TestPattern(kSpriteFanLayerMark), 0);
  const std::vector<EffectTrailStep> poses =
      ComposeSpriteFanPoses(tracks, {}, glm::vec3{0.0F, 0.0F, 0.0F});
  // ⚠ LE REPÈRE DU CLIENT DESCEND : son moins cinq est un cinq AU-DESSUS, et la
  // profondeur d'une branche à quarante-cinq degrés se nie en passant par `S`.
  for (const EffectTrailStep& pose : poses) {
    REQUIRE(pose.ground_position.y == Approx(-kSpriteFanSpawnHeight));
  }
  REQUIRE(poses[1].ground_position.x > 0.0F);
  REQUIRE(poses[1].ground_position.z < 0.0F);
}

TEST_CASE("le second eventail tombe ENTRE les branches du premier", "[render][sprite_fan]") {
  const std::array<EffectTrack, kEffectTracksPerNode> first =
      SeedEffectSpriteFanTracks(TestPattern(kSpriteFanLayerMark), 0);
  const std::array<EffectTrack, kEffectTracksPerNode> second =
      SeedEffectSpriteFanTracks(TestPattern(kSpriteFanLayerMark), 1);
  // ⚠ QUARANTE-CINQ FOIS QUATRE FONT CENT QUATRE-VINGTS : les huit branches se
  // répartissent sur le tour. Un autre décalage les superposerait deux à deux.
  for (std::size_t rang = 0; rang < first.size(); ++rang) {
    REQUIRE(second[rang].azimuth_degrees - first[rang].azimuth_degrees == 180);
  }
}

TEST_CASE("chaque piste s'ecarte dans la direction de son azimut", "[render][sprite_fan]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectSpriteFanTracks(TestPattern(kSpriteFanLayerMark), 0);
  const std::array<EffectTrack, kEffectTracksPerNode> before = tracks;
  AdvanceSpriteFanTracks(tracks);
  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    const float gagne = FlatRadiusOf(tracks[rang].anchor) - FlatRadiusOf(before[rang].anchor);
    REQUIRE(gagne == Approx(kSpriteFanSpreadStep).epsilon(1e-3));
    // ⚠ L'ÉVENTAIL S'OUVRE À PLAT : le client laisse la hauteur où elle est.
    REQUIRE(tracks[rang].anchor.y == Approx(before[rang].anchor.y));
  }
}

TEST_CASE("une piste eteinte ne s'ecarte pas", "[render][sprite_fan]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectSpriteFanTracks(TestPattern(kSpriteFanLayerMark), 0);
  tracks[1].active = false;
  const glm::vec3 before = tracks[1].anchor;
  AdvanceSpriteFanTracks(tracks);
  REQUIRE(tracks[1].anchor.x == Approx(before.x));
  REQUIRE(tracks[1].anchor.z == Approx(before.z));
}

TEST_CASE("la marque choisit la teinte, et l'alpha est rabattu", "[render][sprite_fan]") {
  const auto expected = static_cast<std::uint8_t>(255.0F * kSpriteFanAlphaShare);

  const std::array<std::uint8_t, 4> calques = EffectSpriteFanTint(kSpriteFanLayerMark);
  REQUIRE(calques[0] == 255);
  REQUIRE(calques[3] == expected);

  const std::array<std::uint8_t, 4> blue = EffectSpriteFanTint(kSpriteFanBlueMark);
  REQUIRE(blue[0] == kSpriteFanBlue[0]);
  REQUIRE(blue[1] == kSpriteFanBlue[1]);
  REQUIRE(blue[2] == kSpriteFanBlue[2]);
  REQUIRE(blue[3] == expected);

  // ⚠ LA MARQUE QUATRE NE POSE RIEN CHEZ LE CLIENT : son dessinateur n'a que
  // deux branches. Nous rendons du blanc plein plutôt que du sable, et c'est un
  // choix, pas une mesure.
  const std::array<std::uint8_t, 4> sans = EffectSpriteFanTint(kSpriteFanUntintedMark);
  REQUIRE(sans[3] == 255);
}

TEST_CASE("la teinte de la piste voyage jusqu'a la pose", "[render][sprite_fan]") {
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectSpriteFanTracks(TestPattern(kSpriteFanBlueMark), 0);
  const std::vector<EffectTrailStep> poses =
      ComposeSpriteFanPoses(tracks, {}, glm::vec3{0.0F, 0.0F, 0.0F});
  REQUIRE(poses[0].color[0] == Approx(static_cast<float>(kSpriteFanBlue[0])));
  REQUIRE(poses[0].color[2] == Approx(static_cast<float>(kSpriteFanBlue[2])));
}

TEST_CASE("le noeud seme porte son sprite et la bonne forme", "[render][sprite_fan]") {
  const ro::render::PooledEffectNode node =
      SowEffectSpriteFanNode(TestPattern(kSpriteFanLayerMark), glm::vec3{1.0F, 2.0F, 3.0F}, 100, 0);
  REQUIRE(node.shape == ro::render::PooledNodeShape::kSpriteFan);
  REQUIRE(node.resource_paths.size() == 1);
  // ⚠ LE NŒUD SUIT SON PORTEUR : son mouvement recopie la position à chaque
  // tic, et le semis part de la même.
  REQUIRE(node.owner_position.z == Approx(3.0F));
}

TEST_CASE("la table des eventails porte les trois identifiants, triee", "[render][sprite_fan]") {
  const std::span<const EffectSpriteFanPattern> table = EffectSpriteFanPatternTable();
  REQUIRE(table.size() == 3);
  REQUIRE(std::ranges::is_sorted(table, {}, &EffectSpriteFanPattern::effect_id));
  for (const std::uint32_t effect_id : {901U, 923U, 941U}) {
    REQUIRE(EffectSpriteFanPatternsFor(effect_id).size() == 1);
  }
  REQUIRE(EffectSpriteFanPatternsFor(1U).empty());
}

TEST_CASE("les trois identifiants n'ont ni la meme teinte ni le meme sprite",
          "[render][sprite_fan]") {
  REQUIRE(EffectSpriteFanPatternsFor(901U)[0].color_mark == kSpriteFanLayerMark);
  REQUIRE(EffectSpriteFanPatternsFor(923U)[0].color_mark == kSpriteFanBlueMark);
  REQUIRE(EffectSpriteFanPatternsFor(941U)[0].color_mark == kSpriteFanUntintedMark);
  // Les deux premiers partagent leur sprite ; le troisième en a un autre.
  REQUIRE(EffectSpriteFanPatternsFor(901U)[0].sprite_base ==
          EffectSpriteFanPatternsFor(923U)[0].sprite_base);
  REQUIRE(EffectSpriteFanPatternsFor(941U)[0].sprite_base !=
          EffectSpriteFanPatternsFor(901U)[0].sprite_base);
}

TEST_CASE("le semis s'arrete au SECOND tic", "[render][sprite_fan]") {
  // ⚠ LA GARDE DU CLIENT EST « âge inférieur ou égal à un », pas « âge nul ».
  // N'en semer qu'un montrerait la moitié de l'éventail.
  REQUIRE(kSpriteFanLastSowTick == 1);
}

TEST_CASE("sa forme se dessine par un SPRITE, et le dit", "[render][sprite_fan]") {
  // ⚠ C'EST CE CONTRÔLE QUI MANQUAIT, et son absence a coûté trois identifiants
  // en table sans un seul à l'écran : la fenêtre cherchait le `.spr` de cette
  // forme dans son cache de TEXTURES, ne l'y trouvait pas, et la comptait « sans
  // texture ». Une forme de sprite doit se déclarer, ou elle ne paraît jamais.
  REQUIRE(ro::render::EffectNodeShapeDrawsASprite(ro::render::PooledNodeShape::kSpriteFan));
  REQUIRE(ro::render::EffectNodeShapeDrawsASprite(ro::render::PooledNodeShape::kSprite));
  // Et une forme qui peint une texture ne doit surtout pas le dire.
  REQUIRE_FALSE(ro::render::EffectNodeShapeDrawsASprite(ro::render::PooledNodeShape::kRingBand));
}

TEST_CASE("le bleu de la marque un a le rouge ET le vert a cinquante-cinq",
          "[render][sprite_fan]") {
  REQUIRE(kSpriteFanBlue[0] == 55);
  REQUIRE(kSpriteFanBlue[1] == 55);
  REQUIRE(kSpriteFanBlue[2] == 255);
}

TEST_CASE("le halo a demi-alpha, et la marque zero y rougeoie", "[render][sprite_fan]") {
  const auto half = static_cast<std::uint8_t>(255.0F * ro::render::kSpriteFanHaloAlphaShare);
  const std::array<std::uint8_t, 4> calques =
      ro::render::EffectSpriteFanHaloTint(kSpriteFanLayerMark);
  REQUIRE(calques[0] == 255);
  REQUIRE(calques[1] == 55);
  REQUIRE(calques[2] == 55);
  REQUIRE(calques[3] == half);
  const std::array<std::uint8_t, 4> blue = ro::render::EffectSpriteFanHaloTint(kSpriteFanBlueMark);
  REQUIRE(blue[2] == 255);
  REQUIRE(blue[3] == half);
  // ⚠ LA MARQUE QUATRE NE POSE RIEN : le halo reprend la teinte de la première
  // passe.
  REQUIRE(ro::render::EffectSpriteFanHaloTint(kSpriteFanUntintedMark) ==
          EffectSpriteFanTint(kSpriteFanUntintedMark));

  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectSpriteFanTracks(TestPattern(kSpriteFanLayerMark), 0);
  const std::vector<EffectTrailStep> poses =
      ComposeSpriteFanPoses(tracks, {}, glm::vec3{0.0F, 0.0F, 0.0F});
  const std::vector<EffectTrailStep> halos =
      ro::render::ComposeSpriteFanHaloPoses(tracks, {}, glm::vec3{0.0F, 0.0F, 0.0F});
  REQUIRE(halos.size() == poses.size());
  REQUIRE(halos[2].ground_position.x == Approx(poses[2].ground_position.x));
  REQUIRE(halos[2].alpha == Approx(static_cast<float>(half)));
}

TEST_CASE("l'eventail joue son animation une fois, quatre tics par image", "[render][sprite_fan]") {
  ro::render::PooledEffectNode node =
      SowEffectSpriteFanNode(TestPattern(kSpriteFanLayerMark), glm::vec3{}, 500, 0);
  REQUIRE(node.node.frame_hold_ticks == 4);
  REQUIRE_FALSE(node.node.loops_frames);
  // Deux images : l'âge zéro passe à la seconde, l'âge quatre dépasse la
  // dernière — le nœud meurt à sa cinquième mise à jour.
  node.node.texture_count = 2;
  for (int tick = 0; tick < 4; ++tick) {
    REQUIRE(ro::render::AdvanceEffectNode(node.node));
  }
  REQUIRE_FALSE(ro::render::AdvanceEffectNode(node.node));
}
