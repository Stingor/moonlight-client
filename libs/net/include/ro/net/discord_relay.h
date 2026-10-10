#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/chat_line.h"
#include "ro/sim/world_fwd.h"

namespace ro::net {

// ── LE RELAI DU CANAL DISCORD (`ZC_MOONLIGHT_DISCORD_MSG 0x0F08`) ────────────
//
// Ce qui se dit sur le canal Discord du serveur, poussé aux clients reconnus
// qui se trouvent sur une carte de Gonryun : une ligne de texte UTF-8, déjà
// préfixée « [#canal][Pseudo] ».
//
// Disposition : [op:2][len:2][texte UTF-8 terminé par NUL, 243 octets au plus].
//
// 🔴 LE SERVEUR L'ENVOIE SANS TENIR COMPTE DU RÉGLAGE : c'est ici que la ligne
// se jette quand le joueur a décoché le relai, ou hors de Gonryun.

// Le bleu de Discord, la couleur de toute ligne relayée.
inline constexpr std::uint32_t kDiscordRelayRgb = 0x7289DA;
inline constexpr std::size_t kDiscordRelayTextMax = 243;
inline constexpr std::size_t kDiscordRelayHeader = 4;
inline constexpr std::string_view kDiscordRelayMapPrefix = "gonryun";

// Préfixe, casse ASCII ignorée.
[[nodiscard]] bool IsDiscordRelayMap(std::string_view map_name);
// Les octets [4..len), arrêtés au NUL et à 243, rendus en UTF-8.
[[nodiscard]] std::optional<std::string> ReadDiscordRelayText(std::span<const std::uint8_t> frame);

// Le pseudo de « [x][Pseudo] … » : son début et sa longueur en octets.
struct DiscordRelayAuthor {
  std::size_t offset = 0;
  std::size_t length = 0;
};
[[nodiscard]] std::optional<DiscordRelayAuthor> FindDiscordRelayAuthor(std::string_view text);

// ── LE MÊME MESSAGE, ENTIER ET DÉCRIT (`ZC_MOONLIGHT_DISCORD_RICH 0x0F3C`) ──
//
// Il part À LA PLACE de 0x0F08 à qui annonce `kUiCapDiscordRich`, sans la
// coupure à 243 octets : identifiant et nom de l'auteur, avatar, texte où les
// mentions sont résolues mais les emotes `<:nom:id>` gardées.
//
// Disposition : un bloc fixe de 18 octets, puis canal | nom | avatar | texte,
// en UTF-8, sans NUL, de longueurs `channel_len`, `name_len`, `avatar_len` et
// `text_len`, qui remplissent la trame exactement.
//
// La ligne enregistrée a la forme de 0x0F08, « [#canal][Nom] texte » : le lien
// du pseudo, les mentions et le journal la lisent sans savoir d'où elle vient.

// Le plafond du texte, en octets UTF-8.
inline constexpr std::size_t kDiscordRichTextMax = 4000;
// L'adresse devant le chemin d'un avatar, que le serveur donne relatif.
inline constexpr std::string_view kDiscordCdnAddress = "https://cdn.discordapp.com/";

// UN CHEMIN D'AVATAR ADMIS : non vide, fait de `[A-Za-z0-9_./-]` seuls, sans
// `..` ni `//`, ni `/` en tête. Tout autre chemin pourrait sortir du CDN.
[[nodiscard]] bool IsDiscordAvatarPath(std::string_view path);
// L'adresse de l'avatar, ou vide si le chemin n'est pas admis.
[[nodiscard]] std::string DiscordAvatarUrl(std::string_view path);

// Ce qu'un message riche apporte : la ligne recomposée, et son auteur.
struct DiscordRichLine {
  std::string text;
  sim::ChatAuthor author;
};
// Faux si la trame ne se lit pas : longueurs qui ne remplissent pas la trame
// exactement, texte au-delà du plafond, NUL, canal ou nom vides. Un avatar
// refusé ne refuse pas le message : il s'efface.
[[nodiscard]] std::optional<DiscordRichLine> ReadDiscordRichLine(
    std::span<const std::uint8_t> frame);

// Les deux formes, sous les mêmes filtres ; la riche donne aussi l'auteur.
[[nodiscard]] FrameOutcome ApplyDiscordRelayFrame(const Frame& frame, sim::World& world,
                                                  std::uint32_t now_ms);

}  // namespace ro::net
