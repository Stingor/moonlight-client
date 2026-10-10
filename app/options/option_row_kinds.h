#pragma once

#include <span>

#include "ro/ui/options_window.h"

#include "options/game_options.h"
#include "options/percent.h"

namespace moonclient {

// LES TROIS FORMES D'UNE RANGÉE DE RÉGLAGE : un interrupteur, une liste, un
// curseur.
//
// 🔴 UN SEUL FOYER, PARCE QU'IL Y A DEUX FICHIERS DE RANGÉES. Le catalogue
// (`game_option_rows.cpp`) et les boucles des barres (`game_option_bars.cpp`)
// s'en servent tous les deux ; recopiées, elles divergeraient le jour où une
// rangée gagne un champ — et c'est la copie qu'on ne relit pas qui garderait
// l'ancienne forme.

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

}  // namespace moonclient
