#pragma once

#include <string>
#include <string_view>

#include "ro/ui/entity_menu.h"

#include "world/entity_menu.h"

namespace moonclient {

// ── LE MENU D'UN JOUEUR DE LA LISTE DU STAFF ────────────────────────────────
//
// Le menu d'entité d'un joueur, réduit aux lignes qui valent par son nom ou par
// son AID : il n'est peut-être pas à l'écran. Deux lignes s'y ajoutent, que le
// clic droit du monde n'offre pas : aller à lui, et le rappeler.
//
// Une ligne nouvelle du menu d'entité n'y entre que nommée dans
// `staff_roster_menu.cpp` : ce qui vise un acteur visible ne ferait rien ici.

// `facts` décrit le joueur de la liste : sa famille — `kSelf` pour notre propre
// ligne —, son AID, son nom, et les droits du compte. La section du staff s'y
// montre quel que soit le réglage du menu du monde.
[[nodiscard]] ro::ui::EntityMenuView BuildStaffRosterMenu(const EntityMenuFacts& facts);

// La commande @ du verbe de voyage `verb`, telle qu'on la taperait ; vide pour
// tout autre verbe, ou sans nom.
[[nodiscard]] std::string StaffTravelCommand(EntityVerb verb, std::string_view name);

}  // namespace moonclient
