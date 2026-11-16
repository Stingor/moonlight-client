#include "ro/ui/item_enchant_window.h"

#include <algorithm>
#include <cfloat>
#include <string>

#include <imgui_internal.h>

#include "ro/core/i18n.h"
#include "ro/core/printed.h"
#include "ro/ui/modal_box.h"
#include "ro/ui/shortcut_bar.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"

#include "trade_gestures.h"
#include "ui_scale.h"
#include "ui_text.h"

namespace ro::ui {
namespace {

constexpr float kFirstLeftPixels = 140.0F;
constexpr float kFirstTopPixels = 110.0F;
constexpr float kFirstWidthPixels = 720.0F;
constexpr float kFirstHeightPixels = 500.0F;
constexpr float kLeastWidthPixels = 560.0F;
constexpr float kLeastHeightPixels = 380.0F;
constexpr float kCloseButtonPixels = 90.0F;
constexpr float kChildRoundingPixels = 6.0F;
// La part de la largeur que prend la liste des pièces.
constexpr float kListShare = 0.36F;
// Sous les deux volets : le mot du serveur, ce qui retient, et le bouton.
constexpr float kFooterLines = 3.0F;
// Les rangs d'identité des cases, des lignes et des matériaux, loin de ceux des pièces.
constexpr int kSlotIdBase = 1000;
constexpr int kOptionIdBase = 2000;
constexpr int kMaterialIdBase = 3000;
constexpr int kPreviewIdBase = 4000;
// Ce que l'historique montre au plus.
constexpr std::size_t kHistoryShown = 8;
// Les cases s'affichent numérotées à partir de un, comme dans le jeu.
constexpr int kShownSlotOffset = 1;

constexpr ImVec4 kGoodText = ImVec4(0.10F, 0.55F, 0.20F, 1.0F);
// La case que le prochain enchantement remplirait.
constexpr ImVec4 kNextSlotText = ImVec4(0.20F, 0.40F, 0.80F, 1.0F);

constexpr const char* kConfirmId = "##item_enchant_confirm";
constexpr const char* kItemMenuId = "##item_enchant_item_menu";

void DrawCandidate(ItemEnchantWindowState& state, const ItemEnchantPieceView& row, bool waiting,
                   const SkinArt& art, ItemEnchantWindowAction& action) {
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
  // OÙ EN SONT SES CASES, sous son nom : de quoi choisir sans ouvrir chaque pièce.
  if (!row.progress.empty()) {
    ImGui::Indent();
    if (row.full) {
      ColoredText(kGoodText, row.progress);
    } else {
      ImGui::TextDisabled("%.*s", static_cast<int>(row.progress.size()), row.progress.data());
    }
    ImGui::Unindent();
  }
}

// LES TENTATIVES DE LA PARTIE, la plus récente en tête.
void DrawHistory(const ItemEnchantView& view) {
  if (view.history.empty()) {
    return;
  }
  ImGui::Spacing();
  ImGui::TextUnformatted(core::Tr("Dernières tentatives"));
  ImGui::Separator();
  const std::size_t shown = std::min(view.history.size(), kHistoryShown);
  for (std::size_t index = 0; index < shown; ++index) {
    const ItemEnchantHistoryView& entry = view.history[index];
    ImGui::PushStyleColor(ImGuiCol_Text, entry.good ? kGoodText : kTradeRefusedText);
    ImGui::TextWrapped("%.*s", static_cast<int>(entry.text.size()), entry.text.data());
    ImGui::PopStyleColor();
  }
}

void DrawList(ItemEnchantWindowState& state, const ItemEnchantView& view, const SkinArt& art,
              ItemEnchantWindowAction& action) {
  ImGui::TextUnformatted(core::Tr("Pièces enchantables"));
  ImGui::Separator();
  for (std::size_t index = 0; index < view.candidates.size(); ++index) {
    ImGui::PushID(static_cast<int>(index));
    DrawCandidate(state, view.candidates[index], view.waiting, art, action);
    ImGui::PopID();
  }
  if (view.candidates.empty()) {
    ImGui::TextDisabled("%s", core::Tr("(aucune pièce de ce groupe dans l'inventory)"));
  }
  DrawHistory(view);
  PaintWindowScrollbar(art);
}

// UNE CASE : son numéro, ce qu'elle porte, et ce que la page en fait.
void DrawSlot(ItemEnchantWindowState& state, const ItemEnchantView& view,
              const ItemEnchantSlotView& slot, const SkinArt& art,
              ItemEnchantWindowAction& action) {
  const std::string number = core::Printed(core::Tr("Case %d"), slot.slot + kShownSlotOffset);
  if (state.page == ItemEnchantPage::kUpgrade && slot.selectable) {
    if (SkinRadio(number.c_str(), slot.selected, art) && !view.waiting) {
      state.upgrade_slot = slot.slot;
    }
  } else {
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(number.c_str());
  }
  ImGui::SameLine();
  if (slot.item.item_id != 0) {
    NoteItemById(DrawTradeItem(slot.item), slot.item, slot.item.item_id, state.item_menu, action);
  } else if (slot.card_slot) {
    ImGui::TextDisabled("%s", core::Tr("(case de carte, vide)"));
  } else if (slot.next && state.page == ItemEnchantPage::kEnchant) {
    ColoredText(kNextSlotText, core::Tr("(libre — reçoit le prochain enchantement)"));
  } else {
    ImGui::TextDisabled("%s", core::Tr("(libre)"));
  }
}

void DrawSlots(ItemEnchantWindowState& state, const ItemEnchantView& view, const SkinArt& art,
               ItemEnchantWindowAction& action) {
  for (std::size_t index = 0; index < view.slots.size(); ++index) {
    ImGui::PushID(kSlotIdBase + static_cast<int>(index));
    DrawSlot(state, view, view.slots[index], art, action);
    ImGui::PopID();
  }
}

// LES ONGLETS : une page sans rien à faire sur la pièce se grise, sans se cacher.
void DrawPages(ItemEnchantWindowState& state, const ItemEnchantView& view, const SkinArt& art) {
  if (!BeginSkinTabBar("##item_enchant_pages", art)) {
    return;
  }
  const struct {
    ItemEnchantPage page;
    const char* label;
    bool open;
  } pages[] = {{ItemEnchantPage::kEnchant, core::Tr("Enchanter"), view.enchant_open},
               {ItemEnchantPage::kUpgrade, core::Tr("Améliorer"), view.upgrade_open},
               {ItemEnchantPage::kReset, core::Tr("Tout ôter"), view.reset_open}};
  for (const auto& entry : pages) {
    const bool held = !entry.open || view.waiting;
    if (held) {
      ImGui::BeginDisabled();
    }
    if (ImGui::BeginTabItem(entry.label)) {
      state.page = entry.page;
      ImGui::EndTabItem();
    }
    if (held) {
      ImGui::EndDisabled();
    }
  }
  EndSkinTabBar();
  if (state.page != ItemEnchantPage::kEnchant) {
    return;
  }
  // TIRAGE OU CHOIX SÛR : deux façons de remplir la même case. Quand la case n'en
  // offre qu'une (Snowflower : choix sûrs seulement), elle est prise d'office.
  if (!view.random_open && view.perfect_open) {
    state.perfect = true;
  } else if (view.random_open && !view.perfect_open) {
    state.perfect = false;
  }
  const bool random_held = !view.random_open || view.waiting;
  if (random_held) {
    ImGui::BeginDisabled();
  }
  if (SkinRadio(core::Tr("Tirage"), !state.perfect, art)) {
    state.perfect = false;
  }
  if (random_held) {
    ImGui::EndDisabled();
  }
  ImGui::SameLine();
  const bool perfect_held = !view.perfect_open || view.waiting;
  if (perfect_held) {
    ImGui::BeginDisabled();
  }
  if (SkinRadio(core::Tr("Choix sûr"), state.perfect, art)) {
    state.perfect = true;
  }
  if (perfect_held) {
    ImGui::EndDisabled();
  }
  // LE TIRAGE DU GRADE SUIVANT, pour savoir s'il vaut la peine de monter la pièce
  // d'abord. Sans lui, la case se décoche : elle ne montrerait plus rien.
  if (view.other_grade_label.empty()) {
    state.other_grade = false;
  } else if (!state.perfect) {
    const std::string label(view.other_grade_label);
    static_cast<void>(SkinCheckBox(label.c_str(), &state.other_grade, art));
  }
}

void DrawOption(ItemEnchantWindowState& state, const ItemEnchantView& view,
                const ItemEnchantOptionView& option, const SkinArt& art,
                ItemEnchantWindowAction& action) {
  if (option.selectable) {
    if (SkinRadio("##chosen", option.chosen, art) && !view.waiting) {
      state.enchant_item_id = option.item.item_id;
    }
    ImGui::SameLine();
  }
  if (option.short_of) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
  }
  const float trailing =
      option.odds.empty()
          ? 0.0F
          : ImGui::CalcTextSize(option.odds.data(), option.odds.data() + option.odds.size()).x;
  NoteItemById(DrawTradeItemBefore(option.item, trailing), option.item, option.item.item_id,
               state.item_menu, action);
  if (option.short_of) {
    ImGui::PopStyleColor();
  }
  if (!option.odds.empty()) {
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(option.odds.data(), option.odds.data() + option.odds.size());
  }
  // CE QU'IL DONNE, sous son nom : choisir sans ouvrir la fiche.
  if (!option.effect.empty()) {
    ImGui::Indent();
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextWrapped("%.*s", static_cast<int>(option.effect.size()), option.effect.data());
    ImGui::PopStyleColor();
    ImGui::Unindent();
  }
}

// UNE CASE AVANT ET APRÈS : « Case 4 : (libre) → Orbe de FOR ».
void DrawPreviewRow(ItemEnchantWindowState& state, const ItemEnchantPreviewView& row,
                    ItemEnchantWindowAction& action) {
  const std::string number = core::Printed(core::Tr("Case %d"), row.slot + kShownSlotOffset);
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted(number.c_str());
  ImGui::SameLine();
  if (row.before.item_id != 0) {
    ImGui::PushID("before");
    NoteItemById(DrawTradeItem(row.before), row.before, row.before.item_id, state.item_menu,
                 action);
    ImGui::PopID();
  } else {
    ImGui::TextDisabled("%s", core::Tr("(libre)"));
  }
  ImGui::Indent();
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted("→");
  ImGui::SameLine();
  if (row.drawn) {
    ColoredText(kNextSlotText, core::Tr("(un enchantement tiré au sort)"));
  } else if (row.after.item_id != 0) {
    ImGui::PushID("after");
    NoteItemById(DrawTradeItem(row.after), row.after, row.after.item_id, state.item_menu, action);
    ImGui::PopID();
  } else {
    ImGui::TextDisabled("%s", core::Tr("(libre)"));
  }
  if (!row.effect.empty()) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextWrapped("%.*s", static_cast<int>(row.effect.size()), row.effect.data());
    ImGui::PopStyleColor();
  }
  ImGui::Unindent();
}

void DrawPreview(ItemEnchantWindowState& state, const ItemEnchantView& view,
                 ItemEnchantWindowAction& action) {
  if (view.preview.empty()) {
    return;
  }
  ImGui::Spacing();
  ImGui::TextUnformatted(core::Tr("Après la tentative"));
  ImGui::Separator();
  for (std::size_t index = 0; index < view.preview.size(); ++index) {
    ImGui::PushID(kPreviewIdBase + static_cast<int>(index));
    DrawPreviewRow(state, view.preview[index], action);
    ImGui::PopID();
  }
}

void DrawMaterial(ItemEnchantWindowState& state, const ItemEnchantMaterialView& material,
                  ItemEnchantWindowAction& action) {
  const std::string count = core::Printed("%d / %d", material.owned, material.needed);
  NoteItemById(DrawTradeItemBefore(material.item, ImGui::CalcTextSize(count.c_str()).x),
               material.item, material.item.item_id, state.item_menu, action);
  ImGui::AlignTextToFramePadding();
  if (material.owned < material.needed) {
    ColoredText(kTradeRefusedText, count);
  } else {
    ImGui::TextUnformatted(count.c_str());
  }
}

// CE QUE L'OPÉRATION POSE, COÛTE ET RÉUSSIT.
void DrawOperation(ItemEnchantWindowState& state, const ItemEnchantView& view, const SkinArt& art,
                   ItemEnchantWindowAction& action) {
  if (!view.summary.empty()) {
    ImGui::TextWrapped("%.*s", static_cast<int>(view.summary.size()), view.summary.data());
  }
  if (!view.options_title.empty()) {
    ImGui::Spacing();
    ImGui::TextUnformatted(view.options_title.data(),
                           view.options_title.data() + view.options_title.size());
    ImGui::Separator();
    for (std::size_t index = 0; index < view.options.size(); ++index) {
      ImGui::PushID(kOptionIdBase + static_cast<int>(index));
      DrawOption(state, view, view.options[index], art, action);
      ImGui::PopID();
    }
  }
  DrawPreview(state, view, action);
  ImGui::Spacing();
  ImGui::TextUnformatted(core::Tr("Coût"));
  ImGui::Separator();
  for (std::size_t index = 0; index < view.materials.size(); ++index) {
    ImGui::PushID(kMaterialIdBase + static_cast<int>(index));
    DrawMaterial(state, view.materials[index], action);
    ImGui::PopID();
  }
  const std::string zeny = TradeAmountText(view.zeny, "z");
  if (view.purse >= 0 && view.purse < view.zeny) {
    ColoredText(kTradeRefusedText, zeny);
  } else {
    ImGui::TextUnformatted(zeny.c_str());
  }
  if (!view.chance.empty()) {
    ImGui::TextUnformatted(view.chance.data(), view.chance.data() + view.chance.size());
  }
}

void DrawDetail(ItemEnchantWindowState& state, const ItemEnchantView& view, const SkinArt& art,
                ItemEnchantWindowAction& action) {
  if (view.chosen.item_id == 0) {
    ImGui::TextDisabled("%s", core::Tr("Choisissez une pièce à gauche."));
    PaintWindowScrollbar(art);
    return;
  }
  ImGui::PushID("chosen");
  NoteItemById(DrawTradeItem(view.chosen), view.chosen, view.chosen.item_id, state.item_menu,
               action);
  ImGui::PopID();
  DrawSlots(state, view, art, action);
  ImGui::Spacing();
  DrawPages(state, view, art);
  DrawOperation(state, view, art, action);
  PaintWindowScrollbar(art);
}

void DrawActionButton(ItemEnchantWindowState& state, const ItemEnchantView& view,
                      const SkinArt& art) {
  if (!view.notice.empty()) {
    ColoredText(view.notice_good ? kGoodText : kTradeRefusedText, view.notice);
  }
  if (!view.blockers.empty()) {
    ColoredText(kTradeRefusedText, view.blockers[0]);
  }
  const bool held = view.waiting || !view.blockers.empty();
  if (held) {
    ImGui::BeginDisabled();
  }
  const std::string label(view.action_label);
  if (SkinButton(label.c_str(), art, ImVec2(ImGui::GetContentRegionAvail().x, 0.0F))) {
    state.asking = true;
    state.asked_choice_seq = view.choice_seq;
  }
  if (held) {
    ImGui::EndDisabled();
  }
  TipWhenHeld(view.blockers);
}

void DrawConfirmation(ItemEnchantWindowState& state, const ItemEnchantView& view,
                      const SkinArt& art, ItemEnchantWindowAction& action) {
  // 🔴 LA BOÎTE SE DEMANDE AU NIVEAU DE LA FENÊTRE : un surgissant se nomme dans la
  // pile d'identités courante, et celle d'un volet enfant ne s'ouvrirait jamais.
  if (state.asking && !ImGui::IsPopupOpen(kConfirmId)) {
    AskModal(kConfirmId);
  }
  const std::string question(view.confirmation);
  const std::string yes(view.action_label);
  switch (DrawModalBox(kConfirmId, question.c_str(), yes.c_str(), core::Tr("Annuler"), art)) {
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

void DrawBody(ItemEnchantWindowState& state, const ItemEnchantView& view, const SkinArt& art,
              ItemEnchantWindowAction& action) {
  DrawTradeHint(view.instruction);
  ImGui::Separator();
  const float footer = TradeFooterHeight(art) + kFooterLines * ImGui::GetFrameHeightWithSpacing();
  const float list_width = ImGui::GetContentRegionAvail().x * kListShare;
  const float detail_width =
      ImGui::GetContentRegionAvail().x - list_width - ImGui::GetStyle().ItemSpacing.x;
  ImGui::BeginChild("##item_enchant_list", ImVec2(list_width, -footer), ImGuiChildFlags_Borders);
  DrawList(state, view, art, action);
  ImGui::EndChild();
  ImGui::SameLine();
  ImGui::BeginChild("##item_enchant_detail", ImVec2(detail_width, -footer),
                    ImGuiChildFlags_Borders);
  DrawDetail(state, view, art, action);
  ImGui::EndChild();

  DrawActionButton(state, view, art);
  ImGui::Separator();
  if (SkinButton(core::Tr("Fermer"), art, ImVec2(kCloseButtonPixels * Scale(), 0.0F))) {
    state.open = false;
  }
  // RECOMMENCER : la séance est close, le PNJ seul en rouvre une. Le bouton lui
  // reparle ; son dialogue reprend comme au premier clic.
  if (view.retry_open) {
    ImGui::SameLine();
    if (SkinButton(core::Tr("Recommencer"), art)) {
      action.retry = true;
    }
    if (ImGui::IsItemHovered()) {
      SkinTooltip(core::Tr("Reparler au PNJ pour une nouvelle tentative."));
    }
  }
}

// UNE PIÈCE LÂCHÉE DEPUIS LE SAC, n'importe où sur la fenêtre : la choisir.
// ⚠ TOUTE LA FENÊTRE EST CIBLE, comme l'album : viser la liste serait trop étroit.
void TakeHandDrop(const ItemEnchantView& view, ItemEnchantWindowAction& action) {
  ImGuiWindow* window = ImGui::GetCurrentWindow()->RootWindow;
  if (!ImGui::BeginDragDropTargetCustom(window->Rect(), window->ID)) {
    return;
  }
  const ImGuiPayload* payload = ImGui::GetDragDropPayload();
  const ItemEnchantPieceView* piece = nullptr;
  bool from_bag = false;
  if (payload != nullptr && payload->IsDataType(kItemDragPayload) &&
      payload->DataSize == static_cast<int>(sizeof(ItemDrag))) {
    const auto& carried = *static_cast<const ItemDrag*>(payload->Data);
    from_bag = carried.from == ItemHolder::kInventory;
    const auto found = std::ranges::find_if(view.candidates, [&](const ItemEnchantPieceView& row) {
      return row.client_slot == carried.client_slot;
    });
    piece = from_bag && found != view.candidates.end() ? &*found : nullptr;
  }
  // PENDANT LE SURVOL, ce qui ne se choisira pas se dit : on peut encore renoncer.
  if (piece == nullptr) {
    SkinTooltip(core::Tr("Cette pièce ne s'enchante pas ici."));
  } else if (!piece->refusal.empty()) {
    SkinTooltip(std::string(piece->refusal).c_str());
  }
  if (const ImGuiPayload* dropped = ImGui::AcceptDragDropPayload(kItemDragPayload);
      dropped != nullptr && piece != nullptr && piece->refusal.empty() && !view.waiting) {
    action.choose_slot = piece->client_slot;
  }
  ImGui::EndDragDropTarget();
}

}  // namespace

void OpenItemEnchantOn(ItemEnchantWindowState& state, std::uint32_t opening_seq) {
  state.open = true;
  state.opening_seen = opening_seq;
  state.page = ItemEnchantPage::kEnchant;
  state.perfect = false;
  state.enchant_item_id = 0;
  state.upgrade_slot = kItemEnchantNoSlot;
  state.other_grade = false;
  state.asking = false;
  state.asked_choice_seq = 0;
}

ItemEnchantWindowAction DrawItemEnchantWindow(ItemEnchantWindowState& state,
                                              const ItemEnchantView& view, const SkinArt& art,
                                              ItemLinkFacts link_facts) {
  ItemEnchantWindowAction action;
  if (!state.open) {
    state.asking = false;
    return action;
  }
  const float scale = Scale();
  ImGui::SetNextWindowPos(ImVec2(kFirstLeftPixels, kFirstTopPixels), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(kFirstWidthPixels * scale, kFirstHeightPixels * scale),
                           ImGuiCond_FirstUseEver);
  // EN DEÇÀ, les cases et la liste n'ont plus la place d'un nom.
  ImGui::SetNextWindowSizeConstraints(ImVec2(Px(kLeastWidthPixels), Px(kLeastHeightPixels)),
                                      ImVec2(FLT_MAX, FLT_MAX));
  const std::string title = std::string(view.title) + "###" + kItemEnchantWindowId;
  const bool body_shown = BeginSkinnedWindow(title.c_str(), &state.open, art, SkinnedTitle::kBar,
                                             ImGuiWindowFlags_NoCollapse);
  if (body_shown && state.open) {
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, kChildRoundingPixels * scale);
    DrawBody(state, view, art, action);
    DrawConfirmation(state, view, art, action);
    TakeHandDrop(view, action);
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
    action = ItemEnchantWindowAction{.close = true};
    state.asking = false;
  }
  return action;
}

}  // namespace ro::ui
