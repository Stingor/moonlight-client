#include "ro/render/emotion.h"

#include <array>
#include <cstddef>
#include <optional>

#include "ro/render/sprite_frame.h"

namespace ro::render {
namespace {

// Un numéro que le sprite ne dessine pas : « ce joueur est muet » s'affiche par
// un autre chemin, et le serveur refuse de le relayer.
constexpr std::uint8_t kNoAction = 0xFF;

// L'ACTION DU SPRITE POUR CHAQUE NUMÉRO DU PROTOCOLE, rang pour rang.
//
// 🔴 PAS L'IDENTITÉ. Le sprite range la grosse palpitation juste après la
// petite, ciseaux et pierre dans l'autre sens, et n'a pas de case pour le
// mutisme : de 4 à 14, puis à partir de 34, le numéro et l'action divergent.
// Chaque émoticône montrait sa voisine — « /... » l'argent.
//
// ⚠ C'EST LA TABLE DU JEU, le quatrième argument d'`InsertEmotionListTable`
// dans `CashEmotionList.lub`, dont `EMOTION_TYPE` donne les numéros.
constexpr std::array<std::uint8_t, 93> kEmotionActions = {
    0,  1,  2,  3,  5,  6,  7,  8,  9,  10, 12, 11, 13, 14, 4,  15,        16, 17, 18,
    19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, kNoAction, 34, 35, 36,
    37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52,        53, 54, 55,
    56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71,        72, 73, 74,
    75, 76, 77, 78, 79, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96,        97,
};

// L'action d'une émoticône, ou le nombre d'actions si le sprite ne la porte
// pas — auquel cas rien ne se compose.
std::size_t ActionOf(const formats::ActAnimation& animation, std::uint16_t emotion) {
  const std::optional<std::size_t> index = EmotionAction(emotion);
  return index.has_value() && *index < animation.actions().size() ? *index
                                                                  : animation.actions().size();
}

}  // namespace

std::optional<std::size_t> EmotionAction(std::uint16_t emotion) {
  if (emotion >= kEmotionActions.size() || kEmotionActions[emotion] == kNoAction) {
    return std::nullopt;
  }
  return kEmotionActions[emotion];
}

float EmotionLifeMs(const formats::ActAnimation& animation, std::uint16_t emotion) {
  const std::size_t action_index = ActionOf(animation, emotion);
  if (action_index >= animation.actions().size()) {
    return 0.0F;
  }
  const formats::ActAction& action = animation.actions()[action_index];
  return static_cast<float>(action.frames.size()) * action.frame_interval_ms();
}

ComposedActor ComposeEmotion(const formats::SprSprite& sprite,
                             const formats::ActAnimation& animation, std::uint16_t emotion,
                             float elapsed_ms) {
  ComposedActor composed;
  if (elapsed_ms < 0.0F || elapsed_ms >= EmotionLifeMs(animation, emotion)) {
    return composed;
  }
  const std::size_t action_index = ActionOf(animation, emotion);
  // Jouée UNE fois : la bulle apparaît, tient, puis s'efface, et la boucle la
  // ferait battre indéfiniment au-dessus de qui n'a rien dit depuis longtemps.
  const std::size_t frame = FrameAtTimeOnce(animation.actions()[action_index], elapsed_ms);
  for (const SpriteQuad& quad : BuildFrameQuads(sprite, animation, action_index, frame)) {
    composed.sources.push_back(SpriteImageKey{&sprite, nullptr, quad.section, quad.image_index});
    composed.quads.push_back(quad);
  }
  return composed;
}

}  // namespace ro::render
