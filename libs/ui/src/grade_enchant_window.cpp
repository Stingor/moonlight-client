// L'amélioration de grade. Voir l'en-tête.

#include "ro/ui/grade_enchant_window.h"

#include <algorithm>
#include <cfloat>
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

constexpr float kFirstLeftPixels = 180.0F;
constexpr float kFirstTopPixels = 120.0F;
constexpr float kFirstWidthPixels = 660.0F;
constexpr float kFirstHeightPixels = 460.0F;
// EN DEÇÀ, les deux volets n'ont plus la place d'un nom et de ce qui le suit.
constexpr float kLeastWidthPixels = 500.0F;
constexpr float kLeastHeightPixels = 340.0F;
constexpr float kCloseButtonPixels = 90.0F;
constexpr float kChildRoundingPixels = 6.0F;
// La part de la largeur que prend la liste des armes.
constexpr float kListShare = 0.44F;
// Les identités des mises et de la poussière, loin de celles des armes.
constexpr int kStakeIdBase = 2000;
constexpr int kBlessingId = 3000;
// Le retrait de la ligne d'une mise sous son bouton radio.
constexpr float kStakeIndentPixels = 22.0F;
// Sous les deux volets : le mot du serveur, ce qui retient, et le bouton.
constexpr float kFooterLines = 3.0F;
// Le vert d'une réussite.
constexpr ImVec4 kGoodText = ImVec4(0.10F, 0.55F, 0.20F, 1.0F);

constexpr const char* kConfirmId = "##grade_enchant_confirm";
constexpr const char* kItemMenuId = "##grade_enchant_item_menu";

void DrawCandidate(GradeEnchantWindowState& state, const GradeEnchantPieceView& row, bool waiting,
                   const SkinArt& art, GradeEnchantWindowAction& action) {
  const char* label = row.chosen ? core::Tr("Choisie") : core::Tr("Choisir");
  const float button = ImGui::CalcTextSize(label).x + ImGui::GetStyle().FramePadding.x * 2.0F;
  NoteItemBySlot(DrawTradeItemBefore(row.item, button), row.item, row.client_slot, state.item_menu,
                 action);
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

void DrawList(GradeEnchantWindowState& state, const GradeEnchantView& view, const SkinArt& art,
              GradeEnchantWindowAction& action) {
  ImGui::TextUnformatted(core::Tr("Armes"));
  ImGui::Separator();
  for (std::size_t index = 0; index < view.candidates.size(); ++index) {
    ImGui::PushID(static_cast<int>(index));
    DrawCandidate(state, view.candidates[index], view.waiting, art, action);
    ImGui::PopID();
  }
  if (view.candidates.empty()) {
    ImGui::TextDisabled("%s", core::Tr("(aucune arme dans l'inventory)"));
  }
  PaintWindowScrollbar(art);
}

// UN OBJET ET SON COMPTE, calé à droite : rouge quand il manque.
void DrawCountedItem(GradeEnchantWindowState& state, const TradeItemView& item, int owned,
                     int needed, GradeEnchantWindowAction& action) {
  const std::string count = core::Printed("%d / %d", owned, needed);
  NoteItemById(DrawTradeItemBefore(item, ImGui::CalcTextSize(count.c_str()).x), item, item.item_id,
               state.item_menu, action);
  ImGui::AlignTextToFramePadding();
  if (owned < needed) {
    ColoredText(TradeInk(kTradeRefusedText), count);
  } else {
    ImGui::TextUnformatted(count.c_str());
  }
}

void DrawStakes(GradeEnchantWindowState& state, const GradeEnchantView& view, const SkinArt& art,
                GradeEnchantWindowAction& action) {
  ImGui::TextUnformatted(core::Tr("Mise"));
  ImGui::Separator();
  const int count = static_cast<int>(view.stakes.size());
  state.stake = std::clamp(state.stake, 0, std::max(count - 1, 0));
  for (int index = 0; index < count; ++index) {
    const GradeEnchantStakeView& stake = view.stakes[static_cast<std::size_t>(index)];
    ImGui::PushID(kStakeIdBase + index);
    const std::string label = TradeAmountText(stake.zeny, "z");
    if (SkinRadio(label.c_str(), state.stake == index, art) && !view.waiting) {
      state.stake = index;
    }
    ImGui::SameLine();
    ColoredText(stake.risky ? TradeInk(kTradeRefusedText) : kGoodText, stake.risk);
    ImGui::Indent(Px(kStakeIndentPixels));
    DrawCountedItem(state, stake.item, stake.owned, stake.needed, action);
    ImGui::Unindent(Px(kStakeIndentPixels));
    ImGui::PopID();
  }
}

void DrawBlessing(GradeEnchantWindowState& state, const GradeEnchantView& view, const SkinArt& art,
                  GradeEnchantWindowAction& action) {
  if (view.blessing.item_id == 0 || view.blessing_most_steps <= 0) {
    state.blessing_steps = 0;
    return;
  }
  ImGui::Spacing();
  ImGui::TextUnformatted(core::Tr("Poussière bénie"));
  ImGui::Separator();
  state.blessing_steps = std::clamp(state.blessing_steps, 0, view.blessing_most_steps);
  ImGui::PushID(kBlessingId);
  DrawCountedItem(state, view.blessing, view.blessing_owned,
                  view.blessing_per_step * state.blessing_steps, action);
  const std::string format =
      core::Printed(core::Tr("%%d dose(s), +%d %% par dose"), view.blessing_bonus_percent);
  ImGui::SetNextItemWidth(-FLT_MIN);
  if (view.waiting) {
    ImGui::BeginDisabled();
  }
  static_cast<void>(SkinSliderInt("##blessing_steps", &state.blessing_steps, 0,
                                  view.blessing_most_steps, art, format.c_str(), 1));
  if (view.waiting) {
    ImGui::EndDisabled();
  }
  ImGui::PopID();
}

void DrawDetail(GradeEnchantWindowState& state, const GradeEnchantView& view, const SkinArt& art,
                GradeEnchantWindowAction& action) {
  if (view.chosen.item_id == 0) {
    ImGui::TextDisabled("%s", core::Tr("Choisissez une arme à gauche."));
    PaintWindowScrollbar(art);
    return;
  }
  ImGui::TextUnformatted(core::Tr("Pièce"));
  ImGui::PushID("chosen");
  NoteItemById(DrawTradeItem(view.chosen), view.chosen, view.chosen.item_id, state.item_menu,
               action);
  ImGui::PopID();
  ImGui::TextUnformatted(std::string(view.step).c_str());
  if (!view.offer_ready) {
    ImGui::TextDisabled("%s", core::Tr("Le serveur évalue la pièce…"));
    PaintWindowScrollbar(art);
    return;
  }
  if (!view.offer_refusal.empty()) {
    ColoredText(TradeInk(kTradeRefusedText), view.offer_refusal);
    PaintWindowScrollbar(art);
    return;
  }
  const int steps = view.blessing.item_id == 0 ? 0 : state.blessing_steps;
  const int chance =
      std::min(static_cast<int>(view.chance_percent) + steps * view.blessing_bonus_percent, 100);
  ImGui::TextUnformatted(core::Printed(core::Tr("Chance de réussite : %d %%"), chance).c_str());
  ImGui::Spacing();
  DrawStakes(state, view, art, action);
  DrawBlessing(state, view, art, action);
  PaintWindowScrollbar(art);
}

void DrawEnchantButton(GradeEnchantWindowState& state, const GradeEnchantView& view,
                       const SkinArt& art) {
  if (!view.notice.empty()) {
    ColoredText(view.notice_good ? kGoodText : TradeInk(kTradeRefusedText), view.notice);
  }
  if (!view.blockers.empty()) {
    ColoredText(TradeInk(kTradeRefusedText), view.blockers[0]);
  }
  const bool held = view.waiting || !view.blockers.empty();
  if (held) {
    ImGui::BeginDisabled();
  }
  if (SkinButton(core::Tr("Améliorer le grade"), art,
                 ImVec2(ImGui::GetContentRegionAvail().x, 0.0F))) {
    state.asking = true;
    state.asked_choice_seq = view.choice_seq;
  }
  if (held) {
    ImGui::EndDisabled();
  }
  TipWhenHeld(view.blockers);
}

void DrawConfirmation(GradeEnchantWindowState& state, const GradeEnchantView& view,
                      const SkinArt& art, GradeEnchantWindowAction& action) {
  // 🔴 LA BOÎTE SE DEMANDE AU NIVEAU DE LA FENÊTRE : celle d'un volet enfant ne
  // s'ouvrirait jamais.
  if (state.asking && !ImGui::IsPopupOpen(kConfirmId)) {
    AskModal(kConfirmId);
  }
  const std::string question(view.confirmation);
  switch (
      DrawModalBox(kConfirmId, question.c_str(), core::Tr("Améliorer"), core::Tr("Annuler"), art)) {
    case ModalAnswer::kYes:
      action.enchant = true;
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

void DrawBody(GradeEnchantWindowState& state, const GradeEnchantView& view, const SkinArt& art,
              GradeEnchantWindowAction& action) {
  DrawTradeHint(view.instruction);
  ImGui::Separator();
  const float footer = TradeFooterHeight(art) + kFooterLines * ImGui::GetFrameHeightWithSpacing();
  const float list_width = ImGui::GetContentRegionAvail().x * kListShare;
  const float detail_width =
      ImGui::GetContentRegionAvail().x - list_width - ImGui::GetStyle().ItemSpacing.x;
  ImGui::BeginChild("##grade_enchant_list", ImVec2(list_width, -footer), ImGuiChildFlags_Borders);
  DrawList(state, view, art, action);
  ImGui::EndChild();
  ImGui::SameLine();
  ImGui::BeginChild("##grade_enchant_detail", ImVec2(detail_width, -footer),
                    ImGuiChildFlags_Borders);
  DrawDetail(state, view, art, action);
  ImGui::EndChild();

  DrawEnchantButton(state, view, art);
  ImGui::Separator();
  if (SkinButton(core::Tr("Fermer"), art, ImVec2(kCloseButtonPixels * Scale(), 0.0F))) {
    state.open = false;
  }
}

}  // namespace

void OpenGradeEnchantOn(GradeEnchantWindowState& state, std::uint32_t opening_seq) {
  state.open = true;
  state.opening_seen = opening_seq;
  state.stake = 0;
  state.blessing_steps = 0;
  state.asking = false;
  state.asked_choice_seq = 0;
}

GradeEnchantWindowAction DrawGradeEnchantWindow(GradeEnchantWindowState& state,
                                                const GradeEnchantView& view, const SkinArt& art,
                                                ItemLinkFacts link_facts) {
  GradeEnchantWindowAction action;
  if (!state.open) {
    state.asking = false;
    return action;
  }
  const float scale = Scale();
  ImGui::SetNextWindowPos(ImVec2(kFirstLeftPixels, kFirstTopPixels), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(kFirstWidthPixels * scale, kFirstHeightPixels * scale),
                           ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSizeConstraints(ImVec2(Px(kLeastWidthPixels), Px(kLeastHeightPixels)),
                                      ImVec2(FLT_MAX, FLT_MAX));
  const std::string title =
      std::string(core::Tr("Amélioration de grade")) + "###" + kGradeEnchantWindowId;
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
    action = GradeEnchantWindowAction{.close = true};
    state.asking = false;
  }
  return action;
}

}  // namespace ro::ui
