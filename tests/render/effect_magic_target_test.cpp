#include "ro/render/effect_magic_target.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <set>
#include <span>
#include <utility>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_magic_target_seed.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

using Catch::Approx;
using ro::render::AdvanceMagicTargetGridTracks;
using ro::render::ComposeMagicTargetGridCells;
using ro::render::EffectGroundCellPaint;
using ro::render::EffectMagicTargetPattern;
using ro::render::EffectMagicTargetPatternsFor;
using ro::render::EffectMagicTargetPatternTable;
using ro::render::EffectTrack;
using ro::render::kEffectTracksPerNode;
using ro::render::kMagicTargetFarColor;
using ro::render::kMagicTargetGridCenter;
using ro::render::kMagicTargetGridFirstCell;
using ro::render::kMagicTargetGridSide;
using ro::render::kMagicTargetNearColor;
using ro::render::kMagicTargetScaleFloor;
using ro::render::kMagicTargetScaleSwing;
using ro::render::kMagicTargetSecondPassScale;
using ro::render::PooledNodeShape;
using ro::render::SeedEffectMagicTargetTracks;
using ro::render::SowEffectMagicTargetNode;

namespace {

[[nodiscard]] EffectTrack LiveTrack(std::int16_t azimuth, float scale) {
  EffectTrack track;
  track.active = true;
  track.azimuth_degrees = azimuth;
  track.lift_key = scale;
  return track;
}

}  // namespace

TEST_CASE("la grille de visée peint quarante-neuf cases, deux fois chacune") {
  const std::vector<EffectGroundCellPaint> cases =
      ComposeMagicTargetGridCells(LiveTrack(0, kMagicTargetScaleFloor));

  REQUIRE(cases.size() ==
          static_cast<std::size_t>(kMagicTargetGridSide * kMagicTargetGridSide * 2));

  std::set<std::pair<std::int32_t, std::int32_t>> visitees;
  for (const EffectGroundCellPaint& peinture : cases) {
    visitees.emplace(peinture.cell_offset_x, peinture.cell_offset_y);
  }
  CHECK(visitees.size() == static_cast<std::size_t>(kMagicTargetGridSide * kMagicTargetGridSide));
  CHECK(visitees.contains({kMagicTargetGridFirstCell, kMagicTargetGridFirstCell}));
  CHECK(visitees.contains({-kMagicTargetGridFirstCell, -kMagicTargetGridFirstCell}));
  CHECK(visitees.contains({0, 0}));
}

TEST_CASE("une piste éteinte ne peint rien") {
  EffectTrack track = LiveTrack(0, kMagicTargetScaleFloor);
  track.active = false;
  CHECK(ComposeMagicTargetGridCells(track).empty());
}

TEST_CASE("les deux passes d'une case diffèrent par la teinte et par l'échelle") {
  // ⚠ L'ÉCHELLE UN EST CELLE QUI FAIT TENIR L'IMAGE EXACTEMENT SUR LE BLOC :
  // c'est le seul point de la course où les coordonnées de texture s'arrêtent à
  // zéro et à un, et c'est ce qui rend ce test lisible.
  const std::vector<EffectGroundCellPaint> cases = ComposeMagicTargetGridCells(LiveTrack(0, 1.0F));
  const EffectGroundCellPaint& franche = cases[0];
  const EffectGroundCellPaint& pale = cases[1];

  CHECK(franche.cell_offset_x == pale.cell_offset_x);
  CHECK(franche.cell_offset_y == pale.cell_offset_y);
  CHECK(franche.color == kMagicTargetNearColor);
  CHECK(pale.color == kMagicTargetFarColor);
  // La passe pâle est PLUS GRANDE en coordonnées de texture, donc plus PETITE à
  // l'écran : le cercle qu'elle montre est en retrait du premier.
  CHECK(pale.texture_uv[0].x < franche.texture_uv[0].x);

  // Sans rotation et à l'échelle un, le coin de plus petites coordonnées de la
  // première case est l'origine de l'image.
  CHECK(franche.texture_uv[0].x == Approx(0.0F).margin(1e-5));
  CHECK(franche.texture_uv[0].y == Approx(0.0F).margin(1e-5));
  const float one_step = 1.0F / static_cast<float>(kMagicTargetGridSide);
  CHECK(franche.texture_uv[1].x == Approx(one_step).margin(1e-5));
  CHECK(franche.texture_uv[1].y == Approx(0.0F).margin(1e-5));
  CHECK(franche.texture_uv[2].x == Approx(0.0F).margin(1e-5));
  CHECK(franche.texture_uv[2].y == Approx(one_step).margin(1e-5));
}

TEST_CASE("le quart de tour échange les deux axes de l'image") {
  const std::vector<EffectGroundCellPaint> right = ComposeMagicTargetGridCells(LiveTrack(0, 1.0F));
  const std::vector<EffectGroundCellPaint> tournee =
      ComposeMagicTargetGridCells(LiveTrack(90, 1.0F));

  // Le coin de plus petites coordonnées de la première case est à l'écart
  // (-3.5, -3.5) du centre. Tourné d'un quart de tour, il devient (-3.5, +3.5).
  const float cote = static_cast<float>(kMagicTargetGridSide);
  CHECK(right[0].texture_uv[0].x == Approx(0.0F).margin(1e-5));
  CHECK(tournee[0].texture_uv[0].x ==
        Approx((-kMagicTargetGridCenter + kMagicTargetGridCenter) / cote).margin(1e-4));
  CHECK(tournee[0].texture_uv[0].y ==
        Approx((kMagicTargetGridCenter + kMagicTargetGridCenter) / cote).margin(1e-4));
}

TEST_CASE("le mouvement tourne d'un degré par tic et fait battre l'échelle") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks = SeedEffectMagicTargetTracks();
  REQUIRE(tracks.front().active);
  // ⚠ TROIS PISTES SUR QUATRE SONT ÉTEINTES, et le mouvement n'y touche pas.
  CHECK_FALSE(tracks[1].active);
  CHECK(tracks.front().azimuth_degrees == 0);
  CHECK(tracks.front().lift_key == Approx(kMagicTargetScaleFloor));

  AdvanceMagicTargetGridTracks(tracks);
  CHECK(tracks.front().azimuth_degrees == 1);
  CHECK(tracks[1].azimuth_degrees == 0);

  // Au quart de tour, le sinus vaut un : l'échelle est à son maximum, et c'est
  // le cercle le plus PETIT de la course.
  for (std::int32_t tick = 1; tick < 90; ++tick) {
    AdvanceMagicTargetGridTracks(tracks);
  }
  CHECK(tracks.front().azimuth_degrees == 90);
  CHECK(tracks.front().lift_key == Approx(kMagicTargetScaleFloor + kMagicTargetScaleSwing));

  // Aux trois quarts, il vaut moins un : l'image tient exactement sur le bloc.
  for (std::int32_t tick = 90; tick < 270; ++tick) {
    AdvanceMagicTargetGridTracks(tracks);
  }
  CHECK(tracks.front().azimuth_degrees == 270);
  CHECK(tracks.front().lift_key == Approx(kMagicTargetScaleFloor - kMagicTargetScaleSwing));
}

TEST_CASE("l'azimut revient dans le tour au lieu de déborder") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks = SeedEffectMagicTargetTracks();
  tracks.front().azimuth_degrees = 359;
  AdvanceMagicTargetGridTracks(tracks);
  CHECK(tracks.front().azimuth_degrees == 0);
}

TEST_CASE("la seconde passe suit l'échelle de la première") {
  const float scale = 1.7F;
  const std::vector<EffectGroundCellPaint> cases = ComposeMagicTargetGridCells(LiveTrack(0, scale));
  const float cote = static_cast<float>(kMagicTargetGridSide);
  // Coin de plus petites coordonnées de la première case : écart (-3.5, -3.5).
  CHECK(cases[0].texture_uv[0].x ==
        Approx((-kMagicTargetGridCenter * scale + kMagicTargetGridCenter) / cote));
  CHECK(cases[1].texture_uv[0].x ==
        Approx((-kMagicTargetGridCenter * (scale + kMagicTargetSecondPassScale) +
                kMagicTargetGridCenter) /
               cote));
}

TEST_CASE("la table des grilles de visée est rangée et nomme son image") {
  const std::span<const EffectMagicTargetPattern> table = EffectMagicTargetPatternTable();
  REQUIRE_FALSE(table.empty());
  for (std::size_t rang = 1; rang < table.size(); ++rang) {
    CHECK(table[rang - 1].effect_id <= table[rang].effect_id);
  }
  for (const EffectMagicTargetPattern& pattern : table) {
    CHECK_FALSE(pattern.texture.empty());
    CHECK_FALSE(pattern.client_function.empty());
    CHECK_FALSE(EffectMagicTargetPatternsFor(pattern.effect_id).empty());
  }
}

TEST_CASE("le semis pose la forme de la grille et son image") {
  const std::span<const EffectMagicTargetPattern> table = EffectMagicTargetPatternTable();
  REQUIRE_FALSE(table.empty());
  const ro::render::PooledEffectNode node =
      SowEffectMagicTargetNode(table.front(), glm::vec3{10.0F, 0.0F, -20.0F}, 120);

  CHECK(node.shape == PooledNodeShape::kMagicTargetGrid);
  CHECK(node.node.life_ticks == 120);
  CHECK(node.ground_position.x == Approx(10.0F));
  REQUIRE(node.resource_paths.size() == 1);
  CHECK(node.resource_paths.front() == table.front().texture);
  CHECK(node.tracks.front().active);
}
