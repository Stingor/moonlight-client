// CE QUE DEMANDENT LES GESTES QUI CHERCHENT : les commandes d'un nom de
// monstre, sa recherche « où le trouver », et la carte où mène « Y aller ».
//
// 🔴 TOUT NOM DE MONSTRE PASSE PAR LÀ, quelle que soit la fenêtre qui l'offre :
// une commande qui partirait avec un autre identifiant, ou une recherche bornée
// par erreur, se répéterait partout.

#include "chat/chat_search_gestures.h"

#include <cstdint>
#include <string>

#include <catch2/catch_test_macros.hpp>

using moonclient::MonsterCommandLine;
using moonclient::MonsterDatabaseId;
using moonclient::MonsterLink;
using moonclient::NaviSearchDestination;
using moonclient::NaviSearchFamily;
using moonclient::NaviSearchLink;
using moonclient::WhereToFindSearch;
using ro::ui::ChatLinkGesture;

namespace {

constexpr std::uint32_t kBaphomet = 1039;

}  // namespace

TEST_CASE("Gestes de monstre : les deux commandes du serveur", "[app][chat][navi]") {
  CHECK(MonsterCommandLine(ChatLinkGesture::kMobInfo, kBaphomet) == "@mobinfo 1039");
  CHECK(MonsterCommandLine(ChatLinkGesture::kWhereIs, kBaphomet) == "@whereis 1039");
  // AUCUN AUTRE GESTE n'envoie de commande, et rien sans identifiant.
  CHECK(MonsterCommandLine(ChatLinkGesture::kDescribe, kBaphomet).empty());
  CHECK(MonsterCommandLine(ChatLinkGesture::kWhereToFind, kBaphomet).empty());
  CHECK(MonsterCommandLine(ChatLinkGesture::kMobInfo, 0).empty());
}

TEST_CASE("Gestes de monstre : une classe de sprite ne vaut pas un identifiant de la base",
          "[app][chat][navi]") {
  const MonsterLink monster{.monster_id = kBaphomet, .name = "Baphomet"};
  CHECK(MonsterDatabaseId(monster, /*by_view=*/false) == kBaphomet);
  // 🔴 LA NAVIGATION ET LES ACTEURS NE PORTENT QU'UNE CLASSE : un MVP maison au
  // sprite de Baphomet ne doit pas faire demander `@mobinfo 1039`.
  CHECK(MonsterDatabaseId(monster, /*by_view=*/true) == 0);
  CHECK(MonsterCommandLine(ChatLinkGesture::kMobInfo, MonsterDatabaseId(monster, true)).empty());
  CHECK(MonsterCommandLine(ChatLinkGesture::kWhereIs, MonsterDatabaseId(monster, true)).empty());
}

TEST_CASE("Gestes de monstre : « où le trouver » cherche le nom, partout", "[app][chat][navi]") {
  const NaviSearchLink search =
      WhereToFindSearch(MonsterLink{.monster_id = kBaphomet, .rank = 2, .name = "Baphomet"});
  CHECK(search.family == NaviSearchFamily::kMonster);
  // ⚠ SANS CARTE : on veut tous ses lieux d'apparition.
  CHECK(search.map.empty());
  CHECK(search.term == "Baphomet");
  // LA BALISE QUI PART : sans contexte, comme chez les autres écrivains.
  CHECK(moonclient::BuildNaviSearchLink(search) == "<NAVS>2::Baphomet</NAVS>");
  CHECK(WhereToFindSearch(MonsterLink{.monster_id = kBaphomet}).term.empty());
}

TEST_CASE("Recherche : « Y aller » mène au terme d'une carte, au contexte du reste",
          "[app][chat][navi]") {
  CHECK(NaviSearchDestination({.family = NaviSearchFamily::kMap, .map = {}, .term = "prontera"}) ==
        "prontera");
  CHECK(NaviSearchDestination(
            {.family = NaviSearchFamily::kNpc, .map = "payon", .term = "Kafra Employee"}) ==
        "payon");
  CHECK(NaviSearchDestination({.family = NaviSearchFamily::kMonster, .map = {}, .term = "Poring"})
            .empty());
}
