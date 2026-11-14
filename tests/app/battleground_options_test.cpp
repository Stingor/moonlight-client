// LA PAGE DU CHAMP DE BATAILLE : ses défauts, ses clés, ses lignes, ses
// changements.

#include "options/battleground_options.h"

#include <algorithm>
#include <span>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "game/settings_file.h"
#include "support/temp_directory.h"

using moonclient::BattlegroundOptions;
using moonclient::BattlegroundRow;
using moonclient::BattlegroundRowId;
using moonclient::kBattlegroundRowBase;
using moonclient::SettingsFile;
using ro::test_support::TempDirectory;
using ro::ui::OptionKind;
using ro::ui::OptionRowView;
using ro::ui::OptionsWindowAction;

namespace {

[[nodiscard]] const OptionRowView* Row(std::span<const OptionRowView> rows, BattlegroundRow row) {
  const auto found = std::ranges::find(rows, BattlegroundRowId(row), &OptionRowView::id);
  return found != rows.end() ? &*found : nullptr;
}

[[nodiscard]] std::vector<OptionRowView> Rows(const BattlegroundOptions& page) {
  std::vector<OptionRowView> rows;
  page.ComposeInto(rows);
  return rows;
}

[[nodiscard]] OptionsWindowAction Flip(BattlegroundRow row, bool on) {
  OptionsWindowAction asked;
  asked.changed = BattlegroundRowId(row);
  asked.on = on;
  return asked;
}

void LoadInto(BattlegroundOptions& page, const TempDirectory& folder, std::string_view text) {
  folder.WriteText("settings.yml", text);
  SettingsFile file(folder.path() / "settings.yml");
  page.Declare(file);
  file.Load();
}

}  // namespace

TEST_CASE("Réglages du champ : tout est montré par défaut", "[app][battleground]") {
  const BattlegroundOptions options;
  CHECK(options.settings().score_shown);
  CHECK(options.settings().own_side_highlighted);
  CHECK(options.settings().minimap_mates);
  CHECK(options.settings().enemy_marker);
}

TEST_CASE("Réglages du champ : quatre cases sous leur intertitre, dans l'ordre",
          "[app][battleground]") {
  const std::vector<OptionRowView> rows = Rows(BattlegroundOptions());
  REQUIRE(rows.size() == static_cast<std::size_t>(BattlegroundRow::kCount));
  CHECK(rows[0].id == BattlegroundRowId(BattlegroundRow::kScoreShown));
  CHECK(rows[0].heading != nullptr);
  CHECK(rows[1].id == BattlegroundRowId(BattlegroundRow::kOwnSideHighlighted));
  CHECK(rows[2].id == BattlegroundRowId(BattlegroundRow::kMinimapMates));
  CHECK(rows[3].id == BattlegroundRowId(BattlegroundRow::kEnemyMarker));
  for (const OptionRowView& row : rows) {
    CHECK(row.kind == OptionKind::kToggle);
    CHECK(row.on);
    CHECK(row.help != nullptr);
  }
}

TEST_CASE("Réglages du champ : le surlignage se grise avec le score qu'il précise",
          "[app][battleground]") {
  BattlegroundOptions page;
  {
    const std::vector<OptionRowView> rows = Rows(page);
    const OptionRowView* own = Row(rows, BattlegroundRow::kOwnSideHighlighted);
    REQUIRE(own != nullptr);
    CHECK(own->indented);
    CHECK_FALSE(own->disabled);
  }
  REQUIRE(page.Change(Flip(BattlegroundRow::kScoreShown, false)));
  const std::vector<OptionRowView> rows = Rows(page);
  CHECK(Row(rows, BattlegroundRow::kOwnSideHighlighted)->disabled);
  CHECK_FALSE(Row(rows, BattlegroundRow::kMinimapMates)->disabled);
  CHECK_FALSE(Row(rows, BattlegroundRow::kEnemyMarker)->disabled);
}

TEST_CASE("Réglages du champ : chaque case change son seul réglage", "[app][battleground]") {
  BattlegroundOptions page;
  CHECK(page.Change(Flip(BattlegroundRow::kScoreShown, false)));
  CHECK_FALSE(page.settings().score_shown);
  CHECK(page.settings().own_side_highlighted);
  CHECK(page.Change(Flip(BattlegroundRow::kOwnSideHighlighted, false)));
  CHECK_FALSE(page.settings().own_side_highlighted);
  CHECK(page.settings().minimap_mates);
  CHECK(page.Change(Flip(BattlegroundRow::kMinimapMates, false)));
  CHECK_FALSE(page.settings().minimap_mates);
  CHECK(page.settings().enemy_marker);
  CHECK(page.Change(Flip(BattlegroundRow::kEnemyMarker, false)));
  CHECK_FALSE(page.settings().enemy_marker);
  CHECK(page.Change(Flip(BattlegroundRow::kScoreShown, true)));
  CHECK(page.settings().score_shown);
}

TEST_CASE("Réglages du champ : une ligne d'une autre page n'est pas la sienne",
          "[app][battleground]") {
  BattlegroundOptions page;
  OptionsWindowAction asked;
  asked.on = false;
  // Juste avant la plage, juste après, et rien du tout.
  asked.changed = kBattlegroundRowBase - 1;
  CHECK_FALSE(page.Change(asked));
  asked.changed = BattlegroundRowId(BattlegroundRow::kCount);
  CHECK_FALSE(page.Change(asked));
  asked.changed = -1;
  CHECK_FALSE(page.Change(asked));
  CHECK(page.settings().score_shown);
  CHECK(page.settings().enemy_marker);
}

TEST_CASE("Réglages du champ : les quatre clés anglaises se relisent", "[app][battleground]") {
  const TempDirectory folder;
  BattlegroundOptions page;
  LoadInto(page, folder,
           "battleground_score_shown: false\n"
           "battleground_own_side_highlighted: false\n"
           "battleground_minimap_mates: false\n"
           "battleground_enemy_marker: false\n");
  CHECK_FALSE(page.settings().score_shown);
  CHECK_FALSE(page.settings().own_side_highlighted);
  CHECK_FALSE(page.settings().minimap_mates);
  CHECK_FALSE(page.settings().enemy_marker);
}
