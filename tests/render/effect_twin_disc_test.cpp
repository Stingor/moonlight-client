#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_breathing_quad.h"
#include "ro/render/effect_fading_twin_disc_seed.h"
#include "ro/render/effect_frame.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_render_word.h"
#include "ro/render/effect_twin_disc_seed.h"

#include "support/effect_bearers.h"

using Catch::Approx;
using ro::render::AdvanceTwinDiscTracks;
using ro::render::ComposeBreathingQuad;
using ro::render::EffectFadingTwinDiscSeedFor;
using ro::render::EffectNodePool;
using ro::render::EffectTrack;
using ro::render::EffectTwinDiscPattern;
using ro::render::EffectTwinDiscPatternsFor;
using ro::render::EffectTwinDiscPatternTable;
using ro::render::EffectWorldQuad;
using ro::render::kEffectTracksPerNode;
using ro::render::kSlotOfBreathingHeight;
using ro::render::kTwinDiscHeight;
using ro::render::kTwinDiscLiveTracks;
using ro::render::kTwinDiscSecondTilt;
using ro::render::kTwinDiscTiltPerTick;
using ro::render::PooledEffectNode;
using ro::render::SeedEffectTwinDiscTracks;
using ro::render::SowEffectTwinDiscNode;

namespace {

constexpr glm::vec3 kAtTheOrigin{0.0F, 0.0F, 0.0F};

// LE MOT DE RENDU DU PREMIER BÂTISSEUR (13), et celui du second (0 | 1 | 5).
constexpr std::uint32_t kWordOfTwoArguments = 13;
constexpr std::uint32_t kWordOfOneArgument = 5;

// Le facteur de Direct3D « un » : la destination d'une addition.
constexpr std::int32_t kBlendOne = 2;

[[nodiscard]] EffectTwinDiscPattern TestPattern() {
  EffectTwinDiscPattern pattern;
  pattern.effect_id = 1;
  pattern.client_function = "test";
  pattern.texture = "essai.bmp";
  pattern.node_style = 4;
  pattern.radius = 15.0F;
  pattern.alpha = 200;
  pattern.render_word = kWordOfTwoArguments;
  pattern.builder_argument = std::uint8_t{0};
  return pattern;
}

// LE PATRON DE L'EFFET, pour l'image que son site passe.
[[nodiscard]] const EffectTwinDiscPattern& PatternOf(std::uint32_t effect_id,
                                                     std::string_view texture) {
  const std::span<const EffectTwinDiscPattern> patterns = EffectTwinDiscPatternsFor(effect_id);
  const auto found = std::ranges::find(patterns, texture, &EffectTwinDiscPattern::texture);
  REQUIRE(found != patterns.end());
  return *found;
}

// LE PREMIER QUAD de la piste zéro, tel que la figure le compose.
[[nodiscard]] EffectWorldQuad FirstQuad(const EffectTwinDiscPattern& pattern) {
  const std::array<EffectTrack, kEffectTracksPerNode> tracks = SeedEffectTwinDiscTracks(pattern);
  const std::vector<EffectWorldQuad> quads =
      ComposeBreathingQuad(tracks[0], pattern.node_style, 0, kAtTheOrigin);
  REQUIRE(quads.size() == 1);
  return quads.front();
}

}  // namespace

TEST_CASE("le semis n'allume que DEUX pistes sur quatre", "[render][twin_disc]") {
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectTwinDiscTracks(TestPattern());
  std::size_t living = 0;
  for (const EffectTrack& track : tracks) {
    if (track.active) {
      ++living;
    }
  }
  REQUIRE(living == kTwinDiscLiveTracks);
  REQUIRE(tracks[0].active);
  REQUIRE(tracks[1].active);
}

TEST_CASE("les deux disques se font FACE", "[render][twin_disc]") {
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectTwinDiscTracks(TestPattern());
  // ⚠ ZÉRO ET CENT QUATRE-VINGTS : leurs souffles sont en opposition de phase.
  // Les poser à la même inclinaison les superposerait.
  REQUIRE(tracks[0].tilt_degrees == 0);
  REQUIRE(tracks[1].tilt_degrees == kTwinDiscSecondTilt);
}

TEST_CASE("la hauteur du disque est NEGATIVE chez le client", "[render][twin_disc]") {
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectTwinDiscTracks(TestPattern());
  // ⚠ LE REPÈRE DU CLIENT DESCEND : son moins un est un AU-DESSUS.
  REQUIRE(tracks[0].value[kSlotOfBreathingHeight] == Approx(kTwinDiscHeight));
  REQUIRE(kTwinDiscHeight < 0.0F);
}

TEST_CASE("les deux disques de l'aura du niveau 99 flottent AU-DESSUS du sol",
          "[render][twin_disc]") {
  // 🔴 SOUS LE SOL, L'ÉPREUVE DE PROFONDEUR LES EFFACE. Le mot treize passe
  // l'épreuve, et le terrain passe par l'ancre : un carré couché à une unité
  // SOUS elle ne laisse pas un pixel. C'est le moins un du client, recopié sans
  // être retourné, qui rendait l'effet 201 invisible.
  constexpr glm::vec3 kAnchor{40.0F, 7.5F, -12.0F};
  const EffectTwinDiscPattern& pattern = PatternOf(201U, "data\\texture\\effect\\pikapika2.bmp");
  REQUIRE(ro::render::EffectLotIsDepthTested(pattern.render_word));
  const std::array<EffectTrack, kEffectTracksPerNode> tracks = SeedEffectTwinDiscTracks(pattern);
  std::size_t quads_seen = 0;
  for (std::size_t rang = 0; rang < kTwinDiscLiveTracks; ++rang) {
    for (const EffectWorldQuad& quad :
         ComposeBreathingQuad(tracks[rang], pattern.node_style, rang, kAnchor)) {
      ++quads_seen;
      for (const glm::vec3& corner : quad.corners) {
        CHECK(corner.y == Approx(kAnchor.y - kTwinDiscHeight));
        CHECK(corner.y > kAnchor.y);
      }
      // ⚠ ET LES DEUX DISQUES PARTAGENT LEUR CENTRE : aucun décalage ne les
      // sépare, seuls leur angle de départ et leur souffle diffèrent.
      const glm::vec3 centre = (quad.corners[0] + quad.corners[2]) * 0.5F;
      CHECK(centre.x == Approx(kAnchor.x).margin(0.001F));
      CHECK(centre.z == Approx(kAnchor.z).margin(0.001F));
    }
  }
  CHECK(quads_seen == kTwinDiscLiveTracks);
}

TEST_CASE("l'inclinaison tourne de trois par tic et reboucle", "[render][twin_disc]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks = SeedEffectTwinDiscTracks(TestPattern());
  AdvanceTwinDiscTracks(tracks);
  REQUIRE(tracks[0].tilt_degrees == kTwinDiscTiltPerTick);
  REQUIRE(tracks[1].tilt_degrees == kTwinDiscSecondTilt + kTwinDiscTiltPerTick);

  tracks[0].tilt_degrees = 358;
  AdvanceTwinDiscTracks(tracks);
  REQUIRE(tracks[0].tilt_degrees == 358 + kTwinDiscTiltPerTick - 360);
}

TEST_CASE("le mouvement ne touche QUE les deux premieres pistes", "[render][twin_disc]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks = SeedEffectTwinDiscTracks(TestPattern());
  tracks[2].tilt_degrees = 40;
  tracks[3].tilt_degrees = 50;
  AdvanceTwinDiscTracks(tracks);
  REQUIRE(tracks[2].tilt_degrees == 40);
  REQUIRE(tracks[3].tilt_degrees == 50);
}

TEST_CASE("il tourne meme une piste ETEINTE", "[render][twin_disc]") {
  // ⚠ LE CLIENT AJOUTE AVANT DE REGARDER : il ne teste rien. Cela ne se voit
  // pas — cette famille allume toujours ses deux premières pistes —, mais
  // borner la boucle aux vives masquerait ce qu'il fait.
  std::array<EffectTrack, kEffectTracksPerNode> tracks = SeedEffectTwinDiscTracks(TestPattern());
  tracks[0].active = false;
  AdvanceTwinDiscTracks(tracks);
  REQUIRE(tracks[0].tilt_degrees == kTwinDiscTiltPerTick);
}

TEST_CASE("les deux disques se dessinent par la figure du sous-type 39", "[render][twin_disc]") {
  const EffectTwinDiscPattern pattern = TestPattern();
  std::array<EffectTrack, kEffectTracksPerNode> tracks = SeedEffectTwinDiscTracks(pattern);
  std::size_t count = 0;
  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    count += ComposeBreathingQuad(tracks[rang], pattern.node_style, rang, kAtTheOrigin).size();
  }
  // Les deux pistes éteintes ne rendent rien ; les deux vives rendent au moins
  // un quad chacune.
  REQUIRE(count >= 2);
}

TEST_CASE("l'inclinaison fait RESPIRER le rayon", "[render][twin_disc]") {
  // ⚠ C'EST L'INCLINAISON QUI MÈNE LE SOUFFLE, pas un compteur de tics — voir
  // `effect_breathing_quad.h`. L'oublier figerait les deux disques.
  const EffectTwinDiscPattern pattern = TestPattern();
  std::array<EffectTrack, kEffectTracksPerNode> tracks = SeedEffectTwinDiscTracks(pattern);
  const std::vector<EffectWorldQuad> before =
      ComposeBreathingQuad(tracks[0], pattern.node_style, 0, kAtTheOrigin);
  for (int tick = 0; tick < 30; ++tick) {
    AdvanceTwinDiscTracks(tracks);
  }
  const std::vector<EffectWorldQuad> after =
      ComposeBreathingQuad(tracks[0], pattern.node_style, 0, kAtTheOrigin);
  REQUIRE_FALSE(before.empty());
  REQUIRE_FALSE(after.empty());
  REQUIRE(before[0].corners[0].x != Approx(after[0].corners[0].x));
}

TEST_CASE("le noeud seme porte son style, son mot de rendu et son image", "[render][twin_disc]") {
  const PooledEffectNode node = SowEffectTwinDiscNode(
      PatternOf(201U, "data\\texture\\effect\\pikapika2.bmp"), kAtTheOrigin, 100);
  REQUIRE(node.shape == ro::render::PooledNodeShape::kTwinDisc);
  // ⚠ LE STYLE EST UN FLOTTANT DU NŒUD — quatre pour l'octet zéro —, et non
  // le mot de rendu treize. La variante de traînée le
  // porte, comme pour le sous-type 39.
  CHECK(node.trail_variant == 4);
  // ⚠ ET LE MOT DE RENDU VA DANS LE NŒUD : 13.
  REQUIRE(node.node.render_word.has_value());
  CHECK(*node.node.render_word == kWordOfTwoArguments);
  REQUIRE(node.resource_paths.size() == 1);
  CHECK(node.resource_paths.front() == "data\\texture\\effect\\pikapika2.bmp");
  CHECK(node.node.life_ticks == 100);
}

TEST_CASE("les deux mots de rendu gardent l'addition que le style demande", "[render][twin_disc]") {
  // Treize et cinq portent tous deux le bit du mélange PROPRE (0x4) : le seau
  // lit celui de la primitive et
  // passe l'épreuve de profondeur. Le quad additif reste additif.
  for (const std::uint32_t word : {kWordOfTwoArguments, kWordOfOneArgument}) {
    INFO("mot " << word);
    const ro::render::EffectRenderBlend blend =
        ro::render::EffectWorldQuadBlend(kBlendOne, kBlendOne, word);
    CHECK(blend.source == kBlendOne);
    CHECK(blend.destination == kBlendOne);
    CHECK(ro::render::EffectLotIsDepthTested(word));
  }
  for (const EffectTwinDiscPattern& pattern : EffectTwinDiscPatternTable()) {
    INFO("effet " << pattern.effect_id);
    CHECK(pattern.render_word ==
          (pattern.builder_argument.has_value() ? kWordOfTwoArguments : kWordOfOneArgument));
  }
}

TEST_CASE("la table des deux disques porte cinq identifiants sur six lignes",
          "[render][twin_disc]") {
  const std::span<const EffectTwinDiscPattern> table = EffectTwinDiscPatternTable();
  REQUIRE(table.size() == 6);
  REQUIRE(std::ranges::is_sorted(table, {}, &EffectTwinDiscPattern::effect_id));
  for (const std::uint32_t each_effect_id : {201U, 398U, 650U, 680U}) {
    REQUIRE(EffectTwinDiscPatternsFor(each_effect_id).size() == 1);
  }
  // ⚠ LE 881 APPELLE SON BÂTISSEUR DEUX FOIS, sur deux images différentes, et
  // dans cet ordre : octet deux, puis octet trois.
  const std::span<const EffectTwinDiscPattern> du_881 = EffectTwinDiscPatternsFor(881U);
  REQUIRE(du_881.size() == 2);
  CHECK(du_881[0].builder_argument == std::uint8_t{2});
  CHECK(du_881[1].builder_argument == std::uint8_t{3});
  REQUIRE(EffectTwinDiscPatternsFor(1U).empty());
}

TEST_CASE("le batisseur a un argument : style quatre, rayon quarante-six, alpha vingt-cinq",
          "[render][twin_disc]") {
  // ⚠ QUARANTE-SIX, pas quarante-huit ; et le cinq est son MOT DE RENDU, pas son style.
  const EffectTwinDiscPattern& single = PatternOf(650U, "data\\texture\\effect\\pikapika2.bmp");
  CHECK(single.client_function == "Effect_Build_TwinDisc1cFromParam");
  CHECK_FALSE(single.builder_argument.has_value());
  CHECK(single.node_style == 4);
  CHECK(single.radius == Approx(46.0F));
  CHECK(single.alpha == 25);
  CHECK(single.render_word == kWordOfOneArgument);

  const EffectTwinDiscPattern& two = PatternOf(201U, "data\\texture\\effect\\pikapika2.bmp");
  CHECK(two.node_style == 4);
  CHECK(two.radius == Approx(15.0F));
  CHECK(two.alpha == 200);
  CHECK(two.render_word == kWordOfTwoArguments);
}

TEST_CASE("l'octet se lit comme chez le sous-type 161, qui est le meme code",
          "[render][twin_disc]") {
  // ⚠ DEUX LECTURES DU MÊME SWITCH, l'une par bâtisseur. Elles doivent dire la
  // même chose, octet pour octet.
  std::size_t relus = 0;
  for (const EffectTwinDiscPattern& pattern : EffectTwinDiscPatternTable()) {
    if (!pattern.builder_argument.has_value()) {
      continue;
    }
    INFO("effet " << pattern.effect_id << ", octet "
                  << static_cast<int>(*pattern.builder_argument));
    const ro::render::EffectFadingTwinDiscSeed seed =
        EffectFadingTwinDiscSeedFor(*pattern.builder_argument);
    CHECK(pattern.node_style == seed.node_style);
    CHECK(pattern.radius == Approx(seed.radius_at_anchor));
    ++relus;
  }
  CHECK(relus == 5);
}

TEST_CASE("l'octet trois est le seul a changer le rayon, et il vaut vingt-quatre",
          "[render][twin_disc]") {
  // ⚠ VINGT-QUATRE, pas vingt-cinq.
  for (const EffectTwinDiscPattern& pattern : EffectTwinDiscPatternTable()) {
    if (!pattern.builder_argument.has_value()) {
      continue;
    }
    INFO("effet " << pattern.effect_id);
    const bool large = *pattern.builder_argument == 3;
    CHECK(pattern.radius == Approx(large ? 24.0F : 15.0F));
  }
}

TEST_CASE("les disques prennent la teinte que leur octet choisit", "[render][twin_disc]") {
  // Les teintes du dessinateur, en ADDITION : l'octet
  // zéro rend le blanc du cas par défaut, les trois autres une couleur.
  struct Expected {
    std::uint32_t effect_id;
    std::string_view texture;
    std::array<std::uint8_t, 4> tint;
  };
  const std::array<Expected, 4> attendus = {{
      {201U, "data\\texture\\effect\\pikapika2.bmp", {255, 255, 255, 200}},
      {680U, "data\\texture\\effect\\pikapika2.bmp", {35, 255, 35, 200}},
      {881U, "data\\texture\\effect\\emp shock.tga", {105, 105, 255, 200}},
      {881U, "data\\texture\\effect\\cir0002.bmp", {35, 35, 255, 200}},
  }};
  for (const Expected& expected : attendus) {
    INFO("effet " << expected.effect_id << ", " << expected.texture);
    const EffectWorldQuad quad = FirstQuad(PatternOf(expected.effect_id, expected.texture));
    CHECK(quad.tint == expected.tint);
    CHECK(quad.blend_destination == kBlendOne);
  }
}

TEST_CASE("les disques suivent leur porteur", "[render][twin_disc]") {
  // ⚠ LE MOUVEMENT RECOPIE LA PLACE DE L'INSTANCE, que
  // le bâtisseur recale sur le porteur à chaque tic : le pas que la troupe
  // applique doit survivre au tic suivant.
  constexpr std::uint32_t kBearer = 7;
  constexpr glm::vec3 kFarther{10.0F, 0.0F, 4.0F};
  EffectNodePool pool;
  pool.AnchorNextTo(kBearer, kAtTheOrigin);
  pool.Add(SowEffectTwinDiscNode(TestPattern(), kAtTheOrigin, 100));
  pool.AnchorNextTo(0);
  ro::test_support::MoveBearer(pool, kBearer, kFarther);
  pool.Advance(static_cast<std::uint32_t>(ro::render::kEffectTickMs));
  REQUIRE(pool.size() == 1);
  const PooledEffectNode& node = pool.nodes().front();
  CHECK(node.ground_position == kFarther);
  CHECK(node.tracks[0].tilt_degrees == kTwinDiscTiltPerTick);
}
