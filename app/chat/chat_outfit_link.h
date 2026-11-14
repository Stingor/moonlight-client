#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace moonclient {

// ── LA BALISE QUI PORTE UNE TENUE DANS UNE LIGNE DE CONVERSATION ────────────
//
// `<OUTF>étiquette:CODE</OUTF>` : le code court qu'un serveur a donné à une
// tenue, et l'étiquette qui la nomme — « Stingor (Soirée) ».
//
// 🔴 LE CODE COURT SUFFIT, contrairement au style : la tenue vit sur le
// serveur, et c'est lui qui la rend à qui la demande. Le lien ne transporte
// qu'une désignation.
//
// ⚠ LA BALISE RESTE LISIBLE CHEZ QUI NE LA REND PAS : l'étiquette vient en
// tête, puis le code, qui se tape tel quel dans l'éditeur.
inline constexpr std::string_view kOutfitLinkOpen = "<OUTF>";
inline constexpr std::string_view kOutfitLinkClose = "</OUTF>";

struct OutfitLink {
  // L'étiquette, telle qu'elle a voyagé. Peut être vide.
  std::string label;
  // Le code, normalisé : huit signes, en majuscules, sans tiret.
  std::string code;
};

// La balise complète, ou vide si le code n'est pas un code court. L'étiquette
// est nettoyée de ce qui couperait la balise.
[[nodiscard]] std::string BuildOutfitLink(std::string_view label, std::string_view code);

// Où commence la prochaine balise, ou `npos`.
[[nodiscard]] std::size_t FindOutfitLink(std::string_view text, std::size_t from);

// Lit la balise qui commence à `text[0]`. Vrai si elle est entière et que son
// code se lit ; `after` vaut alors ce qu'elle occupe.
//
// ⚠ `after` VAUT CE QU'IL FAUT SAUTER DÈS QUE LA FERMANTE EST LÀ, même sur un
// code illisible : l'appelant repartirait sinon sur la même balise. Zéro :
// rien à sauter, la balise est tronquée.
[[nodiscard]] bool ParseOutfitLink(std::string_view text, OutfitLink* out, std::size_t* after);

// Le libellé cliquable : « [Tenue : étiquette] », ou « [Tenue] » sans elle.
[[nodiscard]] std::string OutfitLinkLabel(std::string_view label);

}  // namespace moonclient
