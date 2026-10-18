#pragma once

#include <algorithm>
#include <span>

#include "ro/ui/options_window.h"

#include "options/game_options.h"
#include "options/percent.h"

namespace moonclient {

// LES FORMES D'UNE RANGÉE DE RÉGLAGE : un interrupteur, une liste, un curseur,
// un nuancier.
//
// 🔴 UN SEUL FOYER, PARCE QUE CHAQUE PAGE DE RÉGLAGES A SES RANGÉES. Le
// catalogue (`game_option_rows.cpp`), les boucles des barres
// (`game_option_bars.cpp`) et les pages à part s'en servent ; recopiées, elles
// divergeraient le jour où une rangée gagne un champ — et c'est la copie qu'on
// ne relit pas qui garderait l'ancienne forme.

[[nodiscard]] inline ro::ui::OptionRowView Toggle(GameOption which, const char* label,
                                                  const char* help, const char* command, bool on) {
  return ro::ui::OptionRowView{.id = static_cast<int>(which),
                               .label = label,
                               .help = help,
                               .command = command,
                               .kind = ro::ui::OptionKind::kToggle,
                               .on = on};
}

[[nodiscard]] inline ro::ui::OptionRowView Choice(GameOption which, const char* label,
                                                  const char* help,
                                                  std::span<const char* const> choices,
                                                  int chosen) {
  return ro::ui::OptionRowView{.id = static_cast<int>(which),
                               .label = label,
                               .help = help,
                               .kind = ro::ui::OptionKind::kChoice,
                               .chosen = chosen,
                               .choices = choices};
}

// Un curseur qui compte en pixels, en lignes ou en icônes.
[[nodiscard]] inline ro::ui::OptionRowView Measure(GameOption which, const char* label,
                                                   const char* help, float value, float lowest,
                                                   float highest, const char* format) {
  return ro::ui::OptionRowView{.id = static_cast<int>(which),
                               .label = label,
                               .help = help,
                               .kind = ro::ui::OptionKind::kSlider,
                               .value = value,
                               .lowest = lowest,
                               .highest = highest,
                               .format = format};
}

// Un curseur entier. `id` est celui de la page : chacune numérote ses rangées.
[[nodiscard]] inline ro::ui::OptionRowView WholeSlider(int id, const char* label, const char* help,
                                                       int value, int lowest, int highest,
                                                       const char* format) {
  return ro::ui::OptionRowView{.id = id,
                               .label = label,
                               .help = help,
                               .kind = ro::ui::OptionKind::kSlider,
                               .value = static_cast<float>(value),
                               .lowest = static_cast<float>(lowest),
                               .highest = static_cast<float>(highest),
                               .format = format};
}

[[nodiscard]] inline ro::ui::OptionRowView WholeSlider(GameOption which, const char* label,
                                                       const char* help, int value, int lowest,
                                                       int highest, const char* format) {
  return WholeSlider(static_cast<int>(which), label, help, value, lowest, highest, format);
}

// Une valeur de curseur, entière et bornée.
[[nodiscard]] inline int Stepped(float value, int lowest, int highest) {
  return std::clamp(static_cast<int>(value + 0.5F), lowest, highest);
}

[[nodiscard]] inline ro::ui::OptionRowView Swatch(GameOption which, const char* label,
                                                  const char* help, ImU32 color) {
  return ro::ui::OptionRowView{.id = static_cast<int>(which),
                               .label = label,
                               .help = help,
                               .kind = ro::ui::OptionKind::kColor,
                               .color = color};
}

}  // namespace moonclient
