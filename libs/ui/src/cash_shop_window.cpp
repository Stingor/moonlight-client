#include "ro/ui/cash_shop_window.h"

#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <cstring>
#include <string>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/bug_report_window.h"
#include "ro/ui/list_keys.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"
#include "ro/ui/trade_widgets.h"
#include "ro/ui/wheel_gate.h"

namespace ro::ui {
namespace {

[[nodiscard]] float Scale() { return ImGui::GetStyle().FontScaleMain; }

// ── LES MESURES, en pixels d'interface ──────────────────────────────────────
// La place de la toute première ouverture ; ImGui retient ensuite celle du joueur.
constexpr float kFirstLeftPixels = 120.0F;
constexpr float kFirstTopPixels = 80.0F;
constexpr float kFirstWidthPixels = 680.0F;
constexpr float kFirstHeightPixels = 500.0F;
constexpr float kWindowRoundingPixels = 8.0F;
constexpr float kChildRoundingPixels = 6.0F;
constexpr float kCartWidthPixels = 220.0F;
constexpr float kCartGapPixels = 8.0F;
constexpr float kLeastGridWidthPixels = 120.0F;
// Le pied du panier : le total et le bouton d'achat.
constexpr float kCartFooterPixels = 64.0F;
constexpr float kSlotComboPixels = 200.0F;
constexpr float kSortComboPixels = 90.0F;
constexpr float kQuantityPixels = 42.0F;
constexpr float kQuantityGapPixels = 2.0F;

// La carte : sa hauteur, l'intervalle entre deux cartes, la hauteur de sa
// rangée basse, et ses marges.
constexpr float kCardHeightPixels = 100.0F;
constexpr float kCardGapPixels = 4.0F;
constexpr float kCardBoxPixels = 78.0F;
constexpr float kCardPadXPixels = 5.0F;
constexpr float kCardPadYPixels = 3.0F;
// Entre l'image et la colonne du prix et des boutons.
constexpr float kCardColumnGapPixels = 8.0F;
// L'image, un peu plus petite que sa rangée : des marges.
constexpr float kCardImageMarginPixels = 10.0F;
// La plus petite largeur de carte ; elle s'élargit pour tenir les libellés
// traduits des deux boutons.
constexpr float kCardLeastWidthPixels = 172.0F;
constexpr float kCardRoundingPixels = 6.0F;
// Le nom, dans son bandeau : sa marge, et le plancher de sa police réduite.
constexpr float kCardNameMarginPixels = 4.0F;
constexpr float kCardNameMarginBothPixels = 8.0F;
constexpr float kCardNameLeastScale = 0.55F;

// ── LES COULEURS DE LA CARTE : celles de l'habillage (`SkinLook::card`…) ────
constexpr ImVec4 kBlackText(0.0F, 0.0F, 0.0F, 1.0F);
constexpr ImVec4 kBoughtColor(0.4F, 1.0F, 0.4F, 1.0F);
constexpr ImVec4 kRefusedColor(1.0F, 0.4F, 0.4F, 1.0F);

constexpr int kDollDirections = 8;

// Le compte d'une ligne de panier, hors de laquelle le serveur refuse.
constexpr int kLeastPerLine = 1;

constexpr std::size_t kLabelBytes = 96;

constexpr const char* kItemMenuId = "##cash_shop_item_menu";

// Les onglets du serveur. Voir `CashShopTabShown`.
constexpr std::array<bool, kCashShopTabCount> kTabShown = {true,  true, false, false, false,
                                                           false, true, true,  false};

enum SortKey : int { kSortName = 0, kSortId = 1, kSortPrice = 2 };
constexpr std::array<const char*, 3> kSortLabels = {core::TrText("Nom"), core::TrText("ID"),
                                                    core::TrText("Coût")};

// Le libellé d'un onglet du serveur, traduit.
[[nodiscard]] const char* TabLabel(std::size_t tab) {
  switch (tab) {
    case 0:
      return core::Tr("Nouveautés");
    case 1:
      return core::Tr("Populaire");
    case 2:
      return core::Tr("Limité");
    case 3:
      return core::Tr("Location");
    case 4:
      return core::Tr("Permanent");
    case 5:
      return core::Tr("Parchemins");
    case 6:
      return core::Tr("Consommables");
    case 7:
      return core::Tr("Divers");
    default:
      return core::Tr("Soldes");
  }
}

// ── CE QUE LA FENÊTRE MONTRE D'UNE CARTE ────────────────────────────────────

// Le nom d'un item du panier, lu dans la carte de son onglet.
[[nodiscard]] std::string_view NameOf(const CashShopView& view, const CashShopCartLine& line) {
  if (line.tab < kCashShopTabCount) {
    for (const CashShopCard& card : view.tabs[line.tab]) {
      if (card.item_id == line.item_id) {
        return card.name;
      }
    }
  }
  return {};
}

[[nodiscard]] bool SlotMatches(const CashShopCard& card, int slot) {
  if (slot == kCashShopSlotAll) {
    return true;
  }
  if (slot == kCashShopSlotHatEffect) {
    return card.hat_effect;
  }
  return CashShopSlotOf(card.equip_location).key == slot;
}

// Les emplacements présents dans l'onglet, triés par clé, et l'entrée des
// effets de chapeau s'il en porte.
[[nodiscard]] std::vector<CashShopSlot> SlotsOf(std::span<const CashShopCard> cards) {
  std::vector<CashShopSlot> slots;
  bool has_hat_effect = false;
  for (const CashShopCard& card : cards) {
    has_hat_effect = has_hat_effect || card.hat_effect;
    const CashShopSlot slot = CashShopSlotOf(card.equip_location);
    const bool seen = std::ranges::any_of(
        slots, [&slot](const CashShopSlot& known) { return known.key == slot.key; });
    if (!seen) {
      const auto at = std::ranges::find_if(
          slots, [&slot](const CashShopSlot& known) { return known.key > slot.key; });
      slots.insert(at, slot);
    }
  }
  if (has_hat_effect) {
    slots.push_back(CashShopSlot{kCashShopSlotHatEffect, core::Tr("Costumes hat-effect")});
  }
  return slots;
}

// ── L'EN-TÊTE : les points du compte, la case, et le dernier achat ──────────
void DrawHeader(CashShopWindowState& state, const CashShopView& view, const SkinArt& art) {
  const int currency_bytes = static_cast<int>(view.currency.size());
  const int event_bytes = static_cast<int>(view.event_points_label.size());
  ImGui::TextColored(kBlackText, "%.*s: %u", currency_bytes, view.currency.data(),
                     view.vote_points);
  ImGui::SameLine();
  ImGui::TextColored(kBlackText, " | %.*s: %u", event_bytes, view.event_points_label.data(),
                     view.event_points);
  ImGui::SameLine();
  // L'identifiant ne dépend pas du libellé, qui peut manquer.
  const std::string use_label = std::string(view.use_event_points_label) + "###cs_use_event";
  static_cast<void>(SkinCheckBox(use_label.c_str(), &state.use_event_points, art));
  if (view.purchase == CashShopPurchaseNote::kBought) {
    ImGui::SameLine();
    ImGui::TextColored(kBoughtColor, " | %s", core::Tr("Achat OK"));
  } else if (view.purchase == CashShopPurchaseNote::kRefused) {
    ImGui::SameLine();
    ImGui::TextColored(kRefusedColor, " | %.*s", static_cast<int>(view.refusal.size()),
                       view.refusal.data());
  }
  ImGui::Separator();
}

void DrawTabs(CashShopWindowState& state, const CashShopView& view, const SkinArt& art) {
  if (BeginSkinTabBar("cashshop_tabs", art, ImGuiTabBarFlags_FittingPolicyScroll)) {
    for (std::size_t tab = 0; tab < kCashShopTabCount; ++tab) {
      if (!kTabShown[tab]) {
        continue;
      }
      std::array<char, kLabelBytes> label{};
      std::snprintf(label.data(), label.size(), core::Tr("%s (%d)###cstab%d"), TabLabel(tab),
                    static_cast<int>(view.tabs[tab].size()), static_cast<int>(tab));
      const ImGuiTabItemFlags flags = state.forced_tab == tab ? ImGuiTabItemFlags_SetSelected : 0;
      if (ImGui::BeginTabItem(label.data(), nullptr, flags)) {
        // Changer d'onglet remet le filtre d'emplacement à « tous ».
        if (state.tab != tab) {
          state.slot = kCashShopSlotAll;
        }
        state.tab = tab;
        ImGui::EndTabItem();
      }
    }
    EndSkinTabBar();
  }
  // Une seule image : après quoi le joueur reprend la main.
  state.forced_tab = kCashShopTabCount;
}

void DrawFilters(CashShopWindowState& state, std::span<const CashShopCard> cards,
                 const SkinArt& art) {
  const float scale = Scale();
  ImGui::SetNextItemWidth(-1.0F);
  if (ImGui::InputTextWithHint("##cs_filter", core::Tr("Filtrer..."), state.filter.InputBuf,
                               IM_ARRAYSIZE(state.filter.InputBuf))) {
    state.filter.Build();
  }

  // ⚠ UN SEUL EMPLACEMENT — « autre » — VEUT DIRE UN ONGLET QUI NE S'ÉQUIPE
  // PAS : le filtre n'y servirait à rien.
  const std::vector<CashShopSlot> slots = SlotsOf(cards);
  const bool slot_filter_useful = slots.size() > 1;
  if (slot_filter_useful) {
    const char* shown = core::Tr("Emplacement: tous");
    if (state.slot != kCashShopSlotAll) {
      const auto found = std::ranges::find_if(
          slots, [&state](const CashShopSlot& slot) { return slot.key == state.slot; });
      if (found == slots.end()) {
        state.slot = kCashShopSlotAll;
      } else {
        shown = found->label;
      }
    }
    ImGui::SetNextItemWidth(kSlotComboPixels * scale);
    int step = 0;
    const bool open = BeginSkinCombo("##cs_slot", shown, art, &step);
    // Le rang zéro est « tous », les emplacements suivent.
    const auto at = std::ranges::find(slots, state.slot, &CashShopSlot::key);
    const int rank = at == slots.end() ? 0 : static_cast<int>(at - slots.begin()) + 1;
    if (const int to = SkinComboStepped(rank, step, static_cast<int>(slots.size()) + 1);
        to != rank) {
      state.slot = to == 0 ? kCashShopSlotAll : slots[static_cast<std::size_t>(to - 1)].key;
    }
    if (open) {
      ComboKeys keys(rank, static_cast<int>(slots.size()) + 1);
      if (keys.Took(0, ImGui::Selectable(core::Tr("Emplacement: tous"),
                                         state.slot == kCashShopSlotAll))) {
        state.slot = kCashShopSlotAll;
      }
      for (std::size_t index = 0; index < slots.size(); ++index) {
        const CashShopSlot& slot = slots[index];
        if (keys.Took(static_cast<int>(index) + 1,
                      ImGui::Selectable(slot.label, state.slot == slot.key))) {
          state.slot = slot.key;
        }
      }
      EndSkinCombo();
    }
  } else {
    state.slot = kCashShopSlotAll;
  }

  if (slot_filter_useful) {
    ImGui::SameLine();
  }
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted(core::Tr("Tri"));
  ImGui::SameLine();
  ImGui::SetNextItemWidth(kSortComboPixels * scale);
  static_cast<void>(SkinCombo("##cs_sort", &state.sort, kSortLabels, art));
  ImGui::SameLine();
  if (SkinButton(state.ascending ? core::Tr("Asc") : core::Tr("Desc"), art)) {
    state.ascending = !state.ascending;
  }
}

// Les cartes que les filtres retiennent, dans l'ordre du tri.
[[nodiscard]] std::vector<const CashShopCard*> VisibleCards(const CashShopWindowState& state,
                                                            std::span<const CashShopCard> cards) {
  std::vector<const CashShopCard*> shown;
  shown.reserve(cards.size());
  for (const CashShopCard& card : cards) {
    if (!SlotMatches(card, state.slot)) {
      continue;
    }
    // ⚠ PAR SES BORNES, sans copie : la liste se refiltre à chaque image, et
    // un onglet compte des milliers de cartes.
    if (state.filter.PassFilter(card.name.data(), card.name.data() + card.name.size())) {
      shown.push_back(&card);
    }
  }
  std::ranges::sort(shown, [&state](const CashShopCard* left, const CashShopCard* right) {
    int order = 0;
    if (state.sort == kSortId) {
      order = left->item_id < right->item_id ? -1 : (left->item_id > right->item_id ? 1 : 0);
    } else if (state.sort == kSortPrice) {
      order = left->price < right->price ? -1 : (left->price > right->price ? 1 : 0);
    } else {
      order = left->name.compare(right->name);
    }
    return state.ascending ? order < 0 : order > 0;
  });
  return shown;
}

// LE PANTIN QUI PORTE LA PIÈCE SURVOLÉE, et la molette qui le tourne.
void DrawDoll(CashShopWindowState& state, const CashShopCard& card) {
  state.doll.wanted = true;
  state.doll_item = card.item_id;
  state.doll_view = card.view_sprite;
  state.doll_location = card.equip_location;
  // ⚠ UNE IMAGE N'A PAS D'IDENTIFIANT : la zone de la molette se nomme sous
  // celui de la carte.
  const float notches = WheelOver(state.doll.wheel, ImGui::GetID("doll"), true);
  if (notches != 0.0F) {
    const int step = notches > 0.0F ? 1 : kDollDirections - 1;
    state.doll.direction =
        static_cast<std::uint8_t>((state.doll.direction + step) % kDollDirections);
  }
  DrawItemDollTooltip(state.doll);
}

// La géométrie d'une carte, toute à l'échelle de l'interface : le nom, le prix
// et les libellés des boutons sont du TEXTE, et une carte restée à sa taille
// les laisserait se chevaucher par-dessus une image minuscule.
struct CardGeometry {
  float height = 0.0F;
  float width = 0.0F;
  float gap = 0.0F;
  float pad_x = 0.0F;
  float pad_y = 0.0F;
  float column_gap = 0.0F;
  float banner = 0.0F;
  float low_top = 0.0F;
  float low_height = 0.0F;
  float image = 0.0F;
  float content_width = 0.0F;
};

[[nodiscard]] CardGeometry CardGeometryOf(const SkinArt& art) {
  const float scale = Scale();
  CardGeometry card;
  card.height = kCardHeightPixels * scale;
  card.gap = kCardGapPixels * scale;
  card.pad_x = kCardPadXPixels * scale;
  card.pad_y = kCardPadYPixels * scale;
  card.column_gap = kCardColumnGapPixels * scale;
  card.banner = card.height - kCardBoxPixels * scale;
  const float content_height = card.height - 2.0F * card.pad_y;
  card.low_top = card.banner - card.pad_y;
  card.low_height = content_height - card.low_top;
  card.image = card.low_height - kCardImageMarginPixels * scale;
  // ⚠ CALÉE SUR LES LIBELLÉS TRADUITS : la largeur d'origine tient « Achat
  // 1-Click », pas « Compra en 1 clic ». La grille recalcule son nombre de
  // colonnes, et le palier de la fenêtre suit.
  const std::array<const char*, 2> buttons = {core::Tr("Panier"), core::Tr("Achat 1-Click")};
  card.width =
      std::max(kCardLeastWidthPixels * scale,
               2.0F * card.pad_x + card.image + card.column_gap + SkinWidestButton(buttons, art));
  card.content_width = card.width - 2.0F * card.pad_x;
  return card;
}

// Le nom sur UNE ligne, dans le bandeau : la police rapetisse pour tenir dans
// la largeur, jusqu'à un plancher où elle reste lisible.
void DrawCardName(const CashShopCard& card, const CardGeometry& geometry, const SkinLook& look) {
  ImDrawList* draw = ImGui::GetWindowDrawList();
  const ImVec2 origin = ImGui::GetWindowPos();
  const float scale = Scale();
  draw->AddRectFilled(origin, ImVec2(origin.x + geometry.width, origin.y + geometry.banner),
                      look.card_band, kCardRoundingPixels * scale, ImDrawFlags_RoundCornersTop);
  const std::string name(card.name);
  const float base = ImGui::GetFontSize();
  const float room = geometry.width - kCardNameMarginBothPixels * scale;
  const float natural = ImGui::CalcTextSize(name.c_str()).x;
  float size = natural > room && natural > 0.0F ? base * (room / natural) : base;
  size = std::max(size, base * kCardNameLeastScale);
  draw->AddText(
      ImGui::GetFont(), size,
      ImVec2(origin.x + kCardNameMarginPixels * scale, origin.y + (geometry.banner - size) * 0.5F),
      look.card_band_text, name.c_str());
}

// LES GESTES D'UNE CARTE, ceux de tout item montré dans un commerce : le
// survol demande l'aperçu de sa description, le clic droit ouvre le menu des
// objets, Maj + clic gauche repose son lien dans la saisie. Rien ne se glisse :
// l'item n'est à personne tant qu'il n'est pas acheté.
//
// ⚠ UN BOUTON OU LE PANTIN SURVOLÉ GARDE SON INFOBULLE, et l'aperçu se tait :
// deux bulles se recouvriraient. Maj + clic sur un bouton reste au bouton.
//
// 🔴 LES CLICS SE LISENT AU RELÂCHEMENT : partie de l'enfoncement, la
// fenêtre qu'ils ouvrent naîtrait derrière la boutique.
void NoteCardGestures(const CashShopCard& card, bool card_hovered, bool button_hovered,
                      bool doll_shown, ItemLinkMenu& menu, CashShopWindowAction& action) {
  if (!card_hovered) {
    return;
  }
  if (!button_hovered && !doll_shown) {
    action.item.hovered_id = card.item_id;
    action.item.hovered_kind = ChatLinkKind::kItem;
    action.item.hovering = true;
  }
  const bool right_released = ImGui::IsMouseReleased(ImGuiMouseButton_Right);
  const TradeItemGesture gesture{.hovered = true,
                                 .repost = !button_hovered &&
                                           ImGui::IsMouseReleased(ImGuiMouseButton_Left) &&
                                           ImGui::GetIO().KeyShift,
                                 .menu = right_released && !ImGui::GetIO().KeyCtrl,
                                 .describe = right_released && ImGui::GetIO().KeyCtrl};
  if (NoteTradeItemLink(gesture, TradeItemView{.item_id = card.item_id, .name = card.name}, 0,
                        menu)) {
    action.item.gesture = ChatLinkGesture::kRepost;
    action.item.item_id = card.item_id;
  }
}

void DrawCard(CashShopWindowState& state, const CashShopView& view, const CashShopCard& card,
              const CardGeometry& geometry, const SkinArt& art, CashShopWindowAction& action,
              bool& preview_now) {
  ImGui::PushID(static_cast<int>(card.item_id));
  ImGui::PushStyleColor(ImGuiCol_ChildBg, art.look().card);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(geometry.pad_x, geometry.pad_y));
  // ⚠ LA CARTE NE PREND PAS LA MOLETTE : elle revient à la grille, sans quoi
  // survoler une carte bloquerait le défilement.
  ImGui::BeginChild("card", ImVec2(geometry.width, geometry.height), ImGuiChildFlags_Borders,
                    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
  action.drawn.push_back(card.item_id);
  DrawCardName(card, geometry, art.look());

  // ── L'IMAGE, à gauche, centrée dans sa rangée ─────────────────────────────
  float image_width = geometry.image;
  float image_height = geometry.image;
  if (card.image.usable() && card.image.width > 0.0F && card.image.height > 0.0F) {
    const float fit = geometry.image / std::max(card.image.width, card.image.height);
    image_width = card.image.width * fit;
    image_height = card.image.height * fit;
  }
  ImGui::SetCursorPos(ImVec2((geometry.image - image_width) * 0.5F,
                             geometry.low_top + (geometry.low_height - image_height) * 0.5F));
  if (card.image.usable()) {
    ImGui::Image(card.image.id, ImVec2(image_width, image_height));
  } else {
    ImGui::Dummy(ImVec2(image_width, image_height));
  }
  const bool doll_shown = card.wearable_preview && ImGui::IsItemHovered();
  if (doll_shown) {
    DrawDoll(state, card);
    preview_now = true;
  }

  // ⚠ LE SOLDE QUI COMPTE EST CELUI QUE L'ACHAT DÉPENSERA : les deux monnaies
  // avec la case, les points de vote seuls sans elle. Et quand cocher la case
  // suffirait, l'infobulle le dit.
  const long long spendable =
      CashShopSpendable(view.vote_points, view.event_points, state.use_event_points);
  const long long both = CashShopSpendable(view.vote_points, view.event_points, true);
  const bool affordable = static_cast<long long>(card.price) <= spendable;

  // ── LA COLONNE DE DROITE : le prix, puis deux boutons pleine largeur ──────
  const float frame_height = ImGui::GetFrameHeight();
  const float spacing = ImGui::GetStyle().ItemSpacing.y;
  const float column_x = geometry.image + geometry.column_gap;
  const float column_width = geometry.content_width - column_x;
  const float column_height = ImGui::GetTextLineHeight() + 2.0F * frame_height + 2.0F * spacing;
  const float column_y = geometry.low_top + (geometry.low_height - column_height) * 0.5F;
  std::array<char, kLabelBytes> price{};
  std::snprintf(price.data(), price.size(), core::Tr("%d pts"), card.price);
  const float price_width = ImGui::CalcTextSize(price.data()).x;
  ImGui::SetCursorPos(
      ImVec2(column_x + (column_width > price_width ? (column_width - price_width) * 0.5F : 0.0F),
             column_y));
  // Un prix hors de portée s'écrit en rouge, comme dans tout commerce.
  ImGui::TextColored(affordable ? kTradeBlackText : kTradeUnaffordableText, "%s", price.data());

  const bool event_helps = !state.use_event_points && static_cast<long long>(card.price) <= both;
  const char* why =
      event_helps ? core::Tr("Solde Vote insuffisant - cochez \"Utiliser Event\" pour cumuler")
                  : core::Tr("Solde Vote + Event insuffisant pour cet item");
  if (!affordable) {
    ImGui::BeginDisabled();
  }
  ImGui::SetCursorPos(ImVec2(column_x, column_y + ImGui::GetTextLineHeight() + spacing));
  if (SkinButton(core::Tr("Panier"), art, ImVec2(column_width, frame_height))) {
    AddToCashShopCart(state, card.item_id, state.tab, card.price);
  }
  bool button_hovered = false;
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    button_hovered = true;
    SkinTooltipFormat("%s",
                      affordable ? core::Tr("Ajouter au panier (achat groupé via Acheter)") : why);
  }
  ImGui::SetCursorPos(
      ImVec2(column_x, column_y + ImGui::GetTextLineHeight() + frame_height + 2.0F * spacing));
  if (SkinButton(core::Tr("Achat 1-Click"), art, ImVec2(column_width, frame_height))) {
    action.purchase = {CashShopCartLine{
        .item_id = card.item_id, .tab = state.tab, .amount = 1, .price = card.price}};
    action.event_points_spent =
        CashShopEventPointsToSpend(card.price, view.event_points, state.use_event_points);
    action.close_after_purchase = true;
  }
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    button_hovered = true;
    SkinTooltipFormat(
        "%s", affordable ? core::Tr("Achat immédiat d'1 unité, puis fermeture du shop") : why);
  }
  if (!affordable) {
    ImGui::EndDisabled();
  }
  ImGui::EndChild();
  ImGui::PopStyleVar();
  ImGui::PopStyleColor();
  NoteCardGestures(card, ImGui::IsItemHovered(), button_hovered, doll_shown, state.item_menu,
                   action);
  ImGui::PopID();
}

void DrawGrid(CashShopWindowState& state, const CashShopView& view, const SkinArt& art,
              float grid_width, CashShopWindowAction& action) {
  const float window_width = ImGui::GetWindowWidth();
  const float window_height = ImGui::GetWindowHeight();
  // Un aperçu survolé à l'image précédente gèle le défilement : la molette
  // reste au pantin.
  ImGui::BeginChild("cs_grid", ImVec2(grid_width, 0.0F), ImGuiChildFlags_Borders,
                    state.preview_active ? ImGuiWindowFlags_NoScrollWithMouse : 0);
  bool preview_now = false;
  const CardGeometry geometry = CardGeometryOf(art);
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(geometry.gap, geometry.gap));
  const ImVec2 inner = ImGui::GetContentRegionAvail();
  // Ce que la fenêtre porte autour des cartes : le palier de la taille s'en
  // sert à l'image suivante.
  state.card_width = geometry.width;
  state.card_height = geometry.height;
  state.card_gap = geometry.gap;
  state.chrome_width = window_width - inner.x;
  state.chrome_height = window_height - inner.y;
  state.snap_known = true;
  const int columns =
      std::max(1, static_cast<int>((inner.x + geometry.gap) / (geometry.width + geometry.gap)));

  // 🔴 UN ONGLET PEUT COMPTER DES MILLIERS DE CARTES : seules les rangées
  // visibles se dessinent, sans quoi la fenêtre coûterait des milliers
  // d'enfants à chaque image.
  const std::vector<const CashShopCard*> shown = VisibleCards(state, view.tabs[state.tab]);
  const int count = static_cast<int>(shown.size());
  const int rows = (count + columns - 1) / columns;
  ImGuiListClipper clipper;
  clipper.Begin(rows, geometry.height + geometry.gap);
  while (clipper.Step()) {
    for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
      for (int column = 0; column < columns; ++column) {
        const int index = row * columns + column;
        if (index >= count) {
          break;
        }
        if (column > 0) {
          ImGui::SameLine(0.0F, geometry.gap);
        }
        DrawCard(state, view, *shown[static_cast<std::size_t>(index)], geometry, art, action,
                 preview_now);
      }
    }
  }
  clipper.End();
  ImGui::PopStyleVar();
  if (count == 0) {
    ImGui::TextDisabled("%s", core::Tr("(aucun item dans cette catégorie)"));
  }
  ImGui::EndChild();
  state.preview_active = preview_now;
}

void DrawCart(CashShopWindowState& state, const CashShopView& view, const SkinArt& art,
              float cart_width, CashShopWindowAction& action) {
  const float scale = Scale();
  ImGui::SameLine();
  ImGui::BeginChild("cs_cart", ImVec2(cart_width, 0.0F), ImGuiChildFlags_Borders);
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted(core::Tr("Panier"));
  ImGui::SameLine();
  if (!state.cart.empty() && SkinButton(core::Tr("Vider"), art)) {
    state.cart.clear();
  }
  ImGui::Separator();
  long long total = 0;
  std::size_t removed = state.cart.size();
  ImGui::BeginChild("cs_cart_items", ImVec2(0.0F, -kCartFooterPixels * scale));
  for (std::size_t index = 0; index < state.cart.size(); ++index) {
    CashShopCartLine& line = state.cart[index];
    total += static_cast<long long>(line.price) * line.amount;
    ImGui::PushID(static_cast<int>(index));
    const std::string_view name = NameOf(view, line);
    ImGui::TextWrapped("%.*s", static_cast<int>(name.size()), name.data());
    // [-] [champ] [+] : de petits boutons carrés, à la hauteur de la ligne.
    const float step = ImGui::GetFrameHeight();
    if (SkinButton("-", art, ImVec2(step, step)) && line.amount > kLeastPerLine) {
      --line.amount;
    }
    ImGui::SameLine(0.0F, kQuantityGapPixels * scale);
    ImGui::SetNextItemWidth(kQuantityPixels * scale);
    if (ImGui::InputInt("##qty", &line.amount, 0, 0)) {
      line.amount = std::clamp(line.amount, kLeastPerLine, kCashShopMostPerLine);
    }
    ImGui::SameLine(0.0F, kQuantityGapPixels * scale);
    if (SkinButton("+", art, ImVec2(step, step)) && line.amount < kCashShopMostPerLine) {
      ++line.amount;
    }
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(kBlackText, core::Tr("%lld pts"),
                       static_cast<long long>(line.price) * line.amount);
    ImGui::SameLine();
    const float right = ImGui::GetContentRegionMax().x - step;
    if (right > ImGui::GetCursorPosX()) {
      ImGui::SetCursorPosX(right);
    }
    if (SkinButton("x", art, ImVec2(step, step))) {
      removed = index;
    }
    ImGui::Separator();
    ImGui::PopID();
  }
  ImGui::EndChild();
  if (removed < state.cart.size()) {
    state.cart.erase(state.cart.begin() + static_cast<std::ptrdiff_t>(removed));
  }
  ImGui::Separator();
  ImGui::Text(core::Tr("Total: %lld pts"), total);
  const bool affordable =
      total <= CashShopSpendable(view.vote_points, view.event_points, state.use_event_points);
  const bool empty = state.cart.empty();
  if (empty) {
    ImGui::BeginDisabled();
  }
  if (SkinButton(affordable ? core::Tr("Acheter") : core::Tr("Points insuffisants"), art,
                 ImVec2(ImGui::GetContentRegionAvail().x, 0.0F))) {
    action.purchase = state.cart;
    action.event_points_spent =
        CashShopEventPointsToSpend(total, view.event_points, state.use_event_points);
  }
  if (empty) {
    ImGui::EndDisabled();
  }
  ImGui::EndChild();
}

// La taille ne saute que d'une colonne ou d'une ligne entière de cartes.
void SnapToCards(ImGuiSizeCallbackData* data) {
  const auto* state = static_cast<const CashShopWindowState*>(data->UserData);
  const float step_x = state->card_width + state->card_gap;
  const float step_y = state->card_height + state->card_gap;
  const float grid_width = data->DesiredSize.x - state->chrome_width;
  const float grid_height = data->DesiredSize.y - state->chrome_height;
  const int columns = std::max(1, static_cast<int>((grid_width + state->card_gap) / step_x + 0.5F));
  const int rows = std::max(1, static_cast<int>((grid_height + state->card_gap) / step_y + 0.5F));
  data->DesiredSize.x =
      state->chrome_width + static_cast<float>(columns) * step_x - state->card_gap;
  data->DesiredSize.y = state->chrome_height + static_cast<float>(rows) * step_y - state->card_gap;
}

}  // namespace

bool CashShopTabShown(std::size_t tab) noexcept {
  return tab < kCashShopTabCount && kTabShown[tab];
}

void AddToCashShopCart(CashShopWindowState& state, std::uint32_t item_id, std::size_t tab,
                       std::int32_t price) {
  for (CashShopCartLine& line : state.cart) {
    if (line.item_id == item_id && line.tab == tab) {
      line.amount = std::min(line.amount + 1, kCashShopMostPerLine);
      return;
    }
  }
  state.cart.push_back(
      CashShopCartLine{.item_id = item_id, .tab = tab, .amount = 1, .price = price});
}

void RevealCashShopItem(CashShopWindowState& state, std::uint32_t item_id, std::size_t tab,
                        std::int32_t price, std::string_view name) {
  state.forced_tab = tab;
  state.tab = tab;
  // Un filtre d'emplacement resté d'avant le cacherait.
  state.slot = kCashShopSlotAll;
  // Le filtre posé sur son nom : l'onglet peut compter des milliers de cartes,
  // et « ajouté au panier » sans le montrer laisserait le joueur devant une
  // grille où rien n'a l'air d'avoir bougé.
  std::snprintf(state.filter.InputBuf, IM_ARRAYSIZE(state.filter.InputBuf), "%.*s",
                static_cast<int>(name.size()), name.data());
  state.filter.Build();
  AddToCashShopCart(state, item_id, tab, price);
}

void OpenCashShopOn(CashShopWindowState& state, std::uint32_t opening_seq,
                    std::uint32_t purchase_seq, std::uint32_t server_tab) {
  state.open = true;
  state.opening_seen = opening_seq;
  state.purchase_seen = purchase_seq;
  state.placed = false;
  if (server_tab < kCashShopTabCount && CashShopTabShown(server_tab)) {
    state.tab = server_tab;
    state.forced_tab = server_tab;
  }
}

std::uint32_t CashShopEventPointsToSpend(long long total, std::uint32_t balance,
                                         bool use_event_points) noexcept {
  if (!use_event_points || total <= 0) {
    return 0;
  }
  return static_cast<std::uint32_t>(std::min<long long>(total, balance));
}

long long CashShopSpendable(std::uint32_t vote_points, std::uint32_t event_points,
                            bool use_event_points) noexcept {
  return static_cast<long long>(vote_points) + (use_event_points ? event_points : 0U);
}

CashShopWindowAction DrawCashShopWindow(CashShopWindowState& state, const CashShopView& view,
                                        const SkinArt& art) {
  CashShopWindowAction action;
  state.doll.wanted = false;
  if (!state.open) {
    return action;
  }
  const float scale = Scale();
  if (!state.placed) {
    state.placed = true;
    ImGui::SetNextWindowFocus();
  }
  ImGui::SetNextWindowPos(ImVec2(kFirstLeftPixels, kFirstTopPixels), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(kFirstWidthPixels * scale, kFirstHeightPixels * scale),
                           ImGuiCond_FirstUseEver);
  if (state.snap_known) {
    ImGui::SetNextWindowSizeConstraints(
        ImVec2(state.chrome_width + state.card_width, state.chrome_height + state.card_height),
        ImVec2(FLT_MAX, FLT_MAX), SnapToCards, &state);
  }
  SetNextWindowRounding(kWindowRoundingPixels);
  PoseBugReportButton(state.bug_report);
  // ⚠ LE TITRE EST LE NOM QUE LE SERVEUR DONNE À SA BOUTIQUE, l'identité reste
  // la nôtre : la place se retient sous un nom qui ne dépend pas de la table.
  const std::string title = std::string(view.title) + "###" + kCashShopWindowId;
  const bool body_shown = BeginSkinnedWindow(title.c_str(), &state.open, art, SkinnedTitle::kBar,
                                             ImGuiWindowFlags_NoCollapse);
  action.report_bug = state.bug_report && TitleActionClicked(0);
  if (!state.open) {
    action.close = true;
  }
  if (body_shown && state.open) {
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, kChildRoundingPixels * scale);
    DrawHeader(state, view, art);
    DrawTabs(state, view, art);
    DrawFilters(state, view.tabs[state.tab], art);
    // LA GRILLE À GAUCHE, LE PANIER À DROITE — et le panier se masque vide : la
    // grille prend alors toute la largeur.
    const float available = ImGui::GetContentRegionAvail().x;
    const bool show_cart = !state.cart.empty();
    const float cart_width = kCartWidthPixels * scale;
    const float grid_width = show_cart ? std::max(kLeastGridWidthPixels * scale,
                                                  available - cart_width - kCartGapPixels * scale)
                                       : 0.0F;
    DrawGrid(state, view, art, grid_width, action);
    if (show_cart) {
      DrawCart(state, view, art, cart_width, action);
    }
    if (const ChatLinkAsk chosen = DrawTradeItemMenu(kItemMenuId, state.item_menu, {});
        chosen.gesture != ChatLinkGesture::kNothing) {
      action.item.gesture = chosen.gesture;
      action.item.item_id = chosen.item_id;
    }
    ImGui::PopStyleVar();
  }
  EndSkinnedWindow();
  return action;
}

}  // namespace ro::ui
