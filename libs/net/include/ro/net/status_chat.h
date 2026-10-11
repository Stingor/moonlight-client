#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "ro/net/world_update.h"
#include "ro/sim/party_roster.h"
#include "ro/sim/status_effects.h"
#include "ro/sim/world_fwd.h"

namespace ro::net {

// CE QU'UN ÉTAT POSÉ OU LEVÉ ÉCRIT DANS LA CONVERSATION.
//
// Une ligne par transition, sur SOI (case 8 « Altérations d'état ») ou sur un
// membre CONNECTÉ du groupe (case 10 « Altérations d'état du groupe »), le
// nom du membre en tête. Rien pour un autre acteur, ni pour un membre hors
// ligne.
//
// ⚠ UN ÉTAT RAFRAÎCHI SE RÉÉCRIT : le serveur renvoie la pose, et le client
// d'origine écrit chaque pose qu'il reçoit, sans regarder si l'état y était.
//
// ⚠ DEUX TABLES, SELON LE PAQUET, et elles ne sont pas les mêmes. La forme à
// durée (`ZC_MSG_STATE_CHANGE3` 0x0983, sa forme ancienne 0x043F, et l'état
// d'un acteur qui entre en vue 0x0984) connaît les phrases chiffrées et les
// poses des classes 4 ; la forme nue (`ZC_MSG_STATE_CHANGE` 0x0196), que le
// serveur emploie pour TOUTE levée, connaît leurs levées. Une pose sans durée,
// qui arrive par la forme nue, n'écrit donc que les phrases anciennes.
//
// Le paquet ne change rien d'autre : le monde l'a déjà appliqué, ceci ne fait
// qu'écrire.
[[nodiscard]] FrameOutcome ApplyStatusChatFrame(const Frame& frame, sim::World& world,
                                                const sim::PartyRoster& party,
                                                std::uint32_t now_ms);

// La forme du paquet, qui choisit la table.
enum class StatusChatForm : std::uint8_t {
  // 0x0983, 0x043F, 0x0984.
  kTimed,
  // 0x0196.
  kBare,
};

// Ce qu'une transition dit, tel que la phrase le lit.
struct StatusChatFacts {
  StatusChatForm form = StatusChatForm::kTimed;
  sim::StatusEffectId effect = 0;
  bool active = false;
  // Ce qui reste de l'état ; zéro pour la forme nue.
  std::uint32_t remaining_ms = 0;
  std::int32_t val1 = 0;
  std::int32_t val2 = 0;
};

// Une phrase écrite, et sa teinte en `0xRRGGBB`.
struct StatusChatPhrase {
  std::string text;
  std::uint32_t color_rgb = 0;
};

// La phrase d'une transition, sans le nom du membre ; rien pour un état que
// le client d'origine tait.
[[nodiscard]] std::optional<StatusChatPhrase> StatusChatPhraseOf(const StatusChatFacts& facts);

}  // namespace ro::net
