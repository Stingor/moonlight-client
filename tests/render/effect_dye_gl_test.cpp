// LA TEINTURE D'UN EFFET, PEINTE PAR LES VRAIS NUANCEURS : un effet synthétique
// d'un quad rouge, additif, sur une cible noire hors écran, peint sans puis
// avec une teinture. Le nuanceur ne se compile qu'à l'exécution : cet essai
// est ce qui prouve qu'il se lie, qu'un lot teint change de couleur, et
// qu'« assombrir » ne fait rien disparaître.

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <glm/glm.hpp>

#include "ro/formats/image.h"
#include "ro/formats/spr.h"
#include "ro/formats/str.h"
#include "ro/render/actor_sprite.h"
#include "ro/render/camera.h"
#include "ro/render/effect_catalog.h"
#include "ro/render/effect_color_tally.h"
#include "ro/render/effect_dye.h"
#include "ro/render/effect_frame.h"
#include "ro/render/effect_renderer.h"
#include "ro/render/gl_context.h"
#include "ro/render/offscreen_target.h"
#include "ro/render/sprite_frame.h"
#include "ro/render/sprite_renderer.h"

#include "support/effect_builder.h"

namespace {

constexpr int kSide = 64;
constexpr std::size_t kRgbaBytes = 4;
constexpr int kTextureSide = 8;
constexpr std::uint8_t kFull = 255;
constexpr std::array<float, 4> kBlack = {0.0F, 0.0F, 0.0F, 1.0F};
// Un tour de teinte d'un tiers : le rouge devient vert.
constexpr int kThirdTurn = 120;

// Un effet d'une couche et d'un quad, peint d'un rouge plein.
ro::render::LoadedEffect RedQuad() {
  ro::test_support::StrLayerSpec layer;
  layer.texture_names = {"red.bmp"};
  layer.key_frames.push_back(ro::test_support::StrKeyFrameSpec{
      .offset_x = ro::formats::kStrOriginX, .offset_y = ro::formats::kStrOriginY});
  ro::test_support::StrBuildOptions options;
  options.layers = {layer};
  const std::vector<std::uint8_t> bytes = ro::test_support::BuildStr(options);
  auto parsed = ro::formats::StrEffect::Parse(bytes);
  REQUIRE(parsed.has_value());

  ro::render::LoadedEffect loaded;
  loaded.prepared = ro::render::PrepareEffect(*parsed);
  ro::formats::Image red;
  red.width = kTextureSide;
  red.height = kTextureSide;
  for (int texel = 0; texel < kTextureSide * kTextureSide; ++texel) {
    red.pixels.insert(red.pixels.end(), {kFull, 0, 0, kFull});
  }
  loaded.textures = {red};
  loaded.texture_paths = {"essai\\red.bmp"};
  return loaded;
}

// Le pixel du centre de la cible après avoir peint l'effet.
std::array<std::uint8_t, 4> CentreAfterPainting(const ro::render::GlContext& context,
                                                const ro::render::LoadedEffect& loaded,
                                                const ro::render::EffectDye* dye) {
  auto target = ro::render::OffscreenTarget::Create(kSide, kSide);
  REQUIRE(target.has_value());
  target->Bind();
  context.ClearBoundTarget(kBlack);
  auto renderer = ro::render::EffectRenderer::Create();
  REQUIRE(renderer.has_value());

  ro::render::DrawnEffect drawn;
  drawn.effect = &loaded;
  drawn.lift_pixels = 0.0F;
  if (dye != nullptr) {
    drawn.dye = *dye;
  }
  ro::render::Camera camera;
  const std::array<ro::render::DrawnEffect, 1> effects = {drawn};
  renderer->Draw(camera, effects, 1.0F);

  auto painted = target->ReadPixels();
  REQUIRE(painted.has_value());
  const std::size_t centre = (static_cast<std::size_t>(kSide / 2) * kSide + kSide / 2) * kRgbaBytes;
  return {painted->pixels[centre], painted->pixels[centre + 1], painted->pixels[centre + 2],
          painted->pixels[centre + 3]};
}

// Le pixel du centre après avoir peint un lot de QUADS DU MONDE — ce que
// composent les nœuds et les classes — couché au sol sous la caméra.
std::array<std::uint8_t, 4> CentreAfterPaintingWorld(const ro::render::GlContext& context,
                                                     const ro::render::LoadedEffect& loaded,
                                                     const ro::render::EffectDye* dye) {
  auto target = ro::render::OffscreenTarget::Create(kSide, kSide);
  REQUIRE(target.has_value());
  target->Bind();
  context.ClearBoundTarget(kBlack);
  auto renderer = ro::render::EffectRenderer::Create();
  REQUIRE(renderer.has_value());

  constexpr float kHalf = 40.0F;
  ro::render::EffectWorldQuad quad;
  quad.corners = {glm::vec3{-kHalf, 0.0F, -kHalf}, glm::vec3{kHalf, 0.0F, -kHalf},
                  glm::vec3{kHalf, 0.0F, kHalf}, glm::vec3{-kHalf, 0.0F, kHalf}};
  quad.texture_uv = {glm::vec2{0.0F, 0.0F}, glm::vec2{1.0F, 0.0F}, glm::vec2{1.0F, 1.0F},
                     glm::vec2{0.0F, 1.0F}};
  quad.blend_source = ro::test_support::StrKeyFrameSpec{}.blend_source;
  quad.blend_destination = ro::test_support::StrKeyFrameSpec{}.blend_destination;
  const std::array<ro::render::EffectWorldQuad, 1> quads = {quad};
  ro::render::DrawnWorldQuads lot;
  lot.quads = quads;
  lot.texture_path = &loaded.texture_paths.front();
  lot.texture = &loaded.textures.front();
  if (dye != nullptr) {
    lot.dye = *dye;
  }
  const std::array<ro::render::DrawnWorldQuads, 1> lots = {lot};
  renderer->DrawWorldQuads(ro::render::Camera{}, lots, 1.0F);

  auto painted = target->ReadPixels();
  REQUIRE(painted.has_value());
  const std::size_t centre = (static_cast<std::size_t>(kSide / 2) * kSide + kSide / 2) * kRgbaBytes;
  return {painted->pixels[centre], painted->pixels[centre + 1], painted->pixels[centre + 2],
          painted->pixels[centre + 3]};
}

// Le pixel du centre après avoir peint un SPRITE — ce que sèment les nœuds et
// les classes à image d'ACT —, un carré rouge debout sous la caméra.
std::array<std::uint8_t, 4> CentreAfterPaintingSprite(
    const ro::render::GlContext& context, const ro::render::LoadedEffect& loaded,
    const ro::render::EffectDye* dye, ro::render::EffectColorTally* tally = nullptr) {
  auto target = ro::render::OffscreenTarget::Create(kSide, kSide);
  REQUIRE(target.has_value());
  target->Bind();
  context.ClearBoundTarget(kBlack);
  auto renderer = ro::render::SpriteRenderer::Create();
  REQUIRE(renderer.has_value());

  const ro::formats::SprSprite sprite = ro::formats::SprSprite::FromRgbaFrames(loaded.textures);
  constexpr float kHalfPixels = 60.0F;
  constexpr std::int32_t kBgraSection = 1;
  ro::render::SpriteQuad quad;
  quad.image_index = 0;
  quad.section = kBgraSection;
  quad.corners = {glm::vec2{-kHalfPixels, -kHalfPixels}, glm::vec2{kHalfPixels, -kHalfPixels},
                  glm::vec2{kHalfPixels, kHalfPixels}, glm::vec2{-kHalfPixels, kHalfPixels}};
  ro::render::ComposedActor actor;
  actor.quads = {quad};
  actor.sources = {ro::render::SpriteImageKey{&sprite, nullptr, kBgraSection, 0}};
  ro::render::DrawnActor drawn;
  drawn.actor = &actor;
  if (dye != nullptr) {
    drawn.dye = *dye;
  }
  const std::array<ro::render::DrawnActor, 1> actors = {drawn};
  renderer->SetColorTally(tally);
  renderer->Draw(ro::render::Camera{}, actors, 1.0F);

  auto painted = target->ReadPixels();
  REQUIRE(painted.has_value());
  const std::size_t centre = (static_cast<std::size_t>(kSide / 2) * kSide + kSide / 2) * kRgbaBytes;
  return {painted->pixels[centre], painted->pixels[centre + 1], painted->pixels[centre + 2],
          painted->pixels[centre + 3]};
}

}  // namespace

TEST_CASE("Teinture d'effet : la mesure lit la couleur d'origine d'un sprite sondé",
          "[render][effect_dye][snapshot]") {
  // 🔴 UN EFFET PEINT EN SPRITES A AUSSI UNE COULEUR : la mesure compte les
  // calques teints de sa sonde, et eux seuls.
  auto context = ro::render::GlContext::CreateHeadless(kSide, kSide);
  if (!context) SKIP("pas de contexte OpenGL : " + ro::core::Describe(context.error()));
  const ro::render::LoadedEffect loaded = RedQuad();
  const ro::render::EffectDye probe = ro::render::MakeEffectDye(kThirdTurn, 0, 0, false);
  const ro::render::EffectDye other = ro::render::MakeEffectDye(-kThirdTurn, 0, 0, false);

  ro::render::EffectColorTally tally;
  tally.Reset(probe);
  static_cast<void>(CentreAfterPaintingSprite(**context, loaded, &other, &tally));
  CHECK_FALSE(tally.Color().has_value());

  static_cast<void>(CentreAfterPaintingSprite(**context, loaded, &probe, &tally));
  const std::optional<std::array<std::uint8_t, 3>> color = tally.Color();
  REQUIRE(color.has_value());
  // Le carré est ROUGE : c'est sa couleur d'origine qu'on lit, pas le vert
  // que la sonde lui donne.
  CHECK((*color)[0] == 255);
  CHECK((*color)[1] < 30);
}

TEST_CASE("Teinture d'effet : un sprite né d'un chapeau se teint, sans teinture il reste",
          "[render][effect_dye][snapshot]") {
  auto context = ro::render::GlContext::CreateHeadless(kSide, kSide);
  if (!context) SKIP("pas de contexte OpenGL : " + ro::core::Describe(context.error()));
  const ro::render::LoadedEffect loaded = RedQuad();

  const std::array<std::uint8_t, 4> plain = CentreAfterPaintingSprite(**context, loaded, nullptr);
  CHECK(plain[0] > 200);
  CHECK(plain[1] < 30);

  const ro::render::EffectDye green = ro::render::MakeEffectDye(kThirdTurn, 0, 0, false);
  const std::array<std::uint8_t, 4> dyed = CentreAfterPaintingSprite(**context, loaded, &green);
  CHECK(dyed[0] < 30);
  CHECK(dyed[1] > 200);
}

TEST_CASE("Teinture d'effet : un lot de quads du monde se teint aussi",
          "[render][effect_dye][snapshot]") {
  auto context = ro::render::GlContext::CreateHeadless(kSide, kSide);
  if (!context) SKIP("pas de contexte OpenGL : " + ro::core::Describe(context.error()));
  const ro::render::LoadedEffect loaded = RedQuad();

  const std::array<std::uint8_t, 4> plain = CentreAfterPaintingWorld(**context, loaded, nullptr);
  CHECK(plain[0] > 200);
  CHECK(plain[1] < 30);

  const ro::render::EffectDye green = ro::render::MakeEffectDye(kThirdTurn, 0, 0, false);
  const std::array<std::uint8_t, 4> dyed = CentreAfterPaintingWorld(**context, loaded, &green);
  CHECK(dyed[0] < 30);
  CHECK(dyed[1] > 200);
}

TEST_CASE("Teinture d'effet : le nuanceur tourne la teinte du seul effet teint",
          "[render][effect_dye][snapshot]") {
  auto context = ro::render::GlContext::CreateHeadless(kSide, kSide);
  if (!context) SKIP("pas de contexte OpenGL : " + ro::core::Describe(context.error()));
  const ro::render::LoadedEffect loaded = RedQuad();

  const std::array<std::uint8_t, 4> plain = CentreAfterPainting(**context, loaded, nullptr);
  CHECK(plain[0] > 200);
  CHECK(plain[1] < 30);

  const ro::render::EffectDye green = ro::render::MakeEffectDye(kThirdTurn, 0, 0, false);
  const std::array<std::uint8_t, 4> dyed = CentreAfterPainting(**context, loaded, &green);
  CHECK(dyed[0] < 30);
  CHECK(dyed[1] > 200);
}

TEST_CASE("Teinture d'effet : assombrir peint l'additif en alpha sans le perdre",
          "[render][effect_dye][snapshot]") {
  auto context = ro::render::GlContext::CreateHeadless(kSide, kSide);
  if (!context) SKIP("pas de contexte OpenGL : " + ro::core::Describe(context.error()));
  const ro::render::LoadedEffect loaded = RedQuad();

  const ro::render::EffectDye dark = ro::render::MakeEffectDye(0, 0, 0, true);
  const std::array<std::uint8_t, 4> painted = CentreAfterPainting(**context, loaded, &dark);
  // Un rouge opaque peint en alpha reste un rouge plein.
  CHECK(painted[0] > 200);
  CHECK(painted[1] < 30);
}
