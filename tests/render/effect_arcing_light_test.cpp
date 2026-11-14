// LA LUEUR BLANCHE QUI PASSE EN ARC : le sous-type 109, l'effet 503.
//
// Un bâtisseur qui ne lit de son cas que son image, un mouvement écrit en ligne
// dans le cas 109 du répartiteur des nœuds, et une figure qui fait naître
// l'effet 502 à chaque peinture tant que la piste est jeune.
#include "ro/render/effect_arcing_light.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string_view>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <glm/glm.hpp>

#include "ro/render/effect_arcing_light_seed.h"
#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pattern_table.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"

using Catch::Approx;
using ro::render::AdvanceArcingLightTracks;
using ro::render::ArcingLightSowsFollowUp;
using ro::render::ComposeArcingLightQuad;
using ro::render::EffectArcingLightPattern;
using ro::render::EffectArcingLightPatternsFor;
using ro::render::EffectArcingLightPatternTable;
using ro::render::EffectTrack;
using ro::render::EffectWorldQuad;
using ro::render::kArcingLightSlotOfAlpha;
using ro::render::kArcingLightSlotOfDepthBelow;
using ro::render::kArcingLightSlotOfSideways;
using ro::render::kArcingLightSlotOfSweepDegrees;
using ro::render::kEffectTracksPerNode;
using ro::render::PooledEffectNode;
using ro::render::PooledNodeShape;
using ro::render::SeedArcingLightTracks;
using ro::render::SowArcingLightNode;

namespace {

constexpr std::uint32_t kSoulChangeEffectId = 503;
constexpr float kTolerance = 1.0e-4F;
constexpr double kDegreesPerRadian = 180.0 / 3.14159265358979323846;

std::array<EffectTrack, kEffectTracksPerNode> Sown() {
  std::uint32_t generator = 11U;
  return SeedArcingLightTracks(generator);
}

// Les pistes après autant de tics, caméra au tour et à l'inclinaison donnés.
std::array<EffectTrack, kEffectTracksPerNode> Advanced(std::int32_t ticks, std::int16_t tour = 0,
                                                       std::int16_t tilt = 0) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks = Sown();
  for (std::int32_t tick = 0; tick < ticks; ++tick) {
    AdvanceArcingLightTracks(tracks, tour, tilt);
  }
  return tracks;
}

glm::vec3 CentreDe(const EffectWorldQuad& quad) {
  return (quad.corners[0] + quad.corners[1] + quad.corners[2] + quad.corners[3]) * 0.25F;
}

}  // namespace

TEST_CASE("Lueur en arc : un seul site, l'effet 503, et son image",
          "[render][effectnode][arcinglight]") {
  REQUIRE(EffectArcingLightPatternTable().size() == 1);
  const auto patterns = EffectArcingLightPatternsFor(kSoulChangeEffectId);
  REQUIRE(patterns.size() == 1);
  CHECK(patterns.front().client_function == "Effect_Build_Sprite6dFromParam");
  CHECK(patterns.front().texture == "data\\texture\\effect\\whitelight.tga");
  CHECK(ro::render::KnowsProceduralEffect(kSoulChangeEffectId));
  CHECK(EffectArcingLightPatternsFor(502).empty());
}

TEST_CASE("Lueur en arc : une seule piste allumée, née à gauche et à quarante de haut",
          "[render][effectnode][arcinglight]") {
  const auto tracks = Sown();
  CHECK(tracks[0].active);
  CHECK_FALSE(tracks[1].active);
  CHECK_FALSE(tracks[2].active);
  CHECK_FALSE(tracks[3].active);
  const EffectTrack& track = tracks[0];
  CHECK(track.ticks == 0);
  CHECK(track.radius_at_anchor == 2.5F);
  CHECK(track.value[kArcingLightSlotOfAlpha] == 0.0F);
  CHECK(track.tail[kArcingLightSlotOfSweepDegrees] == 0.0F);
  CHECK(track.tail[kArcingLightSlotOfSideways] == -30.0F);
  CHECK(track.tail[kArcingLightSlotOfDepthBelow] == -40.0F);
}

TEST_CASE("Lueur en arc : le semis tire quatre angles, dans l'ordre du client",
          "[render][effectnode][arcinglight]") {
  std::uint32_t generator = 11U;
  const auto tracks = SeedArcingLightTracks(generator);
  std::uint32_t witness = 11U;
  for (std::size_t rang = 0; rang < 4; ++rang) {
    CHECK(tracks[0].value[rang] ==
          static_cast<float>(ro::render::NextEffectNodeDraw(witness) % 360U));
  }
  CHECK(generator == witness);
}

TEST_CASE("Lueur en arc : le nœud porte le mot cinq, une image et la vie de l'effet",
          "[render][effectnode][arcinglight]") {
  std::uint32_t generator = 3U;
  const EffectArcingLightPattern& pattern =
      EffectArcingLightPatternsFor(kSoulChangeEffectId).front();
  const PooledEffectNode node =
      SowArcingLightNode(pattern, glm::vec3{10.0F, 0.0F, 20.0F}, 500, generator);
  CHECK(node.shape == PooledNodeShape::kArcingLight);
  REQUIRE(node.node.render_word.has_value());
  CHECK(*node.node.render_word == 5U);
  CHECK(node.node.life_ticks == 500);
  REQUIRE(node.resource_paths.size() == 1);
  CHECK(node.resource_paths.front() == "data\\texture\\effect\\whitelight.tga");
  CHECK(node.ground_position == glm::vec3{10.0F, 0.0F, 20.0F});
  CHECK(node.owner_position == glm::vec3{10.0F, 0.0F, 20.0F});
}

TEST_CASE("Lueur en arc : les angles de panneau se rangent tels quels, même éteints",
          "[render][effectnode][arcinglight]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  AdvanceArcingLightTracks(tracks, 123, 45);
  CHECK(tracks[0].style == 123);
  CHECK(tracks[1].style == 45);
  // Une piste éteinte n'avance pas.
  CHECK(tracks[0].ticks == 0);
}

TEST_CASE("Lueur en arc : l'alpha monte dix tics, se tient, puis pâlit jusqu'à zéro",
          "[render][effectnode][arcinglight]") {
  for (std::int32_t tick = 1; tick <= 10; ++tick) {
    CHECK(Advanced(tick)[0].value[kArcingLightSlotOfAlpha] ==
          Approx(20.0F * static_cast<float>(tick)));
  }
  CHECK(Advanced(11)[0].value[kArcingLightSlotOfAlpha] == Approx(200.0F));
  CHECK(Advanced(170)[0].value[kArcingLightSlotOfAlpha] == Approx(200.0F));
  CHECK(Advanced(171)[0].value[kArcingLightSlotOfAlpha] == Approx(195.0F));
  CHECK(Advanced(209)[0].value[kArcingLightSlotOfAlpha] == Approx(5.0F));
  CHECK(Advanced(210)[0].value[kArcingLightSlotOfAlpha] == 0.0F);
  // Sans descendre sous zéro.
  CHECK(Advanced(260)[0].value[kArcingLightSlotOfAlpha] == 0.0F);
}

TEST_CASE("Lueur en arc : la course accélère et s'arrête à trois cent cinquante-neuf",
          "[render][effectnode][arcinglight]") {
  // Un tic : un trentième de degré ; deux tics : un dixième.
  CHECK(Advanced(1)[0].tail[kArcingLightSlotOfSweepDegrees] == Approx(1.0F / 30.0F));
  CHECK(Advanced(2)[0].tail[kArcingLightSlotOfSweepDegrees] == Approx(3.0F / 30.0F));
  // `n (n + 1) / 60` : 357,7 au cent quarante-sixième tic, plafonné au suivant.
  CHECK(Advanced(146)[0].tail[kArcingLightSlotOfSweepDegrees] == Approx(357.7F).margin(0.01F));
  CHECK(Advanced(147)[0].tail[kArcingLightSlotOfSweepDegrees] == 359.0F);
  CHECK(Advanced(400)[0].tail[kArcingLightSlotOfSweepDegrees] == 359.0F);
}

TEST_CASE("Lueur en arc : l'écart et la hauteur se réécrivent depuis l'angle tronqué",
          "[render][effectnode][arcinglight]") {
  for (const std::int32_t ticks : {1, 30, 73, 100, 120, 146, 300}) {
    const EffectTrack track = Advanced(ticks)[0];
    const auto degrees = static_cast<std::int32_t>(track.tail[kArcingLightSlotOfSweepDegrees]);
    CHECK(track.tail[kArcingLightSlotOfSideways] ==
          Approx(static_cast<float>(degrees) / 6.0F - 30.0F).margin(kTolerance));
    const double sinus = std::sin(static_cast<double>(degrees) / kDegreesPerRadian);
    CHECK(track.tail[kArcingLightSlotOfDepthBelow] ==
          Approx(-sinus * 20.0 - 40.0).margin(kTolerance));
    CHECK(track.tail[5] == 0.0F);
  }
  // Arrêtée à droite : 359 / 6 − 30.
  CHECK(Advanced(300)[0].tail[kArcingLightSlotOfSideways] == Approx(29.8333F).margin(1.0e-3F));
}

TEST_CASE("Lueur en arc : le carré se déporte le long du tour de la caméra, en bleu pâle",
          "[render][effectnode][arcinglight]") {
  const glm::vec3 origin{100.0F, 5.0F, -50.0F};
  for (const std::int16_t turn_degrees : {std::int16_t{0}, std::int16_t{90}, std::int16_t{200}}) {
    const auto tracks = Advanced(40, turn_degrees, 0);
    const EffectTrack& track = tracks[0];
    const std::optional<EffectWorldQuad> quad =
        ComposeArcingLightQuad(track, tracks[0].style, tracks[1].style, origin);
    REQUIRE(quad.has_value());
    const double sideways = static_cast<double>(track.tail[kArcingLightSlotOfSideways]);
    const double radians = static_cast<double>(turn_degrees) / kDegreesPerRadian;
    const glm::vec3 centre = CentreDe(*quad);
    CHECK(centre.x ==
          Approx(static_cast<double>(origin.x) + sideways * std::cos(radians)).margin(1.0e-3));
    // Au-dessus du nœud : la hauteur du client descend, la nôtre monte.
    CHECK(centre.y == Approx(origin.y - track.tail[kArcingLightSlotOfDepthBelow]).margin(1.0e-3));
    // L'écart suit le tour dans le repère du client, dont la profondeur est
    // l'opposée de la nôtre.
    CHECK(centre.z ==
          Approx(static_cast<double>(origin.z) - sideways * std::sin(radians)).margin(1.0e-3));
    CHECK(quad->tint[0] == 125);
    CHECK(quad->tint[1] == 125);
    CHECK(quad->tint[2] == 255);
    CHECK(quad->tint[3] == 200);
  }
}

TEST_CASE("Lueur en arc : l'alpha flottant se tronque, et un alpha nul se peint encore",
          "[render][effectnode][arcinglight]") {
  auto tracks = Advanced(20);
  tracks[0].value[kArcingLightSlotOfAlpha] = 12.9F;
  const std::optional<EffectWorldQuad> quad =
      ComposeArcingLightQuad(tracks[0], 0, 0, glm::vec3{0.0F});
  REQUIRE(quad.has_value());
  CHECK(quad->tint[3] == 12);
  // Au deux cent dixième tic, éteint mais toujours émis.
  const auto eteintes = Advanced(210);
  CHECK(ComposeArcingLightQuad(eteintes[0], 0, 0, glm::vec3{0.0F}).has_value());
  // Une piste éteinte ne rend rien.
  CHECK_FALSE(ComposeArcingLightQuad(eteintes[1], 0, 0, glm::vec3{0.0F}).has_value());
}

TEST_CASE("Lueur en arc : un premier coin passé sous le nœud ne se peint pas",
          "[render][effectnode][arcinglight]") {
  auto tracks = Advanced(20);
  tracks[0].tail[kArcingLightSlotOfDepthBelow] = 10.0F;
  CHECK_FALSE(ComposeArcingLightQuad(tracks[0], 0, 0, glm::vec3{0.0F}).has_value());
  CHECK_FALSE(ArcingLightSowsFollowUp(tracks[0], 0, 0));
}

TEST_CASE("Lueur en arc : l'effet 502 naît à chaque peinture jusqu'au cent quatre-vingtième tic",
          "[render][effectnode][arcinglight]") {
  CHECK(ro::render::kArcingLightFollowUpEffectId == 502U);
  // Dès la naissance, avant tout tic.
  CHECK(ArcingLightSowsFollowUp(Sown()[0], 0, 0));
  CHECK(ArcingLightSowsFollowUp(Advanced(179)[0], 0, 0));
  CHECK_FALSE(ArcingLightSowsFollowUp(Advanced(180)[0], 0, 0));
  CHECK_FALSE(ArcingLightSowsFollowUp(Advanced(250)[0], 0, 0));
  // Une piste éteinte ne sème rien.
  CHECK_FALSE(ArcingLightSowsFollowUp(Sown()[1], 0, 0));
}
