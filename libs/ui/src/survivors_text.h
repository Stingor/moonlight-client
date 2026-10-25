#pragma once

// LES TEXTES DU MODE SURVIVORS QUI SE DÉCLARENT LOIN DES FENÊTRES : les
// descriptions du catalogue de `sim`, les raisons de refus et les bandeaux
// d'alerte d'`app`. Ils arrivent par la vue, en français, marqués par
// `TrText` à leur définition, et se traduisent ici, à la lecture.
//
// En-tête PRIVÉ de `libs/ui/src`.

namespace ro::ui {

// La traduction d'un littéral marqué par `TrText` ; un texte nul rend "".
[[nodiscard]] const char* SurvivorsText(const char* french);

}  // namespace ro::ui
