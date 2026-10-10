#pragma once

#include <span>

#include "ro/net/connection.h"
#include "ro/sim/world_fwd.h"
#include "ro/ui/character_staff.h"
#include "ro/ui/staff_item_grant.h"
#include "ro/ui/staff_tools.h"
#include "ro/vfs/job_names.h"

#include "character/staff_jobs.h"
#include "chat/chat_place_link.h"
#include "game/homunculus_staff_words.h"

namespace moonclient {

// LE VOLET DU STAFF, VU DE LA BOUCLE : ce qu'il a besoin de savoir, et ce qu'il
// demande.
//
// 🔴 LE VOCABULAIRE DU SERVEUR EST ICI, PAS DANS LA FENÊTRE. Celle-ci émet une
// intention ; c'est `app` qui sait que « remettre les caractéristiques à zéro »
// s'écrit « @resetstat ». Le même partage que pour l'ouverture du coffre, où
// `ui` ne sait pas non plus que cela s'écrit « @storage ».

// CE QUE LE VOLET TIENT LE TEMPS D'UNE PARTIE, et pas plus : elle renaît avec
// chaque connexion, comme l'état du serveur qu'elle suit.
struct StaffSession {
  // ⚠ ELLE VIT ICI parce que les vues qu'elle rend POINTENT dedans : la refaire
  // à chaque image ferait pendre celles que la fenêtre vient de lire.
  StaffJobs jobs;
  // Les homoncules que la ligne du staff du grimoire peut créer ; sa vue
  // pointe dedans, pour la même raison.
  std::vector<ro::ui::StaffHomunculusChoice> homunculi = CreatableHomunculi();
  // « IGNORÉ DES MONSTRES » TEL QUE LE SERVEUR L'A, d'après nos propres
  // bascules : il ne l'annonce jamais. ⚠ FAUX À CHAQUE PARTIE, parce que le
  // serveur l'oublie à la déconnexion — c'est ce qui fait reposer le souhait.
  bool battle_ignored = false;
};

// CE QUE LE VOLET A BESOIN DE SAVOIR, posé à chaque image : la classe courante,
// la liste de celles qu'on peut prendre, l'état visuel du personnage, son
// invisibilité, et la vitesse que le serveur annonce.
void FeedStaffPane(const ro::sim::World& world, StaffJobs& jobs, const ro::vfs::JobNames* names,
                   ro::ui::StaffPaneView& pane);

// ALIGNE LE SERVEUR SUR LE SOUHAIT « IGNORÉ DES MONSTRES ».
//
// 🔴 LA COMMANDE EST UNE BASCULE, et le serveur n'en dit l'état que par une
// phrase dans sa langue. On l'envoie donc seulement quand le souhait diffère de
// ce qu'on sait lui avoir fait : à la connexion si la case est cochée, puis à
// chaque clic.
//
// 🔴 CELA SUPPOSE QUE LE SERVEUR N'IMMUNISE PERSONNE À LA CONNEXION. S'il
// posait l'immunité d'office sur un groupe, chaque bascule partirait à
// contre-sens : la case cochée la retirerait.
//
// ⚠ UN `@battleignore` TAPÉ À LA MAIN NE SE VOIT PAS ICI : la case et le
// serveur divergent alors jusqu'au clic suivant ou à la reconnexion.
void KeepBattleIgnore(bool wanted, StaffSession& session, ro::net::Connection& link,
                      const ro::sim::World& world);

// LA COMMANDE QU'IL A DEMANDÉE, composée puis envoyée COMME SI LE JOUEUR LA
// TAPAIT.
//
// ⚠ RIEN N'EST VALIDÉ ICI : le serveur revalide selon le groupe du compte, et
// son refus arrive dans la conversation comme d'habitude. Le droit qui masque le
// volet ne décide que de ce qui se MONTRE.
void SendStaffOrder(const ro::ui::StaffAction& asked, ro::net::Connection& link,
                    const ro::sim::World& world);

// LE `@warp` DU STAFF, le même pour la navigation, la minimap, la carte du monde
// et le menu d'un lieu du chat. ⚠ `rights` SE RELIT AU GESTE : le bouton a pu
// être dessiné une image avant que le lien ne tombe. Sans eux, rien ne part.
void SendStaffWarp(const PlaceLink& place, ro::ui::SessionRights rights, ro::net::Connection& link,
                   const ro::sim::World& world);

// LES `@item` DU STAFF, ceux de l'Atlas des recettes comme celui d'une
// description. ⚠ `rights` SE RELIT AU GESTE, comme pour `SendStaffWarp`.
//
// ⚠ ENVOYÉS D'UN TRAIT, SANS CADENCE : le serveur traite une commande avant
// tout frein de parole — le délai entre deux phrases ne vaut que pour une
// parole dite, et il est nul sur ce serveur —, et il lit au plus trois paquets
// par tour de boucle et par connexion. Le reste attend le tour suivant : rien
// n'est jeté, rien n'est puni.
void SendStaffItems(std::span<const ro::ui::StaffItemGrant> grants, ro::ui::SessionRights rights,
                    ro::net::Connection& link, const ro::sim::World& world);

}  // namespace moonclient
