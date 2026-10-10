#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "ro/net/connection.h"
#include "ro/proto/chat_author_packets.h"
#include "ro/sim/chat_line.h"

namespace ro::net {

// ── LA PHRASE D'UNE TRAME DE PAROLE, ET SON AUTEUR ──────────────────────────
//
// L'auteur arrive en `Frame::preamble` (`ZC_MOONLIGHT_CHAT_AUTHOR`). Il désigne
// des octets de la phrase TELLE QU'ELLE A VOYAGÉ : « [alias ]Nom : » en tête,
// sur `header_len` octets, le nom à `name_offset`.
//
// 🔴 CHAQUE MORCEAU SE CONVERTIT À PART, ET LES DÉCALAGES SE COMPTENT APRÈS : un
// nom en 1252 change de longueur en devenant de l'UTF-8, et un décalage pris
// sur le fil désignerait alors le milieu d'un caractère.
struct AuthoredText {
  // La phrase ENTIÈRE, en UTF-8.
  std::string text;
  // Vide quand rien ne décrit la phrase, ou que la description ne s'accorde
  // pas avec elle : la ligne se montre alors comme avant.
  std::optional<sim::ChatAuthor> author;
  // Le type du canal quand l'auteur en décrit un, même si sa tête ne s'accorde
  // pas avec la phrase.
  std::uint8_t channel_type = proto::kChatAuthorNoChannelType;
};

// `message` : la phrase telle qu'elle a voyagé, avant toute conversion.
[[nodiscard]] AuthoredText ReadAuthoredText(const Frame& frame, std::string_view message);

}  // namespace ro::net
