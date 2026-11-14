#include "chat/chat_setting_link.h"

#include <cstddef>
#include <initializer_list>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/i18n.h"
#include "ro/ui/chat_link.h"
#include "ro/vfs/item_link.h"
#include "ro/vfs/item_table.h"
#include "ro/vfs/item_words.h"

#include "chat/chat_item_link.h"
#include "chat/chat_rich_decor.h"
#include "options/setting_links.h"

using moonclient::BuildSettingLink;
using moonclient::ChatDecorTables;
using moonclient::DecorateBubbleLine;
using moonclient::DecorateChatLine;
using moonclient::DecoratedChatLine;
using moonclient::ParseSettingLink;
using moonclient::SettingLink;
using moonclient::SettingLinkRead;
using moonclient::SettingReach;
using ro::ui::ChatLinkKind;

namespace {

[[nodiscard]] ro::vfs::ItemTable SmallTable() {
  ro::vfs::ItemTable table;
  table.Put(501, ro::vfs::ItemEntry{.name = "Red Potion"});
  return table;
}

// La ligne décorée, vue par un lecteur qui a ce que `reach` dit.
[[nodiscard]] DecoratedChatLine Decorated(const std::string& raw, const SettingReach& reach) {
  ro::core::UseFrench();
  DecoratedChatLine out;
  REQUIRE(DecorateChatLine(raw, SmallTable(), ro::vfs::ItemWords{}, &out, nullptr, nullptr, nullptr,
                           &reach));
  return out;
}

// Le texte du morceau cliquable `rank`.
[[nodiscard]] std::string Shown(const DecoratedChatLine& line, std::size_t rank) {
  return line.text.substr(line.links[rank].offset, line.links[rank].length);
}

}  // namespace

TEST_CASE("Lien de réglage : la balise s'écrit et se relit", "[app][lien]") {
  const std::string wire =
      BuildSettingLink(SettingLink{.key = "sheet_titles", .fallback = "Personnage › Titres"});
  CHECK(wire == "<SETL>sheet_titles:Personnage › Titres</SETL>");
  SettingLink back;
  std::size_t after = 0;
  REQUIRE(ParseSettingLink(wire, &back, &after) == SettingLinkRead::kRead);
  CHECK(back.key == "sheet_titles");
  CHECK(back.fallback == "Personnage › Titres");
  CHECK(after == wire.size());
  // ⚠ LE LIBELLÉ PEUT PORTER UN `:` : la clé s'arrête au premier.
  REQUIRE(ParseSettingLink("<SETL>chat:Réglages : chat</SETL>", &back, nullptr) ==
          SettingLinkRead::kRead);
  CHECK(back.key == "chat");
  CHECK(back.fallback == "Réglages : chat");
}

TEST_CASE("Lien de réglage : une clé qui couperait la balise ne part pas", "[app][lien]") {
  CHECK(BuildSettingLink(SettingLink{.key = "a:b", .fallback = "x"}).empty());
  CHECK(BuildSettingLink(SettingLink{.key = "a<b", .fallback = "x"}).empty());
  CHECK(BuildSettingLink(SettingLink{.key = "", .fallback = "x"}).empty());
  CHECK(BuildSettingLink(SettingLink{.key = "chat", .fallback = "x</SETL>y"}).empty());
  CHECK(BuildSettingLink(SettingLink{.key = "chat", .fallback = "x<SETL>y"}).empty());
}

TEST_CASE("Lien de réglage : le lien posé se relit comme le lien reçu", "[app][lien]") {
  ro::core::UseFrench();
  const moonclient::SettingDestination* const colors =
      moonclient::FindSettingDestination("damage_colors");
  REQUIRE(colors != nullptr);
  const moonclient::PendingChatLink posed = moonclient::SettingPendingLink(*colors);
  CHECK(posed.kind == ChatLinkKind::kSetting);
  CHECK(posed.shown == "[Réglage: Dégâts › Couleurs]");
  CHECK(posed.wire == "<SETL>damage_colors:Dégâts › Couleurs</SETL>");
  // 🔴 CE QUE LE JOUEUR VOIT AVANT L'ENVOI EST CE QUE TOUS LIRONT APRÈS.
  const DecoratedChatLine line = Decorated(posed.wire, SettingReach{});
  REQUIRE(line.links.size() == 1);
  CHECK(Shown(line, 0) == posed.shown);
}

TEST_CASE("Lien de réglage : une clé connue devient un lien local", "[app][lien]") {
  // ⚠ LE LIBELLÉ TRANSPORTÉ NE COMPTE PAS : c'est le nôtre qui s'affiche.
  const DecoratedChatLine line =
      Decorated("va voir <SETL>chat:Chat settings</SETL> !", SettingReach{});
  CHECK(line.text == "va voir [Réglage: Conversation] !");
  REQUIRE(line.links.size() == 1);
  CHECK(line.links[0].kind == ChatLinkKind::kSetting);
  CHECK(Shown(line, 0) == "[Réglage: Conversation]");
  REQUIRE(line.carried.size() == 1);
  CHECK(line.carried[0].kind == ChatLinkKind::kSetting);
  CHECK(line.carried[0].setting_key == "chat");
}

TEST_CASE("Lien de réglage : une clé inconnue reste du texte inerte", "[app][lien]") {
  const DecoratedChatLine line =
      Decorated("<SETL>staff_tools:Outils du staff</SETL> ici", SettingReach{});
  CHECK(line.text == "[Outils du staff] ici");
  CHECK(line.links.empty());
}

TEST_CASE("Lien de réglage : une destination absente reste du texte inerte", "[app][lien]") {
  const std::string raw = "<SETL>sheet_homunculus:Personnage › Homoncule</SETL>";
  const DecoratedChatLine without = Decorated(raw, SettingReach{});
  CHECK(without.text == "[Personnage › Homoncule]");
  CHECK(without.links.empty());
  // 🔴 LA CLÉ EST CONNUE : le nom est le NÔTRE, et non le repli de l'expéditeur.
  const DecoratedChatLine foreign =
      Decorated("<SETL>sheet_mercenary:Character > Mercenary</SETL>", SettingReach{});
  CHECK(foreign.text == "[Personnage › Mercenaire]");
  CHECK(foreign.links.empty());
  // Le même lien, chez qui a un homoncule.
  const DecoratedChatLine with = Decorated(raw, SettingReach{.homunculus = true});
  REQUIRE(with.links.size() == 1);
  CHECK(Shown(with, 0) == "[Fenêtre: Personnage › Homoncule]");
}

TEST_CASE("Lien de réglage : une balise malformée reste brute", "[app][lien]") {
  // Sans `:`, rien à couper : la balise reste telle quelle.
  const DecoratedChatLine no_colon = Decorated("a <SETL>chat</SETL> b", SettingReach{});
  CHECK(no_colon.text == "a <SETL>chat</SETL> b");
  CHECK(no_colon.links.empty());
  // Sans fermante, l'ouvrante reste et ce qui suit se lit encore.
  const DecoratedChatLine unclosed =
      Decorated("<SETL>chat:Chat <SETL>skin:Skin</SETL>", SettingReach{});
  CHECK(unclosed.text == "<SETL>chat:Chat [Réglage: Skin]");
  REQUIRE(unclosed.links.size() == 1);
  CHECK(unclosed.carried[0].setting_key == "skin");
}

TEST_CASE("Lien de réglage : plusieurs liens, et un objet à côté", "[app][lien]") {
  ro::vfs::ItemLink potion;
  potion.item_id = 501;
  const std::string raw = "<SETL>chat:Chat</SETL> et " + ro::vfs::BuildItemLink(potion, 0) +
                          " puis <SETL>sheet_titles:Titres</SETL>";
  const DecoratedChatLine line = Decorated(raw, SettingReach{});
  CHECK(line.text == "[Réglage: Conversation] et <Red Potion> puis [Fenêtre: Personnage › Titres]");
  REQUIRE(line.links.size() == 3);
  CHECK(line.links[0].kind == ChatLinkKind::kSetting);
  CHECK(line.links[1].kind == ChatLinkKind::kItem);
  CHECK(line.links[2].kind == ChatLinkKind::kSetting);
  REQUIRE(line.carried.size() == 3);
  CHECK(line.carried[0].setting_key == "chat");
  CHECK(line.carried[2].setting_key == "sheet_titles");
  CHECK(Shown(line, 1) == "Red Potion");
}

TEST_CASE("Lien de réglage : une bulle le montre comme le fil, selon le lecteur", "[app][lien]") {
  ro::core::UseFrench();
  const ro::vfs::ItemTable items = SmallTable();
  const ro::vfs::ItemWords words;
  // La bulle, vue par un lecteur qui a ce que `reach` dit.
  const auto bubble_of = [&items, &words](const std::string& raw, const SettingReach& reach) {
    DecoratedChatLine bubble;
    REQUIRE(DecorateBubbleLine(
        raw, ChatDecorTables{.items = &items, .words = &words, .settings = &reach}, {}, &bubble));
    return bubble;
  };
  const std::string raw = "va voir <SETL>sheet_homunculus:Personnage › Homoncule</SETL>";
  for (const SettingReach& reach : {SettingReach{}, SettingReach{.homunculus = true}}) {
    const DecoratedChatLine bubble = bubble_of(raw, reach);
    const DecoratedChatLine thread = Decorated(raw, reach);
    CHECK(bubble.text == thread.text);
    CHECK(bubble.links.size() == thread.links.size());
  }
  // Chez qui a un homoncule, c'est un lien ; sans, du texte inerte.
  const DecoratedChatLine linked = bubble_of(raw, SettingReach{.homunculus = true});
  REQUIRE(linked.links.size() == 1);
  CHECK(linked.links[0].kind == ChatLinkKind::kSetting);
  CHECK(Shown(linked, 0) == "[Fenêtre: Personnage › Homoncule]");
  const DecoratedChatLine inert = bubble_of(raw, SettingReach{});
  CHECK(inert.text == "va voir [Personnage › Homoncule]");
  CHECK(inert.links.empty());
}
