#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "ro/formats/navi_tables.h"

namespace ro::sim {

// LA RECHERCHE DANS LE GRAPHE DE NAVIGATION : cartes, PNJ, monstres.
//
// Tolérante là où celle du client d'origine est exacte : insensible à la casse
// et aux accents, par morceaux de mots — « kaf pront » trouve la Kafra de
// Prontera — et chaque mot peut se trouver dans le nom OU dans la carte qui le
// porte. Le graphe est en mémoire : chercher à chaque frappe ne coûte rien au
// réseau.

enum class NaviHitKind : std::uint8_t { kMap, kNpc, kSpawn };

// Un résultat : sa nature, et son RANG dans la table correspondante de
// `NaviData` (`maps`, `npcs` ou `spawns`).
struct NaviHit {
  NaviHitKind kind = NaviHitKind::kMap;
  std::uint32_t rank = 0;
};

// Les résultats qui portent le même NOM et la même nature : un monstre qui
// apparaît sur trois cartes, les trente-huit PNJ d'un même service.
struct NaviHitGroup {
  NaviHitKind kind = NaviHitKind::kMap;
  std::string name;
  std::vector<NaviHit> hits;
};

struct NaviSearchQuery {
  std::string_view term;
  // Borne les résultats à une carte ; `kNoNaviMap` pour ne rien borner.
  formats::NaviMapIndex only_map = formats::kNoNaviMap;
  // La carte du joueur : ses résultats passent en tête de leur groupe.
  formats::NaviMapIndex prefer_map = formats::kNoNaviMap;
  // Au-delà, la liste se tronque — un terme d'une lettre ramènerait tout.
  std::size_t max_hits = 400;
};

// Les groupes, les meilleures correspondances d'abord : le nom exact, puis le nom
// qui commence par le terme, puis celui qui le contient, puis ce qui ne
// correspond que par sa carte.
[[nodiscard]] std::vector<NaviHitGroup> SearchNavi(const formats::NaviData& data,
                                                   const NaviSearchQuery& query);

// Ce qu'il y a SUR une carte, sans rien chercher : ses PNJ puis ses monstres, un
// groupe par nature.
[[nodiscard]] std::vector<NaviHitGroup> ContentsOfMap(const formats::NaviData& data,
                                                      formats::NaviMapIndex map);

// Un nom de PNJ tel que le transportent la plaque d'un acteur ou un lien de chat
// porte parfois sa partie CACHÉE, derrière un dièse ; les tables ne connaissent
// que la partie visible.
[[nodiscard]] std::string_view VisibleNpcName(std::string_view name) noexcept;

// Le texte replié pour la comparaison : minuscules, sans accents.
[[nodiscard]] std::string FoldForSearch(std::string_view utf8);

}  // namespace ro::sim
