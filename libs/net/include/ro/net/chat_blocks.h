#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "ro/core/error.h"
#include "ro/net/connection.h"

namespace ro::net {

// ── LE JOURNAL DES BLOCAGES DE CHAT D'UN JOUEUR : pour le staff ─────────────
//
//   CZ 0x0F3E [op][len:2][aid:4]
//   ZC 0x0F3F [op][len:2][aid:4][statut:1]
//             puis, par blocage : [date:4][minutes:4][genre:1][auteur:24]
//
// Réservé aux clients ayant donné leur poignée de main (ADR 0040), sans bit de
// capacité. Le serveur relit son journal des commandes (`atcommandlog`) : les
// `@mute` et `@unmute` RÉUSSIS dont le dernier argument nomme le personnage en
// ligne sous cet AID. Le nom est la seule clé que ce journal porte : un
// personnage renommé perd les lignes d'avant.
//
// 🔴 LA GARDE EST CELLE DU SERVEUR : niveau de groupe 80. La réponse nomme le
// staff qui a agi.
//
// 🔴 UNE LISTE VIDE NE VEUT DIRE QU'« AUCUN BLOCAGE » : un refus, un joueur
// introuvable, un journal éteint et une lecture échouée ont chacun leur statut.

// Le statut de la réponse, tel que le fil le porte. Un numéro inconnu se lit
// comme une lecture échouée : il ne promet rien.
enum class ChatBlocksStatus : std::uint8_t {
  kFound = 0,
  kMissing = 1,
  kDenied = 2,
  // Le serveur ne journalise pas les commandes en SQL.
  kLogOff = 3,
  kReadFailed = 4,
};

enum class ChatBlockKind : std::uint8_t {
  kMute = 0,
  kUnmute = 1,
};

struct ChatBlock {
  // Secondes depuis 1970, UTC.
  std::uint32_t date_s = 0;
  // La durée donnée à `@mute`, telle que tapée ; zéro pour `@unmute`.
  std::int32_t minutes = 0;
  ChatBlockKind kind = ChatBlockKind::kMute;
  // Le personnage du staff qui a tapé la commande, en UTF-8.
  std::string author;
};

struct ChatBlocksReply {
  // L'AID DEMANDÉ, renvoyé en écho : c'est lui qui apparie la réponse.
  std::uint32_t aid = 0;
  ChatBlocksStatus status = ChatBlocksStatus::kReadFailed;
  // Du plus récent au plus ancien ; vide hors du statut « trouvé ».
  std::vector<ChatBlock> blocks;
};

// L'opcode de la réponse, pour qui doit reconnaître une trame qui ne se décode
// pas.
inline constexpr std::uint16_t kChatBlocksReplyOpcode = 0x0F3F;

// « Quels blocages ce joueur a-t-il reçus ? »
[[nodiscard]] core::Result<void> RequestChatBlocks(Connection& link, std::uint32_t aid);

// La réponse, ou rien quand la trame n'en est pas une ou qu'elle ne se découpe
// pas en entrées entières. Une entrée d'un genre inconnu est sautée.
[[nodiscard]] std::optional<ChatBlocksReply> DecodeChatBlocks(std::span<const std::uint8_t> frame);
[[nodiscard]] std::optional<ChatBlocksReply> ReadChatBlocks(const Frame& frame);

}  // namespace ro::net
