#pragma once

// LES SPRITES DES DÉGÂTS FLOTTANTS, RÉDUITS À CE QU'IL FAUT : les chiffres, et
// l'étoile d'impact du coup critique.
//
// ⚠ PARTAGÉ PAR DEUX ESSAIS — les nombres et le nombre critique —, parce qu'ils
// composent les MÊMES sprites. Deux copies de ce montage auraient divergé au
// premier champ ajouté, et l'un des deux aurait alors éprouvé un fichier qui
// n'existe nulle part.
//
// ⚠ AUCUN ASSET DU JEU : tout est bâti en mémoire, comme partout ailleurs dans
// cette suite.

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/act.h"
#include "ro/formats/spr.h"
#include "ro/render/actor_sprite.h"
#include "ro/render/damage_mark.h"

#include "support/sprite_builder.h"

namespace ro::test_support {

// Le sprite des chiffres tel qu'il est dans le jeu : douze images de huit sur
// onze — les dix chiffres, puis K et M.
inline constexpr std::int32_t kDigitWidth = 8;
inline constexpr std::int32_t kDigitHeight = 11;
inline constexpr std::size_t kDigitImages = 12;

// L'étoile d'impact : une image de deux sur deux. Une taille PAIRE pose son
// centre exactement sur l'origine, ce qui rend sa montée lisible sans
// défalquer le demi-pixel d'ancrage.
inline constexpr std::int32_t kStarSide = 2;
inline constexpr std::size_t kStarImages = 8;

// La cadence déclarée par ces fichiers, en unités RO : quatre, soit
// quatre-vingt-seize millisecondes par image.
inline constexpr float kDamageSpeedUnits = 4.0F;

inline formats::SprSprite DigitsSprite() {
  SprBuildOptions options;
  for (std::size_t image = 0; image < kDigitImages; ++image) {
    options.indexed.push_back(SprIndexedFrameSpec{
        kDigitWidth, kDigitHeight,
        std::vector<std::uint8_t>(static_cast<std::size_t>(kDigitWidth * kDigitHeight), 1)});
  }
  options.palette = {SpriteColor{}, SpriteColor{.red = 255, .green = 255, .blue = 255}};
  auto sprite = formats::SprSprite::Parse(BuildSpr(options));
  REQUIRE(sprite.has_value());
  return std::move(*sprite);
}

inline formats::ActAnimation DigitsAnimation() {
  ActActionSpec action;
  for (std::size_t image = 0; image < kDigitImages; ++image) {
    ActLayerSpec layer;
    layer.frame_index = static_cast<std::int32_t>(image);
    action.frames.push_back(ActFrameSpec{.layers = {layer}});
  }
  ActBuildOptions options;
  options.actions = {action};
  options.speeds = {kDamageSpeedUnits};
  auto animation = formats::ActAnimation::Parse(BuildAct(options));
  REQUIRE(animation.has_value());
  return std::move(*animation);
}

inline formats::SprSprite StarSprite() {
  SprBuildOptions options;
  for (std::size_t image = 0; image < kStarImages; ++image) {
    options.indexed.push_back(SprIndexedFrameSpec{
        kStarSide, kStarSide,
        std::vector<std::uint8_t>(static_cast<std::size_t>(kStarSide * kStarSide), 1)});
  }
  options.palette = {SpriteColor{}, SpriteColor{.red = 255, .green = 128, .blue = 0}};
  auto sprite = formats::SprSprite::Parse(BuildSpr(options));
  REQUIRE(sprite.has_value());
  return std::move(*sprite);
}

// Le fichier de l'étoile porte assez d'actions pour que la sienne existe.
inline formats::ActAnimation StarAnimation() {
  ActBuildOptions options;
  for (std::size_t index = 0; index <= render::DamageMarkAction(render::DamageMark::kCritical);
       ++index) {
    ActActionSpec action;
    for (std::size_t image = 0; image < kStarImages; ++image) {
      ActLayerSpec layer;
      layer.frame_index = static_cast<std::int32_t>(image);
      action.frames.push_back(ActFrameSpec{.layers = {layer}});
    }
    options.actions.push_back(action);
    options.speeds.push_back(kDamageSpeedUnits);
  }
  auto animation = formats::ActAnimation::Parse(BuildAct(options));
  REQUIRE(animation.has_value());
  return std::move(*animation);
}

// ── CE QU'ON MESURE SUR UN QUAD ─────────────────────────────────────────────

inline bool NearlyEqual(float actual, float expected, float tolerance = 1e-3F) {
  return std::abs(actual - expected) < tolerance;
}

inline float CenterX(const render::SpriteQuad& quad) {
  return (quad.corners[0].x + quad.corners[1].x) * 0.5F;
}

inline float Width(const render::SpriteQuad& quad) { return quad.corners[1].x - quad.corners[0].x; }

// De combien le nombre s'est élevé au-dessus des pieds de sa cible, en pixels
// de sprite. L'axe Y d'un sprite descend, d'où l'opposé.
//
// L'ancrage d'une image de hauteur IMPAIRE la pose un demi-pixel au-dessus de
// son centre, et ce demi-pixel suit l'échelle : il se défalque, sans quoi la
// mesure croîtrait avec la taille du nombre.
inline float Rise(const render::SpriteQuad& quad) {
  const float scale = Width(quad) / static_cast<float>(kDigitWidth);
  return -(quad.corners[0].y + quad.corners[2].y) * 0.5F - 0.5F * scale;
}

// De combien l'étoile s'est élevée. Son image étant de côté pair, son centre
// est sur l'origine et sa montée se lit sans correction.
inline float StarRise(const render::ComposedActor& composed) {
  const render::SpriteQuad& quad = composed.quads.front();
  return -(quad.corners[0].y + quad.corners[2].y) * 0.5F;
}

}  // namespace ro::test_support
