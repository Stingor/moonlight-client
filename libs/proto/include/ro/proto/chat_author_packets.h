#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "ro/proto/map_packets.h"

namespace ro::proto {

// ── L'AUTEUR D'UNE LIGNE DE PAROLE (`ZC_MOONLIGHT_CHAT_AUTHOR 0x0F3A`) ───────
//
// Il PRÉCÈDE la trame de parole qu'il décrit — 0x008D, 0x0109, 0x017F, 0x098E ou
// 0x02C1 —, qui part, elle, inchangée. Le serveur ne l'envoie qu'à qui annonce
// `kUiCapChatAuthor`.
//
// Disposition : le bloc fixe de 44 octets engendré, puis, pour un canal
// seulement, son nom sans « # » ni NUL jusqu'à la fin de la trame.
//
// Sous un faux nom, `name` porte le faux nom et les deux identifiants valent
// zéro : l'en-tête ne trahit pas qui se cache derrière.

// D'où vient la parole décrite.
enum class ChatAuthorScope : std::uint8_t {
  kPublic = 1,
  kParty = 2,
  kGuild = 3,
  kClan = 4,
  kChannel = 5,
};

// Le `channel_type` d'une parole qui ne passe pas par un canal.
inline constexpr std::uint8_t kChatAuthorNoChannelType = 0xFF;
// Le `channel_type` du canal d'une alliance de guildes.
inline constexpr std::uint8_t kChatAuthorAllyChannelType = 3;

struct ChatAuthorNotice {
  ZcMoonlightChatAuthor fixed;
  // Le nom du canal tel qu'il a voyagé, vide hors d'un canal.
  std::string channel;
};

// Faux si la trame n'est pas un auteur lisible : trop courte, opcode étranger,
// portée inconnue, nom vide, nom de canal qui porte un NUL, ou queue hors d'un
// canal.
//
// ⚠ L'ACCORD AVEC LA LIGNE DÉCRITE NE SE JUGE PAS ICI : `header_len` et
// `name_offset` désignent des octets d'une AUTRE trame.
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ChatAuthorNotice& notice);

// L'ENVERS DU DÉCODEUR, pour les essais : le client n'émet pas ce paquet.
[[nodiscard]] std::vector<std::uint8_t> Encode(const ChatAuthorNotice& notice);

}  // namespace ro::proto
