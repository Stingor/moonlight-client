#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

#include "ro/render/emotion.h"

// LA MARQUE DE QUÊTE AU-DESSUS D'UN PNJ : un sprite-enfant du sprite des
// émoticônes, que le serveur choisit par un numéro d'effet.
//
// Elle se tient à hauteur FIXE au-dessus des pieds du PNJ, quelle que soit sa
// taille, un peu décalée vers les X décroissants du monde, et tourne sans fin à la cadence de son
// fichier, à l'échelle un et sans teinte. La couleur que le serveur annonce
// avec elle ne change pas l'image : elle ne sert qu'à la minimap.
namespace ro::render {

inline constexpr std::string_view kQuestMarkerSpritePath = kEmotionSpritePath;

// De combien la marque flotte au-dessus des pieds de son PNJ, et de combien
// elle se décale le long de l'axe X du monde, en unités du monde.
inline constexpr float kQuestMarkerRiseUnits = 20.0F;
inline constexpr float kQuestMarkerShiftXUnits = -3.0F;

// Les effets 0 à 5 jouent les actions 80 à 85 ; les effets 6 à 10, les actions
// 93 à 97 — partagées avec des émoticônes. Au-delà, rien n'est prévu.
inline constexpr std::uint16_t kQuestMarkerFirstLateEffect = 6;
inline constexpr std::uint16_t kQuestMarkerEffectCount = 11;
inline constexpr std::size_t kQuestMarkerEarlyActionOffset = 80;
inline constexpr std::size_t kQuestMarkerLateActionOffset = 87;

// L'action du sprite des émoticônes qui dessine cet effet, ou rien quand
// l'effet n'a pas d'image.
[[nodiscard]] constexpr std::optional<std::size_t> QuestMarkerAction(std::uint16_t effect) {
  if (effect >= kQuestMarkerEffectCount) {
    return std::nullopt;
  }
  return effect + (effect < kQuestMarkerFirstLateEffect ? kQuestMarkerEarlyActionOffset
                                                        : kQuestMarkerLateActionOffset);
}

}  // namespace ro::render
