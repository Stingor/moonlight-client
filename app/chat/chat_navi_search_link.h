#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "ro/ui/chat_link.h"
#include "ro/vfs/map_display_names.h"

namespace moonclient {

// ── LE LIEN D'UNE RECHERCHE DE NAVIGATION : `<NAVS>famille:carte:terme</NAVS>` ─
//
// Une QUESTION posée à la fenêtre de navigation, pas un lieu : « où trouver ce
// PNJ, ce monstre, cette carte ». La famille est un chiffre — `0` carte, `1`
// PNJ, `2` monstre —, la carte est le CONTEXTE de la recherche (nom interne,
// vide pour aucun), et le terme est ce qu'on cherche, en UTF-8 : le nom INTERNE
// pour une carte, le nom affiché sinon.
//
// 🔴 LE CHAMP CARTE S'ÉCRIT MÊME VIDE : `<NAVS>2::Baphomet</NAVS>`. C'est la
// forme que les autres lecteurs de la balise attendent.
//
// 🔴 LA LECTURE ACCEPTE AUSSI LA FORME À DEUX CHAMPS, `famille:terme`. Le
// segment entre les deux premiers `:` n'est une carte que s'il ne porte ni
// espace ni tabulation ; sinon le terme commence après le premier `:` et garde
// les suivants. Un terme qui contient `:` sans espace avant lui se lit donc
// comme carte + terme — `<NAVS>1:Agent:Bob</NAVS>` cherche « Bob » sur la carte
// « Agent » —, et c'est voulu : tous les lecteurs le lisent ainsi.
//
// ⚠ LE LIBELLÉ SE COMPOSE CHEZ LE LECTEUR, dans sa langue : « [PNJ: Kafra
// (Prontera)] ». Une carte s'y montre par son nom lisible.

inline constexpr std::string_view kNaviSearchLinkOpen = "<NAVS>";
inline constexpr std::string_view kNaviSearchLinkClose = "</NAVS>";

// Les valeurs de la balise : elles voyagent, elles ne se renumérotent pas.
enum class NaviSearchFamily : std::uint8_t { kMap = 0, kNpc = 1, kMonster = 2 };

struct NaviSearchLink {
  NaviSearchFamily family = NaviSearchFamily::kMap;
  // Le contexte : le nom INTERNE d'une carte, vide pour aucun.
  std::string map;
  // Ce qu'on cherche, en UTF-8 ; le nom interne pour une carte.
  std::string term;
};

// La balise. VIDE quand le terme est vide ou porte un chevron : il couperait la
// balise à la relecture. Un contexte vide, ou qui porte un chevron, un `:`, une
// espace ou une tabulation, s'écrit VIDE — la recherche part alors sans carte.
[[nodiscard]] std::string BuildNaviSearchLink(const NaviSearchLink& link);

// LE TERME D'UN « LINKER CE NPC » : la partie VISIBLE du nom, la seule que les
// tables de navigation connaissent et que la plaque montre — un dièse ouvre la
// partie cachée. VIDE quand la balise ne peut pas le porter : un nom anonyme,
// ou qui porte un chevron. Le menu l'offre sur ce seul critère, et le lien
// publie ce terme-là.
[[nodiscard]] std::string NpcSearchTerm(std::string_view npc_name);

// CE QUE LA LECTURE D'UNE BALISE A DONNÉ.
enum class NaviSearchLinkRead : std::uint8_t {
  // Une recherche lue, rangée dans `out`.
  kRead,
  // Le texte ne commence pas par l'ouvrante.
  kNoTag,
  // Pas de fermante, ou pas de `:` dans le corps : l'OUVRANTE reste du texte, et
  // la lecture reprend juste après elle.
  kNotATag,
  // Famille inconnue, ou terme refusé (chevron) : la balise se montre en texte,
  // `[terme]`, sans mener nulle part. `raw_term` reçoit ce terme.
  kAsText,
  // Terme vide : la balise disparaît de la ligne.
  kDropped,
};

// Relit une balise qui COMMENCE à `text[0]`. Une famille non numérique se lit
// `0`, comme les autres lecteurs de la balise.
//
// ⚠ `after` REÇOIT CE QUI SUIT LA FERMANTE pour `kRead`, `kAsText` et
// `kDropped`, et ce qui suit l'OUVRANTE pour `kNotATag`. `raw_term`, facultatif,
// reçoit le terme tel qu'il est dans `text` — c'est lui que montre `kAsText`.
[[nodiscard]] NaviSearchLinkRead ParseNaviSearchLink(std::string_view text, NaviSearchLink* out,
                                                     std::size_t* after,
                                                     std::string_view* raw_term = nullptr);

// Où commence la prochaine balise à partir de `from`, ou `npos`.
[[nodiscard]] std::size_t FindNaviSearchLink(std::string_view text, std::size_t from);

// Le libellé cliquable, composé chez le lecteur : « [Carte: X] », « [PNJ: X] »,
// « [Monstre: X] », et avec contexte « [PNJ: X (Carte lisible)] ». `names`,
// facultatif, donne le nom lisible d'une carte : sans lui, le nom interne. Un
// PNJ s'y montre sans la partie cachée de son nom, qu'un autre écrivain peut
// avoir publiée. Trop long, il est coupé à une frontière de caractère.
[[nodiscard]] std::string NaviSearchLinkLabel(const NaviSearchLink& link,
                                              const ro::vfs::MapDisplayNames* names);

// Ce que le menu du lien sait d'avance : sa famille, et s'il est borné à une
// carte.
[[nodiscard]] ro::ui::NaviSearchMenuFacts NaviSearchFactsOf(const NaviSearchLink& link);

}  // namespace moonclient
