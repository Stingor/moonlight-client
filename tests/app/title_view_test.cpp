// LES TITRES QU'ON POSSÈDE : le croisement des succès réclamés avec les deux
// tables, et l'ordre de la liste.
//
// 🔴 DEUX PANNES MUETTES EN JEU tant qu'on n'a qu'un titre : un doublon quand
// deux succès récompensent le même, et un ordre qui ne serait pas celui du
// client — rangé par le libellé de la TABLE, octet par octet.

#include "character/title_view.h"

#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/title_table.h"
#include "ro/sim/title_book.h"
#include "ro/vfs/achievement_database.h"
#include "ro/vfs/achievement_titles.h"

using moonclient::NamedTitle;
using moonclient::OwnedTitles;
using moonclient::ShownTitle;
using moonclient::TitleView;

namespace {

constexpr std::string_view kRewards = R"(
version: 1
achievements: {
  1: {name: a, score: 0, targets: [], reward: {item: 0, title: 1103, script: false}, dependents: []},
  2: {name: b, score: 0, targets: [], reward: {item: 0, title: 1000, script: false}, dependents: []},
  3: {name: c, score: 0, targets: [], reward: {item: 0, title: 1103, script: false}, dependents: []},
  4: {name: d, score: 0, targets: [], reward: {item: 501, title: 0, script: false}, dependents: []},
  5: {name: e, score: 0, targets: [], reward: {item: 0, title: 1999, script: false}, dependents: []},
  6: {name: f, score: 0, targets: [], reward: {item: 0, title: 1101, script: false}, dependents: []},
}
)";

constexpr std::string_view kLabels = R"(
version: 1
titles:
  1000: Junction of life
  1101: Archiviste
  1103: Conservateur
)";

[[nodiscard]] ro::vfs::AchievementTitles Rewards() {
  auto database = ro::vfs::AchievementDatabase::Parse(kRewards);
  REQUIRE(database.has_value());
  return ro::vfs::AchievementTitles::Of(*database);
}

[[nodiscard]] ro::render::TitleTable Labels() {
  auto table = ro::render::TitleTable::Parse(kLabels);
  REQUIRE(table.has_value());
  return std::move(*table);
}

}  // namespace

TEST_CASE("Titres : deux succès du même titre n'en font qu'un", "[app][titres]") {
  const ro::vfs::AchievementTitles rewards = Rewards();
  const ro::render::TitleTable labels = Labels();
  const std::vector<std::uint32_t> rewarded = {1, 3};
  const std::vector<NamedTitle> owned = OwnedTitles(rewarded, &rewards, &labels);
  REQUIRE(owned.size() == 1);
  CHECK(owned[0].title_id == 1103);
  CHECK(owned[0].label == "Conservateur");
}

TEST_CASE("Titres : un succès sans titre ne donne rien", "[app][titres]") {
  const ro::vfs::AchievementTitles rewards = Rewards();
  const std::vector<std::uint32_t> rewarded = {4, 99};
  CHECK(OwnedTitles(rewarded, &rewards, nullptr).empty());
}

TEST_CASE("Titres : la liste se range par le libellé de la table", "[app][titres]") {
  // ⚠ LE TITRE SANS LIBELLÉ PASSE EN TÊTE : c'est son libellé VIDE qui range, et
  // pas le numéro qu'on affiche à sa place.
  const ro::vfs::AchievementTitles rewards = Rewards();
  const ro::render::TitleTable labels = Labels();
  const std::vector<std::uint32_t> rewarded = {2, 1, 6, 5};
  const std::vector<NamedTitle> owned = OwnedTitles(rewarded, &rewards, &labels);
  REQUIRE(owned.size() == 4);
  CHECK(owned[0].title_id == 1999);
  CHECK(owned[1].title_id == 1101);
  CHECK(owned[2].title_id == 1103);
  CHECK(owned[3].title_id == 1000);
}

TEST_CASE("Titres : un titre que la table ne nomme pas s'écrit par son numéro", "[app][titres]") {
  const ro::render::TitleTable labels = Labels();
  CHECK(ShownTitle(1999, &labels) == "Titre #1999");
  CHECK(ShownTitle(1000, &labels) == "Junction of life");
  CHECK(ShownTitle(1000, nullptr) == "Titre #1000");
}

TEST_CASE("Titres : sans la table des succès, on ne possède rien", "[app][titres]") {
  const std::vector<std::uint32_t> rewarded = {1, 2};
  CHECK(OwnedTitles(rewarded, nullptr, nullptr).empty());
}

TEST_CASE("Titres : la vue nomme le titre porté et pointe sur ses libellés", "[app][titres]") {
  const ro::vfs::AchievementTitles rewards = Rewards();
  const ro::render::TitleTable labels = Labels();
  ro::sim::TitleBook book;
  static_cast<void>(book.NoteRewarded(1));
  static_cast<void>(book.NoteRewarded(6));
  book.TellEquipped(1103);

  TitleView view(&rewards, &labels);
  view.Refresh(book);
  CHECK(view.view().equipped == 1103);
  CHECK(view.view().equipped_label == "Conservateur");
  REQUIRE(view.view().owned.size() == 2);
  CHECK(view.view().owned[0].label == "Archiviste");
  CHECK(view.view().owned[1].label == "Conservateur");

  // ⚠ SANS TITRE PORTÉ, PAS DE LIBELLÉ : la fenêtre écrit alors « aucun ».
  book.TellEquipped(0);
  view.Refresh(book);
  CHECK(view.view().equipped == 0);
  CHECK(view.view().equipped_label.empty());
}
