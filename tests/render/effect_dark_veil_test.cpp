#include "ro/render/effect_dark_veil.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <set>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/camera.h"
#include "ro/render/effect_dark_veil_seed.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_render_word.h"
#include "ro/render/effect_screen_quad.h"

using Catch::Approx;
using ro::render::Camera;
using ro::render::ComposeScreenDarkVeilQuads;
using ro::render::DarkVeilScreenRadiusPixels;
using ro::render::EffectDarkVeilPattern;
using ro::render::EffectDarkVeilPatternsFor;
using ro::render::EffectDarkVeilPatternTable;
using ro::render::EffectDarkVeilShapeFor;
using ro::render::EffectNodePool;
using ro::render::EffectScreenPositionOf;
using ro::render::EffectScreenQuad;
using ro::render::EffectTrack;
using ro::render::kDarkVeilCoreQuads;
using ro::render::kDarkVeilCoreTexture;
using ro::render::kDarkVeilDeepChannel;
using ro::render::kDarkVeilFrameTexture;
using ro::render::kDarkVeilPaleChannel;
using ro::render::kDarkVeilQuads;
using ro::render::kEffectTrackAlphaMax;
using ro::render::kEffectTracksPerNode;
using ro::render::kGameElevationDegrees;
using ro::render::kGameFovDegrees;
using ro::render::kGameViewDistance;
using ro::render::PooledNodeShape;
using ro::render::SeedEffectDarkVeilTracks;
using ro::render::SowEffectDarkVeilNode;

namespace {

constexpr float kViewportWidth = 1760.0F;
constexpr float kViewportHeight = 990.0F;
constexpr glm::vec2 kScreenMiddle{kViewportWidth / 2.0F, kViewportHeight / 2.0F};
constexpr glm::vec3 kBearer{120.0F, 4.0F, -80.0F};

// La piste d'un voile, déjà rendue visible — le semis la donne éteinte.
[[nodiscard]] EffectTrack VisibleTrack(std::uint8_t builder_byte, std::int16_t alpha) {
  const EffectDarkVeilPattern pattern{0, "essai", {{"coeur", "cadre"}}, builder_byte};
  EffectTrack track = SeedEffectDarkVeilTracks(pattern).front();
  track.alpha = alpha;
  return track;
}

// La caméra du jeu, visant le porteur, au recul donné.
[[nodiscard]] Camera GameCamera(float distance, float yaw_degrees) {
  Camera camera;
  camera.look_at = kBearer;
  camera.distance = distance;
  camera.yaw_degrees = yaw_degrees;
  camera.pitch_degrees = kGameElevationDegrees;
  camera.fov_degrees = kGameFovDegrees;
  return camera;
}

struct PixelBox {
  glm::vec2 min{0.0F, 0.0F};
  glm::vec2 max{0.0F, 0.0F};
};

[[nodiscard]] PixelBox BoxOf(const EffectScreenQuad& quad) {
  PixelBox box{quad.corners[0], quad.corners[0]};
  for (const glm::vec2& corner : quad.corners) {
    box.min = glm::min(box.min, corner);
    box.max = glm::max(box.max, corner);
  }
  return box;
}

[[nodiscard]] float AreaOf(const PixelBox& box) {
  return (box.max.x - box.min.x) * (box.max.y - box.min.y);
}

[[nodiscard]] float OverlapOf(const PixelBox& first, const PixelBox& second) {
  const float width = std::min(first.max.x, second.max.x) - std::max(first.min.x, second.min.x);
  const float height = std::min(first.max.y, second.max.y) - std::max(first.min.y, second.min.y);
  return std::max(width, 0.0F) * std::max(height, 0.0F);
}

// La boîte du cœur : l'union de ses quatre quadrants.
[[nodiscard]] PixelBox CoreBoxOf(const std::vector<EffectScreenQuad>& quads) {
  PixelBox box = BoxOf(quads.front());
  for (const EffectScreenQuad& quad : quads) {
    if (quad.texture != kDarkVeilCoreTexture) {
      continue;
    }
    const PixelBox own = BoxOf(quad);
    box.min = glm::min(box.min, own.min);
    box.max = glm::max(box.max, own.max);
  }
  return box;
}

// L'écran est pavé exactement, sans recouvrement : la somme des aires vaut
// celle de la fenêtre, et aucune paire ne se chevauche.
void CheckPavesTheScreen(const std::vector<EffectScreenQuad>& quads) {
  float area = 0.0F;
  for (std::size_t first = 0; first < quads.size(); ++first) {
    const PixelBox box = BoxOf(quads[first]);
    CHECK(box.min.x >= 0.0F);
    CHECK(box.min.y >= 0.0F);
    CHECK(box.max.x <= kViewportWidth);
    CHECK(box.max.y <= kViewportHeight);
    area += AreaOf(box);
    for (std::size_t second = first + 1; second < quads.size(); ++second) {
      INFO("quads " << first << " et " << second);
      CHECK(OverlapOf(box, BoxOf(quads[second])) == Approx(0.0F).margin(0.01F));
    }
  }
  CHECK(area == Approx(kViewportWidth * kViewportHeight));
}

}  // namespace

TEST_CASE("le voile ne dessine rien tant qu'il n'est pas venu", "[render][dark_veil]") {
  // ⚠ LA PROPRIÉTÉ QUI COMPTE : le semis donne un alpha NUL, donc aucun quad au
  // premier tic. C'est le mouvement qui fait venir la nuit.
  const EffectDarkVeilPattern& pattern = EffectDarkVeilPatternsFor(334).front();
  const EffectTrack fresh = SeedEffectDarkVeilTracks(pattern).front();
  CHECK(fresh.active);
  CHECK(fresh.alpha == 0);
  CHECK(ComposeScreenDarkVeilQuads(fresh, kScreenMiddle, kViewportWidth, kViewportHeight).empty());

  EffectTrack faded = fresh;
  faded.active = false;
  faded.alpha = 200;
  CHECK(ComposeScreenDarkVeilQuads(faded, kScreenMiddle, kViewportWidth, kViewportHeight).empty());
}

TEST_CASE("le cœur a, à tous les zooms, le rayon qu'il a au recul du jeu", "[render][dark_veil]") {
  // 🔴 L'ESSAI DU CHOIX DE CE CLIENT. Au recul du jeu, un point écarté du
  // porteur de son rayon, perpendiculairement à la visée, tombe à
  // `DarkVeilScreenRadiusPixels` pixels de lui : c'est la taille que le jeu
  // donne au cœur. Posé dans le monde, le voile rapetissait de moitié au recul
  // double ; peint à l'écran, il la garde.
  const float radius = EffectDarkVeilShapeFor(0).radius_at_anchor;
  const Camera at_game_distance = GameCamera(kGameViewDistance, 0.0F);
  const std::optional<glm::vec2> centre =
      EffectScreenPositionOf(at_game_distance, kBearer, kViewportWidth, kViewportHeight);
  const std::optional<glm::vec2> aside = EffectScreenPositionOf(
      at_game_distance, kBearer + glm::vec3{radius, 0.0F, 0.0F}, kViewportWidth, kViewportHeight);
  REQUIRE(centre.has_value());
  REQUIRE(aside.has_value());
  const float projected = glm::length(*aside - *centre);
  CHECK(DarkVeilScreenRadiusPixels(radius, kViewportHeight) == Approx(projected).epsilon(0.001));
  // Soixante-cinq unités font environ deux cents pixels sur une fenêtre de
  // neuf cent quatre-vingt-dix : la moitié du cœur que montre le jeu.
  CHECK(projected == Approx(200.0F).margin(2.0F));

  for (const float distance : {kGameViewDistance, kGameViewDistance * 2.0F, 300.0F}) {
    for (const float yaw : {0.0F, 45.0F, 200.0F}) {
      INFO("recul " << distance << ", lacet " << yaw);
      const Camera camera = GameCamera(distance, yaw);
      const std::optional<glm::vec2> bearer =
          EffectScreenPositionOf(camera, kBearer, kViewportWidth, kViewportHeight);
      REQUIRE(bearer.has_value());
      const std::vector<EffectScreenQuad> quads =
          ComposeScreenDarkVeilQuads(VisibleTrack(0, 128), bearer, kViewportWidth, kViewportHeight);
      REQUIRE(quads.size() == kDarkVeilQuads);
      const PixelBox core = CoreBoxOf(quads);
      // Centré sur la place d'écran du porteur, du même rayon à chaque recul.
      CHECK((core.min.x + core.max.x) / 2.0F == Approx(bearer->x));
      CHECK((core.min.y + core.max.y) / 2.0F == Approx(bearer->y));
      CHECK((core.max.x - core.min.x) / 2.0F == Approx(projected).epsilon(0.001));
      CHECK((core.max.y - core.min.y) / 2.0F == Approx(projected).epsilon(0.001));
    }
  }
}

TEST_CASE("le rayon suit la hauteur de la fenêtre", "[render][dark_veil]") {
  const float radius = EffectDarkVeilShapeFor(0).radius_at_anchor;
  CHECK(DarkVeilScreenRadiusPixels(radius, 2.0F * kViewportHeight) ==
        Approx(2.0F * DarkVeilScreenRadiusPixels(radius, kViewportHeight)));
}

TEST_CASE("chaque quadrant pose le coin transparent de l'image au centre", "[render][dark_veil]") {
  // ⚠ L'IMAGE DU CŒUR EST UN QUART DE DISQUE dont le coin BAS-GAUCHE est
  // transparent : c'est lui qui va au centre, et le coin opposé au bord. Le
  // poser à l'envers mettait le noir sur le porteur et la lumière aux coins.
  const std::vector<EffectScreenQuad> quads = ComposeScreenDarkVeilQuads(
      VisibleTrack(0, 128), kScreenMiddle, kViewportWidth, kViewportHeight);
  REQUIRE(quads.size() == kDarkVeilQuads);
  std::set<std::pair<int, int>> quadrants;
  for (std::size_t rank = 0; rank < kDarkVeilCoreQuads; ++rank) {
    INFO("quadrant " << rank);
    CHECK(quads[rank].texture == kDarkVeilCoreTexture);
    bool centre_seen = false;
    for (std::size_t corner = 0; corner < quads[rank].corners.size(); ++corner) {
      const glm::vec2 offset = quads[rank].corners[corner] - kScreenMiddle;
      if (glm::length(offset) < 0.01F) {
        centre_seen = true;
        CHECK(quads[rank].texture_uv[corner].x == Approx(0.0F).margin(1e-5));
        CHECK(quads[rank].texture_uv[corner].y == Approx(1.0F));
      } else if (std::abs(offset.x) > 1.0F && std::abs(offset.y) > 1.0F) {
        CHECK(quads[rank].texture_uv[corner].x == Approx(1.0F));
        CHECK(quads[rank].texture_uv[corner].y == Approx(0.0F).margin(1e-5));
        quadrants.insert({offset.x > 0.0F ? 1 : -1, offset.y > 0.0F ? 1 : -1});
      }
    }
    CHECK(centre_seen);
  }
  // Les quatre coins du carré, et non deux fois le même.
  CHECK(quadrants.size() == kDarkVeilCoreQuads);
}

TEST_CASE("le noir couvre tout l'écran hors du cœur", "[render][dark_veil]") {
  SECTION("porteur au milieu") {
    const std::vector<EffectScreenQuad> quads = ComposeScreenDarkVeilQuads(
        VisibleTrack(0, 128), kScreenMiddle, kViewportWidth, kViewportHeight);
    REQUIRE(quads.size() == kDarkVeilQuads);
    for (std::size_t rank = kDarkVeilCoreQuads; rank < quads.size(); ++rank) {
      CHECK(quads[rank].texture == kDarkVeilFrameTexture);
    }
    CheckPavesTheScreen(quads);
  }
  SECTION("porteur au bord : le cœur déborde, rien ne sort de l'écran que lui") {
    const glm::vec2 near_corner{30.0F, kViewportHeight - 20.0F};
    const std::vector<EffectScreenQuad> quads = ComposeScreenDarkVeilQuads(
        VisibleTrack(0, 128), near_corner, kViewportWidth, kViewportHeight);
    float frame_area = 0.0F;
    for (const EffectScreenQuad& quad : quads) {
      if (quad.texture == kDarkVeilFrameTexture) {
        const PixelBox box = BoxOf(quad);
        CHECK(box.min.x >= 0.0F);
        CHECK(box.max.y <= kViewportHeight);
        frame_area += AreaOf(box);
      }
    }
    // Le cadre couvre l'écran moins la part du cœur qui y tombe.
    const float radius =
        DarkVeilScreenRadiusPixels(EffectDarkVeilShapeFor(0).radius_at_anchor, kViewportHeight);
    const float core_inside =
        (near_corner.x + radius) * (radius + (kViewportHeight - near_corner.y));
    CHECK(frame_area == Approx(kViewportWidth * kViewportHeight - core_inside));
  }
  SECTION("porteur sans place d'écran : tout est noir") {
    const std::vector<EffectScreenQuad> quads = ComposeScreenDarkVeilQuads(
        VisibleTrack(0, 128), std::nullopt, kViewportWidth, kViewportHeight);
    REQUIRE(quads.size() == 1);
    CHECK(quads.front().texture == kDarkVeilFrameTexture);
    CheckPavesTheScreen(quads);
  }
}

TEST_CASE("le voile ne peut qu'assombrir", "[render][dark_veil]") {
  // ⚠ SA TEINTE N'EST PAS UN PARAMÈTRE : deux noirs, et rien d'autre.
  SECTION("sans marque, le noir profond") {
    const std::vector<EffectScreenQuad> quads = ComposeScreenDarkVeilQuads(
        VisibleTrack(0, 200), kScreenMiddle, kViewportWidth, kViewportHeight);
    REQUIRE_FALSE(quads.empty());
    for (const EffectScreenQuad& quad : quads) {
      CHECK(quad.tint[0] == kDarkVeilDeepChannel);
      CHECK(quad.tint[1] == kDarkVeilDeepChannel);
      CHECK(quad.tint[2] == kDarkVeilDeepChannel);
      CHECK(quad.tint[3] == 200);
      CHECK(quad.blend_mode == ro::render::kDarkVeilOrdinaryBlend);
    }
  }
  SECTION("avec la marque, un noir moins profond") {
    const std::vector<EffectScreenQuad> quads = ComposeScreenDarkVeilQuads(
        VisibleTrack(5, 200), kScreenMiddle, kViewportWidth, kViewportHeight);
    REQUIRE_FALSE(quads.empty());
    CHECK(quads.front().tint[0] == kDarkVeilPaleChannel);
  }
  // Et le pâle reste un noir : plus clair que le profond, loin du blanc.
  CHECK(kDarkVeilPaleChannel > kDarkVeilDeepChannel);
  CHECK(kDarkVeilPaleChannel < 128);
}

TEST_CASE("les onze voiles du client sont en table", "[render][dark_veil]") {
  const std::span<const EffectDarkVeilPattern> table = EffectDarkVeilPatternTable();
  CHECK(table.size() == 11);
  CHECK(std::ranges::is_sorted(table, {}, &EffectDarkVeilPattern::effect_id));

  std::set<std::uint8_t> octets;
  std::set<float> radii;
  for (const EffectDarkVeilPattern& pattern : table) {
    octets.insert(pattern.builder_argument);
    for (const std::string_view image : pattern.textures) {
      CHECK_FALSE(image.empty());
    }
    const float radius = EffectDarkVeilShapeFor(pattern.builder_argument).radius_at_anchor;
    INFO("effet " << pattern.effect_id);
    CHECK(radius > 0.0F);
    radii.insert(radius);
    CHECK(EffectDarkVeilPatternsFor(pattern.effect_id).size() == 1);
  }
  // Onze octets distincts, et onze rayons distincts : aucun des onze voiles n'en
  // répète un autre.
  CHECK(octets.size() == table.size());
  CHECK(radii.size() == table.size());
  CHECK(EffectDarkVeilPatternsFor(1).empty());
}

TEST_CASE("l'octet zéro est un cas à part, pas le premier d'une suite", "[render][dark_veil]") {
  // ⚠ SON RAYON NE SUIT PAS LA RÈGLE DES DIX AUTRES — le client l'écrit comme une
  // constante —, et lui seul donne le noir profond. Le ranger dans la suite
  // l'aurait mis à cent quarante, plus large que tous.
  CHECK(EffectDarkVeilShapeFor(0).radius_at_anchor == Approx(65.0F));
  CHECK(EffectDarkVeilShapeFor(0).shade_mark == 0);
  CHECK(EffectDarkVeilShapeFor(1).radius_at_anchor == Approx(130.0F));
  CHECK(EffectDarkVeilShapeFor(10).radius_at_anchor == Approx(40.0F));
  for (std::uint8_t octet = 1; octet <= 10; ++octet) {
    INFO("octet " << static_cast<int>(octet));
    CHECK(EffectDarkVeilShapeFor(octet).shade_mark != 0);
    // La suite RÉTRÉCIT : chaque octet donne un voile plus étroit que le
    // précédent. La croire croissante inverserait les onze.
    if (octet > 1) {
      CHECK(EffectDarkVeilShapeFor(octet).radius_at_anchor <
            EffectDarkVeilShapeFor(static_cast<std::uint8_t>(octet - 1)).radius_at_anchor);
    }
  }
}

TEST_CASE("le voile s'épaissit d'une unité par tic, puis s'arrête", "[render][dark_veil]") {
  const EffectDarkVeilPattern& pattern = EffectDarkVeilPatternsFor(475).front();
  ro::render::PooledEffectNode node =
      SowEffectDarkVeilNode(pattern, glm::vec3{10.0F, 0.0F, 20.0F}, 4000);
  CHECK(node.shape == PooledNodeShape::kDarkVeil);
  CHECK(node.resource_paths.size() == 2);
  // ⚠ LE VOILE EST SUR SON PORTEUR : l'élévation du bâtisseur ne dure qu'un tic
  // chez le client, et la garder l'enfonçait sous le sol pour toute sa vie.
  CHECK(node.ground_position.y == Approx(0.0F));
  // Le voile se peint par-dessus tout, sous le mélange ordinaire.
  REQUIRE(node.node.render_word.has_value());
  CHECK(*node.node.render_word ==
        (ro::render::kEffectRenderWordOfConstructor | ro::render::kEffectRenderWordOverEverything));
  // Une seule piste vive : les trois autres sont éteintes.
  CHECK(node.tracks[0].active);
  for (std::size_t rang = 1; rang < kEffectTracksPerNode; ++rang) {
    CHECK_FALSE(node.tracks[rang].active);
  }

  EffectNodePool pool;
  pool.Add(node);
  for (int tick = 0; tick < 10; ++tick) {
    pool.Advance(16);
  }
  REQUIRE(pool.size() == 1);
  CHECK(pool.nodes().front().tracks[0].ticks == 10);
  CHECK(pool.nodes().front().tracks[0].alpha == 10);
  // Une piste éteinte n'a ni avancé ni pâli.
  CHECK(pool.nodes().front().tracks[1].ticks == 0);
  CHECK(pool.nodes().front().tracks[1].alpha == 0);

  // ⚠ ET L'ALPHA S'ARRÊTE AU PLAFOND au lieu de reboucler : un alpha qui
  // repasserait par zéro ferait clignoter la nuit.
  for (int tick = 0; tick < 400; ++tick) {
    pool.Advance(16);
  }
  CHECK(pool.nodes().front().tracks[0].alpha == kEffectTrackAlphaMax);
}

namespace {

// UN CŒUR À LA MANIÈRE DU JEU : un disque d'opacité autour du coin bas-gauche,
// qui ne vaut que 245 au milieu des deux bords opposés et atteint le plein vers
// les coins — le défaut que l'image du jeu porte.
constexpr std::int32_t kCoreSide = 64;
constexpr std::uint32_t kGameEdgeAlpha = 245;
constexpr std::size_t kRgba = 4;

[[nodiscard]] ro::formats::Image GameLikeCore() {
  ro::formats::Image image;
  image.width = kCoreSide;
  image.height = kCoreSide;
  image.pixels.assign(static_cast<std::size_t>(kCoreSide * kCoreSide) * kRgba, 255);
  const float side = static_cast<float>(kCoreSide - 1);
  for (std::int32_t row = 0; row < kCoreSide; ++row) {
    for (std::int32_t column = 0; column < kCoreSide; ++column) {
      const float distance = std::hypot(static_cast<float>(column), side - static_cast<float>(row));
      const float alpha = std::min(255.0F, distance / side * static_cast<float>(kGameEdgeAlpha));
      image.pixels[static_cast<std::size_t>(row * kCoreSide + column) * kRgba + 3] =
          static_cast<std::uint8_t>(std::lround(alpha));
    }
  }
  return image;
}

[[nodiscard]] std::uint8_t AlphaAt(const ro::formats::Image& image, std::int32_t column,
                                   std::int32_t row) {
  return image.pixels[static_cast<std::size_t>(row * image.width + column) * kRgba + 3];
}

// L'OPACITÉ EFFECTIVE d'un texel sous une teinte : ce que le mélange ordinaire
// retire de la scène derrière lui.
[[nodiscard]] float EffectiveOpacity(std::uint8_t texel_alpha, std::uint8_t tint_alpha) {
  return static_cast<float>(texel_alpha) / 255.0F * static_cast<float>(tint_alpha) / 255.0F;
}

}  // namespace

TEST_CASE("le bord du cœur rejoint exactement le noir du cadre", "[render][dark_veil]") {
  // 🔴 LE DÉFAUT QUE LE JOUEUR A VU : l'image du jeu ne vaut que 245 au milieu
  // de ses bords, le cadre 255 partout — l'arête du carré se lisait. Étirée,
  // chaque texel des deux bords qui touchent le cadre est opaque, et la couleur
  // effective du bord égale celle des bandes.
  const ro::formats::Image game = GameLikeCore();
  REQUIRE(AlphaAt(game, kCoreSide - 1, kCoreSide - 1) == kGameEdgeAlpha);
  const ro::formats::Image joined = ro::render::DarkVeilCoreJoinedToFrame(game);
  REQUIRE(joined.width == game.width);
  REQUIRE(joined.height == game.height);

  const std::vector<EffectScreenQuad> quads = ComposeScreenDarkVeilQuads(
      VisibleTrack(0, 200), kScreenMiddle, kViewportWidth, kViewportHeight);
  REQUIRE(quads.size() == kDarkVeilQuads);
  const EffectScreenQuad& core = quads.front();
  const EffectScreenQuad& band = quads.back();
  REQUIRE(core.texture == kDarkVeilCoreTexture);
  REQUIRE(band.texture == kDarkVeilFrameTexture);
  // Même teinte, même alpha, même mélange : seule l'image les séparerait.
  CHECK(core.tint == band.tint);
  CHECK(core.blend_mode == band.blend_mode);
  // Le cadre est un blanc opaque.
  const float band_opacity = EffectiveOpacity(255, band.tint[3]);
  for (std::int32_t step = 0; step < kCoreSide; ++step) {
    INFO("texel " << step);
    CHECK(EffectiveOpacity(AlphaAt(joined, kCoreSide - 1, step), core.tint[3]) ==
          Approx(band_opacity));
    CHECK(EffectiveOpacity(AlphaAt(joined, step, 0), core.tint[3]) == Approx(band_opacity));
  }
  // Et l'image du jeu, telle quelle, laissait passer la scène au bord.
  CHECK(EffectiveOpacity(AlphaAt(game, kCoreSide - 1, kCoreSide - 1), core.tint[3]) < band_opacity);

  // LE DISQUE GARDE SA FORME : transparent au centre, un dégradé qui monte, et
  // l'opacité atteinte sur le cercle inscrit plutôt qu'aux coins.
  CHECK(AlphaAt(joined, 0, kCoreSide - 1) == 0);
  const std::uint8_t halfway = AlphaAt(joined, kCoreSide / 2, kCoreSide - 1);
  CHECK(halfway > 100);
  CHECK(halfway < 160);
  for (std::int32_t column = 1; column < kCoreSide; ++column) {
    CHECK(AlphaAt(joined, column, kCoreSide - 1) >= AlphaAt(joined, column - 1, kCoreSide - 1));
  }
}

TEST_CASE("une image sans bord à étirer sort telle quelle", "[render][dark_veil]") {
  ro::formats::Image opaque;
  opaque.width = 2;
  opaque.height = 2;
  opaque.pixels.assign(16, 255);
  CHECK(ro::render::DarkVeilCoreJoinedToFrame(opaque).pixels == opaque.pixels);
  CHECK(ro::render::DarkVeilCoreJoinedToFrame(ro::formats::Image{}).pixels.empty());
}
