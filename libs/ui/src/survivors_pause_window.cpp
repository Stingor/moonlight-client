#include "ro/ui/survivors_pause_window.h"

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"
#include "ro/ui/window_name.h"

#include "survivors_text.h"
#include "ui_text.h"

namespace ro::ui {
namespace {

inline constexpr const char* kPauseTitle = core::TrText("Pause");
inline constexpr const char* kPauseId = "survivors_pause";

constexpr float kPauseWidth = 340.0F;
// Le libellé, puis p50, p95 et le maximum.
constexpr int kMeasureColumns = 4;
constexpr ImU32 kAlertColor = IM_COL32(255, 120, 90, 255);
constexpr ImGuiWindowFlags kModalFlags =
    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
    ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings;

// Ce qui a mis en pause, quand ce n'est pas le joueur.
void DrawReason(const SurvivorsPauseView& view) {
  switch (view.reason) {
    case SurvivorsPauseReason::kPlayer:
      return;
    case SurvivorsPauseReason::kFocusLost:
      ColoredText(kAlertColor, core::Tr("La fenêtre du jeu a perdu le focus."));
      break;
    case SurvivorsPauseReason::kWorldAlert:
      ColoredText(kAlertColor, SurvivorsText(view.alert_banner));
      break;
  }
  ImGui::Separator();
}

// Une case de réglage : la valeur nouvelle quand on la clique.
[[nodiscard]] std::optional<bool> Setting(const char* label, bool shown, const SkinArt& art) {
  bool ticked = shown;
  if (SkinCheckBox(label, &ticked, art) && ticked != shown) {
    return ticked;
  }
  return std::nullopt;
}

void DrawMeasures(const SurvivorsPauseView& view) {
  ImGui::SeparatorText(core::Tr("Mesures"));
  if (ImGui::BeginTable("##measures", kMeasureColumns, ImGuiTableFlags_SizingStretchProp)) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(core::Tr("p50 (ms)"));
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(core::Tr("p95 (ms)"));
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(core::Tr("max (ms)"));
    for (const SurvivorsMeasureRow& row : view.measures) {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(SurvivorsText(row.label));
      ImGui::TableNextColumn();
      ImGui::Text("%.2f", row.p50_ms);
      ImGui::TableNextColumn();
      ImGui::Text("%.2f", row.p95_ms);
      ImGui::TableNextColumn();
      ImGui::Text("%.2f", row.max_ms);
    }
    ImGui::EndTable();
  }
  for (const SurvivorsCountRow& row : view.counts) {
    ImGui::Text("%s : %lld", SurvivorsText(row.label), row.value);
  }
}

}  // namespace

SurvivorsPauseAsk DrawSurvivorsPauseWindow(SurvivorsPauseState& state,
                                           const SurvivorsPauseView& view, const SkinArt& art) {
  SurvivorsPauseAsk asked;
  static WindowName name(kPauseTitle, kPauseId);
  const ImGuiViewport* screen = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(screen->GetCenter(), ImGuiCond_Always, ImVec2(0.5F, 0.5F));
  if (!BeginSkinnedWindow(name.Get(), nullptr, art, SkinnedTitle::kBar, kModalFlags)) {
    EndSkinnedWindow();
    return asked;
  }
  ImGui::Dummy(ImVec2(kPauseWidth, 0.0F));
  DrawReason(view);
  // La garde de la reprise (Q2) : un clic des premières millisecondes ne
  // relance pas la horde.
  if (!view.accepts_input) {
    ImGui::BeginDisabled();
  }
  bool resume = false;
  bool abandon = false;
  if (state.confirming_abandon) {
    ImGui::TextUnformatted(core::Tr("Abandonner la partie ?"));
    abandon = SkinButton(core::Tr("Oui, abandonner"), art);
    ImGui::SameLine();
    if (SkinButton(core::Tr("Non"), art)) {
      state.confirming_abandon = false;
    }
  } else {
    resume = SkinButton(core::Tr("Reprendre"), art, ImVec2(-1.0F, 0.0F));
    if (SkinButton(core::Tr("Abandonner"), art, ImVec2(-1.0F, 0.0F))) {
      state.confirming_abandon = true;
    }
  }
  if (!view.accepts_input) {
    ImGui::EndDisabled();
  }
  ImGui::SeparatorText(core::Tr("Réglages"));
  asked.reduced_effects = Setting(core::Tr("Effets réduits"), view.reduced_effects, art);
  asked.show_measures = Setting(core::Tr("Afficher les mesures"), view.show_measures, art);
  asked.horde_sun_shadows =
      Setting(core::Tr("Ombres du soleil de la horde"), view.horde_sun_shadows, art);
  if (view.show_measures) {
    DrawMeasures(view);
  }
  EndSkinnedWindow();
  asked.resume = resume && view.accepts_input;
  asked.abandon = abandon && view.accepts_input;
  if (asked.abandon) {
    state.confirming_abandon = false;
  }
  return asked;
}

}  // namespace ro::ui
