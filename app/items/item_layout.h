#pragma once

#include <cstdint>
#include <map>
#include <vector>

#include "ro/ui/inventory_window.h"

namespace moonclient {

// OÙ LE JOUEUR A POSÉ SES OBJETS DANS LA GRILLE DU SAC.
//
// 🔴 UNE CASE EST UN INDEX ABSOLU — la n-ième case de la grille —, pas une ligne
// et une colonne. C'est ce qui OBLIGE à verrouiller la taille : le nombre de
// colonnes se déduit de la largeur, et l'élargir d'une case ferait glisser toute
// la disposition. Le réglage est donc grisé tant que la taille ne l'est pas.
//
// ⚠ LA CLÉ EST L'IDENTIFIANT DE L'OBJET, jamais sa place serveur : celle-ci se
// renumérote dès qu'on consomme, et la disposition se mélangerait à la première
// potion bue. Un objet vendu puis racheté retrouve donc SA case.
//
// ⚠ ET LA TABLE NE CONNAÎT PAS LES ONGLETS. Un objet ne paraît que dans un seul
// — celui de sa famille —, donc une case par objet suffit. L'onglet « Tout » ne
// s'en sert pas : il mêle les familles, et une case n'y désignerait pas la même
// place que dans l'onglet d'origine.
class ItemLayout {
 public:
  // « Cet objet n'a pas de case à lui » : il remplit alors les trous, dans
  // l'ordre où le serveur l'a listé.
  static constexpr int kNowhere = -1;

  [[nodiscard]] int CellOf(std::uint32_t item_id) const;

  // POSE `item_id` SUR `cell`.
  //
  // ⚠ `sitting_there` EST L'OBJET QUI S'Y TROUVAIT — zéro si la case était vide.
  // Il prend la case que le premier quitte : c'est ce qui fait un ÉCHANGE et non
  // un écrasement. Et s'il n'y a rien à lui donner — l'objet tiré remplissait un
  // trou —, il redevient libre plutôt que de rester collé à une case qu'un autre
  // occupe.
  void Place(std::uint32_t item_id, int cell, std::uint32_t sitting_there);

  // Repose tout à plat : plus aucun objet n'a de case.
  void Clear() noexcept;

  [[nodiscard]] bool empty() const noexcept { return places_.empty(); }

  // ⚠ ORDONNÉE PAR IDENTIFIANT, et c'est ce qui rend le fichier stable d'une
  // écriture à l'autre : une table de hachage l'aurait réordonné à chaque
  // démarrage, et le diff aurait été illisible.
  [[nodiscard]] const std::map<std::uint32_t, int>& places() const noexcept { return places_; }

  // La forme brute, pour la lecture du fichier. Une case négative est ignorée.
  void Put(std::uint32_t item_id, int cell);

 private:
  std::map<std::uint32_t, int> places_;
};

// RANGE LES CASES SELON LA TABLE, trous compris.
//
// Chaque objet qui a une case y va ; les autres remplissent les trous dans
// l'ordre où ils arrivaient. Une case que rien n'occupe reste VIDE — c'est le
// but même du placement libre : un objet ramassé ne pousse plus les autres.
//
// ⚠ UNE CASE VIDE EST UNE VUE NULLE : sa place serveur vaut zéro, et la fenêtre
// n'y dessine ni icône, ni badge, ni infobulle.
//
// ⚠ ET LA GRILLE S'ÉTEND JUSQU'À LA PLUS LOINTAINE CASE OCCUPÉE, jamais
// au-delà : un objet placé loin puis vendu ne laisse pas derrière lui un champ
// de cases vides que rien ne remplirait.
void ArrangeCells(std::vector<ro::ui::InventoryCellView>& cells, const ItemLayout& layout);

}  // namespace moonclient
