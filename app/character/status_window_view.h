#pragma once

#include <string>
#include <string_view>

#include "ro/sim/guild.h"
#include "ro/ui/character_window.h"
#include "ro/ui/status_window.h"

namespace moonclient {

// LA PETITE FENÊTRE DE STATUT, VUE DE LA PARTIE : les nombres de la fiche,
// remis dans l'ordre où elle les écrit, et le nom de la guilde.
//
// 🔴 LA MATK SE REMET À L'ENDROIT ICI : le serveur envoie son maximum dans
// `matk1` et son minimum dans `matk2`, à l'inverse de toutes les autres
// paires. La fenêtre, elle, reçoit deux bornes nommées et n'a plus à le savoir.
//
// `guild_name` VIDE veut dire « sans guilde ». ⚠ LA VUE LE DÉSIGNE SANS LE
// COPIER : il doit vivre aussi longtemps qu'elle.
[[nodiscard]] ro::ui::StatusWindowView StatusWindowViewOf(const ro::ui::CharacterStatsView& stats,
                                                          std::string_view guild_name);

// LE NOM DE LA GUILDE QU'ON MONTRE, lu dans le livre au moment de composer :
// celui du résumé quand le serveur l'a donné, sinon celui de l'appartenance ;
// vide sans guilde.
//
// 🔴 UNE COPIE, JAMAIS UNE VUE SUR LE LIVRE : le réseau le réécrit entre deux
// images — une guilde quittée l'efface, un résumé nouveau remplace l'ancien —,
// et une vue gardée d'une image à l'autre lirait une chaîne libérée.
[[nodiscard]] std::string GuildNameOf(const ro::sim::GuildBook& book);

// CE QUE LA FLÈCHE A DEMANDÉ, dit dans les mots de la fiche : la montée part
// par la même route que son « + ». Rien quand la fenêtre n'a rien demandé.
[[nodiscard]] ro::ui::CharacterWindowAction SheetActionOf(const ro::ui::StatusWindowAction& asked);

}  // namespace moonclient
