// LE LIEN D'UNE RECHERCHE DE NAVIGATION, `<NAVS>famille:carte:terme</NAVS>` :
// écriture, lecture, libellés.
//
// 🔴 LA LECTURE EST CELLE DES AUTRES LECTEURS DE LA BALISE, bizarreries
// comprises : un segment du milieu sans blanc est une carte, même quand
// l'auteur voulait un terme à deux-points. Les diverger ferait lire deux
// recherches différentes au même lien selon le client.

#include "chat/chat_navi_search_link.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/text.h"
#include "ro/vfs/map_display_names.h"

using moonclient::BuildNaviSearchLink;
using moonclient::FindNaviSearchLink;
using moonclient::NaviSearchFactsOf;
using moonclient::NaviSearchFamily;
using moonclient::NaviSearchLink;
using moonclient::NaviSearchLinkLabel;
using moonclient::NaviSearchLinkRead;
using moonclient::NpcSearchTerm;
using moonclient::ParseNaviSearchLink;

namespace {

[[nodiscard]] ro::vfs::MapDisplayNames Names() {
  auto names =
      ro::vfs::MapDisplayNames::Parse("version: 1\nmaps:\n  prontera:\n    name: Prontera\n");
  REQUIRE(names.has_value());
  return std::move(*names);
}

// Une lecture, et ce qu'elle a donné.
struct Read {
  NaviSearchLinkRead read = NaviSearchLinkRead::kNoTag;
  NaviSearchLink link;
  std::size_t after = 0;
  std::string_view raw_term;
};

[[nodiscard]] Read ReadOf(std::string_view text) {
  Read result;
  result.read = ParseNaviSearchLink(text, &result.link, &result.after, &result.raw_term);
  return result;
}

}  // namespace

TEST_CASE("Lien de recherche : les trois familles font l'aller-retour", "[app][chat][navi]") {
  const NaviSearchLink map{.family = NaviSearchFamily::kMap, .map = {}, .term = "prontera"};
  const NaviSearchLink npc{
      .family = NaviSearchFamily::kNpc, .map = "prontera", .term = "Kafra Employee"};
  const NaviSearchLink monster{.family = NaviSearchFamily::kMonster, .map = {}, .term = "Baphomet"};
  CHECK(BuildNaviSearchLink(map) == "<NAVS>0::prontera</NAVS>");
  CHECK(BuildNaviSearchLink(npc) == "<NAVS>1:prontera:Kafra Employee</NAVS>");
  // 🔴 LE CHAMP CARTE S'ÉCRIT MÊME VIDE.
  CHECK(BuildNaviSearchLink(monster) == "<NAVS>2::Baphomet</NAVS>");
  for (const NaviSearchLink& link : {map, npc, monster}) {
    const std::string wire = BuildNaviSearchLink(link) + " suite";
    const Read back = ReadOf(wire);
    REQUIRE(back.read == NaviSearchLinkRead::kRead);
    CHECK(back.link.family == link.family);
    CHECK(back.link.map == link.map);
    CHECK(back.link.term == link.term);
    CHECK(wire.substr(back.after) == " suite");
  }
}

TEST_CASE("Lien de recherche : l'écriture refuse ce qui couperait la balise", "[app][chat][navi]") {
  // UN TERME vide ou à chevron : aucun lien.
  CHECK(BuildNaviSearchLink({.family = NaviSearchFamily::kNpc, .map = {}, .term = ""}).empty());
  CHECK(BuildNaviSearchLink({.family = NaviSearchFamily::kNpc, .map = {}, .term = "a<b"}).empty());
  CHECK(BuildNaviSearchLink({.family = NaviSearchFamily::kNpc, .map = {}, .term = "a>b"}).empty());
  // UN CONTEXTE à espace, à « : », à tabulation ou à chevron s'écrit VIDE : la
  // recherche part sans carte.
  for (const char* const map : {"prt fild", "prt:fild", "prt\tfild", "prt<fild"}) {
    CHECK(BuildNaviSearchLink({.family = NaviSearchFamily::kNpc, .map = map, .term = "Bob"}) ==
          "<NAVS>1::Bob</NAVS>");
  }
  // UNE FAMILLE INCONNUE ne s'écrit pas.
  CHECK(BuildNaviSearchLink({.family = static_cast<NaviSearchFamily>(7), .map = {}, .term = "Bob"})
            .empty());
  // TROP LONGUE pour une ligne : rien.
  CHECK(BuildNaviSearchLink(
            {.family = NaviSearchFamily::kMonster, .map = {}, .term = std::string(250, 'x')})
            .empty());
}

TEST_CASE("Lien de recherche : la forme ancienne à deux champs se lit encore",
          "[app][chat][navi]") {
  const Read old = ReadOf("<NAVS>2:Baphomet</NAVS>");
  REQUIRE(old.read == NaviSearchLinkRead::kRead);
  CHECK(old.link.family == NaviSearchFamily::kMonster);
  CHECK(old.link.map.empty());
  CHECK(old.link.term == "Baphomet");
}

TEST_CASE("Lien de recherche : le terme garde ses espaces et ses deux-points",
          "[app][chat][navi]") {
  // CONTEXTE VIDE : tout ce qui suit le second « : » est le terme.
  const Read spaced = ReadOf("<NAVS>1::Agent: Bob</NAVS>");
  REQUIRE(spaced.read == NaviSearchLinkRead::kRead);
  CHECK(spaced.link.map.empty());
  CHECK(spaced.link.term == "Agent: Bob");
  // UN SEGMENT DU MILIEU À ESPACE n'est pas une carte : le terme commence après
  // le premier « : ».
  const Read blank = ReadOf("<NAVS>1:Warp Agent: Bob</NAVS>");
  REQUIRE(blank.read == NaviSearchLinkRead::kRead);
  CHECK(blank.link.map.empty());
  CHECK(blank.link.term == "Warp Agent: Bob");
  // 🔴 LA BIZARRERIE PARTAGÉE : sans blanc, le segment est une carte.
  const Read odd = ReadOf("<NAVS>1:Agent:Bob</NAVS>");
  REQUIRE(odd.read == NaviSearchLinkRead::kRead);
  CHECK(odd.link.map == "Agent");
  CHECK(odd.link.term == "Bob");
  // UN CONTEXTE À CHEVRON se perd ; la recherche reste.
  const Read chevron = ReadOf("<NAVS>1:a>b:Bob</NAVS>");
  REQUIRE(chevron.read == NaviSearchLinkRead::kRead);
  CHECK(chevron.link.map.empty());
  CHECK(chevron.link.term == "Bob");
}

TEST_CASE("Lien de recherche : ce qui ne se lit pas comme une recherche", "[app][chat][navi]") {
  // PAS L'OUVRANTE en tête.
  CHECK(ReadOf("x<NAVS>2::Bob</NAVS>").read == NaviSearchLinkRead::kNoTag);
  // SANS « : » : l'ouvrante reste du texte, la lecture reprend juste après.
  const Read bare = ReadOf("<NAVS>Bob</NAVS>");
  CHECK(bare.read == NaviSearchLinkRead::kNotATag);
  CHECK(bare.after == std::string_view("<NAVS>").size());
  // SANS FERMANTE : de même.
  const Read cut = ReadOf("<NAVS>2::Bob");
  CHECK(cut.read == NaviSearchLinkRead::kNotATag);
  CHECK(cut.after == std::string_view("<NAVS>").size());
  // FAMILLE INCONNUE : le terme en texte.
  const Read unknown = ReadOf("<NAVS>7::Bob</NAVS>!");
  CHECK(unknown.read == NaviSearchLinkRead::kAsText);
  CHECK(unknown.raw_term == "Bob");
  CHECK(unknown.after == std::string_view("<NAVS>7::Bob</NAVS>").size());
  CHECK(ReadOf("<NAVS>-1::Bob</NAVS>").read == NaviSearchLinkRead::kAsText);
  // TERME À CHEVRON : en texte aussi.
  const Read chevron = ReadOf("<NAVS>2::a<b</NAVS>");
  CHECK(chevron.read == NaviSearchLinkRead::kAsText);
  CHECK(chevron.raw_term == "a<b");
  // TERME VIDE : la balise disparaît, quelle que soit la famille.
  CHECK(ReadOf("<NAVS>2::</NAVS>").read == NaviSearchLinkRead::kDropped);
  CHECK(ReadOf("<NAVS>9:</NAVS>").read == NaviSearchLinkRead::kDropped);
}

TEST_CASE("Lien de recherche : une famille se lit comme un nombre du C", "[app][chat][navi]") {
  // NON NUMÉRIQUE : zéro, une carte.
  const Read letters = ReadOf("<NAVS>x::prontera</NAVS>");
  REQUIRE(letters.read == NaviSearchLinkRead::kRead);
  CHECK(letters.link.family == NaviSearchFamily::kMap);
  // LES BLANCS DE TÊTE SAUTÉS, la queue non numérique ignorée.
  const Read padded = ReadOf("<NAVS> 2abc::Poring</NAVS>");
  REQUIRE(padded.read == NaviSearchLinkRead::kRead);
  CHECK(padded.link.family == NaviSearchFamily::kMonster);
  // UN NOMBRE DÉMESURÉ est inconnu, sans déborder.
  CHECK(ReadOf("<NAVS>99999999999999999999::Bob</NAVS>").read == NaviSearchLinkRead::kAsText);
}

TEST_CASE("Lien de recherche : la prochaine balise", "[app][chat][navi]") {
  const std::string line = "a <NAVS>2::Bob</NAVS> b <NAVS>0::prontera</NAVS>";
  CHECK(FindNaviSearchLink(line, 0) == 2);
  CHECK(FindNaviSearchLink(line, 3) == line.find("<NAVS>0"));
  CHECK(FindNaviSearchLink(line, line.size()) == std::string_view::npos);
  CHECK(FindNaviSearchLink("rien", 0) == std::string_view::npos);
}

TEST_CASE("Lien de recherche : le libellé se compose chez le lecteur", "[app][chat][navi]") {
  const ro::vfs::MapDisplayNames names = Names();
  const NaviSearchLink map{.family = NaviSearchFamily::kMap, .map = {}, .term = "prontera"};
  const NaviSearchLink npc{
      .family = NaviSearchFamily::kNpc, .map = "prontera", .term = "Kafra Employee"};
  const NaviSearchLink monster{.family = NaviSearchFamily::kMonster, .map = {}, .term = "Baphomet"};
  // UNE CARTE SE MONTRE PAR SON NOM LISIBLE, l'interne sans table.
  CHECK(NaviSearchLinkLabel(map, &names) == "[Carte: Prontera]");
  CHECK(NaviSearchLinkLabel(map, nullptr) == "[Carte: prontera]");
  // LE CONTEXTE EST NOMMÉ : deux PNJ homonymes ne se confondent pas.
  CHECK(NaviSearchLinkLabel(npc, &names) == "[PNJ: Kafra Employee (Prontera)]");
  CHECK(NaviSearchLinkLabel(npc, nullptr) == "[PNJ: Kafra Employee (prontera)]");
  CHECK(NaviSearchLinkLabel(monster, &names) == "[Monstre: Baphomet]");
  // UNE CARTE INCONNUE DE LA TABLE garde son nom interne.
  const NaviSearchLink elsewhere{
      .family = NaviSearchFamily::kMonster, .map = "gef_fild10", .term = "Orc Lord"};
  CHECK(NaviSearchLinkLabel(elsewhere, &names) == "[Monstre: Orc Lord (gef_fild10)]");
  const NaviSearchLink bounded_map{
      .family = NaviSearchFamily::kMap, .map = "prontera", .term = "prt_fild08"};
  CHECK(NaviSearchLinkLabel(bounded_map, &names) == "[Carte: prt_fild08 (Prontera)]");
  CHECK(NaviSearchLinkLabel({.family = NaviSearchFamily::kNpc, .map = {}, .term = ""}, &names)
            .empty());
}

TEST_CASE("Lien de recherche : un PNJ se publie et se montre sans sa partie cachée",
          "[app][chat][navi]") {
  // CE QUE « LINKER CE NPC » PUBLIE : la partie visible, celle des tables.
  CHECK(NpcSearchTerm("Kafra Employee#prt_kafra1") == "Kafra Employee");
  CHECK(NpcSearchTerm("Kafra Employee") == "Kafra Employee");
  // RIEN QUE LA BALISE NE PUISSE PORTER : anonyme, tout caché, ou à chevron.
  CHECK(NpcSearchTerm("").empty());
  CHECK(NpcSearchTerm("#caché").empty());
  CHECK(NpcSearchTerm("<Gardien>").empty());
  // UN LIEN D'UN AUTRE ÉCRIVAIN peut porter la partie cachée : le libellé
  // ne la montre pas.
  const ro::vfs::MapDisplayNames names = Names();
  CHECK(NaviSearchLinkLabel({.family = NaviSearchFamily::kNpc,
                             .map = "prontera",
                             .term = "Kafra Employee#prt_kafra1"},
                            &names) == "[PNJ: Kafra Employee (Prontera)]");
  // ⚠ UN MONSTRE garde son nom entier : le dièse n'y cache rien.
  CHECK(NaviSearchLinkLabel({.family = NaviSearchFamily::kMonster, .map = {}, .term = "A#B"},
                            &names) == "[Monstre: A#B]");
}

TEST_CASE("Lien de recherche : un libellé trop long se coupe entre deux caractères",
          "[app][chat][navi]") {
  // Cent vingt « é » : deux octets chacun, et la coupe du tampon tombe au
  // milieu de l'un d'eux.
  constexpr int kAccentedLetters = 120;
  std::string term;
  for (int letter = 0; letter < kAccentedLetters; ++letter) {
    term += "é";
  }
  const std::string label =
      NaviSearchLinkLabel({.family = NaviSearchFamily::kMonster, .map = {}, .term = term}, nullptr);
  REQUIRE_FALSE(label.empty());
  CHECK(label.size() < term.size());
  CHECK(ro::core::DanglingUtf8TailLength(label) == 0);
  CHECK(label.ends_with("é"));
}

TEST_CASE("Lien de recherche : ce que son menu sait d'avance", "[app][chat][navi]") {
  const auto free =
      NaviSearchFactsOf({.family = NaviSearchFamily::kMonster, .map = {}, .term = "X"});
  CHECK(free.family == 2);
  CHECK_FALSE(free.bounded);
  const auto bounded =
      NaviSearchFactsOf({.family = NaviSearchFamily::kNpc, .map = "prontera", .term = "X"});
  CHECK(bounded.family == 1);
  CHECK(bounded.bounded);
}
