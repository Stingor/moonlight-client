#pragma once

#include <cstdint>
#include <string>

#include "ro/sim/friend_list.h"
#include "ro/sim/party_roster.h"

namespace ro::net {

// ── CE QUE LA CONVERSATION ÉCRIT DES AVIS DE GROUPE ET D'AMIS ───────────────
//
// Les phrases sont celles du jeu, dans la langue de l'interface. Elles se
// relaient comme une réponse du serveur, dans l'onglet qui porte les siennes.

// La teinte d'une ligne : un succès, un refus, ou ce qui concerne le groupe
// entier (chef, règles).
enum class SocialTone : std::uint8_t {
  kSuccess,
  kRefusal,
  kParty,
};

struct SocialLine {
  std::string text;
  SocialTone tone = SocialTone::kSuccess;
};

[[nodiscard]] SocialLine PartyNoticeLine(const sim::PartyNotice& notice);
[[nodiscard]] SocialLine FriendNoticeLine(const sim::FriendNotice& notice);

}  // namespace ro::net
