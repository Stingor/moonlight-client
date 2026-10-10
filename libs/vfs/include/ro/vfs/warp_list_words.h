#pragma once

#include <string>
#include <string_view>

#include "ro/vfs/item_words.h"
#include "ro/vfs/map_display_names.h"

namespace ro::vfs {

// LES MOTS DE LA FENÊTRE DES DESTINATIONS : des textes de l'interface, posés par
// `FillWarpListWords` (ADR 0090).

// « Choisissez une zone de Warp » : la ligne au-dessus de la liste.
[[nodiscard]] std::string_view WarpListTitle(const ItemWords& words);

// Le mot d'annulation, dernière ligne de la liste.
[[nodiscard]] std::string_view WarpListCancelWord(const ItemWords& words);

// LA LIGNE D'UNE DESTINATION, telle que le client l'écrit.
//
// Trois cas, dans cet ordre :
// - le nom EXACT « Random » : le mot du départ au hasard ;
// - le mot d'annulation du fil, « cancel » : le mot d'annulation ;
// - tout le reste : le nom lisible de la carte, et, quand la table ne la connaît
//   pas, le mot d'une destination inconnue.
//
// 🔴 « Random.gat » N'EST PAS « Random ». Le serveur ajoute l'extension à tout ce
// qu'il liste, le départ au hasard compris ; ce nom-là passe donc par la table
// des cartes, qui ne le connaît pas, et s'écrit avec le mot d'une destination
// inconnue — que le client dit, lui aussi, « Random Area ».
[[nodiscard]] std::string WarpDestinationLabel(const ItemWords& words,
                                               const MapDisplayNames* map_names,
                                               std::string_view destination);

// Pose ces mots dans la langue courante.
void FillWarpListWords(WarpListWords& words);

}  // namespace ro::vfs
