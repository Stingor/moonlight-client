// L'onglet du compteur de dégâts : rangé parmi ceux de la conversation, il en
// sort pour redevenir sa fenêtre.

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "chat/chat_tab_file.h"
#include "chat/chat_tabs.h"
#include "support/temp_directory.h"

namespace {

using moonclient::ChatTab;
using moonclient::ChatTabContent;
using moonclient::ChatTabs;

[[nodiscard]] bool IsMeter(const ChatTab& tab) {
  return tab.content == ChatTabContent::kDamageMeter;
}

}  // namespace

TEST_CASE("Onglet compteur : il se range à la place visée, une seule fois", "[app][onglets]") {
  ChatTabs tabs;
  tabs.Add("Guilde");
  REQUIRE_FALSE(tabs.MeterRank().has_value());

  // Devant « Guilde », dans la principale.
  tabs.DockMeter("DPS", 0, 1);
  REQUIRE(tabs.MeterRank() == std::optional<std::size_t>(1));
  CHECK(tabs.count() == 3);
  CHECK(tabs.tabs()[1].accepted == 0);

  // ⚠ LE RANGER DE NOUVEAU LE DÉPLACE, il n'en fait pas un second.
  tabs.DockMeter("DPS", 0, tabs.count());
  CHECK(tabs.count() == 3);
  CHECK(tabs.MeterRank() == std::optional<std::size_t>(2));
}

TEST_CASE("Onglet compteur : il prend le verrou de sa fenêtre", "[app][onglets]") {
  ChatTabs tabs;
  tabs.SetGroupLocked(0, true);
  tabs.DockMeter("DPS", 0, tabs.count());
  REQUIRE(tabs.MeterRank().has_value());
  CHECK(tabs.tabs()[*tabs.MeterRank()].locked);
}

TEST_CASE("Onglet compteur : une fenêtre disparue se lit comme la principale", "[app][onglets]") {
  ChatTabs tabs;
  tabs.DockMeter("DPS", 42, tabs.count());
  REQUIRE(tabs.MeterRank().has_value());
  CHECK(tabs.tabs()[*tabs.MeterRank()].group == 0);
}

TEST_CASE("Onglet compteur : il ne compte pas comme un fil", "[app][onglets]") {
  ChatTabs tabs;
  tabs.DockMeter("DPS", 0, tabs.count());
  // Le seul fil ne se retire pas, même avec le compteur à côté : il ne
  // resterait rien à lire.
  tabs.Remove(0);
  CHECK(tabs.count() == 2);
  // Le compteur, lui, se retire.
  tabs.Remove(*tabs.MeterRank());
  CHECK(tabs.count() == 1);
  CHECK_FALSE(tabs.MeterRank().has_value());
}

TEST_CASE("Onglet compteur : seul dans une flottante, il la quitte", "[app][onglets]") {
  ChatTabs tabs;
  tabs.Add("Groupe");
  tabs.DockMeter("DPS", 0, tabs.count());
  CHECK_FALSE(tabs.lone_meter());
  // Sorti dans une fenêtre à lui : rien d'autre ne la justifie.
  const std::uint32_t floating = tabs.Detach(*tabs.MeterRank());
  REQUIRE(floating != 0);
  CHECK(tabs.lone_meter());
  // Rejoint par un fil, il y reste.
  tabs.Attach(1, floating);
  CHECK_FALSE(tabs.lone_meter());
  tabs.UndockMeter();
  CHECK_FALSE(tabs.MeterRank().has_value());
  CHECK(tabs.count() == 2);
}

TEST_CASE("Onglet compteur : il se relit tel qu'on l'a rangé", "[app][onglets]") {
  const ro::test_support::TempDirectory folder;
  ChatTabs written;
  written.Add("Guilde");
  written.DockMeter("DPS", 0, 1);
  REQUIRE(moonclient::SaveChatTabs(folder.path(), written));

  ChatTabs read;
  moonclient::LoadChatTabs(folder.path(), read);
  REQUIRE(read.count() == 3);
  CHECK_FALSE(IsMeter(read.tabs()[0]));
  CHECK(IsMeter(read.tabs()[1]));
  CHECK(read.tabs()[1].accepted == 0);
  CHECK_FALSE(IsMeter(read.tabs()[2]));
}

TEST_CASE("Onglet compteur : une relecture sans fil, ou à deux compteurs, se corrige",
          "[app][onglets]") {
  ChatTabs tabs;
  // ⚠ SANS AUCUN FIL, la liste est refusée : il ne resterait rien à lire.
  tabs.Replace({ChatTab{.name = "DPS", .content = ChatTabContent::kDamageMeter}});
  REQUIRE(tabs.count() == 1);
  CHECK_FALSE(IsMeter(tabs.tabs()[0]));

  // Deux compteurs : le premier rangé l'emporte.
  std::vector<ChatTab> doubled;
  doubled.push_back(ChatTab{.name = "Tout", .accepted = moonclient::EveryChatType()});
  doubled.push_back(ChatTab{.name = "DPS", .content = ChatTabContent::kDamageMeter, .group = 0});
  doubled.push_back(ChatTab{.name = "DPS bis", .content = ChatTabContent::kDamageMeter});
  tabs.Replace(std::move(doubled));
  REQUIRE(tabs.count() == 2);
  CHECK(tabs.tabs()[1].name == "DPS");
}

TEST_CASE("Onglet compteur : le profil de disposition le sort, puis le remet à sa place",
          "[app][onglets][disposition]") {
  ChatTabs tabs;
  tabs.Add("Guilde");
  tabs.DockMeter("DPS", 0, 1);
  REQUIRE(tabs.MeterRank() == std::optional<std::size_t>(1));

  // Un profil posé le veut flottant : il sort.
  CHECK_FALSE(tabs.FollowMeterDock(/*wanted=*/false, /*seen=*/true, "DPS"));
  CHECK_FALSE(tabs.MeterRank().has_value());
  CHECK(tabs.count() == 2);

  // Un autre le veut rangé : il revient entre les deux fils, là où il était.
  CHECK(tabs.FollowMeterDock(/*wanted=*/true, /*seen=*/false, "DPS"));
  CHECK(tabs.MeterRank() == std::optional<std::size_t>(1));
}

TEST_CASE("Onglet compteur : jamais rangé, le profil le range au bout de la principale",
          "[app][onglets][disposition]") {
  ChatTabs tabs;
  tabs.Add("Guilde");
  CHECK(tabs.FollowMeterDock(/*wanted=*/true, /*seen=*/false, "DPS"));
  REQUIRE(tabs.MeterRank() == std::optional<std::size_t>(2));
  CHECK(tabs.tabs()[2].group == 0);
}

TEST_CASE("Onglet compteur : sans profil posé, les gestes du joueur ont la main",
          "[app][onglets][disposition]") {
  // L'interrupteur n'a pas changé depuis l'image d'avant : c'est lui qui
  // recopie les onglets, et non l'inverse.
  ChatTabs tabs;
  tabs.DockMeter("DPS", 0, tabs.count());
  CHECK(tabs.FollowMeterDock(/*wanted=*/false, /*seen=*/false, "DPS"));
  CHECK(tabs.MeterRank().has_value());

  tabs.UndockMeter();
  CHECK_FALSE(tabs.FollowMeterDock(/*wanted=*/true, /*seen=*/true, "DPS"));
  CHECK_FALSE(tabs.MeterRank().has_value());
}

TEST_CASE("Onglet compteur : le couper ne le sort plus du fichier commun",
          "[app][onglets][disposition]") {
  // Éteint dans les réglages d'un personnage, le compteur se cache : le profil
  // de disposition, inchangé, le garde rangé, et le fichier des onglets — que
  // tous les personnages lisent — le porte toujours.
  const ro::test_support::TempDirectory folder;
  ChatTabs tabs;
  tabs.Add("Guilde");
  tabs.DockMeter("DPS", 0, 1);
  CHECK(tabs.FollowMeterDock(/*wanted=*/true, /*seen=*/true, "DPS"));
  REQUIRE(moonclient::SaveChatTabs(folder.path(), tabs));

  ChatTabs read;
  moonclient::LoadChatTabs(folder.path(), read);
  REQUIRE(read.MeterRank().has_value());
  CHECK(*read.MeterRank() == 1);
}
