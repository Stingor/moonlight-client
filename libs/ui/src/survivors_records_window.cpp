#include "ro/ui/survivors_records_window.h"

#include <cstdio>

#include "ro/core/i18n.h"
#include "ro/ui/skin_look.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"
#include "ro/ui/survivors_hud.h"
#include "ro/ui/window_bounds.h"
#include "ro/ui/window_name.h"

#include "survivors_meta_text.h"
#include "ui_text.h"

namespace ro::ui {
namespace {

inline constexpr const char* kRecordsTitle = core::TrText("Records des Survivants");

constexpr float kFirstWidth = 600.0F;
constexpr float kFirstHeight = 460.0F;
constexpr ImU32 kLatestBack = IM_COL32(255, 215, 90, 60);
constexpr ImU32 kOlderText = IM_COL32(150, 150, 150, 255);
// Le corps est clair : la victoire s'y écrit en or sombre.
constexpr ImU32 kVictoryColor = kSkinGoldTextColor;
constexpr ImU32 kDeathColor = IM_COL32(230, 90, 90, 255);
constexpr std::size_t kCellSize = 48;
// Rang ou semaine, score, temps, niveau, issue, tués, date.
constexpr int kBaseColumns = 7;
constexpr ImGuiTableFlags kTableFlags =
    ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersInnerH;

// Ce que montre un tableau : la première colonne est le rang, ou la semaine
// pour les semaines passées ; la Cursed Water ne se montre qu'en partie libre.
struct TableShape {
  bool week_first = false;
  bool cursed_water = false;
};

// Une cellule ; grisée, et « ancien équilibrage » au survol, pour une partie
// d'anciennes règles.
void Cell(const SurvivorsRecordRow& row, const char* text) {
  ImGui::TableNextColumn();
  if (row.older_rules) {
    ColoredText(kOlderText, text);
    SkinTooltip(core::Tr("ancien équilibrage"));
  } else {
    ImGui::TextUnformatted(text);
  }
}

void OutcomeCell(const SurvivorsRecordRow& row) {
  ImGui::TableNextColumn();
  switch (row.outcome) {
    case SurvivorsEndOutcome::kVictory:
      ColoredText(row.older_rules ? kOlderText : kVictoryColor, core::Tr("Victoire"));
      break;
    case SurvivorsEndOutcome::kDied:
      ColoredText(row.older_rules ? kOlderText : kDeathColor, core::Tr("Mort"));
      break;
    case SurvivorsEndOutcome::kAbandoned:
      FaintText(core::Tr("Interrompue"));
      break;
  }
}

void DrawTable(const char* id, std::span<const SurvivorsRecordRow> rows, TableShape shape) {
  if (rows.empty()) {
    FaintText(core::Tr("Aucune partie"));
    return;
  }
  const int columns = kBaseColumns + (shape.cursed_water ? 1 : 0);
  if (!ImGui::BeginTable(id, columns, kTableFlags)) return;
  ImGui::TableSetupColumn(shape.week_first ? core::Tr("Semaine") : core::Tr("Rang"));
  ImGui::TableSetupColumn(core::Tr("Score"));
  ImGui::TableSetupColumn(core::Tr("Temps"));
  ImGui::TableSetupColumn(core::Tr("Niveau"));
  ImGui::TableSetupColumn(core::Tr("Issue"));
  ImGui::TableSetupColumn(core::Tr("Tués"));
  if (shape.cursed_water) ImGui::TableSetupColumn(core::Tr("Cursed Water"));
  ImGui::TableSetupColumn(core::Tr("Date"));
  ImGui::TableHeadersRow();
  std::array<char, kCellSize> cell = {};
  for (std::size_t index = 0; index < rows.size(); ++index) {
    const SurvivorsRecordRow& row = rows[index];
    ImGui::TableNextRow();
    if (row.latest) ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1, kLatestBack);
    if (shape.week_first) {
      std::snprintf(cell.data(), cell.size(), "%d-W%02d", row.week_year, row.week_number);
    } else {
      std::snprintf(cell.data(), cell.size(), "%zu", index + 1);
    }
    Cell(row, cell.data());
    Cell(row, SurvivorsZeny(row.score).data());
    Cell(row, SurvivorsClockText(row.survived_ms).data());
    std::snprintf(cell.data(), cell.size(), "%d", row.level);
    Cell(row, cell.data());
    OutcomeCell(row);
    std::snprintf(cell.data(), cell.size(), "%u", static_cast<unsigned>(row.kills));
    Cell(row, cell.data());
    if (shape.cursed_water) {
      std::snprintf(cell.data(), cell.size(), "%d", row.cursed_water);
      Cell(row, cell.data());
    }
    Cell(row, SurvivorsDateOfUtc(row.played_at_utc_s).data());
  }
  ImGui::EndTable();
}

// L'onglet qui suit `tab` sous Tab ; celui des parties classées seulement
// quand il est montré.
[[nodiscard]] SurvivorsRecordsTab NextTab(SurvivorsRecordsTab tab, bool ranked_shown) noexcept {
  switch (tab) {
    case SurvivorsRecordsTab::kFree:
      return SurvivorsRecordsTab::kChallenge;
    case SurvivorsRecordsTab::kChallenge:
      return ranked_shown ? SurvivorsRecordsTab::kRanked : SurvivorsRecordsTab::kFree;
    case SurvivorsRecordsTab::kRanked:
      break;
  }
  return SurvivorsRecordsTab::kFree;
}

void DrawTabs(SurvivorsRecordsState& state, const SurvivorsRecordsView& view, const SkinArt& art) {
  const SurvivorsRecordsTab shown = state.tab;
  if (SkinToggleButton(core::Tr("Parties libres"), shown == SurvivorsRecordsTab::kFree, art)) {
    state.tab = SurvivorsRecordsTab::kFree;
  }
  ImGui::SameLine();
  if (SkinToggleButton(core::Tr("Défi de la semaine"), shown == SurvivorsRecordsTab::kChallenge,
                       art)) {
    state.tab = SurvivorsRecordsTab::kChallenge;
  }
  if (view.ranked_shown) {
    ImGui::SameLine();
    if (SkinToggleButton(core::Tr("Parties classées"), shown == SurvivorsRecordsTab::kRanked,
                         art)) {
      state.tab = SurvivorsRecordsTab::kRanked;
    }
  }
  if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
      ImGui::IsKeyPressed(ImGuiKey_Tab, false)) {
    state.tab = NextTab(shown, view.ranked_shown);
  }
}

}  // namespace

std::array<char, kSurvivorsWeekTextSize> SurvivorsWeekText(int year, int week) {
  std::array<char, kSurvivorsWeekTextSize> text = {};
  std::snprintf(text.data(), text.size(), core::Tr("Semaine %d de %d"), week, year);
  return text;
}

void DrawSurvivorsRecordsWindow(SurvivorsRecordsState& state, const SurvivorsRecordsView& view,
                                const SkinArt& art) {
  if (!state.open) return;
  static WindowName name(kRecordsTitle, kSurvivorsRecordsId);
  ImGui::SetNextWindowSize(ImVec2(kFirstWidth, kFirstHeight), ImGuiCond_FirstUseEver);
  ConstrainNextWindowToScreen(name.Get());
  if (!BeginSkinnedWindow(name.Get(), &state.open, art)) {
    EndSkinnedWindow();
    return;
  }
  // Un onglet classé retenu alors qu'il n'est plus montré retombe sur l'onglet
  // libre.
  if (state.tab == SurvivorsRecordsTab::kRanked && !view.ranked_shown) {
    state.tab = SurvivorsRecordsTab::kFree;
  }
  DrawTabs(state, view, art);
  ImGui::Separator();
  switch (state.tab) {
    case SurvivorsRecordsTab::kFree:
      DrawTable("##free", view.free_runs, {.cursed_water = true});
      break;
    case SurvivorsRecordsTab::kChallenge:
      ImGui::TextUnformatted(SurvivorsWeekText(view.week_year, view.week_number).data());
      DrawTable("##week", view.week_runs, {});
      ImGui::SeparatorText(core::Tr("Semaines passées"));
      DrawTable("##past", view.past_weeks, {.week_first = true});
      break;
    case SurvivorsRecordsTab::kRanked:
      DrawTable("##ranked", view.ranked_runs, {});
      break;
  }
  EndSkinnedWindow();
}

}  // namespace ro::ui
