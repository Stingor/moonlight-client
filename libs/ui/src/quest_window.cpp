// Le journal des quêtes : la fenêtre, ses onglets, son pied, et ce qui s'ouvre
// au niveau de la fenêtre. Les deux volets sont dans `quest_window_list.cpp`
// et `quest_window_detail.cpp`.

#include "ro/ui/quest_window.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cstdio>
#include <string>
#include <utility>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/bug_report_window.h"
#include "ro/ui/modal_box.h"
#include "ro/ui/skin_palette.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"

#include "quest_window_parts.h"

namespace ro::ui {
namespace {

using namespace quest_window;

// La taille au premier affichage, et la plus petite qu'on puisse lui donner.
constexpr float kFirstWidth = 760.0F;
constexpr float kFirstHeight = 500.0F;
constexpr float kLeastWidth = 600.0F;
constexpr float kLeastHeight = 380.0F;
// L'écart entre la liste et le détail.
constexpr float kPaneGap = 6.0F;

// Les actions de la barre de titre, dans leur ordre de pose.
constexpr int kBugReportRank = 0;

// 🔴 OUVERTES AU NIVEAU DE LA FENÊTRE, là où elles se dessinent : voir
// `PaneRequests`.
constexpr const char* kForgetAllId = "##quetes_tout_oublier";
constexpr const char* kRewardMenuId = "##quetes_menu_recompense";
// L'onglet qu'ImGui montrait à l'image d'avant, retenu dans le rangement de la
// fenêtre.
constexpr const char* kTabShownSlot = "##quetes_onglet_montre";
constexpr int kNoTabShown = -1;

// Assez pour « Historique (18446744073709551615)###quetes_historique » dans
// toute langue.
constexpr std::size_t kTabLabelBytes = 128;

// LE LIBELLÉ D'UN ONGLET, son compte compris. ⚠ L'IDENTITÉ APRÈS `###` NE
// BOUGE PAS : sans elle, l'onglet choisi sauterait quand le compte change.
//
// ⚠ UN GABARIT LITTÉRAL PAR CAS : le compilateur ne vérifie les arguments que
// d'un format qu'il voit.
[[nodiscard]] std::string TabLabel(QuestTab tab, std::size_t count) {
  std::array<char, kTabLabelBytes> shown{};
  const char* identity = "";
  switch (tab) {
    case QuestTab::kActive:
      std::snprintf(shown.data(), shown.size(), core::Tr("En cours (%zu)"), count);
      identity = "###quetes_en_cours";
      break;
    case QuestTab::kSuspended:
      std::snprintf(shown.data(), shown.size(), core::Tr("Suspendues (%zu)"), count);
      identity = "###quetes_suspendues";
      break;
    case QuestTab::kHistory:
      std::snprintf(shown.data(), shown.size(), core::Tr("Historique (%zu)"), count);
      identity = "###quetes_historique";
      break;
    case QuestTab::kRecommended:
      std::snprintf(shown.data(), shown.size(), core::Tr("Recommandées (%zu)"), count);
      identity = "###quetes_recommandees";
      break;
    case QuestTab::kWaiting:
      std::snprintf(shown.data(), shown.size(), core::Tr("En attente (%zu)"), count);
      identity = "###quetes_en_attente";
      break;
  }
  return std::string(shown.data()) + identity;
}

// LES ONGLETS. L'état mène : un onglet posé par `app` s'impose à ImGui, et un
// clic du joueur se reporte dans l'état.
//
// ⚠ UN ONGLET IMPOSÉ NE SE MONTRE QU'À L'IMAGE SUIVANTE : pendant celle-ci,
// ImGui rend encore l'ancien, qui ne doit pas écraser l'état.
void DrawTabs(QuestWindowState& state, const QuestWindowView& view, const SkinArt& skin) {
  if (!BeginSkinTabBar("##quetes_onglets", skin)) {
    return;
  }
  ImGuiStorage* const storage = ImGui::GetStateStorage();
  const ImGuiID slot = ImGui::GetID(kTabShownSlot);
  const int shown = storage->GetInt(slot, kNoTabShown);
  const int wanted = static_cast<int>(state.tab);
  const bool imposed = shown != wanted;
  int reported = kNoTabShown;
  for (const QuestTab tab : kQuestTabOrder) {
    const auto value = static_cast<std::size_t>(tab);
    const std::string label = TabLabel(tab, view.tab_counts[value]);
    const ImGuiTabItemFlags flags = imposed && static_cast<int>(value) == wanted
                                        ? ImGuiTabItemFlags_SetSelected
                                        : ImGuiTabItemFlags_None;
    if (ImGui::BeginTabItem(label.c_str(), nullptr, flags)) {
      reported = static_cast<int>(value);
      ImGui::EndTabItem();
    }
  }
  if (imposed) {
    if (reported == wanted) {
      storage->SetInt(slot, reported);
    }
  } else if (reported != kNoTabShown && reported != wanted) {
    state.tab = static_cast<QuestTab>(reported);
    storage->SetInt(slot, reported);
  }
  EndSkinTabBar();
}

// LE PIED : la case du suivi à gauche, « Fermer » à droite, sur une même
// rangée de la hauteur d'un bouton.
void DrawFooter(QuestWindowState& state, const QuestWindowView& view, const SkinArt& skin,
                QuestWindowAction& action) {
  const float row_top = ImGui::GetCursorPosY();
  const float row_height = SkinButtonHeight(skin);
  ImGui::SetCursorPosY(row_top + std::max(0.0F, (row_height - ImGui::GetFrameHeight()) * 0.5F));
  bool tracker = view.tracker_shown;
  if (SkinCheckBox(core::Tr("Montrer le suivi"), &tracker, skin)) {
    action.show_tracker = tracker;
  }
  SkinTooltip(core::Tr("Le panneau des quêtes et des succès suivis, sur le jeu"));
  const char* const close = core::Tr("Fermer");
  const float close_width = SkinButtonWidth(close, skin);
  ImGui::SameLine();
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                       std::max(0.0F, ImGui::GetContentRegionAvail().x - close_width));
  ImGui::SetCursorPosY(row_top);
  if (SkinButton(close, skin)) {
    state.open = false;
  }
}

// « TOUT OUBLIER », confirmé : la boîte s'ouvre au niveau de la fenêtre.
void DrawForgetAll(const PaneRequests& requests, const SkinArt& skin, QuestWindowAction& action) {
  if (requests.forget_all_asked) {
    AskModal(kForgetAllId);
  }
  const ModalAnswer answer =
      DrawModalBox(kForgetAllId, core::Tr("Oublier toutes les quêtes de l'historique ?"),
                   core::Tr("Tout oublier"), core::Tr("Annuler"), skin);
  if (answer == ModalAnswer::kYes) {
    action.forget_all = true;
  }
}

// LE MENU D'UNE RÉCOMPENSE, le même que celui de tout lien d'objet.
void DrawRewardMenu(QuestWindowState& state, QuestWindowAction& action) {
  DrawItemLinkMenu(kRewardMenuId, state.reward_menu, action.item);
}

void DrawBody(QuestWindowState& state, const QuestWindowView& view, const QuestArt& art,
              const SkinArt& skin, QuestWindowAction& action) {
  action.open_settings = TitleBulletClicked();
  DrawTabs(state, view, skin);
  PaneRequests requests{.reward_menu = state.reward_menu};
  const float panes_height =
      std::max(0.0F, ImGui::GetContentRegionAvail().y - SkinFooterHeight(skin));
  DrawListColumn(state, view, art, skin, panes_height, action, requests);
  ImGui::SameLine(0.0F, Px(kPaneGap));
  // LE DÉTAIL SUR LE PAPIER, comme la liste.
  ImGui::PushStyleColor(ImGuiCol_ChildBg, Paper());
  const bool detail_shown =
      ImGui::BeginChild("##quetes_detail", ImVec2(0.0F, panes_height), ImGuiChildFlags_Borders);
  ImGui::PopStyleColor();
  if (detail_shown) {
    PushPaperStyle(PaintingLook());
    DrawDetail(view, art, skin, action, requests);
    ImGui::PopStyleColor(kPaperStyleColors);
    PaintWindowScrollbar(skin);
  }
  ImGui::EndChild();
  DrawFooter(state, view, skin, action);
  DrawForgetAll(requests, skin, action);
  DrawRewardMenu(state, action);
}

}  // namespace

QuestWindowAction DrawQuestWindow(QuestWindowState& state, const QuestWindowView& view,
                                  const QuestArt& art, const SkinArt& skin) {
  QuestWindowAction action;
  if (!state.open) {
    return action;
  }
  if (std::exchange(state.raise, false)) {
    ImGui::SetNextWindowFocus();
  }
  ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_FirstUseEver,
                          ImVec2(0.5F, 0.5F));
  ImGui::SetNextWindowSize(ImVec2(Px(kFirstWidth), Px(kFirstHeight)), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSizeConstraints(ImVec2(Px(kLeastWidth), Px(kLeastHeight)),
                                      ImVec2(FLT_MAX, FLT_MAX));
  const std::string name = std::string(core::Tr("Quêtes")) + "###" + kQuestWindowId;
  PoseBugReportButton(state.bug_report);
  SetNextWindowBulletClickable(core::Tr("Réglages du panneau de suivi"));
  constexpr ImGuiWindowFlags kFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar |
                                      ImGuiWindowFlags_NoScrollWithMouse;
  const bool body_shown =
      BeginSkinnedWindow(name.c_str(), &state.open, skin, SkinnedTitle::kBar, kFlags);
  // ⚠ LU MÊME CORPS CACHÉ : la barre garde ses boutons.
  action.report_bug = state.bug_report && TitleActionClicked(kBugReportRank);
  if (body_shown) {
    DrawBody(state, view, art, skin, action);
  }
  EndSkinnedWindow();
  return action;
}

}  // namespace ro::ui
