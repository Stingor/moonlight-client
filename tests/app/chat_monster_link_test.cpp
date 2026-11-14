// LE LIEN D'UN MONSTRE : `<MOBL>id:rang:nom</MOBL>`.
//
// 🔴 LE NOM EST LE DERNIER CHAMP, et c'est ce qui le laisse porter des « : » —
// un nom de monstre peut en avoir, et le couper au premier ferait perdre sa fin.

#include "chat/chat_monster_link.h"

#include <string>

#include <catch2/catch_test_macros.hpp>

#include "ro/vfs/item_table.h"
#include "ro/vfs/item_words.h"

#include "chat/chat_item_link.h"

using moonclient::BuildMonsterLink;
using moonclient::MonsterLink;
using moonclient::ParseMonsterLink;

namespace {

constexpr std::uint32_t kBaphomet = 1039;
constexpr std::uint32_t kPoring = 1002;

}  // namespace

TEST_CASE("Lien de monstre : la balise fait l'aller-retour", "[app][chat][monster_sheet]") {
  const MonsterLink link{
      .monster_id = kBaphomet, .rank = moonclient::kMonsterRankMvp, .name = "Baphomet: le vrai"};
  const std::string wire = BuildMonsterLink(link);
  CHECK(wire == "<MOBL>1039:2:Baphomet: le vrai</MOBL>");
  MonsterLink read;
  std::size_t after = 0;
  REQUIRE(ParseMonsterLink(wire + " suite", &read, &after));
  CHECK(read.monster_id == kBaphomet);
  CHECK(read.rank == moonclient::kMonsterRankMvp);
  CHECK(read.name == "Baphomet: le vrai");
  CHECK(after == wire.size());
  CHECK(moonclient::MonsterLinkLabel(read) == "<[MVP] Baphomet: le vrai>");
}

TEST_CASE("Lien de monstre : ce qui ne se relirait pas ne se compose pas",
          "[app][chat][monster_sheet]") {
  CHECK(BuildMonsterLink(MonsterLink{.monster_id = 0, .name = "Poring"}).empty());
  CHECK(BuildMonsterLink(MonsterLink{.monster_id = kPoring}).empty());
  // La fermante dans le nom couperait la balise en deux à la relecture.
  CHECK(BuildMonsterLink(MonsterLink{.monster_id = kPoring, .name = "a</MOBL>b"}).empty());
}

TEST_CASE("Lien de monstre : un rang inconnu est ordinaire, une balise coupée ne se lit pas",
          "[app][chat][monster_sheet]") {
  MonsterLink read;
  REQUIRE(ParseMonsterLink("<MOBL>1002:9:Poring</MOBL>", &read, nullptr));
  CHECK(read.rank == moonclient::kMonsterRankOrdinary);
  CHECK(moonclient::MonsterLinkLabel(read) == "<[Mob] Poring>");

  std::size_t after = 0;
  CHECK_FALSE(ParseMonsterLink("<MOBL>1002:0:Por", &read, &after));
  CHECK(after == std::string("<MOBL>1002:0:Por").size());
  CHECK_FALSE(ParseMonsterLink("<MOBL>1002:Poring</MOBL>", &read, nullptr));
  CHECK_FALSE(ParseMonsterLink("<MOBL>0:0:Poring</MOBL>", &read, nullptr));
}

TEST_CASE("Lien de monstre : le fil le montre, cliquable, entre ses chevrons",
          "[app][chat][monster_sheet]") {
  const ro::vfs::ItemTable items;
  const ro::vfs::ItemWords words;
  moonclient::DecoratedChatLine line;
  REQUIRE(moonclient::DecorateChatLine("Vu : <MOBL>1039:1:Baphomet</MOBL> !", items, words, &line));
  CHECK(line.text == "Vu : <[Boss] Baphomet> !");
  REQUIRE(line.links.size() == 1);
  CHECK(line.links[0].kind == ro::ui::ChatLinkKind::kMonster);
  CHECK(line.links[0].item_id == kBaphomet);
  CHECK(line.text.substr(line.links[0].offset, line.links[0].length) == "[Boss] Baphomet");
  REQUIRE(line.carried.size() == 1);
  CHECK(line.carried[0].monster.name == "Baphomet");
}
