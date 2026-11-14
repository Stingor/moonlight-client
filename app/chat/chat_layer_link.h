#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace moonclient {

// ── LA BALISE QUI PORTE LES COULEURS D'UN CALQUE DANS UNE LIGNE ─────────────
//
// `<LAYC>étiquette:code</LAYC>` : le code d'un calque teint — un chapeau, une
// cape, la tête — et l'objet qu'il habille, « Weissbier Hat ».
//
// 🔴 LE CODE EN ENTIER, comme pour un style : une teinture de calque n'existe
// nulle part hors de son porteur. Il est validé à la lecture, et une balise
// illisible ne devient pas un lien.
//
// ⚠ LE PREMIER DEUX-POINTS SÉPARE : le code en porte, l'étiquette nettoyée
// jamais.
inline constexpr std::string_view kLayerLinkOpen = "<LAYC>";
inline constexpr std::string_view kLayerLinkClose = "</LAYC>";

struct LayerLink {
  // L'étiquette, telle qu'elle a voyagé. Peut être vide.
  std::string label;
  // Le code, tel qu'il a voyagé : c'est lui qu'on relaie, jamais ré-encodé.
  std::string code;
};

// La balise complète, ou vide si le code est illisible, ou si la balise ne
// tiendrait pas dans une ligne de conversation — un calque chargé de
// retouches exactes peut dépasser ce que le serveur accepte.
[[nodiscard]] std::string BuildLayerLink(std::string_view label, std::string_view code);

// Où commence la prochaine balise, ou `npos`.
[[nodiscard]] std::size_t FindLayerLink(std::string_view text, std::size_t from);

// Lit la balise qui commence à `text[0]`. Vrai si elle est entière et que son
// code se lit. ⚠ `after` vaut ce qu'elle occupe dès que la fermante est là,
// même sur un code illisible ; zéro si elle est tronquée.
[[nodiscard]] bool ParseLayerLink(std::string_view text, LayerLink* out, std::size_t* after);

// Le libellé cliquable : « [Personnalisation : étiquette] ».
[[nodiscard]] std::string LayerLinkLabel(std::string_view label);

}  // namespace moonclient
