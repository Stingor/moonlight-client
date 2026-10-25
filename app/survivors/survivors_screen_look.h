#pragma once

#include <cstdint>

#include "survivors/survivors_stage.h"

namespace moonclient {

// CE QUE L'ÉCRAN DU MODE FAIT À L'IMAGE DE L'ARÈNE, décidé sans GPU : app le
// peint par la teinte d'écran, après la scène et avant les fenêtres.

// Le choix de niveau, le coffre et la pause grisent le monde figé derrière
// eux (U11) ; la mort le passe en noir et blanc (U15), et le bilan qui la
// suit le garde.
inline constexpr float kSurvivorsGreyedDesaturation = 0.7F;
inline constexpr float kSurvivorsDeathDesaturation = 1.0F;

// La désaturation de l'image dans cet écran ; zéro quand le monde se montre
// tel quel. `fell` : la partie s'est finie par la mort du héros.
[[nodiscard]] float SurvivorsDesaturation(SurvivorsScreen screen, bool fell) noexcept;

// La musique baisse de moitié en pause (U18), et seulement en pause : un choix
// de niveau ou un coffre ne dure qu'un instant.
[[nodiscard]] bool SurvivorsMusicPaused(SurvivorsScreen screen) noexcept;

// L'arène se peint : de l'arrivée au bilan. Un bilan relu sans partie (Q3) n'a
// pas d'arène derrière lui.
[[nodiscard]] bool SurvivorsArenaShown(SurvivorsScreen screen) noexcept;

}  // namespace moonclient
