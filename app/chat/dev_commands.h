#pragma once

#include <cstdint>
#include <string_view>

#include "chat/chat_view.h"

namespace moonclient {

// LES COMMANDES DE DÉVELOPPEMENT : elles n'existent que dans la construction
// Debug (`ro::core::kDevTools`).
//
// Aucune n'est une commande du jeu. Elles servent à REGARDER ce qu'on porte —
// `/trames`, `/effet`, `/effets`, `/noeud`, `/butin` — ou doublent un raccourci
// que le joueur a déjà — `/grimoire`, `/sac`. Dans la construction livrée,
// elles ne sont pas reconnues : la ligne suit le sort de toute commande que le
// client ne connaît pas.
//
// ⚠ ELLES TIENNENT EN UN SEUL ENDROIT, et c'est tout l'objet : dispersées dans
// la chaîne des commandes, la dernière à retirer aurait été oubliée.

// LA COMMANDE QUI FAIT PLEUVOIR DE FAUX BUTINS autour du joueur, pour régler le
// jaillissement sans attendre qu'une créature en lâche un.
//
//     /butin                  huit potions, les cinq couleurs de pilier
//     /butin 3                trois
//     /butin 5 4174           cinq exemplaires de l'objet 4174
//     /butin 5 4174 2         … tous avec le pilier du mode 2
//     /butin 0                les retire
//
// 🔴 CE SONT DES LEURRES : le serveur n'en sait rien, et les ramasser ne
// donnerait rien. Voir `SowFakeLoot`.
inline constexpr std::string_view kFakeLootCommand = "/butin";

// La commande qui dit ce que les trames reçues sont devenues — et surtout
// celles que le traducteur laisse tomber, qui autrement ne se signalent par
// rien du tout. Elle bascule aussi l'inspecteur de trames.
inline constexpr std::string_view kFrameTallyCommand = "/trames";

// LA COMMANDE QUI DIT QUELS EFFETS CE SERVEUR A DEMANDÉS, et lesquels se sont
// montrés.
//
// ⚠ C'EST LA SEULE MESURE QU'AUCUNE LECTURE NE DONNE. Le juge dit ce que NOUS
// savons montrer, sur les 2 042 identifiants que le client connaît ; il ne peut
// pas dire lesquels ce serveur-ci envoie. Un identifiant demandé et jamais
// montré est un travail à faire ; un identifiant que nous saurions montrer et
// que personne n'envoie n'en est pas un.
inline constexpr std::string_view kEffectTallyCommand = "/effets";

// LES COMMANDES QUI OUVRENT LA FICHE, dont le grimoire est un onglet, et
// l'inventaire. Le jeu n'en a aucune : il les ouvre par Alt+S et Alt+E, que nos
// raccourcis reprennent.
inline constexpr std::string_view kSkillWindowCommand = "/grimoire";
inline constexpr std::string_view kInventoryWindowCommand = "/sac";

// Exécute la ligne si c'est une commande de développement, et le dit.
[[nodiscard]] bool RunDevelopmentCommand(const ChatDispatch& to, std::string_view line,
                                         std::uint32_t said_ms);

}  // namespace moonclient
