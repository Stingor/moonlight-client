#include "ro/ui/survivors_ranking_window.h"

#include <cstdio>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"
#include "ro/ui/survivors_hud.h"
#include "ro/ui/survivors_records_window.h"
#include "ro/ui/window_bounds.h"
#include "ro/ui/window_name.h"

#include "survivors_meta_text.h"
#include "ui_text.h"

namespace ro::ui {
namespace {

inline constexpr const char* kRankingTitle = core::TrText("Classement des Survivants");

constexpr float kFirstWidth = 560.0F;
constexpr float kFirstHeight = 520.0F;
constexpr std::size_t kCellSize = 48;
constexpr std::size_t kLineSize = 96;
constexpr std::int64_t kSecondsPerMinute = 60;
// Rang, nom, score, temps, niveau.
constexpr int kColumns = 5;
constexpr ImGuiTableFlags kTableFlags =
    ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersInnerH;

void DrawTabs(SurvivorsRankingState& state, SurvivorsRankingAsk& asked, const SkinArt& art) {
  const bool ranked = state.tab == SurvivorsRankingTab::kRanked;
  SurvivorsRankingTab chosen = state.tab;
  if (SkinToggleButton(core::Tr("Partie classée"), ranked, art)) {
    chosen = SurvivorsRankingTab::kRanked;
  }
  ImGui::SameLine();
  if (SkinToggleButton(core::Tr("Défi de la semaine"), !ranked, art)) {
    chosen = SurvivorsRankingTab::kChallenge;
  }
  if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
      ImGui::IsKeyPressed(ImGuiKey_Tab, false)) {
    chosen = ranked ? SurvivorsRankingTab::kChallenge : SurvivorsRankingTab::kRanked;
  }
  if (chosen != state.tab) {
    state.tab = chosen;
    asked.tab_changed = true;
  }
}

void DrawTable(const SurvivorsRankingPage& page) {
  if (!ImGui::BeginTable("##ranking", kColumns, kTableFlags)) return;
  ImGui::TableSetupColumn(core::Tr("Rang"));
  ImGui::TableSetupColumn(core::Tr("Personnage"), ImGuiTableColumnFlags_WidthStretch);
  ImGui::TableSetupColumn(core::Tr("Score"));
  ImGui::TableSetupColumn(core::Tr("Temps"));
  ImGui::TableSetupColumn(core::Tr("Niveau"));
  ImGui::TableHeadersRow();
  std::array<char, kCellSize> cell = {};
  for (const SurvivorsRankingRow& row : page.rows) {
    ImGui::TableNextRow();
    if (page.own_rank > 0 && row.rank == page.own_rank) {
      ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, kSurvivorsOwnRowBack);
      ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1, kSurvivorsOwnRowBack);
    }
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(SurvivorsRankText(row.rank).data());
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(row.name);
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(SurvivorsZeny(row.score).data());
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(SurvivorsClockText(row.survived_ms).data());
    ImGui::TableNextColumn();
    std::snprintf(cell.data(), cell.size(), "%d", row.level);
    ImGui::TextUnformatted(cell.data());
  }
  ImGui::EndTable();
}

// La place du joueur quand sa ligne n'est pas dans le tableau.
void DrawOwnLine(const SurvivorsRankingPage& page) {
  std::array<char, kLineSize> line = {};
  switch (SurvivorsOwnPlaceOf(page)) {
    case SurvivorsOwnPlace::kInTable:
      return;
    case SurvivorsOwnPlace::kPinned:
      ImGui::Separator();
      std::snprintf(line.data(), line.size(), core::Tr("Vous : %s — %s"),
                    SurvivorsRankText(page.own_rank).data(), SurvivorsZeny(page.own_score).data());
      ImGui::TextUnformatted(line.data());
      return;
    case SurvivorsOwnPlace::kUnranked:
      ImGui::Separator();
      FaintText(core::Tr("Vous n'êtes pas encore classé."));
      return;
  }
}

void DrawPage(const SurvivorsRankingPage& page) {
  if (!page.received) {
    FaintText(core::Tr("Demande au serveur…"));
    return;
  }
  if (page.rows.empty()) {
    FaintText(core::Tr("Aucune partie classée."));
  } else {
    DrawTable(page);
  }
  DrawOwnLine(page);
}

void DrawAge(const SurvivorsRankingPage& page) {
  if (!page.received) return;
  std::array<char, kLineSize> line = {};
  if (page.age_s < kSecondsPerMinute) {
    std::snprintf(line.data(), line.size(), core::Tr("il y a %d s"), static_cast<int>(page.age_s));
  } else {
    std::snprintf(line.data(), line.size(), core::Tr("il y a %d min"),
                  static_cast<int>(page.age_s / kSecondsPerMinute));
  }
  ImGui::SameLine();
  FaintText(line.data());
}

}  // namespace

SurvivorsOwnPlace SurvivorsOwnPlaceOf(const SurvivorsRankingPage& page) noexcept {
  if (page.own_rank <= 0) return SurvivorsOwnPlace::kUnranked;
  for (const SurvivorsRankingRow& row : page.rows) {
    if (row.rank == page.own_rank) return SurvivorsOwnPlace::kInTable;
  }
  return SurvivorsOwnPlace::kPinned;
}

SurvivorsRankingAsk DrawSurvivorsRankingWindow(SurvivorsRankingState& state,
                                               const SurvivorsRankingView& view,
                                               const SkinArt& art) {
  SurvivorsRankingAsk asked;
  if (!state.open) return asked;
  static WindowName name(kRankingTitle, kSurvivorsRankingId);
  ImGui::SetNextWindowSize(ImVec2(kFirstWidth, kFirstHeight), ImGuiCond_FirstUseEver);
  ConstrainNextWindowToScreen(name.Get());
  if (!BeginSkinnedWindow(name.Get(), &state.open, art)) {
    EndSkinnedWindow();
    return asked;
  }
  std::array<char, kLineSize> line = {};
  std::snprintf(line.data(), line.size(), core::Tr("Règles %d"), view.ruleset);
  FaintText(line.data());
  DrawTabs(state, asked, art);
  ImGui::Separator();
  const bool ranked = state.tab == SurvivorsRankingTab::kRanked;
  if (!ranked) {
    ImGui::TextUnformatted(SurvivorsWeekText(view.week_year, view.week_number).data());
  }
  const SurvivorsRankingPage& page = ranked ? view.ranked : view.challenge;
  DrawPage(page);
  ImGui::Separator();
  if (!view.refresh_ready) ImGui::BeginDisabled();
  const bool refresh = SkinSmallButton(core::Tr("Actualiser"), art);
  if (!view.refresh_ready) ImGui::EndDisabled();
  ImGui::SameLine();
  if (SkinSmallButton(core::Tr("Fermer"), art)) state.open = false;
  DrawAge(page);
  EndSkinnedWindow();
  asked.refresh = refresh && view.refresh_ready;
  return asked;
}

}  // namespace ro::ui
