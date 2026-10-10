#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "ro/render/emotion.h"
#include "ro/render/sleep_bubbles.h"

// LES POINTS DE SUSPENSION DU SILENCE : le sprite-enfant qui naît au-dessus de
// tout acteur que le silence tient (le bit de santé du même nom), et meurt
// quand il cesse. Qui entre dans le champ de vision déjà muet les porte aussi.
//
// C'est la bulle « … » du sprite des émoticônes, mais elle ne vit pas comme une
// émoticône : elle se tient comme les bulles du sommeil — collée à son porteur
// vingt unités au-dessus de ses pieds, en boucle à la cadence de son fichier,
// en avant de toute la scène — tant que le silence dure.
//
// 🔴 ET SON BRUIT À CHAQUE CHANGEMENT D'ÉTAT reçu en direct qui trouve
// l'acteur muet — pas seulement quand le silence tombe : une autre option qui
// bouge le fait réentendre. Jamais à l'apparition.
namespace ro::render {

inline constexpr std::string_view kSilenceDotsSpritePath = kEmotionSpritePath;
// L'action « … » du sprite des émoticônes.
inline constexpr std::size_t kSilenceDotsAction = 10;
inline constexpr float kSilenceDotsRiseUnits = kSleepBubblesRiseUnits;
inline constexpr std::string_view kSilenceSound = "_silence.wav";

}  // namespace ro::render
