#include "ro/ui/trade_widgets.h"

#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <string_view>

#include "ro/core/decimal_text.h"
#include "ro/core/i18n.h"
#include "ro/ui/inventory_window.h"
#include "ro/ui/modal_box.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"

#include "ui_scale.h"
#include "ui_text.h"

namespace ro::ui {
namespace {

// L'écart entre la base grisée, la flèche et le prix facturé.
constexpr float kPriceGapPixels = 3.0F;
// L'écart entre deux boutons de palier.
constexpr float kStepGapPixels = 2.0F;
// La largeur la plus étroite d'un bouton de palier : « +1 » ne doit pas faire
// un bouton plus petit que le doigt qui le vise.
constexpr float kStepLeastWidthPixels = 26.0F;
// Le champ de quantité, entre ses deux boutons.
constexpr float kQuantityFieldPixels = 42.0F;
// Le palier qui s'écrit « +1k » : le dernier de l'achat.
constexpr int kThousandStep = 1000;
// L'épaisseur d'un filet `ImGui::Separator`, qui ne suit pas l'échelle.
constexpr float kSeparatorThicknessPixels = 1.0F;
// L'ombre d'un objet cassé se pose un pixel plus bas et plus à droite.
constexpr float kDamagedShadowOffsetPixels = 1.0F;
// Un montant se tape en base dix, par groupes de trois chiffres.
constexpr std::int64_t kDecimalBase = 10;
constexpr int kThousandsGroupDigits = 3;
constexpr std::int64_t kThousand = 1'000;
constexpr std::int64_t kMillion = 1'000'000;
constexpr std::int64_t kBillion = 1'000'000'000;
// Le badge d'une case se tient à deux pixels du bord droit.
constexpr float kBadgePadPixels = 2.0F;
// La boîte d'avis : la largeur où son texte revient à la ligne, et son bouton.
constexpr float kNoticeWrapPixels = 320.0F;
constexpr float kNoticeButtonPixels = 90.0F;

// Le point d'interrogation d'une icône absente, centré sur sa case.
void DrawMissingIcon(float side) {
  const ImVec2 at = ImGui::GetCursorScreenPos();
  ImGui::Dummy(ImVec2(side, side));
  const ImVec2 mark = ImGui::CalcTextSize("?");
  ImGui::GetWindowDrawList()->AddText(
      ImVec2(at.x + (side - mark.x) * 0.5F, at.y + (side - mark.y) * 0.5F),
      ImGui::GetColorU32(ImGuiCol_TextDisabled), "?");
}

// Le libellé d'un palier, son rang en identité : deux paliers de même valeur
// restent deux boutons.
//
// Au pire : « + », le montant, « ## » et le rang.
constexpr std::size_t kStepLabelBytes = std::string_view("+##").size() +
                                        ro::core::kDecimalTextBytes<int> +
                                        ro::core::kDecimalTextBytes<std::size_t>;
using StepLabel = std::array<char, kStepLabelBytes>;

[[nodiscard]] StepLabel StepLabelOf(int amount, std::size_t rank) {
  StepLabel label{};
  if (amount == kThousandStep) {
    std::snprintf(label.data(), label.size(), "+1k##%zu", rank);
  } else {
    std::snprintf(label.data(), label.size(), "+%d##%zu", amount, rank);
  }
  return label;
}

[[nodiscard]] float StepWidth(const StepLabel& label, const SkinArt& art) {
  return std::max(kStepLeastWidthPixels * Scale(), SkinSmallButtonWidth(label.data(), art));
}

}  // namespace

TradeItemGesture DrawTradeItem(const TradeItemView& item, std::string_view suffix) {
  TradeItemGesture gesture;
  const float side = kTradeIconPixels * Scale();
  if (item.icon.usable()) {
    ImGui::ImageWithBg(item.icon.id, ImVec2(side, side), ImVec2(0.0F, 0.0F), ImVec2(1.0F, 1.0F),
                       ImVec4(0.0F, 0.0F, 0.0F, 0.0F),
                       item.damaged ? ImGui::ColorConvertU32ToFloat4(kItemDamagedTint)
                                    : ImVec4(1.0F, 1.0F, 1.0F, 1.0F));
  } else {
    DrawMissingIcon(side);
  }
  AddLastItemGesture(gesture);
  ImGui::SameLine();

  std::string label(item.name.empty() ? std::string_view("?") : item.name);
  if (!suffix.empty()) {
    label.push_back(' ');
    label.append(suffix);
  }
  // L'OMBRE ROUGE D'UN OBJET CASSÉ, sous le texte : c'est le signe du client,
  // et le seul qui se voie sur une ligne sans icône chargée.
  ImGui::AlignTextToFramePadding();
  if (item.damaged) {
    const ImVec2 at = ImGui::GetCursorScreenPos();
    const float offset = kDamagedShadowOffsetPixels * Scale();
    ImGui::GetWindowDrawList()->AddText(ImVec2(at.x + offset, at.y + offset), kItemDamagedTint,
                                        label.c_str());
  }
  ImGui::TextUnformatted(label.c_str());
  AddLastItemGesture(gesture);
  return gesture;
}

void AddLastItemGesture(TradeItemGesture& gesture) {
  if (!ImGui::IsItemHovered()) {
    return;
  }
  gesture.hovered = true;
  const bool right_released = ImGui::IsMouseReleased(ImGuiMouseButton_Right);
  const bool ctrl_held = ImGui::GetIO().KeyCtrl;
  gesture.menu |= right_released && !ctrl_held;
  gesture.describe |= right_released && ctrl_held;
  gesture.repost |= ImGui::IsMouseReleased(ImGuiMouseButton_Left) && ImGui::GetIO().KeyShift;
}

bool NoteTradeItemLink(const TradeItemGesture& gesture, const TradeItemView& item,
                       std::uint32_t token, ItemLinkMenu& menu) {
  if (gesture.menu || gesture.describe) {
    menu.armed = gesture.menu;
    menu.describe_at_once = gesture.describe;
    menu.kind = ChatLinkKind::kItem;
    menu.item_id = item.item_id;
    menu.token = token;
    menu.label = std::string(item.name);
  }
  return gesture.repost;
}

ChatLinkAsk DrawTradeItemMenu(const char* popup_id, ItemLinkMenu& menu, ItemLinkFacts link_facts) {
  ChatLinkAsk chosen;
  DrawItemLinkMenu(popup_id, menu, chosen, link_facts);
  return chosen;
}

std::string TradeAmountText(std::int64_t amount, std::string_view unit) {
  return GroupThousands(amount) + std::string(unit);
}

void DrawTradePrice(std::int64_t base_price, std::int64_t price, ImVec4 color,
                    std::string_view unit) {
  const float gap = kPriceGapPixels * Scale();
  if (base_price > 0 && base_price != price) {
    ImGui::TextDisabled("%s", GroupThousands(base_price).c_str());
    ImGui::SameLine(0.0F, gap);
    ImGui::TextDisabled("->");
    ImGui::SameLine(0.0F, gap);
  }
  ImGui::TextColored(color, "%s", TradeAmountText(price, unit).c_str());
}

float TradePriceWidth(std::int64_t base_price, std::int64_t price, std::string_view unit) {
  float width = ImGui::CalcTextSize(TradeAmountText(price, unit).c_str()).x;
  if (base_price > 0 && base_price != price) {
    const float gap = kPriceGapPixels * Scale();
    width += ImGui::CalcTextSize(GroupThousands(base_price).c_str()).x +
             ImGui::CalcTextSize("->").x + gap * 2.0F;
  }
  return width;
}

void DrawTradeHint(std::string_view text) {
  ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
  ImGui::PushTextWrapPos(0.0F);
  Unformatted(text);
  ImGui::PopTextWrapPos();
  ImGui::PopStyleColor();
}

float TradeFooterHeight(const SkinArt& art) {
  // L'interligne sous la liste, le filet et son interligne, puis la ligne la
  // plus haute du pied : le bouton habillé ou le texte aligné sur un cadre.
  const ImGuiStyle& style = ImGui::GetStyle();
  return style.ItemSpacing.y * 2.0F + kSeparatorThicknessPixels +
         std::max(SkinButtonHeight(art), ImGui::GetFrameHeight());
}

TradeSteps TradeStepsFor(int most, bool selling) {
  TradeSteps steps;
  if (most <= 1) {
    steps.values[0] = 1;
    steps.count = 1;
    return steps;
  }
  for (const int step : kTradeQuantitySteps) {
    if (step < most && steps.count + 1 < kTradeMostSteps) {
      steps.values[steps.count++] = step;
    } else if (step == most) {
      break;
    }
  }
  // LE DERNIER BOUTON DIT « TOUT » : ce qu'on possède à la vente, et ce qu'on
  // peut prendre à l'achat quand c'est moins qu'un palier entier.
  if (selling || most < kTradeQuantitySteps.back()) {
    steps.values[steps.count++] = most;
  } else {
    steps.values[steps.count++] = kTradeQuantitySteps.back();
  }
  return steps;
}

TradeStepClick DrawTradeSteps(const TradeSteps& steps, const SkinArt& art, std::int64_t unit_price,
                              std::int64_t purse) {
  TradeStepClick click;
  const bool ctrl = ImGui::GetIO().KeyCtrl;
  const float scale = Scale();
  for (std::size_t rank = 0; rank < steps.count; ++rank) {
    const int amount = steps.values[rank];
    if (rank > 0) {
      ImGui::SameLine(0.0F, kStepGapPixels * scale);
    }
    const StepLabel label = StepLabelOf(amount, rank);
    const bool too_dear = ctrl && purse != kTradeNoPurse && unit_price * amount > purse;
    if (too_dear) {
      ImGui::BeginDisabled();
    }
    if (SkinSmallButton(label.data(), art, ImVec2(StepWidth(label, art), 0.0F))) {
      click.amount = amount;
      click.immediate = ctrl;
    }
    if (too_dear) {
      ImGui::EndDisabled();
    }
  }
  return click;
}

float TradeStepsWidth(const TradeSteps& steps, const SkinArt& art) {
  float width = 0.0F;
  for (std::size_t rank = 0; rank < steps.count; ++rank) {
    if (rank > 0) {
      width += kStepGapPixels * Scale();
    }
    width += StepWidth(StepLabelOf(steps.values[rank], rank), art);
  }
  return width;
}

bool DrawTradeQuantity(int& amount, int most, const SkinArt& art) {
  const float scale = Scale();
  const float step = ImGui::GetFrameHeight();
  const int before = amount;
  if (SkinButton("-", art, ImVec2(step, step)) && amount > 1) {
    --amount;
  }
  ImGui::SameLine(0.0F, kStepGapPixels * scale);
  ImGui::SetNextItemWidth(kQuantityFieldPixels * scale);
  if (ImGui::InputInt("##qty", &amount, 0, 0)) {
    amount = std::clamp(amount, 1, std::max(most, 1));
  }
  ImGui::SameLine(0.0F, kStepGapPixels * scale);
  if (SkinButton("+", art, ImVec2(step, step)) && amount < most) {
    ++amount;
  }
  return amount != before;
}

std::optional<std::int64_t> ParseTradeAmount(std::string_view text) {
  std::int64_t amount = 0;
  bool saturated = false;
  // Les chiffres du groupe en cours, et s'il y a déjà eu un séparateur : après
  // le premier, chaque groupe compte trois chiffres, ni plus ni moins.
  int group_digits = 0;
  bool grouped = false;
  bool any_digit = false;
  std::size_t at = 0;
  // Les blancs du bord ne comptent pas : un copier-coller en apporte.
  while (at < text.size() && text[at] == ' ') {
    ++at;
  }
  std::size_t end = text.size();
  while (end > at && text[end - 1] == ' ') {
    --end;
  }
  for (; at < end; ++at) {
    const char typed = text[at];
    if (typed >= '0' && typed <= '9') {
      any_digit = true;
      ++group_digits;
      if (grouped && group_digits > kThousandsGroupDigits) {
        return std::nullopt;
      }
      if (!saturated) {
        amount = amount * kDecimalBase + (typed - '0');
        if (amount > kTradeTypedAmountCeiling) {
          saturated = true;
        }
      }
      continue;
    }
    if (typed == ',' || typed == '.' || typed == ' ' || typed == '\'' || typed == '_') {
      // Le premier groupe a d'un à trois chiffres ; les suivants, trois.
      if (group_digits == 0 || group_digits > kThousandsGroupDigits ||
          (grouped && group_digits != kThousandsGroupDigits)) {
        return std::nullopt;
      }
      grouped = true;
      group_digits = 0;
      continue;
    }
    return std::nullopt;
  }
  if (!any_digit || (grouped && group_digits != kThousandsGroupDigits)) {
    return std::nullopt;
  }
  return saturated ? kTradeTypedAmountCeiling : amount;
}

std::string SpelledTradeAmount(std::int64_t amount) {
  amount = std::max<std::int64_t>(amount, 0);
  const std::int64_t billions = amount / kBillion;
  const std::int64_t millions = amount / kMillion % kThousand;
  const std::int64_t thousands = amount / kThousand % kThousand;
  const std::int64_t units = amount % kThousand;
  std::string spelled;
  const auto say = [&spelled](std::int64_t count, const char* one, const char* many) {
    if (count == 0) {
      return;
    }
    std::array<char, 64> piece{};
    // Les deux gabarits sortent de NOTRE catalogue (`core::Tr` d'un littéral),
    // et portent chacun un seul `%lld` : g++ ne peut pas le voir à travers le
    // choix entre les deux.
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
    std::snprintf(piece.data(), piece.size(), count == 1 ? one : many,
                  static_cast<long long>(count));
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
    if (!spelled.empty()) {
      spelled.push_back(' ');
    }
    spelled.append(piece.data());
  };
  say(billions, core::Tr("%lld milliard"), core::Tr("%lld milliards"));
  say(millions, core::Tr("%lld million"), core::Tr("%lld millions"));
  say(thousands, core::Tr("%lld mille"), core::Tr("%lld mille"));
  if (units != 0 || spelled.empty()) {
    if (!spelled.empty()) {
      spelled.push_back(' ');
    }
    spelled.append(std::to_string(units));
  }
  return spelled;
}

TradeItemGesture DrawTradeTile(const TradeItemView& item, int refine_level, int amount) {
  TradeItemGesture gesture;
  const float side = kTradeTilePixels * Scale();
  const ImVec2 top_left = ImGui::GetCursorScreenPos();
  ImGui::InvisibleButton("##tile", ImVec2(side, side));
  AddLastItemGesture(gesture);
  const ImVec2 bottom_right(top_left.x + side, top_left.y + side);
  ImDrawList* into = ImGui::GetWindowDrawList();
  if (gesture.hovered) {
    into->AddRectFilled(top_left, bottom_right, kItemHoverVeil);
  }
  // L'icône à sa taille d'art, centrée, RÉDUITE seulement si elle déborde.
  if (item.icon.usable() && item.icon.width > 0.0F && item.icon.height > 0.0F) {
    const float scale = Scale();
    const float fit =
        std::min({1.0F, side / (item.icon.width * scale), side / (item.icon.height * scale)});
    const ImVec2 size(item.icon.width * scale * fit, item.icon.height * scale * fit);
    const ImVec2 at(top_left.x + (side - size.x) * 0.5F, top_left.y + (side - size.y) * 0.5F);
    into->AddImage(item.icon.id, at, ImVec2(at.x + size.x, at.y + size.y), ImVec2(0.0F, 0.0F),
                   ImVec2(1.0F, 1.0F), item.damaged ? kItemDamagedTint : IM_COL32_WHITE);
  } else {
    const ImVec2 mark = ImGui::CalcTextSize("?");
    into->AddText(ImVec2(top_left.x + (side - mark.x) * 0.5F, top_left.y + (side - mark.y) * 0.5F),
                  ImGui::GetColorU32(ImGuiCol_TextDisabled), "?");
  }
  // Le badge : le raffinement, SINON la quantité, et jamais les deux.
  std::array<char, 16> badge{};
  if (refine_level > 0) {
    std::snprintf(badge.data(), badge.size(), "+%d", refine_level);
  } else if (amount > 1) {
    std::snprintf(badge.data(), badge.size(), "%d", amount);
  }
  if (badge[0] != '\0') {
    ImFont* const font = ImGui::GetFont();
    const float base = ImGui::GetFontSize() * kItemBadgeScale;
    const float room = side - kBadgePadPixels * Scale();
    ImVec2 size = font->CalcTextSizeA(base, FLT_MAX, 0.0F, badge.data());
    const float font_size = ItemBadgeFontSize(base, size.x, room);
    size = font->CalcTextSizeA(font_size, FLT_MAX, 0.0F, badge.data());
    const ImVec2 at(std::max(top_left.x, bottom_right.x - size.x - kBadgePadPixels * Scale()),
                    bottom_right.y - size.y);
    into->PushClipRect(top_left, bottom_right, true);
    // Cerne blanc, texte noir dessus : lisible sur n'importe quelle icône.
    for (int dx = -1; dx <= 1; ++dx) {
      for (int dy = -1; dy <= 1; ++dy) {
        if (dx != 0 || dy != 0) {
          into->AddText(font, font_size,
                        ImVec2(at.x + static_cast<float>(dx), at.y + static_cast<float>(dy)),
                        IM_COL32_WHITE, badge.data());
        }
      }
    }
    into->AddText(font, font_size, at, IM_COL32_BLACK, badge.data());
    into->PopClipRect();
  }
  return gesture;
}

void DrawTradeNotice(TradeNoticeState& notice, const char* id, const SkinArt& art) {
  if (notice.text.empty()) {
    notice.asked = false;
    return;
  }
  if (!notice.asked) {
    AskModal(id);
    notice.asked = true;
  }
  if (!BeginSkinnedModal(id, art)) {
    // Fermée par Échap : l'avis est lu.
    notice.text.clear();
    notice.asked = false;
    return;
  }
  ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + kNoticeWrapPixels * Scale());
  ImGui::TextUnformatted(notice.text.c_str());
  ImGui::PopTextWrapPos();
  ImGui::Spacing();
  if (SkinButton(core::Tr("OK"), art, ImVec2(kNoticeButtonPixels * Scale(), 0.0F)) ||
      ImGui::IsKeyPressed(ImGuiKey_Enter)) {
    notice.text.clear();
    notice.asked = false;
    ImGui::CloseCurrentPopup();
  }
  EndSkinnedModal();
}

}  // namespace ro::ui
