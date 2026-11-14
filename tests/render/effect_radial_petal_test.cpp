// LES PÉTALES QUI S'ÉCARTENT : le sous-type 94, les effets 411 et 1150.
//
// Un bâtisseur d'éparpillement radial, un mouvement écrit en ligne dans le
// cas 94 du répartiteur des nœuds, et une figure en étoile projetée.
#include "ro/render/effect_radial_petal.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string_view>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <glm/glm.hpp>

#include "ro/render/effect_node_pattern_table.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_node_seed.h"
#include "ro/render/effect_particle_quad.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_radial_petal_seed.h"

using Catch::Approx;
using ro::render::AdvanceRadialPetalTracks;
using ro::render::ComposeRadialPetalQuad;
using ro::render::EffectNodeScheduleLastSowTick;
using ro::render::EffectNodeScheduleSowsAt;
using ro::render::EffectParticleBlend;
using ro::render::EffectRadialPetalPattern;
using ro::render::EffectRadialPetalPatternsFor;
using ro::render::EffectRadialPetalPatternTable;
using ro::render::EffectTrack;
using ro::render::EffectWorldQuad;
using ro::render::kEffectTracksPerNode;
using ro::render::PooledEffectNode;
using ro::render::PooledNodeShape;
using ro::render::RadialPetalSowTick;
using ro::render::SeedRadialPetalTracks;
using ro::render::SowRadialPetalNode;

namespace {

constexpr std::uint32_t kPeongEffectId = 411;
constexpr std::uint32_t kPeongAtOnceEffectId = 1150;
constexpr std::size_t kNodesPerEffect = 16;
constexpr float kTolerance = 1.0e-4F;
constexpr double kDegreesPerRadian = 180.0 / 3.14159265358979323846;

// UNE PISTE VIVE sans retard, au cap donné.
EffectTrack TrackAtHeading(std::int16_t cap) {
  std::uint32_t generator = 7U;
  std::array<EffectTrack, kEffectTracksPerNode> tracks = SeedRadialPetalTracks(0, generator);
  EffectTrack track = tracks.front();
  track.ticks = 0;
  track.tilt_degrees = cap;
  track.tail[3] = 0.0F;
  track.tail[5] = 0.0F;
  return track;
}

// Les pistes après autant de tics, une seule vive.
std::array<EffectTrack, kEffectTracksPerNode> Advanced(const EffectTrack& track,
                                                       std::int32_t ticks) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  tracks.front() = track;
  for (std::int32_t tick = 0; tick < ticks; ++tick) {
    AdvanceRadialPetalTracks(tracks, 0, 0);
  }
  return tracks;
}

glm::vec3 CentreDe(const EffectWorldQuad& quad) {
  return (quad.corners[0] + quad.corners[1] + quad.corners[2] + quad.corners[3]) * 0.25F;
}

}  // namespace

TEST_CASE("Pétales : seize nœuds par effet, quatre quadrants, quatre images",
          "[render][effectnode][petal]") {
  REQUIRE(EffectRadialPetalPatternTable().size() == 2 * kNodesPerEffect);
  const std::array<std::string_view, 4> images = {
      "data\\texture\\effect\\peong1.tga", "data\\texture\\effect\\peong2.tga",
      "data\\texture\\effect\\peong3.tga", "data\\texture\\effect\\peong2.tga"};
  for (const std::uint32_t effect_id : {kPeongEffectId, kPeongAtOnceEffectId}) {
    const auto patterns = EffectRadialPetalPatternsFor(effect_id);
    REQUIRE(patterns.size() == kNodesPerEffect);
    CHECK(ro::render::KnowsProceduralEffect(effect_id));
    for (std::size_t rang = 0; rang < patterns.size(); ++rang) {
      const EffectRadialPetalPattern& pattern = patterns[rang];
      CHECK(pattern.client_function == "Effect_Build_RadialScatter5eFromParam");
      CHECK(pattern.texture == images[rang % images.size()]);
      // La boucle du cas : chaque tour avance d'un quadrant.
      CHECK(pattern.base_heading_degrees == static_cast<std::int16_t>(90 * (rang / images.size())));
    }
  }
  CHECK(EffectRadialPetalPatternsFor(412).empty());
}

TEST_CASE("Pétales : l'entier du site est le tic du semis", "[render][effectnode][petal]") {
  CHECK(RadialPetalSowTick(0) == 35);
  CHECK(RadialPetalSowTick(1) == 0);
  CHECK(RadialPetalSowTick(7) == 7);
  for (const EffectRadialPetalPattern& pattern : EffectRadialPetalPatternsFor(kPeongEffectId)) {
    CHECK_FALSE(EffectNodeScheduleSowsAt(pattern.schedule, 0));
    CHECK(EffectNodeScheduleSowsAt(pattern.schedule, 35));
    CHECK(EffectNodeScheduleLastSowTick(pattern.schedule) == 35);
  }
  for (const EffectRadialPetalPattern& pattern :
       EffectRadialPetalPatternsFor(kPeongAtOnceEffectId)) {
    CHECK(EffectNodeScheduleSowsAt(pattern.schedule, 0));
    CHECK_FALSE(EffectNodeScheduleSowsAt(pattern.schedule, 35));
  }
}

TEST_CASE("Pétales : le semis allume quatre pistes dans leur quadrant",
          "[render][effectnode][petal]") {
  for (std::uint32_t seed = 1U; seed < 40U; ++seed) {
    std::uint32_t generator = seed;
    const auto tracks = SeedRadialPetalTracks(270, generator);
    for (const EffectTrack& track : tracks) {
      CHECK(track.active);
      CHECK(track.alpha == 0);
      CHECK(track.lift_key == Approx(0.03F));
      CHECK(track.ticks <= 0);
      CHECK(track.ticks > -11);
      CHECK(track.radius_at_anchor >= 2.5F - kTolerance);
      CHECK(track.radius_at_anchor <= 4.5F + kTolerance);
      // Un pétale reste dans son quadrant : 270 plus moins de quatre-vingt-dix.
      CHECK(track.tilt_degrees >= 270);
      CHECK(track.tilt_degrees < 360);
      // Il naît à quatre unités du nœud, le long de son cap, à hauteur nulle.
      const double cap = static_cast<double>(track.tilt_degrees) / kDegreesPerRadian;
      CHECK(track.tail[3] == Approx(4.0 * std::cos(cap)).margin(kTolerance));
      CHECK(track.tail[5] == Approx(4.0 * std::sin(cap)).margin(kTolerance));
      CHECK(track.tail[4] == 0.0F);
      // La base du saut : 0xC0800000, quatre unités au-dessus en repère du client.
      CHECK(track.anchor.y == -4.0F);
    }
  }
}

TEST_CASE("Pétales : le nœud porte le mot cinq, une image et la vie de l'effet",
          "[render][effectnode][petal]") {
  std::uint32_t generator = 3U;
  const EffectRadialPetalPattern& pattern = EffectRadialPetalPatternsFor(kPeongEffectId).front();
  const PooledEffectNode node =
      SowRadialPetalNode(pattern, glm::vec3{10.0F, 0.0F, 20.0F}, 150, generator);
  CHECK(node.shape == PooledNodeShape::kRadialPetal);
  REQUIRE(node.node.render_word.has_value());
  CHECK(*node.node.render_word == 5U);
  CHECK(node.node.life_ticks == 150);
  REQUIRE(node.resource_paths.size() == 1);
  CHECK(node.resource_paths.front() == "data\\texture\\effect\\peong1.tga");
  CHECK(node.ground_position == glm::vec3{10.0F, 0.0F, 20.0F});
}

TEST_CASE("Pétales : les angles de panneau se rangent tels quels, même éteints",
          "[render][effectnode][petal]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  AdvanceRadialPetalTracks(tracks, 123, 45);
  CHECK(tracks[0].style == 123);
  CHECK(tracks[1].style == 45);
}

TEST_CASE("Pétales : le retard court sans rien faire", "[render][effectnode][petal]") {
  EffectTrack track = TrackAtHeading(0);
  track.ticks = -3;
  const auto after = Advanced(track, 3);
  CHECK(after.front().ticks == 0);
  CHECK(after.front().alpha == 0);
  CHECK(after.front().tail[3] == 0.0F);
  CHECK(after.front().tail[4] == 0.0F);
}

TEST_CASE("Pétales : il file le long de son cap et fait UN saut", "[render][effectnode][petal]") {
  const EffectTrack track = TrackAtHeading(90);
  // Au quarante-cinquième tic, le sinus du saut vaut un : 0,03 · 50 au-dessus
  // de la base, soit 5,5 unités en repère du client.
  const auto mi_course = Advanced(track, 45);
  CHECK(mi_course.front().tail[5] == Approx(45.0F * 0.03F).margin(kTolerance));
  CHECK(mi_course.front().tail[3] == Approx(0.0F).margin(kTolerance));
  CHECK(mi_course.front().tail[4] == Approx(-5.5F).margin(kTolerance));
  // Au quatre-vingt-dixième, il est retombé sur la base et s'arrête.
  const auto fin = Advanced(track, 90);
  CHECK(fin.front().tail[4] == Approx(-4.0F).margin(kTolerance));
  CHECK(fin.front().tail[5] == Approx(90.0F * 0.03F).margin(kTolerance));
  const auto au_dela = Advanced(track, 120);
  CHECK(au_dela.front().tail[5] == Approx(fin.front().tail[5]).margin(kTolerance));
  CHECK(au_dela.front().tail[4] == Approx(fin.front().tail[4]).margin(kTolerance));
}

TEST_CASE("Pétales : l'alpha monte à cinquante, se tient puis s'éteint",
          "[render][effectnode][petal]") {
  const EffectTrack track = TrackAtHeading(0);
  CHECK(Advanced(track, 10).front().alpha == 20);
  CHECK(Advanced(track, 25).front().alpha == 50);
  CHECK(Advanced(track, 70).front().alpha == 50);
  CHECK(Advanced(track, 80).front().alpha == 40);
  CHECK(Advanced(track, 120).front().alpha == 0);
  CHECK(Advanced(track, 200).front().alpha == 0);
}

TEST_CASE("Pétales : un carré par piste vive, blanc, au mode un, sans garde de hauteur",
          "[render][effectnode][petal]") {
  EffectTrack track = TrackAtHeading(0);
  CHECK_FALSE(ComposeRadialPetalQuad(EffectTrack{}, 0, 0, glm::vec3{0.0F}).has_value());

  track.radius_at_anchor = 3.0F;
  track.alpha = 40;
  track.tail[3] = 2.0F;
  // ⚠ SOUS LE NŒUD dans le repère du client : le grain qui dérive se tairait,
  // le pétale non.
  track.tail[4] = 6.0F;
  track.tail[5] = -1.0F;
  const glm::vec3 origin{100.0F, 5.0F, 200.0F};
  const std::optional<EffectWorldQuad> quad = ComposeRadialPetalQuad(track, 30, 60, origin);
  REQUIRE(quad.has_value());
  const glm::vec3 centre = CentreDe(*quad);
  CHECK(centre.x == Approx(102.0F).margin(kTolerance));
  CHECK(centre.y == Approx(-1.0F).margin(kTolerance));
  // La profondeur du client est l'opposée de la nôtre : son moins un avance.
  CHECK(centre.z == Approx(201.0F).margin(kTolerance));
  for (const glm::vec3& corner : quad->corners) {
    CHECK(glm::length(corner - centre) == Approx(3.0F).margin(kTolerance));
  }
  CHECK(quad->tint == std::array<std::uint8_t, 4>{255, 255, 255, 40});
  const ro::render::EffectBlendPair blend =
      EffectParticleBlend(ro::render::kRadialPetalBlendMode, ro::render::kEffectOwnerTintAbsent);
  CHECK(quad->blend_source == blend.source);
  CHECK(quad->blend_destination == blend.destination);
  CHECK(blend.destination !=
        EffectParticleBlend(0, ro::render::kEffectOwnerTintAbsent).destination);
  CHECK(quad->texture == 0);
  CHECK(quad->texture_uv[0].y == 1.0F);
  CHECK(quad->texture_uv[2].y == 0.0F);
}
