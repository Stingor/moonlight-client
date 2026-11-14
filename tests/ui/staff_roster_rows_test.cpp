// La liste des joueurs en ligne du staff, sans son dessin : ce qu'elle dit
// d'une durée et d'une ligne, ce qu'elle retient, et dans quel ordre.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/staff_roster_window.h"

using ro::ui::FreshestActivity;
using ro::ui::OrderedRows;
using ro::ui::RowIsFlagged;
using ro::ui::RowIsIdle;
using ro::ui::RowMatches;
using ro::ui::StaffRosterActivity;
using ro::ui::StaffRosterClientKind;
using ro::ui::StaffRosterColumn;
using ro::ui::StaffRosterDurationText;
using ro::ui::StaffRosterFilters;
using ro::ui::StaffRosterRow;

namespace {

// Aucun filtre : la case des échoppes hors ligne, cochée d'usine, est levée.
constexpr StaffRosterFilters kNoFilter{.hide_offline_shops = false};

[[nodiscard]] std::size_t RankOf(StaffRosterActivity activity) {
  return static_cast<std::size_t>(activity);
}

// Un joueur sans histoire, tenu par un moonclient.
[[nodiscard]] StaffRosterRow Row(std::string name, std::uint32_t account_id) {
  StaffRosterRow row;
  row.name = std::move(name);
  row.account_id = account_id;
  row.client = StaffRosterClientKind::kMoonclient;
  return row;
}

[[nodiscard]] std::vector<StaffRosterRow> Town() {
  StaffRosterRow aldric = Row("Aldric", 2000001);
  aldric.map = "prontera";
  aldric.guild = "Lune Rousse";
  aldric.party = "Veilleurs";
  aldric.job_name = "Rune Knight";
  aldric.base_level = 99;
  aldric.online_s = 10920;
  aldric.idle_s = 12;
  aldric.staff = true;

  StaffRosterRow mirelle = Row("Mirelle", 2000002);
  mirelle.map = "gl_cas02";
  mirelle.instance_id = 4;
  mirelle.guild = "Lune Rousse";
  mirelle.job_name = "High Priest";
  mirelle.base_level = 98;
  mirelle.online_s = 6420;
  mirelle.idle_s = 3;

  StaffRosterRow ysolde = Row("ysolde", 2000003);
  ysolde.map = "prontera";
  ysolde.job_name = "High Wizard";
  ysolde.base_level = 96;
  ysolde.online_s = 2520;
  ysolde.idle_s = 1080;
  ysolde.machine_group = 1;

  StaffRosterRow shop = Row("Petit Lot", 2000004);
  shop.map = "prontera";
  shop.job_name = "Whitesmith";
  shop.base_level = 91;
  shop.online_s = 33060;
  shop.idle_s = 30000;
  shop.offline_shop = true;
  shop.vending = true;
  shop.machine_group = 1;
  return {aldric, mirelle, ysolde, shop};
}

[[nodiscard]] std::vector<std::string_view> NamesOf(const std::vector<StaffRosterRow>& rows,
                                                    const std::vector<std::size_t>& order) {
  std::vector<std::string_view> names;
  for (const std::size_t rank : order) {
    names.emplace_back(rows[rank].name);
  }
  return names;
}

}  // namespace

TEST_CASE("Joueurs en ligne : une durée prend l'unité de sa grandeur", "[ui][staff_roster]") {
  CHECK(StaffRosterDurationText(0) == "0 s");
  CHECK(StaffRosterDurationText(59) == "59 s");
  CHECK(StaffRosterDurationText(60) == "1 min");
  CHECK(StaffRosterDurationText(3599) == "59 min");
  CHECK(StaffRosterDurationText(3600) == "1 h 00");
  CHECK(StaffRosterDurationText(10920) == "3 h 02");
  CHECK(StaffRosterDurationText(86399) == "23 h 59");
  CHECK(StaffRosterDurationText(86400) == "1 j 00 h");
  CHECK(StaffRosterDurationText(187200) == "2 j 04 h");
}

TEST_CASE("Joueurs en ligne : la dernière action est la plus récente des genres",
          "[ui][staff_roster]") {
  StaffRosterRow row = Row("Tobias", 2000005);
  CHECK_FALSE(FreshestActivity(row).has_value());
  row.action_age_s[RankOf(StaffRosterActivity::kTalk)] = 21600;
  CHECK(FreshestActivity(row) == StaffRosterActivity::kTalk);
  row.action_age_s[RankOf(StaffRosterActivity::kFight)] = 1;
  row.action_age_s[RankOf(StaffRosterActivity::kWalk)] = 30;
  CHECK(FreshestActivity(row) == StaffRosterActivity::kFight);
  // À âge égal, le premier genre dans l'ordre du fil.
  row.action_age_s[RankOf(StaffRosterActivity::kWalk)] = 1;
  CHECK(FreshestActivity(row) == StaffRosterActivity::kWalk);
}

TEST_CASE("Joueurs en ligne : inactif au seuil, jamais pour une échoppe hors ligne",
          "[ui][staff_roster]") {
  StaffRosterRow row = Row("Ysolde", 2000003);
  row.idle_s = ro::ui::kStaffRosterIdleSeconds - 1;
  CHECK_FALSE(RowIsIdle(row));
  row.idle_s = ro::ui::kStaffRosterIdleSeconds;
  CHECK(RowIsIdle(row));
  row.offline_shop = true;
  CHECK_FALSE(RowIsIdle(row));
}

TEST_CASE("Joueurs en ligne : chaque signal suffit à marquer une ligne", "[ui][staff_roster]") {
  CHECK_FALSE(RowIsFlagged(Row("Aldric", 2000001)));
  const auto flagged_with = [](void (*mark)(StaffRosterRow&)) {
    StaffRosterRow row = Row("Tobias", 2000005);
    mark(row);
    return RowIsFlagged(row);
  };
  CHECK(flagged_with([](StaffRosterRow& row) { row.cheat_clock = 1; }));
  CHECK(flagged_with([](StaffRosterRow& row) { row.cheat_cadence = 1; }));
  CHECK(flagged_with([](StaffRosterRow& row) { row.cheat_client = 1; }));
  CHECK(flagged_with([](StaffRosterRow& row) { row.mute_minutes = 34; }));
  CHECK(flagged_with([](StaffRosterRow& row) { row.jailed = true; }));
  CHECK(flagged_with([](StaffRosterRow& row) { row.tree_modified = true; }));
  CHECK(flagged_with([](StaffRosterRow& row) { row.client = StaffRosterClientKind::kNone; }));
  // Un client en retard n'accuse personne, pas plus que le client d'origine.
  CHECK_FALSE(flagged_with([](StaffRosterRow& row) { row.client_outdated = true; }));
  CHECK_FALSE(
      flagged_with([](StaffRosterRow& row) { row.client = StaffRosterClientKind::kOriginal; }));
}

TEST_CASE("Joueurs en ligne : la recherche lit le nom, la guilde, le groupe, la carte et le job",
          "[ui][staff_roster]") {
  const std::vector<StaffRosterRow> rows = Town();
  const StaffRosterRow& aldric = rows[0];
  CHECK(RowMatches(aldric, "", kNoFilter));
  CHECK(RowMatches(aldric, "ALD", kNoFilter));
  CHECK(RowMatches(aldric, "rousse", kNoFilter));
  CHECK(RowMatches(aldric, "veill", kNoFilter));
  CHECK(RowMatches(aldric, "pront", kNoFilter));
  CHECK(RowMatches(aldric, "knight", kNoFilter));
  CHECK_FALSE(RowMatches(aldric, "geffen", kNoFilter));
}

TEST_CASE("Joueurs en ligne : chaque filtre écarte ce qu'il nomme", "[ui][staff_roster]") {
  const std::vector<StaffRosterRow> rows = Town();
  const auto kept = [&rows](const StaffRosterFilters& filters) {
    return NamesOf(rows, OrderedRows(rows, "", filters, StaffRosterColumn::kPlayer, false));
  };
  CHECK(kept(kNoFilter) ==
        std::vector<std::string_view>{"Aldric", "Mirelle", "Petit Lot", "ysolde"});
  // D'usine, les échoppes hors ligne sont masquées.
  CHECK(kept(StaffRosterFilters{}) == std::vector<std::string_view>{"Aldric", "Mirelle", "ysolde"});
  CHECK(kept(StaffRosterFilters{.staff_only = true}) == std::vector<std::string_view>{"Aldric"});
  CHECK(kept(StaffRosterFilters{.hide_offline_shops = false, .idle_only = true}) ==
        std::vector<std::string_view>{"ysolde"});
  CHECK(kept(StaffRosterFilters{.hide_offline_shops = false, .shared_only = true}) ==
        std::vector<std::string_view>{"Petit Lot", "ysolde"});
  CHECK(kept(StaffRosterFilters{.hide_offline_shops = false, .flagged_only = true}).empty());
}

TEST_CASE("Joueurs en ligne : le tri suit la colonne, et le nom départage", "[ui][staff_roster]") {
  const std::vector<StaffRosterRow> rows = Town();
  const auto sorted = [&rows](StaffRosterColumn column, bool descending) {
    return NamesOf(rows, OrderedRows(rows, "", kNoFilter, column, descending));
  };
  using Names = std::vector<std::string_view>;
  // À la casse près : « ysolde » ne passe pas devant les capitales.
  CHECK(sorted(StaffRosterColumn::kPlayer, false) ==
        Names{"Aldric", "Mirelle", "Petit Lot", "ysolde"});
  CHECK(sorted(StaffRosterColumn::kPlayer, true) ==
        Names{"ysolde", "Petit Lot", "Mirelle", "Aldric"});
  CHECK(sorted(StaffRosterColumn::kOnline, true) ==
        Names{"Petit Lot", "Aldric", "Mirelle", "ysolde"});
  CHECK(sorted(StaffRosterColumn::kIdle, false) ==
        Names{"Mirelle", "Aldric", "ysolde", "Petit Lot"});
  CHECK(sorted(StaffRosterColumn::kLevel, true) ==
        Names{"Aldric", "Mirelle", "ysolde", "Petit Lot"});
  // Trois joueurs sur la même carte : le nom les range.
  CHECK(sorted(StaffRosterColumn::kMap, false) ==
        Names{"Mirelle", "Aldric", "Petit Lot", "ysolde"});
  // Sans guilde, après ceux qui en ont une.
  CHECK(sorted(StaffRosterColumn::kGuild, false) ==
        Names{"Aldric", "Mirelle", "Petit Lot", "ysolde"});
}

TEST_CASE("Joueurs en ligne : deux homonymes gardent leur ordre d'une image à l'autre",
          "[ui][staff_roster]") {
  const std::vector<StaffRosterRow> rows = {Row("Jumeau", 2000009), Row("Jumeau", 2000007)};
  for (const bool descending : {false, true}) {
    const std::vector<std::size_t> order =
        OrderedRows(rows, "", kNoFilter, StaffRosterColumn::kPlayer, descending);
    REQUIRE(order.size() == 2);
    CHECK(rows[order[0]].account_id == 2000007);
  }
}

TEST_CASE("Joueurs en ligne : les voisins de machine se nomment, sans la ligne elle-même",
          "[ui][staff_roster]") {
  std::vector<StaffRosterRow> rows = Town();
  CHECK(ro::ui::MachineMates(rows, 2) == std::vector<std::string_view>{"Petit Lot"});
  CHECK(ro::ui::MachineMates(rows, 3) == std::vector<std::string_view>{"ysolde"});
  // Un numéro nul n'est le groupe de personne.
  CHECK(ro::ui::MachineMates(rows, 0).empty());
  CHECK(ro::ui::AccountMates(rows, 2).empty());
  rows[0].account_group = 3;
  rows[1].account_group = 3;
  CHECK(ro::ui::AccountMates(rows, 0) == std::vector<std::string_view>{"Mirelle"});
}

TEST_CASE("Joueurs en ligne : la synthèse compte les joueurs, les échoppes et les cartes",
          "[ui][staff_roster]") {
  std::vector<StaffRosterRow> rows = Town();
  rows[1].client_outdated = true;
  rows[2].mute_minutes = 10;
  const ro::ui::StaffRosterSummary summary = ro::ui::SummaryOf(rows);
  CHECK(summary.players == 3);
  CHECK(summary.offline_shops == 1);
  // « prontera » et l'instance 4 de « gl_cas02 ».
  CHECK(summary.maps == 2);
  CHECK(summary.outdated_clients == 1);
  CHECK(summary.flagged == 1);
}

TEST_CASE("Joueurs en ligne : les cartes se rangent de la plus peuplée à la moins peuplée",
          "[ui][staff_roster]") {
  std::vector<StaffRosterRow> rows = Town();
  // Une seconde instance de la même carte source, et la carte source elle-même.
  StaffRosterRow other_instance = Row("Corvin", 2000006);
  other_instance.map = "gl_cas02";
  other_instance.instance_id = 7;
  rows.push_back(other_instance);
  const std::vector<std::size_t> order =
      OrderedRows(rows, "", kNoFilter, StaffRosterColumn::kPlayer, false);
  const std::vector<ro::ui::StaffRosterMapGroup> groups = ro::ui::MapGroupsOf(rows, order);
  REQUIRE(groups.size() == 3);
  CHECK(groups[0].map == "prontera");
  CHECK(groups[0].players == 2);
  CHECK(groups[0].offline_shops == 1);
  CHECK(NamesOf(rows, groups[0].rows) ==
        std::vector<std::string_view>{"Aldric", "Petit Lot", "ysolde"});
  // À population égale, par nom, puis par numéro d'instance.
  CHECK(groups[1].map == "gl_cas02");
  CHECK(groups[1].instance_id == 4);
  CHECK(groups[2].instance_id == 7);
}
