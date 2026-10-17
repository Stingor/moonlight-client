#include "ro/ui/buying_store_own_window.h"

#include <limits>
#include <string>

#include "ro/core/i18n.h"
#include "ro/ui/modal_box.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"

#include "ui_scale.h"

namespace ro::ui {
namespace {

// ── LES MESURES (« Mon shop ») ──────────────────────────────────────────────
constexpr float kLeastWidthPixels = 260.0F;
constexpr float kMostWidthPixels = 900.0F;
constexpr float kWantedColumnPixels = 45.0F;
constexpr float kPriceColumnPixels = 90.0F;
constexpr float kTotalColumnPixels = 90.0F;

constexpr const char* kCloseConfirmId = "##buying_store_own_close";
constexpr const char* kItemMenuId = "##buying_store_own_item_menu";

}  // namespace

void OpenBuyingStoreOwnOn(BuyingStoreOwnState& state, std::uint32_t opening_seq) {
  state.open = true;
  state.opening_seen = opening_seq;
  state.ask_close = false;
}

BuyingStoreOwnAction DrawBuyingStoreOwnWindow(BuyingStoreOwnState& state,
                                              const BuyingStoreOwnView& view, const SkinArt& art,
                                              ItemLinkFacts link_facts) {
  BuyingStoreOwnAction action;
  // UN INTERRUPTEUR BAISSÉ par la pile d'Échap : la fenêtre revient, et
  // demande.
  if (!state.open) {
    state.open = true;
    state.ask_close = true;
  }
  const float scale = Scale();
  ImGui::SetNextWindowSizeConstraints(
      ImVec2(kLeastWidthPixels * scale, 0.0F),
      ImVec2(kMostWidthPixels * scale, std::numeric_limits<float>::max()));
  const std::string_view shown = view.window_title.empty()
                                     ? std::string_view(core::Tr("Mon buying store"))
                                     : view.window_title;
  const std::string title = std::string(shown) + kBuyingStoreOwnWindowName;
  bool keep = true;
  const bool body_shown =
      BeginSkinnedWindow(title.c_str(), &keep, art, SkinnedTitle::kBar,
                         ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);
  if (body_shown && keep) {
    if (!view.store_title.empty()) {
      ImGui::TextColored(kTradeBlackText, "%.*s", static_cast<int>(view.store_title.size()),
                         view.store_title.data());
      ImGui::Separator();
    }
    if (view.lines.empty()) {
      ImGui::TextDisabled("%s", core::Tr("Tout est acheté."));
    } else if (ImGui::BeginTable("##buying_store_own_lines", 4,
                                 ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg)) {
      ImGui::TableSetupColumn(core::Tr("Item"), ImGuiTableColumnFlags_WidthFixed);
      ImGui::TableSetupColumn(core::Tr("Voulu"), ImGuiTableColumnFlags_WidthFixed,
                              kWantedColumnPixels * scale);
      ImGui::TableSetupColumn(core::Tr("Prix"), ImGuiTableColumnFlags_WidthFixed,
                              kPriceColumnPixels * scale);
      ImGui::TableSetupColumn(core::Tr("Total"), ImGuiTableColumnFlags_WidthFixed,
                              kTotalColumnPixels * scale);
      ImGui::TableHeadersRow();
      for (std::size_t rank = 0; rank < view.lines.size(); ++rank) {
        const BuyingStoreOwnLineView& line = view.lines[rank];
        ImGui::PushID(static_cast<int>(rank));
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        const TradeItemGesture gesture = DrawTradeItem(line.item);
        if (gesture.hovered) {
          action.hovered_id = line.item.item_id;
        }
        if (NoteTradeItemLink(gesture, line.item, 0, state.item_menu)) {
          action.describe_id = line.item.item_id;
          action.item_gesture = ChatLinkGesture::kRepost;
        }
        // Une ligne pleine reste, grisée : le serveur la liste encore.
        const ImVec4 color =
            line.wanted == 0 ? ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled) : kTradeBlackText;
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(color, "%d", line.wanted);
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(color, "%s", TradeAmountText(line.price, "z").c_str());
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(color, "%s", TradeAmountText(line.price * line.wanted, "z").c_str());
        ImGui::PopID();
      }
      ImGui::EndTable();
    }
    ImGui::Separator();
    ImGui::Text(core::Tr("Fonds disponibles : %s"), TradeAmountText(view.zeny_left, "z").c_str());
    ImGui::TextDisabled(core::Tr("Limite à l'ouverture : %s"),
                        TradeAmountText(view.zeny_at_opening, "z").c_str());
    ImGui::TextDisabled(core::Tr("Dépensé : %s en %d achat(s)"),
                        TradeAmountText(view.spent, "z").c_str(), view.purchases);
    if (SkinButton(core::Tr("Fermer le buying store"), art)) {
      state.ask_close = true;
    }
    if (ImGui::IsItemHovered()) {
      SkinTooltip(core::Tr("Met fin aux achats. Il faudra relancer le skill pour rouvrir."));
    }
    if (state.ask_close) {
      state.ask_close = false;
      AskModal(kCloseConfirmId);
    }
    switch (DrawModalBox(kCloseConfirmId, core::Tr("Mettre fin au buying store ?"), core::Tr("Oui"),
                         core::Tr("Non"), art)) {
      case ModalAnswer::kYes:
        action.close = true;
        break;
      case ModalAnswer::kNo:
      case ModalAnswer::kWaiting:
        break;
    }
    if (const ChatLinkAsk chosen = DrawTradeItemMenu(kItemMenuId, state.item_menu, link_facts);
        chosen.gesture != ChatLinkGesture::kNothing) {
      action.describe_id = chosen.item_id;
      action.item_gesture = chosen.gesture;
    }
  }
  EndSkinnedWindow();
  // LA CROIX : la question se pose à l'image suivante, dans la fenêtre.
  if (!keep) {
    state.ask_close = true;
  }
  return action;
}

}  // namespace ro::ui
