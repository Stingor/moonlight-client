#include "ro/ui/laphine_window.h"

#include <string>

#include "ro/core/i18n.h"
#include "ro/ui/modal_box.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"

#include "trade_gestures.h"
#include "ui_scale.h"
#include "ui_text.h"

namespace ro::ui {
namespace {

constexpr float kFirstLeftPixels = 140.0F;
constexpr float kFirstTopPixels = 120.0F;
constexpr float kFirstWidthPixels = 620.0F;
constexpr float kFirstHeightPixels = 440.0F;
constexpr float kCloseButtonPixels = 90.0F;
constexpr float kChildRoundingPixels = 6.0F;
// La part de la largeur que prend ce qui est déposé.
constexpr float kDepositedShare = 0.45F;
// Le rang de départ des identités du dépôt, loin de celles du sac.
constexpr int kDepositedIdBase = 2000;
constexpr int kAcceptedIdBase = 4000;
// Sous les deux volets : le refus du serveur, ce qui retient, et le bouton.
constexpr float kFooterLines = 3.0F;

constexpr const char* kConfirmId = "##laphine_confirm";
constexpr const char* kItemMenuId = "##laphine_item_menu";

[[nodiscard]] std::string AmountSuffix(int amount) {
  return amount > 0 ? "x" + TradeAmountText(amount, {}) : std::string();
}

// Pousse le prochain élément au bord droit de la ligne.
void AlignRight(float width) {
  const float right = ImGui::GetContentRegionMax().x - width;
  if (right > ImGui::GetCursorPosX()) {
    ImGui::SetCursorPosX(right);
  }
}

void DrawWarningMark(std::string_view warning) {
  if (warning.empty()) {
    return;
  }
  ImGui::SameLine();
  ImGui::TextColored(kTradeRefusedText, "(!)");
  if (ImGui::IsItemHovered()) {
    SkinTooltip(std::string(warning).c_str());
  }
}

void DrawCandidate(LaphineWindowState& state, const LaphineItemView& row, bool waiting,
                   const SkinArt& art, LaphineWindowAction& action) {
  NoteItemBySlot(DrawTradeItem(row.item, AmountSuffix(row.amount)), row.item, row.client_slot,
                 state.item_menu, action);
  DrawWarningMark(row.warning);
  ImGui::SameLine();
  const char* label = core::Tr("Déposer");
  AlignRight(ImGui::CalcTextSize(label).x + ImGui::GetStyle().FramePadding.x * 2.0F);
  const bool held = waiting || !row.refusal.empty();
  if (held) {
    ImGui::BeginDisabled();
  }
  if (SkinSmallButton(label, art)) {
    action.deposit_slot = row.client_slot;
  }
  if (held) {
    ImGui::EndDisabled();
  }
  if (!row.refusal.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    SkinTooltip(std::string(row.refusal).c_str());
  }
}

void DrawAccepted(LaphineWindowState& state, const LaphineView& view, LaphineWindowAction& action) {
  if (view.accepted.empty() || !ImGui::TreeNode(core::Tr("Items acceptés"))) {
    return;
  }
  for (std::size_t index = 0; index < view.accepted.size(); ++index) {
    const LaphineAcceptedView& accepted = view.accepted[index];
    ImGui::PushID(kAcceptedIdBase + static_cast<int>(index));
    NoteItemById(DrawTradeItem(accepted.item, AmountSuffix(accepted.amount)), accepted.item,
                 accepted.item.item_id, state.item_menu, action);
    ImGui::PopID();
  }
  ImGui::TreePop();
}

void DrawBag(LaphineWindowState& state, const LaphineView& view, const SkinArt& art,
             LaphineWindowAction& action) {
  ImGui::TextUnformatted(core::Tr("Dans l'inventory"));
  ImGui::Separator();
  for (std::size_t index = 0; index < view.candidates.size(); ++index) {
    ImGui::PushID(static_cast<int>(index));
    DrawCandidate(state, view.candidates[index], view.waiting, art, action);
    ImGui::PopID();
  }
  if (view.candidates.empty()) {
    ImGui::TextDisabled("%s", core::Tr("(aucun item accepté dans l'inventory)"));
  }
  ImGui::Spacing();
  DrawAccepted(state, view, action);
  PaintWindowScrollbar(art);
}

void DrawDeposited(LaphineWindowState& state, const LaphineView& view, const SkinArt& art,
                   LaphineWindowAction& action) {
  ImGui::TextUnformatted(core::Tr("Déposé"));
  ImGui::Separator();
  for (std::size_t index = 0; index < view.deposited.size(); ++index) {
    const LaphineItemView& row = view.deposited[index];
    ImGui::PushID(kDepositedIdBase + static_cast<int>(index));
    NoteItemBySlot(DrawTradeItem(row.item, AmountSuffix(row.amount)), row.item, row.client_slot,
                   state.item_menu, action);
    DrawWarningMark(row.warning);
    ImGui::SameLine();
    const float step = ImGui::GetFrameHeight();
    AlignRight(step);
    if (view.waiting) {
      ImGui::BeginDisabled();
    }
    if (SkinButton("x", art, ImVec2(step, step))) {
      action.withdraw_slot = row.client_slot;
    }
    if (view.waiting) {
      ImGui::EndDisabled();
    }
    ImGui::PopID();
  }
  PaintWindowScrollbar(art);
}

void DrawMakeButton(LaphineWindowState& state, const LaphineView& view, const SkinArt& art) {
  if (!view.notice.empty()) {
    ColoredText(kTradeRefusedText, view.notice);
  }
  if (!view.blockers.empty()) {
    ColoredText(kTradeRefusedText, view.blockers[0]);
  }
  const bool held = view.waiting || !view.blockers.empty();
  if (held) {
    ImGui::BeginDisabled();
  }
  const std::string label(view.make_label);
  if (SkinButton(label.c_str(), art, ImVec2(ImGui::GetContentRegionAvail().x, 0.0F))) {
    state.asking = true;
    state.asked_piles_seq = view.piles_seq;
  }
  if (held) {
    ImGui::EndDisabled();
  }
  TipWhenHeld(view.blockers);
}

void DrawConfirmation(LaphineWindowState& state, const LaphineView& view, const SkinArt& art,
                      LaphineWindowAction& action) {
  // 🔴 LA BOÎTE SE DEMANDE AU NIVEAU DE LA FENÊTRE : un surgissant se nomme dans la
  // pile d'identités courante, et celle d'un volet enfant ne s'ouvrirait jamais.
  if (state.asking && !ImGui::IsPopupOpen(kConfirmId)) {
    AskModal(kConfirmId);
  }
  const std::string question(view.confirmation);
  const std::string yes(view.make_label);
  switch (DrawModalBox(kConfirmId, question.c_str(), yes.c_str(), core::Tr("Annuler"), art)) {
    case ModalAnswer::kYes:
      action.make = true;
      action.piles_seq = state.asked_piles_seq;
      state.asking = false;
      break;
    case ModalAnswer::kNo:
      state.asking = false;
      break;
    case ModalAnswer::kWaiting:
      break;
  }
}

}  // namespace

void OpenLaphineOn(LaphineWindowState& state, std::uint32_t opening_seq) {
  state.open = true;
  state.opening_seen = opening_seq;
  state.asking = false;
  state.asked_piles_seq = 0;
}

LaphineWindowAction DrawLaphineWindow(LaphineWindowState& state, const LaphineView& view,
                                      const SkinArt& art, ItemLinkFacts link_facts) {
  LaphineWindowAction action;
  if (!state.open) {
    state.asking = false;
    return action;
  }
  const float scale = Scale();
  ImGui::SetNextWindowPos(ImVec2(kFirstLeftPixels, kFirstTopPixels), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(kFirstWidthPixels * scale, kFirstHeightPixels * scale),
                           ImGuiCond_FirstUseEver);
  const std::string title = std::string(view.title) + "###" + kLaphineWindowId;
  const bool body_shown = BeginSkinnedWindow(title.c_str(), &state.open, art, SkinnedTitle::kBar,
                                             ImGuiWindowFlags_NoCollapse);
  if (body_shown && state.open) {
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, kChildRoundingPixels * scale);
    DrawTradeHint(view.instruction);
    ImGui::Separator();

    const float footer = TradeFooterHeight(art) + kFooterLines * ImGui::GetFrameHeightWithSpacing();
    const float deposited_width = ImGui::GetContentRegionAvail().x * kDepositedShare;
    const float bag_width =
        ImGui::GetContentRegionAvail().x - deposited_width - ImGui::GetStyle().ItemSpacing.x;
    ImGui::BeginChild("##laphine_bag", ImVec2(bag_width, -footer), ImGuiChildFlags_Borders);
    DrawBag(state, view, art, action);
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("##laphine_deposited", ImVec2(deposited_width, -footer),
                      ImGuiChildFlags_Borders);
    DrawDeposited(state, view, art, action);
    ImGui::EndChild();

    DrawMakeButton(state, view, art);
    ImGui::Separator();
    if (SkinButton(core::Tr("Fermer"), art, ImVec2(kCloseButtonPixels * scale, 0.0F))) {
      state.open = false;
    }
    DrawConfirmation(state, view, art, action);
    if (const ChatLinkAsk chosen = DrawTradeItemMenu(kItemMenuId, state.item_menu, link_facts);
        chosen.gesture != ChatLinkGesture::kNothing) {
      action.describe_id = chosen.item_id;
      action.describe_slot = static_cast<std::uint16_t>(chosen.token);
      action.item_gesture = chosen.gesture;
    }
    ImGui::PopStyleVar();
  }
  EndSkinnedWindow();
  if (!state.open) {
    action = LaphineWindowAction{.close = true};
    state.asking = false;
  }
  return action;
}

}  // namespace ro::ui
