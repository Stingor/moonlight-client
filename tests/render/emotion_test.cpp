#include "ro/render/emotion.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/act.h"
#include "ro/formats/spr.h"
#include "ro/render/quest_marker.h"

#include "support/sprite_builder.h"

using ro::formats::ActAnimation;
using ro::formats::SprSprite;
using ro::render::ComposeEmotion;
using ro::render::EmotionLifeMs;
using ro::test_support::ActActionSpec;
using ro::test_support::ActBuildOptions;
using ro::test_support::ActFrameSpec;
using ro::test_support::ActLayerSpec;
using ro::test_support::BuildAct;
using ro::test_support::BuildSpr;
using ro::test_support::SprBuildOptions;
using ro::test_support::SprIndexedFrameSpec;
using ro::test_support::SpriteColor;

namespace {

// Trois émoticônes, de longueurs différentes : c'est ce qui prouve que
// l'action choisie est bien celle que le serveur a nommée.
constexpr std::size_t kEmotionCount = 3;
constexpr std::array<std::size_t, kEmotionCount> kImagesPerEmotion = {4, 8, 6};
constexpr std::int32_t kBubbleWidth = 16;
constexpr std::int32_t kBubbleHeight = 16;
constexpr float kBubbleSpeedUnits = 2.0F;

std::size_t TotalImages() {
  std::size_t total = 0;
  for (const std::size_t images : kImagesPerEmotion) {
    total += images;
  }
  return total;
}

SprSprite BubbleSprite() {
  SprBuildOptions options;
  for (std::size_t image = 0; image < TotalImages(); ++image) {
    options.indexed.push_back(SprIndexedFrameSpec{
        kBubbleWidth, kBubbleHeight,
        std::vector<std::uint8_t>(static_cast<std::size_t>(kBubbleWidth * kBubbleHeight), 1)});
  }
  options.palette = {SpriteColor{}, SpriteColor{.red = 255, .green = 255, .blue = 255}};
  auto sprite = SprSprite::Parse(BuildSpr(options));
  REQUIRE(sprite.has_value());
  return std::move(*sprite);
}

ActAnimation BubbleAnimation() {
  ActBuildOptions options;
  std::int32_t next_image = 0;
  for (const std::size_t images : kImagesPerEmotion) {
    ActActionSpec action;
    for (std::size_t image = 0; image < images; ++image) {
      ActLayerSpec layer;
      layer.frame_index = next_image++;
      action.frames.push_back(ActFrameSpec{.layers = {layer}});
    }
    options.actions.push_back(action);
    options.speeds.push_back(kBubbleSpeedUnits);
  }
  auto animation = ActAnimation::Parse(BuildAct(options));
  REQUIRE(animation.has_value());
  return std::move(*animation);
}

}  // namespace

TEST_CASE("Émoticône : le numéro du serveur passe par la table du jeu", "[render][emotion]") {
  // 🔴 PAS L'IDENTITÉ : « /... » (9) montrait l'argent, l'action 9, au lieu de
  // la réflexion, l'action 10. Les points où les deux divergent.
  using ro::render::EmotionAction;
  CHECK(EmotionAction(3) == 3U);    // la palpitation
  CHECK(EmotionAction(14) == 4U);   // la grosse, rangée juste après
  CHECK(EmotionAction(4) == 5U);    // la sueur
  CHECK(EmotionAction(9) == 10U);   // la réflexion
  CHECK(EmotionAction(10) == 12U);  // les ciseaux après la pierre
  CHECK(EmotionAction(11) == 11U);
  CHECK(EmotionAction(33) == 33U);
  CHECK_FALSE(EmotionAction(34).has_value());  // le mutisme n'a pas de bulle
  CHECK(EmotionAction(36) == 35U);
  CHECK(EmotionAction(81) == 86U);
  CHECK_FALSE(EmotionAction(200).has_value());
}

TEST_CASE("Émoticône : les premières actions suivent leur numéro", "[render][emotion]") {
  // Un seul sprite les porte toutes. Se tromper d'action montrerait une humeur
  // pour une autre.
  const SprSprite sprite = BubbleSprite();
  const ActAnimation animation = BubbleAnimation();

  // La deuxième émoticône commence à l'image qui suit la première.
  const ro::render::ComposedActor second = ComposeEmotion(sprite, animation, 1, 0.0F);
  REQUIRE(second.sources.size() == 1);
  // `image_index` est signé parce qu'il porte un « absent » à -1 ; le compte
  // d'images, lui, est un `std::size_t`. La conversion se dit ici.
  CHECK(second.sources[0].image_index == static_cast<std::int32_t>(kImagesPerEmotion[0]));

  const ro::render::ComposedActor third = ComposeEmotion(sprite, animation, 2, 0.0F);
  REQUIRE(third.sources.size() == 1);
  CHECK(third.sources[0].image_index == kImagesPerEmotion[0] + kImagesPerEmotion[1]);
}

TEST_CASE("Émoticône : elle joue une fois puis s'éteint", "[render][emotion]") {
  // Rien ne la retire : c'est son animation finie qui ne rend plus de calque.
  // En boucle, une bulle battrait indéfiniment au-dessus d'un silencieux.
  const SprSprite sprite = BubbleSprite();
  const ActAnimation animation = BubbleAnimation();
  const float life_ms = EmotionLifeMs(animation, 1);
  CHECK(life_ms > 0.0F);
  // La plus longue des trois dure bien plus que la plus courte.
  CHECK(life_ms > EmotionLifeMs(animation, 0));

  CHECK_FALSE(ComposeEmotion(sprite, animation, 1, 0.0F).quads.empty());
  CHECK_FALSE(ComposeEmotion(sprite, animation, 1, life_ms * 0.5F).quads.empty());
  CHECK(ComposeEmotion(sprite, animation, 1, life_ms).quads.empty());
  CHECK(ComposeEmotion(sprite, animation, 1, life_ms * 2.0F).quads.empty());
  CHECK(ComposeEmotion(sprite, animation, 1, -1.0F).quads.empty());
}

TEST_CASE("Émoticône : une action que le sprite ne porte pas ne montre rien", "[render][emotion]") {
  // Le serveur peut nommer une émoticône plus récente que l'installation. Lire
  // hors de la table donnerait n'importe quelle image, ou pire.
  const SprSprite sprite = BubbleSprite();
  const ActAnimation animation = BubbleAnimation();
  CHECK(EmotionLifeMs(animation, kEmotionCount) == 0.0F);
  CHECK(ComposeEmotion(sprite, animation, kEmotionCount, 0.0F).quads.empty());
  CHECK(ComposeEmotion(sprite, animation, 255, 0.0F).quads.empty());
}

TEST_CASE("Marque de quête : chaque effet joue son action du sprite des émoticônes",
          "[render][emotion]") {
  using ro::render::QuestMarkerAction;
  // Les six premiers effets jouent 80 à 85, les cinq suivants 93 à 97.
  constexpr std::array<std::size_t, ro::render::kQuestMarkerEffectCount> kExpected = {
      80, 81, 82, 83, 84, 85, 93, 94, 95, 96, 97};
  for (std::uint16_t effect = 0; effect < ro::render::kQuestMarkerEffectCount; ++effect) {
    REQUIRE(QuestMarkerAction(effect).has_value());
    CHECK(*QuestMarkerAction(effect) == kExpected[effect]);
  }
  // Au-delà, ni image ni retrait : rien.
  CHECK_FALSE(QuestMarkerAction(ro::render::kQuestMarkerEffectCount).has_value());
  CHECK_FALSE(QuestMarkerAction(9999).has_value());
  CHECK(ro::render::kQuestMarkerSpritePath == ro::render::kEmotionSpritePath);
}

TEST_CASE("Marque de quête : aucune émoticône ne joue les actions 80 à 85", "[render][emotion]") {
  // Ces six actions-là sont celles des marques ; la table des émoticônes les
  // saute.
  constexpr std::uint16_t kEmotionProbeCount = 256;
  for (std::uint16_t emotion = 0; emotion < kEmotionProbeCount; ++emotion) {
    const std::optional<std::size_t> action = ro::render::EmotionAction(emotion);
    if (!action.has_value()) {
      continue;
    }
    for (std::uint16_t effect = 0; effect < ro::render::kQuestMarkerFirstLateEffect; ++effect) {
      CHECK(*action != *ro::render::QuestMarkerAction(effect));
    }
  }
}
