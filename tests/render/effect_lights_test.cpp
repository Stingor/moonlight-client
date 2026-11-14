// LES LUMIÈRES DES EFFETS : la lueur d'une texture, une lumière par effet
// joué, et leur place dans le jeu de l'image.

#include "ro/render/effect_lights.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/formats/str.h"
#include "ro/render/effect_dye.h"
#include "ro/render/effect_render_word.h"

#include "support/effect_builder.h"

using Catch::Approx;
using ro::render::EffectLightsOf;
using ro::render::GlowOfImage;
using ro::render::PointLight;
using ro::render::TextureGlow;

namespace {

constexpr std::int32_t kSide = 16;
constexpr std::uint8_t kOpaque = 255;

// Une image de `kSide` de côté, noire et opaque, dont les `lit` premiers texels
// prennent la couleur `rgb` et l'opacité `alpha`.
ro::formats::Image Image(std::size_t lit, std::array<std::uint8_t, 3> rgb,
                         std::uint8_t alpha = kOpaque) {
  ro::formats::Image image;
  image.width = kSide;
  image.height = kSide;
  image.pixels.assign(static_cast<std::size_t>(kSide * kSide) * 4, 0);
  for (std::size_t texel = 0; texel < image.pixels.size() / 4; ++texel) {
    image.pixels[texel * 4 + 3] = kOpaque;
  }
  for (std::size_t texel = 0; texel < lit; ++texel) {
    image.pixels[texel * 4 + 0] = rgb[0];
    image.pixels[texel * 4 + 1] = rgb[1];
    image.pixels[texel * 4 + 2] = rgb[2];
    image.pixels[texel * 4 + 3] = alpha;
  }
  return image;
}

constexpr std::size_t kHalf = static_cast<std::size_t>(kSide * kSide) / 2;
constexpr std::array<std::uint8_t, 3> kOrange = {255, 128, 0};

// Un effet d'une couche, centrée sur l'origine du format, de 128 pixels de côté,
// qui se mêle par `blend_destination`.
ro::render::LoadedEffect Loaded(
    std::int32_t blend_destination, ro::formats::Image texture,
    std::int32_t blend_source = ro::render::kEffectRenderBlendSourceAlpha) {
  ro::test_support::StrKeyFrameSpec key;
  key.offset_x = ro::formats::kStrOriginX;
  key.offset_y = ro::formats::kStrOriginY;
  key.blend_source = blend_source;
  key.blend_destination = blend_destination;
  ro::test_support::StrLayerSpec layer;
  layer.texture_names = {"a.bmp"};
  layer.key_frames = {key};
  ro::test_support::StrBuildOptions options;
  options.last_frame = 10;
  options.layers = {layer};
  auto effect = ro::formats::StrEffect::Parse(ro::test_support::BuildStr(options));
  REQUIRE(effect.has_value());
  ro::render::LoadedEffect loaded;
  loaded.prepared = ro::render::PrepareEffect(*effect);
  loaded.textures = {std::move(texture)};
  loaded.texture_paths = {"effect/a.bmp"};
  return loaded;
}

std::vector<PointLight> LightsOf(const ro::render::DrawnEffect& drawn) {
  ro::render::TextureGlowCache glows;
  std::vector<PointLight> lights;
  EffectLightsOf(std::span(&drawn, 1), {}, ro::render::BillboardBasis(), glows, lights);
  return lights;
}

// Un jeu de `count` lumières, à un pas les unes des autres le long de x.
ro::render::PointLightSet Spaced(std::size_t count, float first_x) {
  std::vector<PointLight> lights;
  for (std::size_t rank = 0; rank < count; ++rank) {
    lights.push_back(PointLight{.world_position = {first_x + static_cast<float>(rank), 0.0F, 0.0F},
                                .color = glm::vec3(1.0F),
                                .range_world = 10.0F});
  }
  return ro::render::PointLightsByDistance(lights, glm::vec3(0.0F));
}

}  // namespace

TEST_CASE("Lueur d'une texture : la couleur moyenne de ses texels clairs", "[render][effect]") {
  // Moitié orange, moitié noire : le noir ne tire pas la couleur vers lui.
  const TextureGlow glow = GlowOfImage(Image(kHalf, kOrange));
  CHECK(glow.color.r == Approx(1.0F));
  CHECK(glow.color.g == Approx(128.0F / 255.0F));
  CHECK(glow.color.b == Approx(0.0F));
  CHECK(glow.strength == Approx(1.0F));
}

TEST_CASE("Lueur d'une texture : une texture sombre, vide ou transparente n'éclaire pas",
          "[render][effect]") {
  // Un gris toute l'image, sous le seuil d'un texel clair : une fumée.
  CHECK(GlowOfImage(Image(kHalf * 2, {60, 60, 60})).strength == 0.0F);
  CHECK(GlowOfImage(ro::formats::Image()).strength == 0.0F);
  // Des texels clairs mais transparents : la couleur clé d'un fond.
  CHECK(GlowOfImage(Image(kHalf, {255, 255, 255}, 0)).strength == 0.0F);
  // Un seul texel blanc sur 256 : sous la couverture minimale.
  CHECK(GlowOfImage(Image(1, {255, 255, 255})).strength == 0.0F);
}

TEST_CASE("Lueur d'une texture : une étincelle luit en proportion de ce qu'elle couvre",
          "[render][effect]") {
  // Huit texels blancs sur 256 : une couverture de 1/32, sous la pleine lueur.
  const TextureGlow spark = GlowOfImage(Image(8, {255, 255, 255}));
  CHECK(spark.strength == Approx(1.0F / 32.0F / ro::render::kGlowFullCoverage));
  CHECK(spark.color == glm::vec3(1.0F));
}

TEST_CASE("Lueur d'une texture : le cache la calcule une fois par chemin", "[render][effect]") {
  ro::render::TextureGlowCache glows;
  CHECK(glows.Of("effect/a.bmp", Image(kHalf, kOrange)).strength == Approx(1.0F));
  // Le même chemin rend la même lueur, quelle que soit l'image passée.
  CHECK(glows.Of("effect/a.bmp", ro::formats::Image()).strength == Approx(1.0F));
  CHECK(glows.size() == 1);
}

TEST_CASE("Lumière d'un effet : une couche additive éclaire, à la hauteur de son ancre",
          "[render][effect]") {
  const ro::render::LoadedEffect loaded =
      Loaded(ro::render::kEffectRenderBlendOne, Image(kHalf, kOrange));
  ro::render::DrawnEffect drawn;
  drawn.effect = &loaded;
  drawn.ground_position = {100.0F, 0.0F, 50.0F};
  const std::vector<PointLight> lights = LightsOf(drawn);
  REQUIRE(lights.size() == 1);
  // L'ancre relevée de quatre-vingts pixels, sept par unité du monde.
  CHECK(lights[0].world_position.x == Approx(100.0F));
  CHECK(lights[0].world_position.y ==
        Approx(ro::render::kEffectAnchorLiftPixels / ro::render::kSpritePixelsPerWorldUnit));
  CHECK(lights[0].world_position.z == Approx(50.0F));
  // Orange, au gain d'une couche pleine.
  CHECK(lights[0].color.r == Approx(ro::render::kEffectLightGain));
  CHECK(lights[0].color.g == Approx(ro::render::kEffectLightGain * 128.0F / 255.0F));
  CHECK(lights[0].color.b == Approx(0.0F));
  // Un carré de 128 pixels : un demi-diamètre de 64·√2 pixels.
  const float half_size = 64.0F * std::sqrt(2.0F) / ro::render::kSpritePixelsPerWorldUnit;
  CHECK(lights[0].range_world == Approx(half_size * ro::render::kEffectLightRangePerSize));
  // Son ombre est douce : partielle, avec une pénombre.
  CHECK(lights[0].shadow_depth == ro::render::kEffectShadowDepth);
  CHECK(lights[0].shadow_spread == ro::render::kEffectShadowSpread);
}

TEST_CASE("Lumière d'un effet : ni une couche recouvrante, ni un effet éteint",
          "[render][effect]") {
  // Le recouvrement par l'alpha : la couche peint, elle n'ajoute pas de lumière.
  constexpr std::int32_t kInverseSourceAlpha = 6;
  const ro::render::LoadedEffect over = Loaded(kInverseSourceAlpha, Image(kHalf, kOrange));
  ro::render::DrawnEffect drawn;
  drawn.effect = &over;
  CHECK(LightsOf(drawn).empty());

  // Une source nulle n'ajoute rien, même sur un fond gardé.
  constexpr std::int32_t kZero = 1;
  const ro::render::LoadedEffect from_zero =
      Loaded(ro::render::kEffectRenderBlendOne, Image(kHalf, kOrange), kZero);
  drawn.effect = &from_zero;
  CHECK(LightsOf(drawn).empty());

  // LE COUPLE DOMINANT DES FICHIERS — la destination par son propre alpha — est
  // une addition : la scène est opaque.
  constexpr std::int32_t kDestinationAlpha = 7;
  const ro::render::LoadedEffect added = Loaded(kDestinationAlpha, Image(kHalf, kOrange));
  drawn.effect = &added;
  CHECK(LightsOf(drawn).size() == 1);
  drawn.alpha = 0.0F;
  CHECK(LightsOf(drawn).empty());
  // Une couche à moitié éteinte éclaire à moitié.
  drawn.alpha = 0.5F;
  const std::vector<PointLight> half = LightsOf(drawn);
  REQUIRE(half.size() == 1);
  CHECK(half[0].color.r == Approx(ro::render::kEffectLightGain * 0.5F));
}

TEST_CASE("Lumière d'un effet : la teinture de l'effet teint sa lumière", "[render][effect]") {
  const ro::render::LoadedEffect loaded =
      Loaded(ro::render::kEffectRenderBlendOne, Image(kHalf, kOrange));
  ro::render::DrawnEffect drawn;
  drawn.effect = &loaded;
  // Un tour de 180° : l'orange vire au bleu-vert, et le rouge s'éteint.
  drawn.dye = ro::render::MakeEffectDye(180, 0, 0, false);
  const std::vector<PointLight> turned = LightsOf(drawn);
  REQUIRE(turned.size() == 1);
  CHECK(turned[0].color.r < turned[0].color.b);
  CHECK(turned[0].color.r == Approx(0.0F).margin(0.01F));
  // Assombri, l'effet se peint en fumée : il n'éclaire plus.
  drawn.dye = ro::render::MakeEffectDye(0, 0, 0, true);
  CHECK(LightsOf(drawn).empty());
}

TEST_CASE("Lumière d'un effet : un lot de quads du monde éclaire depuis son centre",
          "[render][effect]") {
  const ro::formats::Image texture = Image(kHalf, {255, 255, 255});
  const std::string path = "effect/ring.bmp";
  ro::render::EffectWorldQuad quad;
  quad.corners = {glm::vec3(0.0F, 5.0F, 0.0F), glm::vec3(20.0F, 5.0F, 0.0F),
                  glm::vec3(20.0F, 5.0F, 20.0F), glm::vec3(0.0F, 5.0F, 20.0F)};
  quad.blend_source = ro::render::kEffectRenderBlendSourceAlpha;
  quad.blend_destination = ro::render::kEffectRenderBlendOne;
  const ro::render::DrawnWorldQuads lot{
      .quads = std::span(&quad, 1), .texture_path = &path, .texture = &texture};

  ro::render::TextureGlowCache glows;
  std::vector<PointLight> lights;
  EffectLightsOf({}, std::span(&lot, 1), ro::render::BillboardBasis(), glows, lights);
  REQUIRE(lights.size() == 1);
  CHECK(lights[0].world_position == glm::vec3(10.0F, 5.0F, 10.0F));
  // Blanc, à pleine intensité.
  CHECK(lights[0].color.r == Approx(ro::render::kEffectLightGain));
  CHECK(lights[0].color.g == Approx(lights[0].color.r));
}

TEST_CASE("Jeu de l'image : le document d'abord, tout entier, puis les effets les plus proches",
          "[render][effect]") {
  // Un grand document et six effets : tout le document, dans son ordre, puis
  // les `kMaxEffectLights` effets les plus proches.
  constexpr std::size_t kDocumentLights = 40;
  const ro::render::PointLightSet document = Spaced(kDocumentLights, 1.0F);
  const ro::render::PointLightSet effects = Spaced(6, 100.0F);
  const ro::render::PointLightSet full =
      ro::render::WithEffectLights(document, effects.lights, glm::vec3(0.0F));
  REQUIRE(full.lights.size() == kDocumentLights + ro::render::kMaxEffectLights);
  CHECK(full.lights[kDocumentLights - 1].world_position.x ==
        Approx(static_cast<float>(kDocumentLights)));
  CHECK(full.lights[kDocumentLights].world_position.x == Approx(100.0F));
  CHECK(full.lights.back().world_position.x ==
        Approx(100.0F + static_cast<float>(ro::render::kMaxEffectLights - 1)));
}
