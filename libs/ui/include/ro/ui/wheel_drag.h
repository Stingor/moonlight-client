#pragma once

#include <array>

namespace ro::ui {

// UN CHAMP À GLISSER QUI ÉCOUTE AUSSI LA MOLETTE : glisser règle finement, la
// molette sur le champ survolé avance d'un pas, Majuscule de dix — les
// commodités des curseurs du jeu. La molette passe par le portail de
// `WheelOverItem` : parcourir une fenêtre à la molette ne règle rien au
// passage.
struct WheelDrag {
  // Ce que vaut un pixel glissé, et un cran de molette.
  float drag_step = 1.0F;
  float wheel_step = 1.0F;
  // Les bornes ; égales, aucune.
  float lowest = 0.0F;
  float highest = 0.0F;
  const char* format = "%.3f";
};

// Majuscule multiplie le cran de molette par autant.
inline constexpr float kWheelShiftMultiplier = 10.0F;

struct WheelDragResult {
  // La valeur a changé à cette image.
  bool changed = false;
  // Un geste s'achève : la fin d'un glissé ou d'une saisie, ou un cran de
  // molette — chacun compte comme un geste entier.
  bool released = false;
};

[[nodiscard]] WheelDragResult WheelDragFloat(const char* label, float& value,
                                             const WheelDrag& drag);

// Trois composantes côte à côte, chacune à sa molette, puis l'étiquette.
[[nodiscard]] WheelDragResult WheelDragFloat3(const char* label, std::array<float, 3>& values,
                                              const WheelDrag& drag);

}  // namespace ro::ui
