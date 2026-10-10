#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <imgui.h>

#include "ro/ui/chat_link.h"
#include "ro/ui/chat_rich_art.h"

namespace ro::ui {

// ── LE BALISAGE D'UNE LIGNE DU FIL ──────────────────────────────────────────
//
// Ce que les joueurs et le relai Discord écrivent pour mettre en forme :
// `**gras**`, `*italique*`, `__souligné__`, `~~barré~~`, `||spoiler||`,
// `` `code` ``, une couleur `^RRGGBB`, l'icône `^i[id]`, les emotes `:nom:` et
// `<:nom:id>`, et les adresses web. Le texte rendu en est débarrassé, et les
// styles se rangent à côté, en suites d'octets.
//
// 🔴 LES LIENS REÇUS RESTENT OPAQUES : copiés tels quels, jamais balisés de
// l'intérieur. Un objet dont le nom porte « * » ne doit pas perdre un caractère.

enum ChatTextStyle : std::uint8_t {
  kChatBold = 1U << 0,
  kChatItalic = 1U << 1,
  kChatUnderline = 1U << 2,
  kChatStrike = 1U << 3,
  kChatCode = 1U << 4,
  kChatSpoiler = 1U << 5,
};

// UNE SUITE D'OCTETS DU TEXTE RENDU, et son style.
struct ChatStyleRun {
  std::uint32_t offset = 0;
  std::uint32_t length = 0;
  std::uint8_t style = 0;
  // Zéro : la couleur de la ligne.
  ImU32 color = 0;
  // Le rang du spoiler dans la ligne, à partir de 1 ; zéro hors spoiler.
  std::uint16_t spoiler = 0;
};

// Le rang d'origine d'un lien que le balisage a CRÉÉ — une adresse.
inline constexpr std::size_t kNewChatLink = static_cast<std::size_t>(-1);

struct ChatMarkupResult {
  std::string text;
  std::vector<ChatLinkSpan> links;
  // Pour chaque lien rendu, son rang dans les liens d'entrée, ou `kNewChatLink`.
  std::vector<std::size_t> link_origin;
  std::vector<ChatStyleRun> runs;
  std::vector<ChatInlineImage> images;
  // Les adresses des images `kPicture`, en regard de leur `picture_key`.
  std::vector<std::string> picture_urls;
};

// UNE LIGNE PEUT-ELLE PORTER DU BALISAGE ? La question se pose à chaque ligne et
// à chaque image.
//
// 🔴 CONTRAT : AUCUNE ALLOCATION. Elle ne lit que des vues et des octets ; une
// copie de chaîne ici se paierait autant de fois qu'il y a de lignes à l'écran,
// à chaque image. Elle peut répondre vrai sans rien à faire, jamais faux quand
// `ApplyChatMarkup` aurait changé quelque chose.
[[nodiscard]] bool MayCarryChatMarkup(std::string_view text);
// Faux, et rien d'écrit : il n'y avait rien à faire.
[[nodiscard]] bool ApplyChatMarkup(std::string_view text, std::span<const ChatLinkSpan> links,
                                   ChatMarkupResult* out);

}  // namespace ro::ui
