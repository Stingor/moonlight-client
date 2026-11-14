// LA PETITE BOÎTE QUI TOURNE (sous-type 114, effet 517).

#include "ro/render/effect_spinning_box.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_life_table.h"
#include "ro/render/effect_node_pattern_table.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_spinning_box_seed.h"
#include "ro/render/effect_track.h"

using Catch::Approx;
using ro::render::AdvanceSpinningBoxTracks;
using ro::render::ComposeSpinningBoxQuads;
using ro::render::EffectSpinningBoxPattern;
using ro::render::EffectSpinningBoxPatternsFor;
using ro::render::EffectTrack;
using ro::render::EffectWorldQuad;
using ro::render::PooledEffectNode;
using ro::render::PooledNodeShape;
using ro::render::SowEffectSpinningBoxNode;

namespace {

constexpr std::uint32_t kSpinningBoxEffect = 517;

[[nodiscard]] PooledEffectNode Sow(std::uint32_t seed) {
  const std::span<const EffectSpinningBoxPattern> own_entries =
      EffectSpinningBoxPatternsFor(kSpinningBoxEffect);
  REQUIRE(own_entries.size() == 1);
  return SowEffectSpinningBoxNode(own_entries.front(), glm::vec3{0.0F, 0.0F, 0.0F}, seed);
}

}  // namespace

TEST_CASE("Boîte : 517 la pose, avec white02, et elle vit autant que l'effet",
          "[render][spinning_box]") {
  const std::span<const EffectSpinningBoxPattern> own_entries =
      EffectSpinningBoxPatternsFor(kSpinningBoxEffect);
  REQUIRE(own_entries.size() == 1);
  CHECK(own_entries.front().texture == "data\\texture\\effect\\white02.bmp");
  CHECK(ro::render::KnowsProceduralEffect(kSpinningBoxEffect));

  const PooledEffectNode node = Sow(7);
  CHECK(node.shape == PooledNodeShape::kSpinningBox);
  CHECK(node.node.life_ticks == ro::render::EffectNodeLifeFor(kSpinningBoxEffect));
  REQUIRE(node.node.render_word.has_value());
  CHECK(*node.node.render_word == 5U);
  // UNE SEULE PISTE VIVE, et ce que le bâtisseur y écrit.
  CHECK(node.tracks[0].active);
  CHECK_FALSE(node.tracks[1].active);
  CHECK_FALSE(node.tracks[2].active);
  CHECK_FALSE(node.tracks[3].active);
  const EffectTrack& track = node.tracks[0];
  CHECK(track.alpha == 0);
  CHECK(track.radius_at_anchor == 1.6F);
  CHECK(track.value[ro::render::kSpinningBoxSlotOfLift] == -12.0F);
  CHECK(track.mark[ro::render::kSpinningBoxMarkOfRedGreen] < 31);
  CHECK(track.tilt_degrees >= 0);
  CHECK(track.tilt_degrees < 360);
  CHECK(track.azimuth_degrees >= 0);
  CHECK(track.azimuth_degrees < 360);
}

TEST_CASE("Boîte : l'alpha monte de deux jusqu'à cent quarante, les angles d'un degré",
          "[render][spinning_box]") {
  PooledEffectNode node = Sow(3);
  node.tracks[0].tilt_degrees = 358;
  node.tracks[0].azimuth_degrees = 359;
  AdvanceSpinningBoxTracks(node.tracks);
  CHECK(node.tracks[0].alpha == 2);
  CHECK(node.tracks[0].tilt_degrees == 359);
  CHECK(node.tracks[0].azimuth_degrees == 0);
  for (int tick = 0; tick < 200; ++tick) {
    AdvanceSpinningBoxTracks(node.tracks);
  }
  CHECK(node.tracks[0].alpha == 140);
}

TEST_CASE("Boîte : six faces, chacune de son bleu, au-dessus du porteur",
          "[render][spinning_box]") {
  PooledEffectNode node = Sow(11);
  EffectTrack& track = node.tracks[0];
  track.alpha = 140;
  track.azimuth_degrees = 0;
  // Le sinus de l'inclinaison vaut un : le rayon est plein.
  track.tilt_degrees = 90;
  track.mark[ro::render::kSpinningBoxMarkOfRedGreen] = 17;

  const std::vector<EffectWorldQuad> faces = ComposeSpinningBoxQuads(track, {10.0F, 0.0F, 20.0F});
  REQUIRE(faces.size() == ro::render::kSpinningBoxFaces);
  for (std::size_t face = 0; face < faces.size(); ++face) {
    INFO("face " << face);
    CHECK(faces[face].tint[0] == 17);
    CHECK(faces[face].tint[1] == 17);
    CHECK(faces[face].tint[2] == ro::render::kSpinningBoxFaceBlues[face]);
    CHECK(faces[face].tint[3] == 140);
  }
  // Le carré du bas à douze unités, sans balancement à l'azimut zéro ; celui du
  // haut un virgule six plus haut.
  for (const glm::vec3& corner : faces.front().corners) {
    CHECK(corner.y == Approx(12.0F));
    CHECK(std::hypot(corner.x - 10.0F, corner.z - 20.0F) == Approx(1.6F));
  }
  for (const glm::vec3& corner : faces.back().corners) {
    CHECK(corner.y == Approx(13.6F));
  }
  // Le premier coin est sur l'azimut, le suivant un quart de tour plus loin,
  // la profondeur retournée.
  CHECK(faces.front().corners[0].x == Approx(11.6F));
  CHECK(faces.front().corners[1].z == Approx(20.0F - 1.6F));
}

TEST_CASE("Boîte : l'inclinaison fait respirer le rayon, l'azimut balancer la hauteur",
          "[render][spinning_box]") {
  PooledEffectNode node = Sow(5);
  EffectTrack& track = node.tracks[0];
  track.tilt_degrees = 270;
  track.azimuth_degrees = 90;
  const std::vector<EffectWorldQuad> faces = ComposeSpinningBoxQuads(track, {});
  REQUIRE_FALSE(faces.empty());
  for (const glm::vec3& corner : faces.front().corners) {
    CHECK(std::hypot(corner.x, corner.z) == Approx(1.6F * 0.8F));
    // Deux fois le sinus de l'azimut, compté vers le bas chez le client.
    CHECK(corner.y == Approx(10.0F));
  }
}

TEST_CASE("Boîte : une piste éteinte ne peint rien", "[render][spinning_box]") {
  const EffectTrack faded;
  CHECK(ComposeSpinningBoxQuads(faded, {}).empty());
}
