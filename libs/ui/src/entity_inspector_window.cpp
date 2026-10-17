#include "ro/ui/entity_inspector_window.h"

#include <algorithm>
#include <cfloat>
#include <string>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"

#include "ui_scale.h"

namespace ro::ui {
namespace {

// ── LES MESURES ─────────────────────────────────────────────────────────────
constexpr float kFirstWidthPixels = 380.0F;
constexpr float kFirstHeightPixels = 360.0F;
constexpr float kLeastWidthPixels = 240.0F;
constexpr float kLeastHeightPixels = 160.0F;
constexpr float kKeyColumnPixels = 120.0F;

// Ce qui se dit à la place du tableau, quand il n'y en a pas.
void DrawPhase(const EntityInspectorView& view) {
  switch (view.phase) {
    case EntityInspectorPhase::kAsking:
      ImGui::TextDisabled("%s", core::Tr("Interrogation du serveur..."));
      return;
    case EntityInspectorPhase::kSilent:
      ImGui::TextWrapped("%s", core::Tr("Pas de réponse du serveur."));
      ImGui::TextDisabled("%s", core::Tr("Le serveur de carte ne connaît peut-être pas encore ce "
                                         "paquet : il attend un redémarrage."));
      return;
    case EntityInspectorPhase::kDenied:
      ImGui::TextWrapped("%s", core::Tr("Le serveur a refusé : ce compte n'est pas staff."));
      return;
    case EntityInspectorPhase::kMissing:
      ImGui::TextWrapped("%s", core::Tr("Le serveur ne connaît aucune entité sous cet "
                                        "identifiant : elle vient de mourir ou de disparaître."));
      return;
    case EntityInspectorPhase::kReady:
      break;
  }
  if (view.rows.empty()) {
    ImGui::TextDisabled("%s", core::Tr("Le serveur n'a rien à dire sur cette entité."));
  }
}

// Le tableau clé / valeur. Un titre de section coupe le tableau : il se lit
// comme un séparateur, sur toute la largeur.
void DrawRows(const EntityInspectorView& view) {
  bool table_open = false;
  int table_rank = 0;
  const auto close_table = [&table_open]() {
    if (table_open) {
      ImGui::EndTable();
      table_open = false;
    }
  };
  for (const EntityInspectorRow& row : view.rows) {
    if (row.value.empty()) {
      close_table();
      ImGui::SeparatorText(row.key.c_str());
      continue;
    }
    if (!table_open) {
      // Un tableau par section, chacun sous son propre nom.
      const std::string table_id = "##inspecteur_lignes_" + std::to_string(table_rank++);
      table_open = ImGui::BeginTable(table_id.c_str(), 2,
                                     ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg);
      if (!table_open) {
        continue;
      }
      ImGui::TableSetupColumn("##cle", ImGuiTableColumnFlags_WidthFixed, Px(kKeyColumnPixels));
      ImGui::TableSetupColumn("##valeur", ImGuiTableColumnFlags_WidthStretch);
    }
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextDisabled("%s", row.key.c_str());
    ImGui::TableNextColumn();
    ImGui::TextWrapped("%s", row.value.c_str());
  }
  close_table();
}

// LE PIED : « Actualiser » à gauche, « Fermer » à droite, du côté de la poignée.
void DrawFooter(EntityInspectorWindowState& state, const EntityInspectorView& view,
                const SkinArt& art, EntityInspectorAnswer& answer) {
  if (view.phase != EntityInspectorPhase::kAsking) {
    if (SkinButton(view.phase == EntityInspectorPhase::kReady ? core::Tr("Actualiser")
                                                              : core::Tr("Redemander"),
                   art)) {
      answer.refresh = true;
    }
    ImGui::SameLine();
  }
  const char* const close = core::Tr("Fermer");
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0F, ImGui::GetContentRegionAvail().x -
                                                                   SkinButtonWidth(close, art)));
  if (SkinButton(close, art)) {
    state.open = false;
  }
}

}  // namespace

EntityInspectorAnswer DrawEntityInspectorWindow(EntityInspectorWindowState& state,
                                                const EntityInspectorView& view,
                                                const SkinArt& art) {
  EntityInspectorAnswer answer;
  if (!state.open) {
    return answer;
  }
  if (state.raise) {
    state.raise = false;
    ImGui::SetNextWindowFocus();
  }
  ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_FirstUseEver,
                          ImVec2(0.5F, 0.5F));
  ImGui::SetNextWindowSize(ImVec2(Px(kFirstWidthPixels), Px(kFirstHeightPixels)),
                           ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSizeConstraints(ImVec2(Px(kLeastWidthPixels), Px(kLeastHeightPixels)),
                                      ImVec2(FLT_MAX, FLT_MAX));
  const std::string name = std::string(core::Tr("Propriétés")) + "###" + kEntityInspectorWindowId;
  constexpr ImGuiWindowFlags kFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar |
                                      ImGuiWindowFlags_NoScrollWithMouse;
  if (BeginSkinnedWindow(name.c_str(), &state.open, art, SkinnedTitle::kBar, kFlags) &&
      state.open) {
    ImGui::TextWrapped("%s", view.subject.c_str());
    ImGui::Separator();
    const float body_height =
        std::max(0.0F, ImGui::GetContentRegionAvail().y - SkinFooterHeight(art));
    if (ImGui::BeginChild("##inspecteur_corps", ImVec2(0.0F, body_height))) {
      DrawPhase(view);
      DrawRows(view);
      PaintWindowScrollbar(art);
    }
    ImGui::EndChild();
    DrawFooter(state, view, art, answer);
  }
  EndSkinnedWindow();
  return answer;
}

}  // namespace ro::ui
