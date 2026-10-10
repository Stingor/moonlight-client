#pragma once

#include <string>
#include <string_view>

#include "chat/chat_place_link.h"

namespace moonclient {

// ── `/navi` ET `/navigation` : LA NAVIGATION AU CLAVIER ─────────────────────
//
//     /navi                        ouvre la fenêtre
//     /navigation                  de même
//     /navi prontera 150/180       lance le guidage vers cette case
//     /navi prontera 150 180       de même
//     /navi poring                 ouvre la fenêtre sur cette recherche
//     /navigation poring           de même
//
// ⚠ LES COORDONNÉES NE SE DEVINENT PAS : sans elles, « /navi terme » est une
// RECHERCHE, et jamais un guidage vers une carte du même nom — la fenêtre montre
// alors ce que le terme désigne, et le joueur choisit. Et « /navigation » ne
// lance jamais de guidage : elle ouvre.

enum class NavigationCommandKind { kNone, kOpen, kSearch, kGuide };

struct NavigationCommand {
  NavigationCommandKind kind = NavigationCommandKind::kNone;
  // Le lieu à rejoindre, pour `kGuide`. Le nom de la carte est en minuscules :
  // c'est la forme du nom interne.
  PlaceLink place;
  // Le terme cherché, pour `kSearch`.
  std::string term;
};

// Lit la ligne. `kNone` quand ce n'est pas l'une des deux commandes — le premier
// mot se compare sans la casse, et le reste de la ligne ne compte pas alors.
[[nodiscard]] NavigationCommand ReadNavigationCommand(std::string_view line);

// LA COMMANDE DU STAFF QUI Y TÉLÉPORTE : « @warp map,x,y », ou « @warp map »
// sans case — le serveur en tire une libre. Une case non praticable, il la
// remplace lui-même par une case libre voisine.
[[nodiscard]] std::string StaffWarpCommand(const PlaceLink& place);

}  // namespace moonclient
