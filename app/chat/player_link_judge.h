#pragma once

#include <cstdint>
#include <string_view>

#include "ro/sim/friend_list.h"
#include "ro/sim/guild.h"
#include "ro/sim/mvp_tracker.h"
#include "ro/sim/party_roster.h"
#include "ro/ui/chat_link.h"

namespace moonclient {

// CE QUE « INVITER DANS MON CARNET MVP » DOIT SAVOIR DE NOUS : le carnet, notre
// guilde, nos amis, l'interrupteur maître du carnet, et la liaison.
struct MvpInviteCircle {
  const ro::sim::MvpTrackerBook& book;
  const ro::sim::GuildBook& guild;
  const ro::sim::FriendList& friends;
  bool enabled = false;
  // La liaison est vérifiée : hors d'elle, le serveur jette l'invitation sans
  // répondre.
  bool link_ready = false;
};

// LA RAISON, DÉJÀ TRADUITE, qui grise « Inviter dans mon carnet MVP », ou nul.
// La première vraie gagne : la liaison n'est pas vérifiée ; le carnet est
// éteint ; aucun groupe de chasse ; ni ami ni membre de la guilde ; déjà membre
// du groupe. ⚠ LA MÊME pour les quatre menus qui la proposent : pseudo du
// chat, entité du monde, membre de guilde, membre du groupe. ⚠ CARNET ÉTEINT,
// CES MENUS NE MONTRENT PAS L'ENTRÉE : la raison n'est qu'un dernier rempart.
//
// `same_account` : ce personnage est l'un des NÔTRES, sur notre compte de jeu —
// seul le menu d'un membre de guilde le sait. Il est alors déjà du groupe : le
// serveur résout l'invitation vers notre propre compte, et la refuse.
[[nodiscard]] const char* JudgeMvpInvite(std::string_view name_utf8, const MvpInviteCircle& circle,
                                         bool same_account = false);

// CE QUE LE MENU D'UN PSEUDO DOIT SAVOIR DE NOUS pour griser ce qui n'a aucune
// chance.
struct PlayerLinkCircle {
  const ro::sim::PartyRoster& party;
  const ro::sim::GuildBook& guild;
  const ro::sim::FriendList& friends;
  std::uint32_t own_account_id = 0;
  std::uint32_t own_char_id = 0;
  // Le cercle du carnet de chasse ; nul, l'invitation au carnet n'est pas
  // refusée d'avance.
  const MvpInviteCircle* mvp = nullptr;
};

// CE QUE LE SERVEUR REFUSERAIT, et pourquoi — le plus souvent sans un mot.
// Les gardes sont celles du serveur : l'invitation dans un groupe ne part que
// d'un chef, dans un groupe qui a une place ; celle d'une guilde demande le
// droit d'inviter ; une amitié ne se demande pas deux fois.
[[nodiscard]] ro::ui::PlayerLinkRefusals JudgePlayerLink(std::string_view name,
                                                         const PlayerLinkCircle& circle);

}  // namespace moonclient
