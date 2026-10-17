#include "ro/ui/chat_block_log_window.h"

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
constexpr float kFirstWidthPixels = 460.0F;
constexpr float kFirstHeightPixels = 320.0F;
constexpr float kLeastWidthPixels = 300.0F;
constexpr float kLeastHeightPixels = 160.0F;
constexpr int kColumnCount = 4;

// Ce qui se dit à la place du tableau, quand il n'y en a pas.
void DrawPhase(const ChatBlockLogView& view) {
  switch (view.phase) {
    case ChatBlockLogPhase::kAsking:
      ImGui::TextDisabled("%s", core::Tr("Interrogation du serveur..."));
      return;
    case ChatBlockLogPhase::kSilent:
      ImGui::TextWrapped("%s", core::Tr("Pas de réponse du serveur."));
      ImGui::TextDisabled("%s", core::Tr("Le serveur de carte ne connaît peut-être pas encore ce "
                                         "paquet : il attend un redémarrage."));
      return;
    case ChatBlockLogPhase::kDenied:
      ImGui::TextWrapped("%s", core::Tr("Le serveur a refusé : ce compte n'est pas staff."));
      return;
    case ChatBlockLogPhase::kMissing:
      ImGui::TextWrapped("%s", core::Tr("Ce joueur n'est plus en ligne : le serveur ne retrouve "
                                        "pas son personnage."));
      return;
    case ChatBlockLogPhase::kLogOff:
      ImGui::TextWrapped("%s", core::Tr("Le journal des commandes est éteint sur ce serveur : "
                                        "aucun blocage n'y est consigné."));
      return;
    case ChatBlockLogPhase::kReadFailed:
      ImGui::TextWrapped("%s", core::Tr("Le serveur n'a pas pu lire son journal des commandes : "
                                        "sa console dit pourquoi."));
      return;
    case ChatBlockLogPhase::kReady:
      break;
  }
  if (view.rows.empty()) {
    ImGui::TextDisabled("%s", core::Tr("Aucun blocage de chat consigné pour ce personnage."));
  }
}

// La liste datée, la plus récente en haut.
void DrawRows(const ChatBlockLogView& view) {
  if (view.phase != ChatBlockLogPhase::kReady || view.rows.empty()) {
    return;
  }
  if (!ImGui::BeginTable("##journal_blocages_lignes", kColumnCount,
                         ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg)) {
    return;
  }
  ImGui::TableSetupColumn(core::Tr("Date"));
  ImGui::TableSetupColumn(core::Tr("Auteur"));
  ImGui::TableSetupColumn(core::Tr("Commande"));
  ImGui::TableSetupColumn(core::Tr("Durée"), ImGuiTableColumnFlags_WidthStretch);
  ImGui::TableHeadersRow();
  for (const ChatBlockLogRow& row : view.rows) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(row.when.c_str());
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(row.author.c_str());
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(row.command.c_str());
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(row.duration.c_str());
  }
  ImGui::EndTable();
}

// LE PIED : « Actualiser » à gauche, « Fermer » à droite, du côté de la poignée.
void DrawFooter(ChatBlockLogWindowState& state, const ChatBlockLogView& view, const SkinArt& art,
                ChatBlockLogAnswer& answer) {
  if (view.phase != ChatBlockLogPhase::kAsking) {
    if (SkinButton(view.phase == ChatBlockLogPhase::kReady ? core::Tr("Actualiser")
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

ChatBlockLogAnswer DrawChatBlockLogWindow(ChatBlockLogWindowState& state,
                                          const ChatBlockLogView& view, const SkinArt& art) {
  ChatBlockLogAnswer answer;
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
  const std::string name =
      std::string(core::Tr("Journal des blocages de chat")) + "###" + kChatBlockLogWindowId;
  constexpr ImGuiWindowFlags kFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar |
                                      ImGuiWindowFlags_NoScrollWithMouse;
  if (BeginSkinnedWindow(name.c_str(), &state.open, art, SkinnedTitle::kBar, kFlags) &&
      state.open) {
    ImGui::TextWrapped("%s", view.subject.c_str());
    ImGui::Separator();
    const float body_height =
        std::max(0.0F, ImGui::GetContentRegionAvail().y - SkinFooterHeight(art));
    if (ImGui::BeginChild("##journal_blocages_corps", ImVec2(0.0F, body_height))) {
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
