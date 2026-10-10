#include "ro/ui/barter_window.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <string>

#include "ro/core/i18n.h"
#include "ro/ui/modal_box.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"

namespace ro::ui {
namespace {

[[nodiscard]] float Scale() { return ImGui::GetStyle().FontScaleMain; }

// ── LES MESURES ─────────────────────────────────────────────────────────────
//
// Celles de la boutique de PNJ, élargies de ce que le troc montre en plus : une
// colonne de prix qui porte plusieurs devises et ce qu'on en tient, une colonne
// de poids, et un panier qui totalise ce qui part.
constexpr float kFirstLeftPixels = 100.0F;
constexpr float kFirstTopPixels = 100.0F;
constexpr float kFirstWidthPixels = 860.0F;
constexpr float kFirstHeightPixels = 500.0F;
constexpr float kCartWidthPixels = 260.0F;
constexpr float kLeastListWidthPixels = 200.0F;
constexpr float kCartGapPixels = 8.0F;
constexpr float kCostColumnPixels = 230.0F;
constexpr float kStockColumnPixels = 60.0F;
constexpr float kWeightColumnPixels = 50.0F;
constexpr float kCloseButtonPixels = 90.0F;
constexpr float kChildRoundingPixels = 6.0F;
// Sous le détail du panier : le poids, le verdict et le bouton.
constexpr float kCartFooterLines = 3.0F;
// Le rang de départ des identités du panier, loin de celles de la liste.
constexpr int kCartIdBase = 2000;

// Ce qui suffit, ce qui manque : les couleurs de verdict des briques.
constexpr ImVec4 kEnoughText = kTradeDoneText;
constexpr ImVec4 kLackingText = kTradeRefusedText;

// Les poids se tiennent en dixièmes, comme le serveur les compte.
constexpr std::int64_t kWeightTenths = 10;

constexpr const char* kConfirmId = "##barter_confirm";
constexpr const char* kItemMenuId = "##barter_item_menu";

bool Matches(const ImGuiTextFilter& filter, std::string_view name) {
  return filter.PassFilter(name.data(), name.data() + name.size());
}

const BarterOfferView* FindOffer(std::span<const BarterOfferView> offers,
                                 std::uint32_t shop_index) {
  const auto found = std::ranges::find_if(offers, [shop_index](const BarterOfferView& offer) {
    return offer.shop_index == shop_index;
  });
  return found == offers.end() ? nullptr : &*found;
}

// Un poids en dixièmes, écrit en unités : « 7 », « 0.5 ».
std::string WeightText(std::int64_t tenths) {
  std::array<char, 32> text{};
  if (tenths % kWeightTenths == 0) {
    std::snprintf(text.data(), text.size(), "%lld", static_cast<long long>(tenths / kWeightTenths));
  } else {
    std::snprintf(text.data(), text.size(), "%lld.%lld",
                  static_cast<long long>(tenths / kWeightTenths),
                  static_cast<long long>(tenths % kWeightTenths));
  }
  return text.data();
}

std::string CountSuffix(std::int64_t amount) { return "x" + TradeAmountText(amount, {}); }

// Le geste d'une ligne. ⚠ L'ITEM SE DÉSIGNE PAR `item_id`, celui de la ligne :
// le jeton du menu reste nul, un item de troc n'est à personne.
void NoteItem(const TradeItemGesture& gesture, const TradeItemView& item, std::uint32_t item_id,
              ItemLinkMenu& menu, BarterWindowAction& action) {
  if (gesture.hovered) {
    action.hovered_id = item_id;
  }
  TradeItemView aimed = item;
  aimed.item_id = item_id;
  if (NoteTradeItemLink(gesture, aimed, 0, menu)) {
    action.describe_id = item_id;
    action.item_gesture = ChatLinkGesture::kRepost;
  }
}

// UNE DEVISE : l'icône, le nom raffinement compris, la quantité, puis ce que le
// joueur en tient — vert s'il suffit à `needed`, rouge sinon.
void DrawPrice(const BarterPriceView& price, std::int64_t needed, ItemLinkMenu& menu,
               BarterWindowAction& action) {
  const std::string suffix = CountSuffix(needed);
  NoteItem(DrawTradeItem(price.item, suffix), price.item, price.item_id, menu, action);
  if (ImGui::IsItemHovered() && price.refine_level > 0) {
    std::array<char, 96> tip{};
    std::snprintf(tip.data(), tip.size(), core::Tr("Raffinement exigé : +%d ou plus"),
                  static_cast<int>(price.refine_level));
    SkinTooltip(tip.data());
  }
  ImGui::SameLine();
  std::array<char, 64> held{};
  std::snprintf(held.data(), held.size(), core::Tr("possédé %s"),
                TradeAmountText(price.held, {}).c_str());
  ImGui::TextColored(price.held >= needed ? kEnoughText : kLackingText, "%s", held.data());
  if (ImGui::IsItemHovered()) {
    SkinTooltip(
        core::Tr("Compté sans les items portés ni les favoris, que l'échange ne prend "
                 "pas."));
  }
  if (price.may_take_higher_refine) {
    ImGui::SameLine();
    ImGui::TextColored(kLackingText, "(!)");
    if (ImGui::IsItemHovered()) {
      SkinTooltip(core::Tr("Un exemplaire raffiné plus haut que l'exigence peut partir."));
    }
  }
}

void DrawZeny(std::int64_t zeny) {
  ImGui::AlignTextToFramePadding();
  ImGui::TextColored(kTradeBlackText, "+ %s", TradeAmountText(zeny, "z").c_str());
}

void AskExchange(BarterWindowState& state, std::vector<BarterLine> lines, bool immediate,
                 std::uint32_t cart_seq) {
  state.pending = std::move(lines);
  state.pending_immediate = immediate;
  state.pending_cart_seq = cart_seq;
}

void DrawOfferRow(BarterWindowState& state, const BarterView& view, const BarterOfferView& offer,
                  const SkinArt& art, BarterWindowAction& action) {
  ImGui::TableNextRow();
  ImGui::TableNextColumn();
  NoteItem(DrawTradeItem(offer.item), offer.item, offer.item.item_id, state.item_menu, action);

  ImGui::TableNextColumn();
  for (const BarterPriceView& price : offer.prices) {
    DrawPrice(price, price.amount, state.item_menu, action);
  }
  if (offer.zeny > 0) {
    DrawZeny(offer.zeny);
  }

  ImGui::TableNextColumn();
  ImGui::AlignTextToFramePadding();
  if (offer.stock == kBarterNoStockLimit) {
    ImGui::TextDisabled("%s", core::Tr("illimité"));
  } else if (offer.stock == 0) {
    ImGui::TextDisabled("%s", core::Tr("épuisé"));
  } else {
    ImGui::TextColored(kTradeBlackText, "%s", TradeAmountText(offer.stock, {}).c_str());
  }

  ImGui::TableNextColumn();
  ImGui::AlignTextToFramePadding();
  ImGui::TextColored(kTradeBlackText, "%s", WeightText(offer.weight_tenths).c_str());

  ImGui::TableNextColumn();
  // 🔴 UN AJOUT QUE LE PANIER REFUSERAIT NE SE PROPOSE PAS : les paliers sont
  // bornés à ce que `app` a jugé possible, et sans rien de possible le bouton
  // reste gris en disant pourquoi.
  if (offer.most <= 0) {
    ImGui::BeginDisabled();
    static_cast<void>(SkinSmallButton("+1", art));
    ImGui::EndDisabled();
    if (!offer.refusal.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
      const std::string refusal(offer.refusal);
      SkinTooltip(refusal.c_str());
    }
    return;
  }
  const TradeStepClick click =
      DrawTradeSteps(TradeStepsFor(offer.most, false), art, 0, kTradeNoPurse);
  if (click.amount <= 0) {
    return;
  }
  const BarterLine line{.shop_index = offer.shop_index,
                        .amount = std::min(click.amount, offer.most)};
  if (click.immediate) {
    AskExchange(state, {line}, true, view.cart_seq);
  } else {
    action.edit = BarterCartEdit{.kind = BarterCartEditKind::kAdd, .line = line};
  }
}

void DrawList(BarterWindowState& state, const BarterView& view, const SkinArt& art,
              BarterWindowAction& action) {
  const float scale = Scale();
  const std::string_view heading =
      view.list_title.empty() ? std::string_view(core::Tr("Items à échanger")) : view.list_title;
  ImGui::TextUnformatted(heading.data(), heading.data() + heading.size());
  constexpr int kColumns = 5;
  // Les paliers se règlent sur leur ligne la plus large ; un item qu'on ne
  // peut pas prendre garde son « +1 » grisé.
  float steps_width = TradeStepsWidth(TradeStepsFor(1, false), art);
  for (const BarterOfferView& offer : view.offers) {
    if (offer.most > 0) {
      steps_width = std::max(steps_width, TradeStepsWidth(TradeStepsFor(offer.most, false), art));
    }
  }
  if (!ImGui::BeginTable(
          "##barter_offers", kColumns,
          ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp)) {
    return;
  }
  ImGui::TableSetupScrollFreeze(0, 1);
  ImGui::TableSetupColumn(core::Tr("Item"), ImGuiTableColumnFlags_WidthStretch);
  ImGui::TableSetupColumn(core::Tr("Coût"), ImGuiTableColumnFlags_WidthFixed,
                          kCostColumnPixels * scale);
  ImGui::TableSetupColumn(core::Tr("Stock"), ImGuiTableColumnFlags_WidthFixed,
                          kStockColumnPixels * scale);
  ImGui::TableSetupColumn(core::Tr("Poids"), ImGuiTableColumnFlags_WidthFixed,
                          kWeightColumnPixels * scale);
  ImGui::TableSetupColumn("##steps", ImGuiTableColumnFlags_WidthFixed, steps_width);
  ImGui::TableHeadersRow();
  for (std::size_t index = 0; index < view.offers.size(); ++index) {
    const BarterOfferView& offer = view.offers[index];
    if (!Matches(state.filter, offer.item.name)) {
      continue;
    }
    ImGui::PushID(static_cast<int>(index));
    DrawOfferRow(state, view, offer, art, action);
    ImGui::PopID();
  }
  if (view.offers.empty()) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextDisabled("%s", core::Tr("(rien à échanger)"));
  }
  PaintWindowScrollbar(art);
  ImGui::EndTable();
}

void DrawCartLines(const BarterView& view, const SkinArt& art, BarterWindowAction& action) {
  for (std::size_t index = 0; index < view.cart.size(); ++index) {
    const BarterCartLineView& line = view.cart[index];
    ImGui::PushID(kCartIdBase + static_cast<int>(index));
    const BarterOfferView* offer = FindOffer(view.offers, line.shop_index);
    const std::string_view name = offer != nullptr ? offer->item.name : std::string_view("?");
    ImGui::TextWrapped("%.*s", static_cast<int>(name.size()), name.data());
    // LA QUANTITÉ SE DEMANDE, elle ne s'écrit pas : c'est le troc qui tient le
    // panier, et la vue suivante montre ce qu'il a accepté.
    int amount = line.amount;
    if (DrawTradeQuantity(amount, std::max(line.most, line.amount), art) && amount != line.amount) {
      action.edit = BarterCartEdit{.kind = BarterCartEditKind::kSet,
                                   .line = {.shop_index = line.shop_index, .amount = amount}};
    }
    ImGui::SameLine();
    const float step = ImGui::GetFrameHeight();
    const float right = ImGui::GetContentRegionMax().x - step;
    if (right > ImGui::GetCursorPosX()) {
      ImGui::SetCursorPosX(right);
    }
    if (SkinButton("x", art, ImVec2(step, step))) {
      action.edit = BarterCartEdit{.kind = BarterCartEditKind::kSet,
                                   .line = {.shop_index = line.shop_index, .amount = 0}};
    }
    ImGui::Separator();
    ImGui::PopID();
  }
}

// CE QUI PART : chaque devise totalisée sur le panier, ce qu'on en tient, et le
// zeny.
void DrawCartCosts(const BarterView& view, ItemLinkMenu& menu, BarterWindowAction& action) {
  std::vector<BarterLine> lines;
  lines.reserve(view.cart.size());
  for (const BarterCartLineView& line : view.cart) {
    lines.push_back({.shop_index = line.shop_index, .amount = line.amount});
  }
  const BarterRecap recap = RecapBarterLines(view.offers, lines);
  ImGui::TextUnformatted(core::Tr("Part :"));
  for (const BarterCost& cost : recap.costs) {
    DrawPrice(cost.price, cost.total, menu, action);
  }
  if (recap.zeny > 0) {
    DrawZeny(recap.zeny);
  }
}

void DrawCart(BarterWindowState& state, const BarterView& view, const SkinArt& art, float width,
              float footer, BarterWindowAction& action) {
  ImGui::SameLine();
  ImGui::BeginChild("##barter_cart", ImVec2(width, -footer), ImGuiChildFlags_Borders);
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted(core::Tr("Panier d'échange"));
  ImGui::SameLine();
  if (SkinButton(core::Tr("Vider"), art)) {
    action.edit = BarterCartEdit{.kind = BarterCartEditKind::kClear};
  }
  ImGui::Separator();

  const float reserved = kCartFooterLines * ImGui::GetFrameHeightWithSpacing();
  ImGui::BeginChild("##barter_cart_lines", ImVec2(0.0F, -reserved));
  DrawCartLines(view, art, action);
  DrawCartCosts(view, state.item_menu, action);
  PaintWindowScrollbar(art);
  ImGui::EndChild();

  ImGui::Separator();
  if (!view.cart_weight.empty()) {
    ImGui::TextDisabled("%.*s", static_cast<int>(view.cart_weight.size()), view.cart_weight.data());
  }
  // 🔴 LE BOUTON SE RETIENT tant qu'une raison ferait refuser l'échange. La
  // première s'écrit au-dessus ; toutes se lisent au survol du bouton.
  if (!view.blockers.empty()) {
    ImGui::TextColored(kTradeRefusedText, "%.*s", static_cast<int>(view.blockers[0].size()),
                       view.blockers[0].data());
  }
  const bool blocked = view.cart.empty() || !view.blockers.empty();
  if (blocked) {
    ImGui::BeginDisabled();
  }
  if (SkinButton(core::Tr("Échanger"), art, ImVec2(ImGui::GetContentRegionAvail().x, 0.0F))) {
    std::vector<BarterLine> lines;
    for (const BarterCartLineView& line : view.cart) {
      lines.push_back({.shop_index = line.shop_index, .amount = line.amount});
    }
    AskExchange(state, std::move(lines), false, view.cart_seq);
  }
  if (blocked) {
    ImGui::EndDisabled();
  }
  if (!view.blockers.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    std::string reasons;
    for (const std::string_view reason : view.blockers) {
      if (!reasons.empty()) {
        reasons.push_back('\n');
      }
      reasons.append(reason);
    }
    SkinTooltip(reasons.c_str());
  }
  ImGui::EndChild();
}

void DrawConfirmation(BarterWindowState& state, const BarterView& view, const SkinArt& art,
                      BarterWindowAction& action) {
  // 🔴 LA BOÎTE SE DEMANDE ICI, AU NIVEAU DE LA FENÊTRE, et non depuis la ligne
  // ou le panier qui l'ont voulue : un surgissant se nomme dans la pile
  // d'identités COURANTE, et celle d'une ligne de table ou d'un volet enfant
  // n'est pas celle où la boîte se dessine. Demandée là-bas, elle ne
  // s'ouvrirait jamais.
  if (!state.pending.empty() && !ImGui::IsPopupOpen(kConfirmId)) {
    AskModal(kConfirmId);
  }
  const std::string question =
      BarterConfirmationText(view.offers, RecapBarterLines(view.offers, state.pending));
  switch (
      DrawModalBox(kConfirmId, question.c_str(), core::Tr("Échanger"), core::Tr("Annuler"), art)) {
    case ModalAnswer::kYes:
      action.exchange = std::move(state.pending);
      action.immediate = state.pending_immediate;
      action.cart_seq = state.pending_cart_seq;
      state.pending.clear();
      break;
    case ModalAnswer::kNo:
      state.pending.clear();
      break;
    case ModalAnswer::kWaiting:
      break;
  }
}

}  // namespace

BarterRecap RecapBarterLines(std::span<const BarterOfferView> offers,
                             std::span<const BarterLine> lines) {
  BarterRecap recap;
  for (const BarterLine& line : lines) {
    const BarterOfferView* offer = FindOffer(offers, line.shop_index);
    if (offer == nullptr || line.amount <= 0) {
      continue;
    }
    recap.arrivals.push_back(line);
    recap.zeny += offer->zeny * line.amount;
    for (const BarterPriceView& price : offer->prices) {
      const std::int64_t total = price.amount * line.amount;
      // UNE DEVISE SE CUMULE PAR OBJET ET PAR RAFFINEMENT EXIGÉ, comme le serveur
      // la cherche : deux exigences différentes sont deux besoins.
      const auto same = std::ranges::find_if(recap.costs, [&price](const BarterCost& cost) {
        return cost.price.item_id == price.item_id && cost.price.refine_level == price.refine_level;
      });
      if (same == recap.costs.end()) {
        recap.costs.push_back(BarterCost{.price = price, .total = total});
      } else {
        same->total += total;
      }
      recap.refined_leaves |= price.refine_level > 0 || price.may_take_higher_refine;
    }
  }
  return recap;
}

std::string BarterConfirmationText(std::span<const BarterOfferView> offers,
                                   const BarterRecap& recap) {
  std::string text = core::Tr("Échanger ?");
  text.append("\n\n");
  text.append(core::Tr("Part :"));
  for (const BarterCost& cost : recap.costs) {
    text.append("\n   ");
    text.append(CountSuffix(cost.total));
    text.push_back(' ');
    text.append(cost.price.item.name);
  }
  if (recap.zeny > 0) {
    text.append("\n   ");
    text.append(TradeAmountText(recap.zeny, "z"));
  }
  text.append("\n\n");
  text.append(core::Tr("Arrive :"));
  for (const BarterLine& line : recap.arrivals) {
    const BarterOfferView* offer = FindOffer(offers, line.shop_index);
    text.append("\n   ");
    text.append(CountSuffix(line.amount));
    text.push_back(' ');
    text.append(offer != nullptr ? offer->item.name : std::string_view("?"));
  }
  if (recap.refined_leaves) {
    text.append("\n\n");
    text.append(core::Tr("(!) Des items raffinés peuvent partir dans l'échange."));
  }
  return text;
}

void OpenBarterOn(BarterWindowState& state, std::uint32_t opening_seq) {
  state.open = true;
  state.opening_seen = opening_seq;
  state.pending.clear();
  state.pending_immediate = false;
  state.pending_cart_seq = 0;
}

BarterWindowAction DrawBarterWindow(BarterWindowState& state, const BarterView& view,
                                    const SkinArt& art, std::span<const VoteShopOffer> vote_shop) {
  BarterWindowAction action;
  if (!state.open) {
    // Une question laissée en suspens ne survit pas à la fenêtre qui la posait.
    state.pending.clear();
    return action;
  }
  const float scale = Scale();
  ImGui::SetNextWindowPos(ImVec2(kFirstLeftPixels, kFirstTopPixels), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(kFirstWidthPixels * scale, kFirstHeightPixels * scale),
                           ImGuiCond_FirstUseEver);
  const std::string_view name =
      view.title.empty() ? std::string_view(core::Tr("Troc")) : view.title;
  const std::string title = std::string(name) + "###" + kBarterWindowId;
  const bool body_shown = BeginSkinnedWindow(title.c_str(), &state.open, art, SkinnedTitle::kBar,
                                             ImGuiWindowFlags_NoCollapse);
  if (body_shown && state.open) {
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, kChildRoundingPixels * scale);
    if (!view.purse_line.empty()) {
      ImGui::TextColored(kTradeBlackText, "%.*s", static_cast<int>(view.purse_line.size()),
                         view.purse_line.data());
    }
    ImGui::SetNextItemWidth(-1.0F);
    if (ImGui::InputTextWithHint("##barter_filter", core::Tr("Filtrer..."), state.filter.InputBuf,
                                 IM_ARRAYSIZE(state.filter.InputBuf))) {
      state.filter.Build();
    }
    DrawTradeHint(
        core::Tr("Clic = panier   -   Ctrl+clic = échange immédiat, "
                 "confirmé"));
    ImGui::Separator();

    const float footer = TradeFooterHeight(art);
    const bool show_cart = !view.cart.empty();
    const float cart_width = kCartWidthPixels * scale;
    const float list_width =
        show_cart ? std::max(kLeastListWidthPixels * scale,
                             ImGui::GetContentRegionAvail().x - cart_width - kCartGapPixels * scale)
                  : 0.0F;
    ImGui::BeginChild("##barter_list", ImVec2(list_width, -footer), ImGuiChildFlags_Borders);
    DrawList(state, view, art, action);
    ImGui::EndChild();
    if (show_cart) {
      DrawCart(state, view, art, cart_width, footer, action);
    }

    ImGui::Separator();
    if (SkinButton(core::Tr("Fermer"), art, ImVec2(kCloseButtonPixels * scale, 0.0F))) {
      state.open = false;
    }
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled(
        "%s",
        core::Tr("Survol : aperçu   -   Clic droit : menu   -   Maj+clic : lien dans le chat"));
    DrawConfirmation(state, view, art, action);
    if (const ChatLinkAsk chosen = DrawTradeItemMenu(kItemMenuId, state.item_menu, vote_shop);
        chosen.gesture != ChatLinkGesture::kNothing) {
      action.describe_id = chosen.item_id;
      action.item_gesture = chosen.gesture;
    }
    ImGui::PopStyleVar();
  }
  EndSkinnedWindow();
  if (!state.open) {
    action.close = true;
    action.edit = {};
    action.exchange.clear();
    state.pending.clear();
  }
  return action;
}

}  // namespace ro::ui
