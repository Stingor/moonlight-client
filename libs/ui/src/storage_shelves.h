#pragma once

#include "ro/ui/skin_art.h"
#include "ro/ui/storage_window.h"

// LA RANGÉE DES COFFRES, et ce que le joueur en fait.
//
// 🔴 ELLE A QUITTÉ LA FENÊTRE PARCE QU'ELLE PORTE MAINTENANT DEUX GESTES DE PLUS
// — renommer un coffre, lui donner une icône —, et que la fenêtre passait son
// plafond de lignes. C'est elle qui est partie parce qu'elle forme un tout : des
// boutons, leur menu, et la saisie qui va avec.
//
// En-tête PRIVÉ : personne hors de cette bibliothèque n'a à dessiner une demi-
// rangée de coffres.
namespace ro::ui {

// 🔴 PERPENDICULAIRES AUX CATÉGORIES, TOUJOURS : deux rangées d'onglets l'une
// sous l'autre se confondent, alors qu'elles ne coûtent pas la même chose —
// celle-ci PARLE AU SERVEUR, qui referme un coffre et en ouvre un autre ;
// celle des catégories ne fait que trier ce qui est déjà là. Perpendiculaires,
// on ne les prend jamais l'une pour l'autre.
//
// ⚠ `length` NE VAUT QUE DEBOUT : c'est la hauteur de la colonne, celle de la
// table qu'elle longe. Couchée, la rangée tient sur une ligne du flux.
//
// ⚠ NE DESSINE RIEN SOUS DEUX COFFRES : un onglet unique n'offre aucun choix et
// prendrait une ligne à une table qui en montre six cents.
void DrawStorageShelfRow(StorageWindowState& state, const StorageWindowView& view,
                         const SkinArt& art, bool lying, float length, StorageWindowAction& action);

// CE QUE LA BANDE PREND SUR SON AXE COURT : la hauteur d'une rangée, la largeur
// d'une colonne.
//
// 🔴 LA BANDE D'EN FACE S'INDENTE D'AUTANT. Les catégories couchées longent une
// colonne de coffres posée à leur gauche : sans cet écart elles démarreraient au
// bord de la fenêtre, au-dessus de rien. C'est le symétrique exact de l'indent
// que la rangée des coffres prend derrière une bande de catégories debout.
[[nodiscard]] float StorageShelfThickness(const SkinArt& art, bool lying);

}  // namespace ro::ui
