// Le tableau des classements. Voir l'en-tête.

#include "ro/ui/ranking_window.h"

#include <array>
#include <cstddef>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"

#include "ui_scale.h"
#include "ui_text.h"

namespace ro::ui {
namespace {

constexpr std::array<RankingTab, kRankingTabCount> kRankingTabOrder = {
    RankingTab::kBlacksmith, RankingTab::kAlchemist, RankingTab::kTaekwon};

// LES ONGLETS. Celui qu'`app` a posé s'impose ; un clic du joueur se lit et
// devient la demande de son classement.
//
// ⚠ UN ONGLET IMPOSÉ NE SE MONTRE QU'À L'IMAGE SUIVANTE : pendant celle-ci,
// ImGui rend encore l'ancien, qui ne doit pas passer pour un clic.
void DrawTabs(RankingWindowState& state, const SkinArt& art, RankingWindowAction& action) {
  if (!BeginSkinTabBar("##ranking_tabs", art)) {
    return;
  }
  const int wanted = static_cast<int>(state.tab);
  const bool imposed = state.tab_shown != wanted;
  int reported = kRankingNoTabShown;
  for (const RankingTab tab : kRankingTabOrder) {
    const int value = static_cast<int>(tab);
    const ImGuiTabItemFlags flags =
        imposed && value == wanted ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
    if (ImGui::BeginTabItem(RankingTabLabel(tab), nullptr, flags)) {
      reported = value;
      ImGui::EndTabItem();
    }
  }
  EndSkinTabBar();
  if (imposed) {
    if (reported == wanted) {
      state.tab_shown = reported;
    }
    return;
  }
  if (reported != kRankingNoTabShown && reported != wanted) {
    state.tab = static_cast<RankingTab>(reported);
    state.tab_shown = reported;
    action.requested = true;
    action.requested_tab = state.tab;
  }
}

// LES DIX LIGNES, puis les points du joueur sous un trait.
void DrawTable(const RankingView& view) {
  constexpr int kColumns = 3;
  if (!ImGui::BeginTable("##ranking_rows", kColumns,
                         ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
    return;
  }
  const float scale = Scale();
  ImGui::TableSetupColumn(core::Tr("Rang"), ImGuiTableColumnFlags_WidthFixed,
                          kRankingRankColumnPixels * scale);
  ImGui::TableSetupColumn(core::Tr("Nom"), ImGuiTableColumnFlags_WidthStretch);
  ImGui::TableSetupColumn(core::Tr("Points"), ImGuiTableColumnFlags_WidthFixed,
                          kRankingPointsColumnPixels * scale);
  ImGui::TableHeadersRow();
  for (std::size_t rank = 0; rank < view.rows.size(); ++rank) {
    const RankingRowView& row = view.rows[rank];
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("%d", static_cast<int>(rank + 1));
    ImGui::TableNextColumn();
    // UNE PLACE SANS NOM se dit comme dans la conversation.
    if (row.name.empty()) {
      ImGui::TextDisabled("%s", core::Tr("Sans nom"));
    } else {
      Unformatted(row.name);
    }
    ImGui::TableNextColumn();
    ImGui::Text("%u", row.points);
  }
  ImGui::EndTable();
  ImGui::Separator();
  ImGui::TextUnformatted(core::Tr("Mes points"));
  ImGui::SameLine();
  ImGui::Text("%u", view.own_points);
}

}  // namespace

const char* RankingTabLabel(RankingTab tab) {
  switch (tab) {
    case RankingTab::kBlacksmith:
      return core::Tr("Blacksmith");
    case RankingTab::kAlchemist:
      return core::Tr("Alchemist");
    case RankingTab::kTaekwon:
      return core::Tr("Taekwon");
  }
  return "";
}

RankingWindowAction DrawRankingWindow(RankingWindowState& state, const RankingView& view,
                                      const SkinArt& art) {
  RankingWindowAction action;
  if (!state.open) {
    state.was_open = false;
    return action;
  }
  if (!state.was_open) {
    state.was_open = true;
    // L'onglet repart de celui qu'`app` a posé, quel qu'ait été le dernier montré.
    state.tab_shown = kRankingNoTabShown;
    ImGui::SetNextWindowFocus();
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_FirstUseEver, ImVec2(0.5F, 0.5F));
  }
  const float scale = Scale();
  ImGui::SetNextWindowSize(
      ImVec2(kRankingFirstWidthPixels * scale, kRankingFirstHeightPixels * scale),
      ImGuiCond_FirstUseEver);
  const bool begun = BeginSkinnedWindow(state.name.Get(), &state.open, art, SkinnedTitle::kBar,
                                        ImGuiWindowFlags_NoCollapse);
  if (!begun) {
    EndSkinnedWindow();
    return action;
  }
  ImGui::TextDisabled("%s", core::Tr("Choisir le type de classement"));
  DrawTabs(state, art, action);
  // ⚠ LES LIGNES D'UN AUTRE CLASSEMENT NE SE MONTRENT PAS sous l'onglet choisi :
  // la réponse à la demande n'est pas encore là.
  if (view.awaiting_list || view.waiting_names || view.tab != state.tab) {
    ImGui::TextUnformatted(core::Tr("Récupération du classement…"));
  } else {
    DrawTable(view);
  }
  EndSkinnedWindow();
  return action;
}

}  // namespace ro::ui
