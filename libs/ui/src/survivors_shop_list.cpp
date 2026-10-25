#include "survivors_shop_list.h"

#include <algorithm>
#include <array>
#include <cstdio>

#include "ro/core/i18n.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"
#include "ro/ui/window_bounds.h"

#include "ui_text.h"

namespace ro::ui {
namespace {

constexpr float kRowIconSide = 24.0F;
constexpr float kPipRadius = 4.0F;
constexpr float kPipGap = 3.0F;
constexpr ImU32 kPipFull = IM_COL32(255, 215, 90, 255);
constexpr ImU32 kPipEmpty = IM_COL32(150, 150, 150, 255);
constexpr ImU32 kSelectedEdge = IM_COL32(255, 215, 90, 200);
constexpr ImU32 kRefusalColor = IM_COL32(255, 120, 90, 255);
constexpr std::size_t kLineSize = 160;

// Les pastilles d'un rang : pleines jusqu'au rang tenu, vides au-delà.
void DrawPips(int rank, int most_rank) {
  const ImVec2 origin = ImGui::GetCursorScreenPos();
  const float line = ImGui::GetTextLineHeight();
  ImDrawList& draw = *ImGui::GetWindowDrawList();
  const float step = kPipRadius * 2.0F + kPipGap;
  for (int pip = 0; pip < most_rank; ++pip) {
    const ImVec2 center(origin.x + kPipRadius + step * static_cast<float>(pip),
                        origin.y + line * 0.5F);
    if (pip < rank) {
      draw.AddCircleFilled(center, kPipRadius, kPipFull);
    } else {
      draw.AddCircle(center, kPipRadius, kPipEmpty);
    }
  }
  ImGui::Dummy(ImVec2(step * static_cast<float>(std::max(most_rank, 0)), line));
}

const char* RefusalText(SurvivorsShopRefusal refusal) {
  switch (refusal) {
    case SurvivorsShopRefusal::kMostRank:
      return core::Tr("Rang maximal");
    case SurvivorsShopRefusal::kNotEnoughZeny:
      return core::Tr("Zeny insuffisants");
  }
  return "";
}

// Une ligne ; vrai quand son bouton d'achat a été cliqué.
[[nodiscard]] bool DrawRow(const SurvivorsShopRow& row, bool selected, std::uint64_t zeny,
                           const SurvivorsShopLook& look, const SkinArt& art) {
  const std::optional<SurvivorsShopRefusal> refusal = RefusalOf(row, zeny);
  ImGui::BeginGroup();
  if (row.icon != 0) {
    ImGui::Image(row.icon, ImVec2(kRowIconSide, kRowIconSide));
  } else {
    ImGui::Dummy(ImVec2(kRowIconSide, kRowIconSide));
  }
  ImGui::SameLine();
  ImGui::BeginGroup();
  ImGui::TextUnformatted(row.name);
  ImGui::SameLine();
  DrawPips(row.rank, row.most_rank);
  FaintText(look.describe(row));
  const SurvivorsEffectText now = look.effect(row, row.effect_now);
  if (row.rank < row.most_rank) {
    const SurvivorsEffectText next = look.effect(row, row.effect_next);
    ImGui::Text("%s  »  %s", now.data(), next.data());
  } else {
    ImGui::TextUnformatted(now.data());
  }
  std::array<char, kLineSize> price = {};
  if (refusal == SurvivorsShopRefusal::kMostRank) {
    FaintText(RefusalText(*refusal));
  } else {
    std::snprintf(price.data(), price.size(), core::Tr("%s Zeny"),
                  SurvivorsZeny(row.next_price).data());
    if (refusal.has_value()) {
      ColoredText(kRefusalColor, price.data());
    } else {
      ImGui::TextUnformatted(price.data());
    }
  }
  ImGui::EndGroup();
  ImGui::SameLine();
  const bool refused = refusal.has_value();
  if (refused) ImGui::BeginDisabled();
  const bool clicked = SkinSmallButton(look.buy_label, art);
  if (refused) {
    ImGui::EndDisabled();
    SkinTooltip(RefusalText(*refusal));
  }
  ImGui::EndGroup();
  if (selected) {
    ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(),
                                        kSelectedEdge);
  }
  return clicked && !refused;
}

// Les touches de la liste, la fenêtre au premier plan : ↑ ↓ et Entrée.
void ReadKeys(SurvivorsShopState& state, const SurvivorsShopView& view, SurvivorsShopAsk& asked) {
  if (view.rows.empty() || !ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
    return;
  }
  const std::size_t last = view.rows.size() - 1;
  if (ImGui::IsKeyPressed(ImGuiKey_UpArrow) && state.selected_row > 0) --state.selected_row;
  if (ImGui::IsKeyPressed(ImGuiKey_DownArrow) && state.selected_row < last) ++state.selected_row;
  if (ImGui::IsKeyPressed(ImGuiKey_Enter, false) ||
      ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false)) {
    if (!RefusalOf(view.rows[state.selected_row], view.zeny).has_value()) {
      asked.buy_row = state.selected_row;
    }
  }
}

// Le pied : « Tout rembourser » puis sa confirmation sur place, et « Fermer ».
void DrawFooter(SurvivorsShopState& state, const SurvivorsShopView& view, SurvivorsShopAsk& asked,
                const SkinArt& art) {
  if (view.refundable_zeny == 0) state.confirming_refund = false;
  if (state.confirming_refund) {
    std::array<char, kLineSize> line = {};
    std::snprintf(line.data(), line.size(), core::Tr("Rendre %s Zeny ?"),
                  SurvivorsZeny(view.refundable_zeny).data());
    ImGui::TextUnformatted(line.data());
    if (SkinButton(core::Tr("Oui"), art)) {
      asked.refund_all = true;
      state.confirming_refund = false;
    }
    ImGui::SameLine();
    if (SkinButton(core::Tr("Non"), art)) state.confirming_refund = false;
    return;
  }
  const bool nothing = view.refundable_zeny == 0;
  if (nothing) ImGui::BeginDisabled();
  if (SkinButton(core::Tr("Tout rembourser"), art) && !nothing) state.confirming_refund = true;
  if (nothing) ImGui::EndDisabled();
  ImGui::SameLine();
  if (SkinButton(core::Tr("Fermer"), art)) state.open = false;
}

}  // namespace

std::optional<SurvivorsShopRefusal> RefusalOf(const SurvivorsShopRow& row,
                                              std::uint64_t zeny) noexcept {
  if (row.rank >= row.most_rank) return SurvivorsShopRefusal::kMostRank;
  if (row.next_price > zeny) return SurvivorsShopRefusal::kNotEnoughZeny;
  return std::nullopt;
}

SurvivorsShopAsk DrawSurvivorsShop(SurvivorsShopState& state, const SurvivorsShopView& view,
                                   const SurvivorsShopLook& look, const SkinArt& art) {
  SurvivorsShopAsk asked;
  if (!state.open) return asked;
  if (!view.rows.empty()) state.selected_row = std::min(state.selected_row, view.rows.size() - 1);
  ImGui::SetNextWindowSize(ImVec2(look.first_width, look.first_height), ImGuiCond_FirstUseEver);
  ConstrainNextWindowToScreen(look.window_name);
  if (!BeginSkinnedWindow(look.window_name, &state.open, art)) {
    EndSkinnedWindow();
    return asked;
  }
  std::array<char, kLineSize> balance = {};
  std::snprintf(balance.data(), balance.size(), core::Tr("Solde : %s Zeny"),
                SurvivorsZeny(view.zeny).data());
  ImGui::TextUnformatted(balance.data());
  ImGui::Separator();
  const float footer = SkinButtonHeight(art) + ImGui::GetFrameHeightWithSpacing();
  ImGui::BeginChild("##rows", ImVec2(0.0F, -footer));
  for (std::size_t index = 0; index < view.rows.size(); ++index) {
    ImGui::PushID(static_cast<int>(index));
    if (DrawRow(view.rows[index], index == state.selected_row, view.zeny, look, art)) {
      asked.buy_row = index;
    }
    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
      state.selected_row = index;
    }
    ImGui::Separator();
    ImGui::PopID();
  }
  ImGui::EndChild();
  ReadKeys(state, view, asked);
  DrawFooter(state, view, asked, art);
  EndSkinnedWindow();
  // Fermée par sa croix ou par « Fermer » : la confirmation ne lui survit pas.
  if (!state.open) state.confirming_refund = false;
  return asked;
}

}  // namespace ro::ui
