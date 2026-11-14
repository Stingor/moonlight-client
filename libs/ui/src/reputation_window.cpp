// La réputation. Voir l'en-tête.

#include "ro/ui/reputation_window.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/list_keys.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"

#include "ui_scale.h"
#include "ui_text.h"

namespace ro::ui {
namespace {

// LES LAMPES D'UN PALIER : un disque par palier possible, côte à côte.
constexpr float kLampRadiusPixels = 5.0F;
constexpr float kLampGapPixels = 4.0F;
constexpr ImU32 kLampEmptyColor = IM_COL32(190, 190, 190, 255);
constexpr ImU32 kLampEdgeColor = IM_COL32(99, 99, 99, 255);
// Les couleurs du client d'origine : bleu pâle pour une réputation positive,
// rouge pour une négative — la lampe comme la barre.
constexpr ImU32 kPositiveColor = IM_COL32(121, 146, 205, 255);
constexpr ImU32 kNegativeColor = IM_COL32(205, 64, 64, 255);
// Le fond de la faction qu'un script désigne.
constexpr ImU32 kHighlightColor = IM_COL32(255, 231, 222, 255);
constexpr float kGroupComboPixels = 140.0F;
constexpr float kRowGapPixels = 6.0F;

// LE TEXTE D'UN NOMBRE DE POINTS, signé : « 1 200 P », « -300 P ».
std::string PointsText(std::int64_t points) {
  std::array<char, 32> text{};
  std::snprintf(text.data(), text.size(), "%lld P", static_cast<long long>(points));
  return text.data();
}

// UN TEXTE CALÉ À DROITE de la ligne commencée à `line_x`, large de `line_width`.
void RightAligned(const std::string& text, float line_x, float line_width) {
  ImGui::SameLine(line_x + line_width - ImGui::CalcTextSize(text.c_str()).x);
  ImGui::TextUnformatted(text.c_str());
}

// LA LISTE DES GROUPES : « toutes les factions », puis chaque groupe. Changer de
// groupe vide la recherche.
void DrawGroupCombo(ReputationWindowState& state, const ReputationView& view, const SkinArt& art) {
  const int count = static_cast<int>(view.groups.size()) + 1;
  // UN RANG QUE LA LISTE N'A PLUS revient à « toutes » : la table a changé.
  if (state.group < kReputationAllGroups || state.group >= count) {
    state.group = kReputationAllGroups;
  }
  const char* all = core::Tr("Toutes les factions");
  const std::string shown =
      state.group == kReputationAllGroups
          ? std::string(all)
          : std::string(view.groups[static_cast<std::size_t>(state.group - 1)]);
  ImGui::SetNextItemWidth(Px(kGroupComboPixels));
  int step = 0;
  const bool open = BeginSkinCombo("##reputation_group", shown.c_str(), art, &step);
  int chosen = state.group;
  if (const int to = SkinComboStepped(state.group, step, count); to != state.group) {
    chosen = to;
  }
  if (open) {
    ComboKeys keys(state.group, count);
    if (keys.Took(kReputationAllGroups,
                  ImGui::Selectable(all, state.group == kReputationAllGroups))) {
      chosen = kReputationAllGroups;
    }
    for (std::size_t index = 0; index < view.groups.size(); ++index) {
      const int rank = static_cast<int>(index) + 1;
      const std::string label(view.groups[index]);
      if (keys.Took(rank, ImGui::Selectable(label.c_str(), state.group == rank))) {
        chosen = rank;
      }
    }
    EndSkinCombo();
  }
  if (chosen != state.group) {
    state.group = chosen;
    state.search.fill('\0');
  }
}

// LES LAMPES d'une faction.
void DrawLamps(const ReputationGauge& gauge) {
  const float radius = Px(kLampRadiusPixels);
  const float pitch = radius * 2.0F + Px(kLampGapPixels);
  const ImVec2 origin = ImGui::GetCursorScreenPos();
  ImDrawList* list = ImGui::GetWindowDrawList();
  const ImU32 lit = gauge.negative ? kNegativeColor : kPositiveColor;
  for (std::uint32_t step = 0; step < gauge.steps; ++step) {
    const ImVec2 center(origin.x + radius + pitch * static_cast<float>(step), origin.y + radius);
    list->AddCircleFilled(center, radius, step < gauge.lit_steps ? lit : kLampEmptyColor);
    list->AddCircle(center, radius, kLampEdgeColor);
  }
  ImGui::Dummy(ImVec2(pitch * static_cast<float>(gauge.steps), radius * 2.0F));
}

// UNE FACTION : son nom et ses points, ses lampes, la barre du palier suivant.
void DrawRow(const ReputationRowView& row) {
  const ReputationGauge gauge = ReputationGaugeOf(row.points, row.least, row.most);
  ImGui::BeginGroup();
  const float line_x = ImGui::GetCursorPosX();
  const float line_width = ImGui::GetContentRegionAvail().x;
  Unformatted(row.name);
  RightAligned(PointsText(row.points), line_x, line_width);
  DrawLamps(gauge);
  std::array<char, 48> label{};
  if (gauge.full) {
    std::snprintf(label.data(), label.size(), "%s", core::Tr("Palier maximal"));
  } else {
    std::snprintf(label.data(), label.size(), "%lld / %lld",
                  static_cast<long long>(gauge.into_step),
                  static_cast<long long>(kReputationPointsPerStep));
  }
  const float ratio = gauge.full ? 1.0F
                                 : static_cast<float>(gauge.into_step) /
                                       static_cast<float>(kReputationPointsPerStep);
  ImGui::PushStyleColor(ImGuiCol_PlotHistogram, gauge.negative ? kNegativeColor : kPositiveColor);
  // HAUTEUR NULLE : celle d'un cadre, que le texte de la barre remplit sans être coupé.
  ImGui::ProgressBar(ratio, ImVec2(-1.0F, 0.0F), label.data());
  ImGui::PopStyleColor();
  ImGui::EndGroup();
  if (row.highlighted) {
    ImDrawList* list = ImGui::GetWindowDrawList();
    list->ChannelsSetCurrent(0);
    list->AddRectFilled(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), kHighlightColor);
    list->ChannelsSetCurrent(1);
  }
}

void DrawRows(const ReputationView& view) {
  if (view.rows.empty()) {
    ImGui::TextDisabled("%s", core::Tr("Aucune faction à montrer."));
    return;
  }
  // DEUX CANAUX : le fond d'une faction désignée se peint sous elle, après elle.
  ImDrawList* list = ImGui::GetWindowDrawList();
  list->ChannelsSplit(2);
  list->ChannelsSetCurrent(1);
  for (const ReputationRowView& row : view.rows) {
    DrawRow(row);
    ImGui::Dummy(ImVec2(0.0F, Px(kRowGapPixels)));
  }
  list->ChannelsMerge();
}

}  // namespace

ReputationGauge ReputationGaugeOf(std::int64_t points, std::int64_t least, std::int64_t most) {
  ReputationGauge gauge;
  gauge.negative = points < 0;
  const std::int64_t bound =
      gauge.negative ? -std::min<std::int64_t>(least, 0) : std::max<std::int64_t>(most, 0);
  const std::int64_t reached = std::min(gauge.negative ? -points : points, bound);
  gauge.steps = static_cast<std::uint32_t>(bound / kReputationPointsPerStep);
  gauge.lit_steps = static_cast<std::uint32_t>(reached / kReputationPointsPerStep);
  gauge.full = gauge.lit_steps >= gauge.steps;
  gauge.into_step = gauge.full ? 0 : reached % kReputationPointsPerStep;
  return gauge;
}

void DrawReputationWindow(ReputationWindowState& state, const ReputationView& view,
                          const SkinArt& art) {
  if (!state.open) {
    state.was_open = false;
    return;
  }
  if (!state.was_open) {
    state.was_open = true;
    ImGui::SetNextWindowFocus();
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_FirstUseEver, ImVec2(0.5F, 0.5F));
  }
  ImGui::SetNextWindowSize(
      ImVec2(Px(kReputationFirstWidthPixels), Px(kReputationFirstHeightPixels)),
      ImGuiCond_FirstUseEver);
  const bool begun = BeginSkinnedWindow(state.name.Get(), &state.open, art, SkinnedTitle::kBar,
                                        ImGuiWindowFlags_NoCollapse);
  if (!begun) {
    EndSkinnedWindow();
    return;
  }
  if (view.book_missing) {
    ImGui::TextWrapped("%s", core::Tr("La table des réputations manque à l'installation."));
    EndSkinnedWindow();
    return;
  }
  DrawGroupCombo(state, view, art);
  ImGui::SameLine();
  ImGui::SetNextItemWidth(-1.0F);
  static_cast<void>(ImGui::InputTextWithHint("##reputation_search", core::Tr("Rechercher…"),
                                             state.search.data(), state.search.size()));
  if (state.group != kReputationAllGroups) {
    const float line_x = ImGui::GetCursorPosX();
    const float line_width = ImGui::GetContentRegionAvail().x;
    ImGui::TextUnformatted(core::Tr("Total du groupe"));
    RightAligned(PointsText(view.group_points), line_x, line_width);
  }
  ImGui::Separator();
  if (view.awaiting_points) {
    ImGui::TextUnformatted(core::Tr("Récupération des réputations…"));
    EndSkinnedWindow();
    return;
  }
  // ⚠ `EndChild` SE PAIE MÊME QUAND `BeginChild` REND FAUX.
  if (ImGui::BeginChild("##reputation_rows", ImVec2(0.0F, 0.0F))) {
    DrawRows(view);
  }
  ImGui::EndChild();
  EndSkinnedWindow();
}

}  // namespace ro::ui
