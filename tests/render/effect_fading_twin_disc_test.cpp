#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <span>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_breathing_quad.h"
#include "ro/render/effect_fading_twin_disc_seed.h"
#include "ro/render/effect_frame.h"
#include "ro/render/effect_node_pattern_table.h"
#include "ro/render/effect_node_pool.h"

using Catch::Approx;
using ro::render::AdvanceFadingTwinDiscTracks;
using ro::render::ComposeBreathingQuad;
using ro::render::EffectFadingTwinDiscPattern;
using ro::render::EffectFadingTwinDiscPatternsFor;
using ro::render::EffectFadingTwinDiscPatternTable;
using ro::render::EffectFadingTwinDiscSeedFor;
using ro::render::EffectNodePool;
using ro::render::EffectTrack;
using ro::render::EffectWorldQuad;
using ro::render::FadingTwinDiscLifeTicks;
using ro::render::kEffectTracksPerNode;
using ro::render::kSlotOfBreathingHeight;
using ro::render::PooledEffectNode;
using ro::render::PooledNodeShape;
using ro::render::SeedEffectFadingTwinDiscTracks;
using ro::render::SowEffectFadingTwinDiscNode;

namespace {

constexpr std::uint32_t kEffectOfTheFamily = 831;
constexpr glm::vec3 kAtTheOrigin{0.0F, 0.0F, 0.0F};

// LA VIE QUE LA TABLE DES DURÉES DONNE À 831, et celle que le nœud emporte
// après le plancher du voisin.
constexpr std::int32_t kDeclaredLife = 100;
constexpr std::int32_t kLifeAfterFloor = 120;

[[nodiscard]] const EffectFadingTwinDiscPattern& Pattern831() {
  return EffectFadingTwinDiscPatternsFor(kEffectOfTheFamily).front();
}

[[nodiscard]] float FirstCornerRadius(const std::vector<EffectWorldQuad>& quads) {
  return std::hypot(quads.front().corners[0].x, quads.front().corners[0].z);
}

}  // namespace

TEST_CASE("le 831 pose UN couple de disques, image et octet de son site",
          "[render][fading_twin_disc]") {
  const std::span<const EffectFadingTwinDiscPattern> table = EffectFadingTwinDiscPatternTable();
  REQUIRE(std::ranges::is_sorted(table, {}, &EffectFadingTwinDiscPattern::effect_id));
  REQUIRE(EffectFadingTwinDiscPatternsFor(kEffectOfTheFamily).size() == 1);
  const EffectFadingTwinDiscPattern& pattern = Pattern831();
  CHECK(pattern.client_function == "Effect_InitScaledSprite2");
  CHECK(pattern.texture == "data\\texture\\effect\\cir0002.bmp");
  // Le site d'appel passe trois au bâtisseur.
  CHECK(pattern.builder_argument == 3);
  CHECK(EffectFadingTwinDiscPatternsFor(1U).empty());
  // ⚠ LE 831 ÉTAIT DÉJÀ CONNU PAR SON RUBAN : la famille s'y ajoute sans rien
  // retirer.
  CHECK(ro::render::KnowsProceduralEffect(kEffectOfTheFamily));
}

TEST_CASE("l'octet choisit le style du noeud et le rayon", "[render][fading_twin_disc]") {
  // Le style du nœud, en flottant : 15, 17, 18, et 4 pour tout autre octet.
  CHECK(EffectFadingTwinDiscSeedFor(1).node_style == 15);
  CHECK(EffectFadingTwinDiscSeedFor(2).node_style == 17);
  CHECK(EffectFadingTwinDiscSeedFor(3).node_style == 18);
  CHECK(EffectFadingTwinDiscSeedFor(0).node_style == 4);
  CHECK(EffectFadingTwinDiscSeedFor(7).node_style == 4);
  // ⚠ VINGT-QUATRE, et pour le seul octet trois ; quinze sinon.
  CHECK(EffectFadingTwinDiscSeedFor(3).radius_at_anchor == Approx(24.0F));
  for (const std::uint8_t octet : std::array<std::uint8_t, 4>{0, 1, 2, 4}) {
    INFO("octet " << static_cast<int>(octet));
    CHECK(EffectFadingTwinDiscSeedFor(octet).radius_at_anchor == Approx(15.0F));
  }
}

TEST_CASE("le semis allume DEUX pistes, en opposition de souffle", "[render][fading_twin_disc]") {
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectFadingTwinDiscTracks(Pattern831());
  for (std::size_t rang = 0; rang < 2; ++rang) {
    INFO("piste " << rang);
    CHECK(tracks[rang].active);
    CHECK(tracks[rang].alpha == 200);
    CHECK(tracks[rang].ticks == 0);
    CHECK(tracks[rang].radius_at_anchor == Approx(24.0F));
    // ⚠ MOINS UN : le repère du client descend.
    CHECK(tracks[rang].value[kSlotOfBreathingHeight] == Approx(-1.0F));
  }
  CHECK(tracks[0].tilt_degrees == 0);
  CHECK(tracks[1].tilt_degrees == 180);
  CHECK_FALSE(tracks[2].active);
  CHECK_FALSE(tracks[3].active);
}

TEST_CASE("le noeud vit cent vingt tics, pas les cent de la table", "[render][fading_twin_disc]") {
  // ⚠ LE VOISIN RELÈVE LA VIE DE L'INSTANCE AVANT QUE CE BÂTISSEUR NE LA COPIE :
  // cent vingt au plancher, et c'est la vie que le nœud reçoit.
  CHECK(Pattern831().instance_life_floor_ticks == kLifeAfterFloor);
  CHECK(FadingTwinDiscLifeTicks(Pattern831(), kDeclaredLife) == kLifeAfterFloor);
  // Un plancher ne raccourcit jamais.
  CHECK(FadingTwinDiscLifeTicks(Pattern831(), 500) == 500);
}

TEST_CASE("le noeud seme porte son style, son mot de rendu et son image",
          "[render][fading_twin_disc]") {
  const PooledEffectNode node =
      SowEffectFadingTwinDiscNode(Pattern831(), glm::vec3{3.0F, 0.0F, 4.0F}, kDeclaredLife);
  CHECK(node.shape == PooledNodeShape::kFadingTwinDisc);
  CHECK(node.trail_variant == 18);
  CHECK(node.node.life_ticks == kLifeAfterFloor);
  // Le mot de rendu vaut 5 : le mélange de la primitive, et l'épreuve de
  // profondeur.
  REQUIRE(node.node.render_word.has_value());
  CHECK(*node.node.render_word == 5U);
  REQUIRE(node.resource_paths.size() == 1);
  CHECK(node.resource_paths.front() == "data\\texture\\effect\\cir0002.bmp");
  CHECK(node.ground_position == glm::vec3{3.0F, 0.0F, 4.0F});
}

TEST_CASE("les deux disques sont BLEUS et s'ajoutent, ecartes de vingt-trois degres",
          "[render][fading_twin_disc]") {
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectFadingTwinDiscTracks(Pattern831());
  const std::int32_t style = EffectFadingTwinDiscSeedFor(3).node_style;
  const std::vector<EffectWorldQuad> first =
      ComposeBreathingQuad(tracks[0], style, 0, kAtTheOrigin);
  const std::vector<EffectWorldQuad> second =
      ComposeBreathingQuad(tracks[1], style, 1, kAtTheOrigin);
  REQUIRE(first.size() == 1);
  REQUIRE(second.size() == 1);
  // Le style dix-huit : (35, 35, 255), en ADDITION.
  CHECK(first.front().tint == std::array<std::uint8_t, 4>{35, 35, 255, 200});
  CHECK(first.front().blend_destination == 2);
  // ⚠ DIX-HUIT NE LIT PAS L'AZIMUT : l'angle de départ est le RANG fois
  // vingt-trois. La première piste part de zéro, la seconde de vingt-trois.
  const float angle_du_second =
      std::atan2(second.front().corners[0].z, second.front().corners[0].x) * 180.0F / 3.14159265F;
  // ⚠ LA PROFONDEUR DU CLIENT SE RETOURNE DANS NOTRE REPÈRE : ses vingt-trois
  // degrés s'y lisent moins vingt-trois.
  CHECK(first.front().corners[0].z == Approx(0.0F).margin(0.001F));
  CHECK(angle_du_second == Approx(-23.0F).margin(0.01F));
  // À l'inclinaison zéro comme à cent quatre-vingts, le sinus est nul : neuf
  // dixièmes du rayon.
  CHECK(FirstCornerRadius(first) == Approx(0.9F * 24.0F).margin(0.001F));
  CHECK(FirstCornerRadius(second) == Approx(0.9F * 24.0F).margin(0.001F));
}

TEST_CASE("l'inclinaison tourne de trois par tic, et les souffles s'opposent",
          "[render][fading_twin_disc]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectFadingTwinDiscTracks(Pattern831());
  constexpr int kTicksToAQuarterTurn = 30;
  for (int tick = 0; tick < kTicksToAQuarterTurn; ++tick) {
    AdvanceFadingTwinDiscTracks(tracks, kLifeAfterFloor);
  }
  CHECK(tracks[0].tilt_degrees == 90);
  CHECK(tracks[1].tilt_degrees == 270);
  CHECK(tracks[0].ticks == kTicksToAQuarterTurn);
  CHECK(tracks[1].ticks == kTicksToAQuarterTurn);
  const std::int32_t style = EffectFadingTwinDiscSeedFor(3).node_style;
  // Le premier au plus large, le second au plus serré.
  CHECK(FirstCornerRadius(ComposeBreathingQuad(tracks[0], style, 0, kAtTheOrigin)) ==
        Approx(24.0F).margin(0.001F));
  CHECK(FirstCornerRadius(ComposeBreathingQuad(tracks[1], style, 1, kAtTheOrigin)) ==
        Approx(0.8F * 24.0F).margin(0.001F));

  // Et elle reboucle à trois cent soixante.
  tracks[0].tilt_degrees = 358;
  AdvanceFadingTwinDiscTracks(tracks, kLifeAfterFloor);
  CHECK(tracks[0].tilt_degrees == 1);
}

TEST_CASE("l'alpha tient, puis perd huit par tic pendant les vingt-cinq derniers",
          "[render][fading_twin_disc]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectFadingTwinDiscTracks(Pattern831());
  // ⚠ STRICTEMENT AU-DELÀ de vie − 25 : au tic quatre-vingt-quinze, rien encore.
  for (int tick = 0; tick < kLifeAfterFloor - 25; ++tick) {
    AdvanceFadingTwinDiscTracks(tracks, kLifeAfterFloor);
  }
  CHECK(tracks[0].alpha == 200);
  CHECK(tracks[1].alpha == 200);
  AdvanceFadingTwinDiscTracks(tracks, kLifeAfterFloor);
  CHECK(tracks[0].alpha == 192);
  CHECK(tracks[1].alpha == 192);
  for (int tick = kLifeAfterFloor - 24; tick < kLifeAfterFloor; ++tick) {
    AdvanceFadingTwinDiscTracks(tracks, kLifeAfterFloor);
  }
  // Vingt-cinq fois huit : zéro au dernier tic, et le plancher tient ensuite.
  CHECK(tracks[0].alpha == 0);
  AdvanceFadingTwinDiscTracks(tracks, kLifeAfterFloor);
  CHECK(tracks[0].alpha == 0);
  CHECK(ComposeBreathingQuad(tracks[0], 18, 0, kAtTheOrigin).empty());
}

TEST_CASE("le mouvement ne touche que les deux premieres pistes, meme eteintes",
          "[render][fading_twin_disc]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectFadingTwinDiscTracks(Pattern831());
  tracks[0].active = false;
  tracks[2].tilt_degrees = 40;
  tracks[3].ticks = 7;
  AdvanceFadingTwinDiscTracks(tracks, kLifeAfterFloor);
  // ⚠ LE CLIENT AJOUTE SANS TESTER `active`.
  CHECK(tracks[0].tilt_degrees == 3);
  CHECK(tracks[0].ticks == 1);
  CHECK(tracks[2].tilt_degrees == 40);
  CHECK(tracks[3].ticks == 7);
}

TEST_CASE("la troupe mene le noeud par son propre mouvement", "[render][fading_twin_disc]") {
  EffectNodePool pool;
  pool.Add(SowEffectFadingTwinDiscNode(Pattern831(), kAtTheOrigin, kDeclaredLife));
  constexpr int kTicks = 10;
  for (int tick = 0; tick < kTicks; ++tick) {
    pool.Advance(static_cast<std::uint32_t>(ro::render::kEffectTickMs));
  }
  REQUIRE(pool.size() == 1);
  const PooledEffectNode& node = pool.nodes().front();
  CHECK(node.tracks[0].tilt_degrees == 3 * kTicks);
  CHECK(node.tracks[1].tilt_degrees == 180 + 3 * kTicks);
  CHECK(node.tracks[0].ticks == kTicks);
}
