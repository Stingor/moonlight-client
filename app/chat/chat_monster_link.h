#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace moonclient {

// ── LE LIEN D'UN MONSTRE : `<MOBL>id:rang:nom</MOBL>` ───────────────────────
//
// Une balise À NOUS : le client d'origine ne la connaît pas,
// et la laisse à l'écran telle quelle.
//
// 🔴 LE NOM VOYAGE DANS LA BALISE, et ce n'est pas une commodité : le client ne
// sait pas nommer un monstre. Un lien qui ne porterait que l'identifiant ferait
// interroger le serveur par CHAQUE lecteur — un lien posté dans un canal public,
// et c'est toute la population qui envoie un paquet.
//
// Champs séparés par ':', et le nom EN DERNIER : il est libre d'en porter, comme
// d'espaces et d'apostrophes.

inline constexpr std::string_view kMonsterLinkOpen = "<MOBL>";
inline constexpr std::string_view kMonsterLinkClose = "</MOBL>";

// Le rang que la balise transporte : 0 ordinaire, 1 boss, 2 MVP.
inline constexpr std::uint8_t kMonsterRankOrdinary = 0;
inline constexpr std::uint8_t kMonsterRankBoss = 1;
inline constexpr std::uint8_t kMonsterRankMvp = 2;

struct MonsterLink {
  // L'identifiant de la base des monstres — ou la classe de sprite d'un acteur
  // du monde, les deux coïncidant pour l'écrasante majorité des monstres.
  std::uint32_t monster_id = 0;
  std::uint8_t rank = kMonsterRankOrdinary;
  // En UTF-8.
  std::string name;
};

// La balise. VIDE sans identifiant ni nom, ou quand le nom porte la fermante :
// il couperait la balise en deux à la relecture.
[[nodiscard]] std::string BuildMonsterLink(const MonsterLink& link);

// Relit une balise qui COMMENCE à `text[0]`. Faux si ce n'en est pas une, sans
// identifiant ou sans nom. Un rang hors bornes se lit comme ordinaire.
//
// ⚠ `after` REÇOIT CE QUI SUIT LA FERMANTE, et la fin du texte quand elle
// manque : une ligne coupée par le serveur ne doit pas faire lire au-delà.
[[nodiscard]] bool ParseMonsterLink(std::string_view text, MonsterLink* out, std::size_t* after);

// Où commence la prochaine balise à partir de `from`, ou `npos`.
[[nodiscard]] std::size_t FindMonsterLink(std::string_view text, std::size_t from);

// Ce que le lecteur voit : « <[MVP] Baphomet> ». ⚠ L'ÉTIQUETTE DE RANG N'EST PAS
// TRADUITE : les joueurs du serveur la lisent déjà telle quelle.
[[nodiscard]] std::string MonsterLinkLabel(const MonsterLink& link);

// Le libellé cliquable, sans les chevrons qui l'encadrent : « [MVP] Baphomet ».
[[nodiscard]] std::string MonsterLinkName(const MonsterLink& link);

// LA PAGE DU MONSTRE DANS LE BESTIAIRE DU SITE. ⚠ L'ADRESSE EST CELLE DE CE
// SERVEUR-LÀ, comme celle de la base d'objets (`ro/vfs/item_table.h`).
inline constexpr std::string_view kBestiaryPageUrl =
    "https://moonlight-destiny.fr/index.php?page=bestiary&mobid=";

[[nodiscard]] std::string MonsterBestiaryUrl(std::uint32_t monster_id);

}  // namespace moonclient
