#pragma once

// LE CLAVIER D'UNE LISTE : flèches haut et bas pour l'entrée d'avant ou
// d'après, Page précédente et Page suivante pour une page visible, Début et Fin
// pour la première et la dernière. La liste DÉFILE pour garder l'entrée choisie
// en vue. Les touches se répètent, tenues.
//
// 🔴 LES FLÈCHES SEULES QUAND UNE SAISIE A LA MAIN : Début, Fin et les pages y
// déplacent le curseur du texte ; haut et bas n'y font rien, et l'usage du
// client — le menu des PNJ, la liste des portails — est qu'elles parcourent la
// liste depuis son champ de recherche.

#include <cstdint>

namespace ro::ui {

enum class ListKeyMove : std::uint8_t {
  kNone,
  kPrevious,
  kNext,
  kPageUp,
  kPageDown,
  kFirst,
  kLast,
};

// Aucune entrée choisie.
inline constexpr int kListNoEntry = -1;

// LE RANG OÙ LA TOUCHE MÈNE, borné à la liste — il ne boucle pas. Sans entrée
// choisie, la flèche du bas et la page suivante mènent à la première, celles
// du haut à la dernière. Une liste vide rend `kListNoEntry`. `page_rows` vaut au
// moins un.
[[nodiscard]] int ListKeyTarget(int current, int count, int page_rows, ListKeyMove move);

// La touche de liste pressée à cette image, ou `kNone`. `arrows_only` ne lit
// que haut et bas.
[[nodiscard]] ListKeyMove PressedListKey(bool arrows_only);

// Combien d'entrées de `row_pitch` pixels la fenêtre courante montre en
// entier, au moins une.
[[nodiscard]] int VisibleListRows(float row_pitch);

// Fait défiler la fenêtre courante juste assez pour montrer le dernier élément
// posé.
void KeepLastItemInView();

}  // namespace ro::ui
