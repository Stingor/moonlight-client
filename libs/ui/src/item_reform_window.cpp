#include "ro/ui/item_reform_window.h"

#include <string>

#include "ro/core/i18n.h"
#include "ro/core/printed.h"
#include "ro/ui/modal_box.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"

#include "trade_gestures.h"
#include "ui_scale.h"
#include "ui_text.h"

namespace ro::ui {
namespace {

constexpr float kFirstLeftPixels = 160.0F;
constexpr float kFirstTopPixels = 130.0F;
constexpr float kFirstWidthPixels = 640.0F;
constexpr float kFirstHeightPixels = 420.0F;
constexpr float kCloseButtonPixels = 90.0F;
constexpr float kChildRoundingPixels = 6.0F;
// La part de la largeur que prend la liste des pièces.
constexpr float kListShare = 0.48F;
// Le rang de départ des identités des matériaux, loin de celles des pièces.
constexpr int kMaterialIdBase = 2000;
// Sous les deux volets : le refus du serveur, ce qui retient, et le bouton.
constexpr float kFooterLines = 3.0F;

constexpr const char* kConfirmId = "##item_reform_confirm";
constexpr const char* kItemMenuId = "##item_reform_item_menu";

// Pousse le prochain élément au bord droit de la ligne.
void AlignRight(float width) {
  const float right = ImGui::GetContentRegionMax().x - width;
  if (right > ImGui::GetCursorPosX()) {
    ImGui::SetCursorPosX(right);
  }
}

void DrawCandidate(ItemReformWindowState& state, const ItemReformPieceView& row, bool waiting,
                   const SkinArt& art, ItemReformWindowAction& action) {
  NoteItemBySlot(DrawTradeItem(row.item), row.item, row.client_slot, state.item_menu, action);
  ImGui::SameLine();
  const char* label = row.chosen ? core::Tr("Choisie") : core::Tr("Choisir");
  AlignRight(ImGui::CalcTextSize(label).x + ImGui::GetStyle().FramePadding.x * 2.0F);
  const bool held = waiting || row.chosen || !row.refusal.empty();
  if (held) {
    ImGui::BeginDisabled();
  }
  if (SkinSmallButton(label, art)) {
    action.choose_slot = row.client_slot;
  }
  if (held) {
    ImGui::EndDisabled();
  }
  if (!row.refusal.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    SkinTooltip(std::string(row.refusal).c_str());
  }
}

void DrawList(ItemReformWindowState& state, const ItemReformView& view, const SkinArt& art,
              ItemReformWindowAction& action) {
  ImGui::TextUnformatted(core::Tr("Pièces réformables"));
  ImGui::Separator();
  for (std::size_t index = 0; index < view.candidates.size(); ++index) {
    ImGui::PushID(static_cast<int>(index));
    DrawCandidate(state, view.candidates[index], view.waiting, art, action);
    ImGui::PopID();
  }
  if (view.candidates.empty()) {
    ImGui::TextDisabled("%s", core::Tr("(aucune pièce réformable dans l'inventory)"));
  }
  PaintWindowScrollbar(art);
}

void DrawMaterial(ItemReformWindowState& state, const ItemReformMaterialView& material,
                  ItemReformWindowAction& action) {
  NoteItemById(DrawTradeItem(material.item), material.item, material.item.item_id, state.item_menu,
               action);
  ImGui::SameLine();
  const std::string count = core::Printed("%d / %d", material.owned, material.needed);
  AlignRight(ImGui::CalcTextSize(count.c_str()).x);
  if (material.owned < material.needed) {
    ColoredText(kTradeRefusedText, count);
  } else {
    ImGui::TextUnformatted(count.c_str());
  }
}

void DrawDetail(ItemReformWindowState& state, const ItemReformView& view, const SkinArt& art,
                ItemReformWindowAction& action) {
  if (view.chosen.item_id == 0) {
    ImGui::TextDisabled("%s", core::Tr("Choisissez une pièce à gauche."));
    PaintWindowScrollbar(art);
    return;
  }
  ImGui::TextUnformatted(core::Tr("Pièce"));
  ImGui::PushID("chosen");
  NoteItemById(DrawTradeItem(view.chosen), view.chosen, view.chosen.item_id, state.item_menu,
               action);
  ImGui::PopID();
  ImGui::TextUnformatted(core::Tr("Devient"));
  ImGui::PushID("result");
  NoteItemById(DrawTradeItem(view.result), view.result, view.result.item_id, state.item_menu,
               action);
  ImGui::PopID();
  ImGui::Spacing();
  ImGui::TextUnformatted(core::Tr("Matériaux"));
  ImGui::Separator();
  for (std::size_t index = 0; index < view.materials.size(); ++index) {
    ImGui::PushID(kMaterialIdBase + static_cast<int>(index));
    DrawMaterial(state, view.materials[index], action);
    ImGui::PopID();
  }
  if (view.materials.empty()) {
    ImGui::TextDisabled("%s", core::Tr("(aucun)"));
  }
  PaintWindowScrollbar(art);
}

void DrawReformButton(ItemReformWindowState& state, const ItemReformView& view,
                      const SkinArt& art) {
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
  if (SkinButton(core::Tr("Réformer"), art, ImVec2(ImGui::GetContentRegionAvail().x, 0.0F))) {
    state.asking = true;
    state.asked_choice_seq = view.choice_seq;
  }
  if (held) {
    ImGui::EndDisabled();
  }
  TipWhenHeld(view.blockers);
}

void DrawConfirmation(ItemReformWindowState& state, const ItemReformView& view, const SkinArt& art,
                      ItemReformWindowAction& action) {
  // 🔴 LA BOÎTE SE DEMANDE AU NIVEAU DE LA FENÊTRE : un surgissant se nomme dans la
  // pile d'identités courante, et celle d'un volet enfant ne s'ouvrirait jamais.
  if (state.asking && !ImGui::IsPopupOpen(kConfirmId)) {
    AskModal(kConfirmId);
  }
  const std::string question(view.confirmation);
  switch (
      DrawModalBox(kConfirmId, question.c_str(), core::Tr("Réformer"), core::Tr("Annuler"), art)) {
    case ModalAnswer::kYes:
      action.reform = true;
      action.choice_seq = state.asked_choice_seq;
      state.asking = false;
      break;
    case ModalAnswer::kNo:
      state.asking = false;
      break;
    case ModalAnswer::kWaiting:
      break;
  }
}

void DrawBody(ItemReformWindowState& state, const ItemReformView& view, const SkinArt& art,
              ItemReformWindowAction& action) {
  DrawTradeHint(view.instruction);
  ImGui::Separator();
  const float footer = TradeFooterHeight(art) + kFooterLines * ImGui::GetFrameHeightWithSpacing();
  const float list_width = ImGui::GetContentRegionAvail().x * kListShare;
  const float detail_width =
      ImGui::GetContentRegionAvail().x - list_width - ImGui::GetStyle().ItemSpacing.x;
  ImGui::BeginChild("##item_reform_list", ImVec2(list_width, -footer), ImGuiChildFlags_Borders);
  DrawList(state, view, art, action);
  ImGui::EndChild();
  ImGui::SameLine();
  ImGui::BeginChild("##item_reform_detail", ImVec2(detail_width, -footer), ImGuiChildFlags_Borders);
  DrawDetail(state, view, art, action);
  ImGui::EndChild();

  DrawReformButton(state, view, art);
  ImGui::Separator();
  if (SkinButton(core::Tr("Fermer"), art, ImVec2(kCloseButtonPixels * Scale(), 0.0F))) {
    state.open = false;
  }
}

}  // namespace

void OpenItemReformOn(ItemReformWindowState& state, std::uint32_t opening_seq) {
  state.open = true;
  state.opening_seen = opening_seq;
  state.asking = false;
  state.asked_choice_seq = 0;
}

ItemReformWindowAction DrawItemReformWindow(ItemReformWindowState& state,
                                            const ItemReformView& view, const SkinArt& art,
                                            ItemLinkFacts link_facts) {
  ItemReformWindowAction action;
  if (!state.open) {
    state.asking = false;
    return action;
  }
  const float scale = Scale();
  ImGui::SetNextWindowPos(ImVec2(kFirstLeftPixels, kFirstTopPixels), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(kFirstWidthPixels * scale, kFirstHeightPixels * scale),
                           ImGuiCond_FirstUseEver);
  const std::string title = std::string(view.title) + "###" + kItemReformWindowId;
  const bool body_shown = BeginSkinnedWindow(title.c_str(), &state.open, art, SkinnedTitle::kBar,
                                             ImGuiWindowFlags_NoCollapse);
  if (body_shown && state.open) {
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, kChildRoundingPixels * scale);
    DrawBody(state, view, art, action);
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
    action = ItemReformWindowAction{.close = true};
    state.asking = false;
  }
  return action;
}

}  // namespace ro::ui
