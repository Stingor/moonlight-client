#include "ro/render/effect_rainstorm_cylinder.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_frame.h"
#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pattern_table.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_rainstorm_cylinder_seed.h"

#include "support/effect_bearers.h"

using Catch::Approx;
using ro::render::AdvanceRainstormCylinderTracks;
using ro::render::ComposeRainstormCylinderQuads;
using ro::render::EffectNodePool;
using ro::render::EffectRainstormCylinderPattern;
using ro::render::EffectRainstormCylinderPatternsFor;
using ro::render::EffectRainstormCylinderPatternTable;
using ro::render::EffectTrack;
using ro::render::EffectWorldQuad;
using ro::render::kEffectNodeDefaultLifeTicks;
using ro::render::kEffectTickMs;
using ro::render::kEffectTrackSlots;
using ro::render::kEffectTracksPerNode;
using ro::render::KnowsProceduralEffect;
using ro::render::kRainstormCylinderImageCount;
using ro::render::kRainstormCylinderRenderWord;
using ro::render::PooledEffectNode;
using ro::render::PooledNodeShape;
using ro::render::RainstormCylinderImages;
using ro::render::RainstormCylinderLifeTicks;
using ro::render::SeedRainstormCylinderTracks;
using ro::render::SowRainstormCylinderNode;

namespace {

using Tracks = std::array<EffectTrack, kEffectTracksPerNode>;

// L'UNIQUE IDENTIFIANT du sous-type 173 : Severe Rainstorm.
constexpr std::uint32_t kSevereRainstormEffectId = 1286;

// La vie que le plancher du bâtisseur donne au nœud.
constexpr std::int32_t kFloorLife = 120;

// Le facteur de destination INVSRCALPHA de Direct3D : la variante zéro du ruban.
constexpr std::int32_t kInvSourceAlpha = 6;

[[nodiscard]] float SinDegrees(std::int32_t degrees) {
  return std::sin(static_cast<float>(degrees) * std::numbers::pi_v<float> / 180.0F);
}

[[nodiscard]] Tracks SownTracks() {
  return SeedRainstormCylinderTracks(
      EffectRainstormCylinderPatternsFor(kSevereRainstormEffectId)[0]);
}

}  // namespace

TEST_CASE("Cylindre de notes : quatre appels du cas, rayons 3, 3, 8 et 8", "[render][effectnode]") {
  const std::span<const EffectRainstormCylinderPattern> table =
      EffectRainstormCylinderPatternTable();
  REQUIRE(table.size() == 4);
  const std::span<const EffectRainstormCylinderPattern> patterns =
      EffectRainstormCylinderPatternsFor(kSevereRainstormEffectId);
  REQUIRE(patterns.size() == 4);
  constexpr std::array<float, 4> kRadii = {3.0F, 3.0F, 8.0F, 8.0F};
  for (std::size_t call_index = 0; call_index < patterns.size(); ++call_index) {
    CHECK(patterns[call_index].radius == Approx(kRadii[call_index]));
    CHECK(patterns[call_index].style == 3);
    CHECK(patterns[call_index].instance_life_floor_ticks == kFloorLife);
    CHECK(patterns[call_index].client_function == "Effect_InitSevereRainstorm");
  }
  CHECK(EffectRainstormCylinderPatternsFor(kSevereRainstormEffectId - 1).empty());
  CHECK(EffectRainstormCylinderPatternsFor(kSevereRainstormEffectId + 1).empty());
  CHECK(KnowsProceduralEffect(kSevereRainstormEffectId));
}

TEST_CASE("Cylindre de notes : neuf images, de music_01 à music_09", "[render][effectnode]") {
  const std::span<const std::string_view> images = RainstormCylinderImages();
  REQUIRE(images.size() == kRainstormCylinderImageCount);
  CHECK(images.front() ==
        "data\\texture\\effect\\new_severerainstorm\\new_severerainstorm_cylinder\\music_01.tga");
  CHECK(images.back() ==
        "data\\texture\\effect\\new_severerainstorm\\new_severerainstorm_cylinder\\music_09.tga");
}

TEST_CASE("Cylindre de notes : le semis pose quatre arcs de cent vingt degrés",
          "[render][effectnode]") {
  constexpr std::array<std::int16_t, 4> kAzimuts = {0, 90, 180, 270};
  constexpr std::array<std::int16_t, 4> kInclinaisons = {60, 55, 65, 70};
  const EffectRainstormCylinderPattern& grand =
      EffectRainstormCylinderPatternsFor(kSevereRainstormEffectId)[2];
  const Tracks tracks = SeedRainstormCylinderTracks(grand);
  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    const EffectTrack& track = tracks[rang];
    CHECK(track.active);
    CHECK(track.ticks == 0);
    CHECK(track.alpha == 0);
    CHECK(track.degrees == 120);
    CHECK(track.lift_key == Approx(20.0F));
    CHECK(track.azimuth_degrees == kAzimuts[rang]);
    CHECK(track.tilt_degrees == kInclinaisons[rang]);
    CHECK(track.radius_at_anchor == Approx(8.0F));
    CHECK(track.style == 3);
    for (const float length : track.value) {
      CHECK(length == 0.0F);
    }
  }
}

TEST_CASE("Cylindre de notes : le nœud semé porte sa forme, son mot de rendu et sa vie",
          "[render][effectnode]") {
  const EffectRainstormCylinderPattern& pattern =
      EffectRainstormCylinderPatternsFor(kSevereRainstormEffectId).front();
  const glm::vec3 bearer{10.0F, 0.0F, -4.0F};
  const PooledEffectNode node =
      SowRainstormCylinderNode(pattern, bearer, kEffectNodeDefaultLifeTicks);
  CHECK(node.shape == PooledNodeShape::kRainstormCylinder);
  REQUIRE(node.node.render_word.has_value());
  CHECK(*node.node.render_word == kRainstormCylinderRenderWord);
  // ⚠ LE PLANCHER L'EMPORTE sur le défaut du constructeur.
  CHECK(node.node.life_ticks == kFloorLife);
  CHECK(RainstormCylinderLifeTicks(pattern, 300) == 300);
  CHECK(node.node.frame_hold_ticks == 0);
  REQUIRE(node.resource_paths.size() == kRainstormCylinderImageCount);
  CHECK(node.resource_paths[4] ==
        "data\\texture\\effect\\new_severerainstorm\\new_severerainstorm_cylinder\\music_05.tga");
  CHECK(node.trail_variant == 0);
  CHECK(node.ground_position.x == Approx(10.0F));
  CHECK(node.owner_position.z == Approx(-4.0F));
}

TEST_CASE("Cylindre de notes : l'azimut tourne d'un degré par tic et se replie",
          "[render][effectnode]") {
  Tracks tracks = SownTracks();
  tracks[3].azimuth_degrees = 359;
  AdvanceRainstormCylinderTracks(tracks, kFloorLife);
  CHECK(tracks[0].ticks == 1);
  CHECK(tracks[0].azimuth_degrees == 1);
  CHECK(tracks[1].azimuth_degrees == 91);
  CHECK(tracks[3].azimuth_degrees == 0);
}

TEST_CASE("Cylindre de notes : l'alpha monte de cinq pendant vingt-neuf tics",
          "[render][effectnode]") {
  Tracks tracks = SownTracks();
  AdvanceRainstormCylinderTracks(tracks, kFloorLife);
  CHECK(tracks[0].alpha == 5);
  for (std::int32_t tick = 2; tick <= 29; ++tick) {
    AdvanceRainstormCylinderTracks(tracks, kFloorLife);
  }
  CHECK(tracks[0].alpha == 145);
  // Au trentième tic la montée s'arrête.
  AdvanceRainstormCylinderTracks(tracks, kFloorLife);
  CHECK(tracks[0].alpha == 145);
}

TEST_CASE("Cylindre de notes : les effets réduits plafonnent à quatre-vingts",
          "[render][effectnode]") {
  Tracks tracks = SownTracks();
  for (std::int32_t tick = 1; tick <= 29; ++tick) {
    AdvanceRainstormCylinderTracks(tracks, kFloorLife, true);
  }
  CHECK(tracks[0].alpha == 80);
}

TEST_CASE("Cylindre de notes : l'alpha tombe de quatre passé la vie moins quarante-six",
          "[render][effectnode]") {
  Tracks tracks = SownTracks();
  for (std::int32_t tick = 1; tick <= 74; ++tick) {
    AdvanceRainstormCylinderTracks(tracks, kFloorLife);
  }
  // Au tic 74 le compteur vaut la vie moins quarante-six : pas encore au-delà.
  CHECK(tracks[0].alpha == 145);
  AdvanceRainstormCylinderTracks(tracks, kFloorLife);
  CHECK(tracks[0].alpha == 141);
  for (std::int32_t tick = 76; tick <= 110; ++tick) {
    AdvanceRainstormCylinderTracks(tracks, kFloorLife);
  }
  CHECK(tracks[0].alpha == 1);
  AdvanceRainstormCylinderTracks(tracks, kFloorLife);
  CHECK(tracks[0].alpha == 0);
}

TEST_CASE("Cylindre de notes : l'onde bat autour de la clé, deux formules de phase",
          "[render][effectnode]") {
  Tracks tracks = SownTracks();
  AdvanceRainstormCylinderTracks(tracks, kFloorLife);
  // Piste 0 : phase = tic (1). Piste 3 : phase = (tic + 3·45)·2 = 272.
  for (std::size_t segment = 0; segment < kEffectTrackSlots; ++segment) {
    const auto le_long = static_cast<std::int32_t>(segment) * 9;
    const float expected0 = 20.0F + (20.0F * 0.3F * SinDegrees(le_long) * SinDegrees(1));
    const float expected3 = 20.0F + (20.0F * 0.3F * SinDegrees(le_long) * SinDegrees(272));
    CHECK(tracks[0].value[segment] == Approx(expected0).margin(1e-4F));
    CHECK(tracks[3].value[segment] == Approx(expected3).margin(1e-4F));
  }
}

TEST_CASE("Cylindre de notes : une piste éteinte ne bouge pas", "[render][effectnode]") {
  Tracks tracks = SownTracks();
  tracks[1].active = false;
  AdvanceRainstormCylinderTracks(tracks, kFloorLife);
  CHECK(tracks[1].ticks == 0);
  CHECK(tracks[1].azimuth_degrees == 90);
  CHECK(tracks[1].alpha == 0);
}

TEST_CASE("Cylindre de notes : vingt quads blancs, abaissés de cinq, l'image au compteur",
          "[render][effectnode]") {
  Tracks tracks = SownTracks();
  for (std::int32_t tick = 1; tick <= 10; ++tick) {
    AdvanceRainstormCylinderTracks(tracks, kFloorLife);
  }
  const glm::vec3 anchor{100.0F, 7.0F, 50.0F};
  const std::vector<EffectWorldQuad> quads =
      ComposeRainstormCylinderQuads(tracks[0], 0, kRainstormCylinderImageCount, anchor);
  // Cent vingt degrés par pas de six : vingt et un points, vingt quads.
  REQUIRE(quads.size() == 20);
  for (const EffectWorldQuad& quad : quads) {
    CHECK(quad.tint[0] == 255);
    CHECK(quad.tint[3] == 50);
    CHECK(quad.blend_destination == kInvSourceAlpha);
    CHECK(quad.texture == 10 % kRainstormCylinderImageCount);
  }
  // LE BORD INTÉRIEUR : au rayon trois, cinq unités SOUS le nœud, à l'azimut
  // que le mouvement a tourné de dix degrés.
  const glm::vec3 inner = quads.front().corners[0];
  CHECK(inner.x == Approx(anchor.x + (3.0F * std::cos(10.0F * std::numbers::pi_v<float> / 180.0F)))
                       .margin(1e-3F));
  CHECK(inner.y == Approx(anchor.y - 5.0F).margin(1e-4F));
  // LE BORD EXTÉRIEUR monte de la longueur fois le sinus de l'inclinaison.
  const glm::vec3 outer = quads.front().corners[3];
  const float attendue = anchor.y - 5.0F + (tracks[0].value[0] * SinDegrees(60));
  CHECK(outer.y == Approx(attendue).margin(1e-3F));
}

TEST_CASE("Cylindre de notes : la clé ne soulève rien, et un compteur nul ne dessine pas",
          "[render][effectnode]") {
  Tracks tracks = SownTracks();
  AdvanceRainstormCylinderTracks(tracks, kFloorLife);
  EffectTrack track = tracks[0];
  // 6,001 est une clé qui soulève la jupe ; ce dessinateur ne la lit pas.
  track.lift_key = 6.001F;
  const std::vector<EffectWorldQuad> quads =
      ComposeRainstormCylinderQuads(track, 0, kRainstormCylinderImageCount, glm::vec3{0.0F});
  REQUIRE_FALSE(quads.empty());
  CHECK(quads.front().corners[0].y == Approx(-5.0F).margin(1e-4F));

  track.ticks = 0;
  CHECK(ComposeRainstormCylinderQuads(track, 0, kRainstormCylinderImageCount, glm::vec3{0.0F})
            .empty());
}

TEST_CASE("Cylindre de notes : il suit son porteur et son image suit le compteur",
          "[render][effectnode]") {
  EffectNodePool pool;
  const glm::vec3 bearer{0.0F, 0.0F, 0.0F};
  const EffectRainstormCylinderPattern& pattern =
      EffectRainstormCylinderPatternsFor(kSevereRainstormEffectId).front();
  pool.AnchorNextTo(11U, bearer);
  pool.Add(SowRainstormCylinderNode(pattern, bearer, kEffectNodeDefaultLifeTicks));
  pool.AnchorNextTo(0U);

  const glm::vec3 farther = bearer + glm::vec3{30.0F, 0.0F, -10.0F};
  ro::test_support::MoveBearer(pool, 11U, farther);
  pool.Advance(kEffectTickMs);
  REQUIRE(pool.size() == 1);
  CHECK(pool.nodes()[0].ground_position.x == Approx(30.0F).margin(0.001F));
  CHECK(pool.nodes()[0].ground_position.z == Approx(-10.0F).margin(0.001F));
  CHECK(pool.nodes()[0].tracks[0].ticks == 1);
  CHECK(pool.nodes()[0].node.frame == 1);
}
