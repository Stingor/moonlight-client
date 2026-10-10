#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "options/setting_links.h"

namespace moonclient {

struct PendingChatLink;

// ── LE LIEN D'UN RÉGLAGE OU D'UN ONGLET : `<SETL>clé:libellé</SETL>` ───────
//
// Un client qui ne connaît pas la balise la laisse à l'écran telle quelle.
//
// 🔴 C'EST LA CLÉ QUI VOYAGE, JAMAIS UN RANG. Un rang décrit l'ordre d'UNE
// version du panneau : une section insérée entre-temps et le lecteur
// atterrirait en silence sur la voisine. Le libellé voyage avec, seulement pour
// qui ne connaît pas la clé — chez nous, c'est le libellé LOCAL, traduit chez le
// lecteur, qui s'affiche.
//
// ⚠ LA CLÉ S'ARRÊTE AU PREMIER `:`, et le libellé court jusqu'à la fermante :
// il est libre d'en porter.

inline constexpr std::string_view kSettingLinkOpen = "<SETL>";
inline constexpr std::string_view kSettingLinkClose = "</SETL>";

struct SettingLink {
  std::string key;
  // Le libellé de repli, tel qu'il a voyagé.
  std::string fallback;
};

// Ce que la relecture d'une balise a trouvé.
enum class SettingLinkRead : std::uint8_t {
  kRead,
  // La fermante manque : la ligne a été coupée.
  kUnclosed,
  // Le corps n'a pas de `:` — ce n'est pas une balise qu'on sache couper.
  kNoSeparator,
};

// La balise. VIDE quand la clé ne peut pas voyager (voir `SettingKeyTravels`)
// ou que le libellé porte la fermante ou l'ouvrante : il couperait la balise.
[[nodiscard]] std::string BuildSettingLink(const SettingLink& link);

// Relit une balise qui COMMENCE à `text[0]` — l'appelant l'a trouvée par
// `FindSettingLink`. `after` reçoit ce qui suit la fermante, ou ce qui suit
// l'ouvrante quand la fermante manque : les liens d'après se lisent encore.
[[nodiscard]] SettingLinkRead ParseSettingLink(std::string_view text, SettingLink* out,
                                               std::size_t* after);

// Où commence la prochaine balise à partir de `from`, ou `npos`.
[[nodiscard]] std::size_t FindSettingLink(std::string_view text, std::size_t from);

// LE LIEN À POSER DANS LA SAISIE pour cette destination : le libellé que le fil
// rendra, et la balise qui partira — le libellé de repli en français nu.
[[nodiscard]] PendingChatLink SettingPendingLink(const SettingDestination& destination);

}  // namespace moonclient
