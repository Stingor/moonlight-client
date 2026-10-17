#include "ro/ui/buying_store_log_window.h"

#include <string>

#include "ro/core/i18n.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"

#include "ui_scale.h"

namespace ro::ui {
namespace {

// ── LES MESURES (« Historique ») ────────────────────────────────────────────
constexpr float kLeastWidthPixels = 260.0F;
constexpr float kMostWidthPixels = 900.0F;
constexpr float kAmountColumnPixels = 45.0F;
constexpr float kPriceColumnPixels = 90.0F;
constexpr float kZenyColumnPixels = 90.0F;
// L'heure de la vente et le vendeur.
constexpr float kClockColumnPixels = 110.0F;
constexpr float kSellerColumnPixels = 80.0F;
// Un long journal fait défiler la fenêtre plutôt que de sortir de l'écran.
constexpr float kMostScreenShare = 0.6F;

constexpr const char* kItemMenuId = "##buying_store_log_item_menu";

}  // namespace

BuyingStoreLogAction DrawBuyingStoreLogWindow(BuyingStoreLogState& state,
                                              const BuyingStoreLogView& view, const SkinArt& art,
                                              ItemLinkFacts link_facts) {
  BuyingStoreLogAction action;
  if (!state.open) {
    return action;
  }
  const float scale = Scale();
  ImGui::SetNextWindowSizeConstraints(
      ImVec2(kLeastWidthPixels * scale, 0.0F),
      ImVec2(kMostWidthPixels * scale, ImGui::GetIO().DisplaySize.y * kMostScreenShare));
  const std::string title =
      std::string(core::Tr("Historique des achats")) + kBuyingStoreLogWindowName;
  const bool body_shown =
      BeginSkinnedWindow(title.c_str(), &state.open, art, SkinnedTitle::kBar,
                         ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);
  if (body_shown && state.open) {
    if (view.lines.empty()) {
      ImGui::TextDisabled("%s", core::Tr("Aucun achat."));
    } else if (ImGui::BeginTable("##buying_store_log", 6,
                                 ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg)) {
      ImGui::TableSetupColumn(core::Tr("Heure"), ImGuiTableColumnFlags_WidthFixed,
                              kClockColumnPixels * scale);
      ImGui::TableSetupColumn(core::Tr("Item"), ImGuiTableColumnFlags_WidthFixed);
      ImGui::TableSetupColumn(core::Tr("Vendeur"), ImGuiTableColumnFlags_WidthFixed,
                              kSellerColumnPixels * scale);
      ImGui::TableSetupColumn(core::Tr("Qté"), ImGuiTableColumnFlags_WidthFixed,
                              kAmountColumnPixels * scale);
      ImGui::TableSetupColumn(core::Tr("Prix"), ImGuiTableColumnFlags_WidthFixed,
                              kPriceColumnPixels * scale);
      ImGui::TableSetupColumn(core::Tr("Montant"), ImGuiTableColumnFlags_WidthFixed,
                              kZenyColumnPixels * scale);
      ImGui::TableHeadersRow();
      for (std::size_t rank = 0; rank < view.lines.size(); ++rank) {
        const BuyingStoreLogLineView& line = view.lines[rank];
        ImGui::PushID(static_cast<int>(rank));
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(kTradeBlackText, "%.*s", static_cast<int>(line.clock.size()),
                           line.clock.data());
        ImGui::TableNextColumn();
        const TradeItemGesture gesture = DrawTradeItem(line.item);
        if (gesture.hovered) {
          action.hovered_id = line.item.item_id;
        }
        if (NoteTradeItemLink(gesture, line.item, 0, state.item_menu)) {
          action.describe_id = line.item.item_id;
          action.item_gesture = ChatLinkGesture::kRepost;
        }
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(kTradeBlackText, "#%u", line.seller_char_id);
        if (ImGui::IsItemHovered()) {
          SkinTooltip(core::Tr("Le numéro du personnage vendeur : le serveur ne dit pas son nom."));
        }
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(kTradeBlackText, "%d", line.amount);
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        // LE PRIX SE DÉDUIT : le serveur ne donne que le montant de la ligne.
        if (line.amount > 0) {
          ImGui::TextColored(kTradeBlackText, "%s",
                             TradeAmountText(line.zeny / line.amount, "z").c_str());
        } else {
          ImGui::TextDisabled("-");
        }
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(kTradeBlackText, "%s", TradeAmountText(line.zeny, "z").c_str());
        ImGui::PopID();
      }
      ImGui::EndTable();
    }
    ImGui::Separator();
    ImGui::Text(core::Tr("Total des achats : %s"), TradeAmountText(view.total, "z").c_str());
    if (SkinButton(core::Tr("Fermer"), art)) {
      state.open = false;
    }
    if (const ChatLinkAsk chosen = DrawTradeItemMenu(kItemMenuId, state.item_menu, link_facts);
        chosen.gesture != ChatLinkGesture::kNothing) {
      action.describe_id = chosen.item_id;
      action.item_gesture = chosen.gesture;
    }
  }
  EndSkinnedWindow();
  if (!state.open) {
    action.close = true;
  }
  return action;
}

}  // namespace ro::ui
