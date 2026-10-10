#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/sim/chat_line.h"

namespace ro::sim {

// ── UNE LIGNE QUI NOUS NOMME ────────────────────────────────────────────────
//
// Ce qui la signale : notre nom, ou l'un des mots-clés du joueur, en mot entier
// et sans casse ASCII. Ce qu'on a dit soi-même ne nous mentionne jamais.
//
// ⚠ LA CASSE NE SE MET DE CÔTÉ QUE POUR L'ASCII : les octets non ASCII se
// comparent exactement, faute de tables Unicode.

struct MentionRules {
  std::string_view own_name;
  std::uint32_t own_id = 0;
  std::span<const std::string> words;
};

// "a;b" : nettoyés, deux octets au moins.
[[nodiscard]] std::vector<std::string> SplitMentionWords(std::string_view typed);
// Mot entier, casse ASCII ignorée.
[[nodiscard]] bool TextMentions(std::string_view text, std::string_view word);
// Vraie pour `kSelf` ; un chuchotement envoyé ou non remis ; `speaker_id ==
// own_id` ; un texte qui commence par « own_name : » — derrière l'étiquette
// « [#canal] » pour un canal ; ou « [x][own_name] » quand le type est
// `kDiscord`.
[[nodiscard]] bool IsOwnChatLine(const ChatLine& line, std::string_view own_name,
                                 std::uint32_t own_id);
// LE PSEUDO D'UNE LIGNE RELAYÉE DE DISCORD, « [x][Pseudo] … » quel que soit x :
// une vue sur `text`, vide quand la forme manque ou que le pseudo l'est.
[[nodiscard]] std::string_view DiscordRelayAuthorName(std::string_view text);
// Seulement pour les genres parlés : public, chuchotement reçu, groupe, guilde,
// canal, clan, champ de bataille, annonce. Jamais pour le serveur, le sac, le
// combat, l'expérience ni l'appel.
[[nodiscard]] bool MentionsMe(const ChatLine& line, const MentionRules& rules);

}  // namespace ro::sim
