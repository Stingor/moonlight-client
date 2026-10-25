#include "ro/ui/survivors_offer_window.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>

#include "ro/core/i18n.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"
#include "ro/ui/window_name.h"

#include "survivors_text.h"
#include "ui_text.h"

namespace ro::ui {
namespace {

inline constexpr const char* kOfferTitle = core::TrText("Niveau supérieur !");
inline constexpr const char* kOfferId = "survivors_offer";

constexpr float kCardWidth = 190.0F;
constexpr float kCardHeight = 250.0F;
constexpr float kIconSide = 64.0F;
constexpr ImU32 kNewColor = IM_COL32(255, 215, 90, 255);
constexpr ImU32 kEvolveColor = IM_COL32(200, 140, 255, 255);
constexpr std::size_t kLineSize = 64;
constexpr ImGuiWindowFlags kModalFlags =
    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
    ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings;

constexpr std::array<ImGuiKey, kMostOfferCards> kRowKeys = {ImGuiKey_1, ImGuiKey_2, ImGuiKey_3,
                                                            ImGuiKey_4};
constexpr std::array<ImGuiKey, kMostOfferCards> kPadKeys = {ImGuiKey_Keypad1, ImGuiKey_Keypad2,
                                                            ImGuiKey_Keypad3, ImGuiKey_Keypad4};

// La carte dont la touche est pressée à cette image, ou rien.
[[nodiscard]] std::optional<std::uint8_t> PressedCard(const SurvivorsOfferView& view) {
  const int shown = std::min(static_cast<int>(view.cards.size()), kMostOfferCards);
  if (view.pressed_card_key >= 1 && view.pressed_card_key <= shown) {
    return static_cast<std::uint8_t>(view.pressed_card_key - 1);
  }
  for (int index = 0; index < shown; ++index) {
    const auto at = static_cast<std::size_t>(index);
    if (ImGui::IsKeyPressed(kRowKeys[at], false) || ImGui::IsKeyPressed(kPadKeys[at], false)) {
      return static_cast<std::uint8_t>(index);
    }
  }
  return std::nullopt;
}

// Une carte ; vrai quand elle est cliquée.
[[nodiscard]] bool DrawCard(const SurvivorsOfferCard& card, std::size_t index, const SkinArt& art) {
  std::array<char, kLineSize> line = {};
  ImGui::PushID(static_cast<int>(index));
  ImGui::BeginChild("##card", ImVec2(kCardWidth, kCardHeight), ImGuiChildFlags_Borders);
  if (card.icon != 0) {
    const float wide = card.icon_size.x > 0.0F ? card.icon_size.x : kIconSide;
    const float high = card.icon_size.y > 0.0F ? card.icon_size.y : kIconSide;
    const float scale = std::min(kIconSide / wide, kIconSide / high);
    ImGui::SetCursorPosX((kCardWidth - wide * scale) * 0.5F);
    ImGui::Image(card.icon, ImVec2(wide * scale, high * scale));
  }
  ImGui::TextWrapped("%s", card.name);
  if (card.owned_level == 0) {
    ColoredText(kNewColor, core::Tr("NEW!"));
  } else {
    std::snprintf(line.data(), line.size(), core::Tr("Lv %d » %d"), card.owned_level,
                  card.next_level);
    ImGui::TextUnformatted(line.data());
  }
  if (card.evolves) {
    ImGui::SameLine();
    ColoredText(kEvolveColor, core::Tr("Évolution"));
  }
  ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
  ImGui::TextWrapped("%s", SurvivorsText(card.description));
  ImGui::PopStyleColor();
  std::snprintf(line.data(), line.size(), core::Tr("%d — Prendre"), static_cast<int>(index + 1));
  ImGui::SetCursorPosY(kCardHeight - SkinButtonHeight(art) - ImGui::GetStyle().WindowPadding.y);
  const bool taken = SkinButton(line.data(), art, ImVec2(-1.0F, 0.0F));
  ImGui::EndChild();
  ImGui::PopID();
  return taken;
}

// Les boutons des charges, ceux dont la charge n'est pas nulle.
[[nodiscard]] std::optional<SurvivorsOfferAsk> DrawCharges(SurvivorsOfferState& state,
                                                           const SurvivorsOfferView& view,
                                                           const SkinArt& art) {
  std::optional<SurvivorsOfferAsk> asked;
  std::array<char, kLineSize> line = {};
  // Dans la garde, les boutons sont grisés et les touches se taisent aussi.
  const bool live = view.accepts_input;
  bool first = true;
  const auto next = [&first]() {
    if (!first) ImGui::SameLine();
    first = false;
  };
  if (view.rerolls > 0) {
    next();
    std::snprintf(line.data(), line.size(), core::Tr("R — Relancer (%d)"), view.rerolls);
    if (SkinButton(line.data(), art) || (live && ImGui::IsKeyPressed(ImGuiKey_R, false))) {
      asked = SurvivorsOfferAsk{.verb = SurvivorsOfferVerb::kReroll};
    }
  }
  if (view.skips > 0) {
    next();
    std::snprintf(line.data(), line.size(), core::Tr("P — Passer (%d)"), view.skips);
    if (SkinButton(line.data(), art) || (live && ImGui::IsKeyPressed(ImGuiKey_P, false))) {
      asked = SurvivorsOfferAsk{.verb = SurvivorsOfferVerb::kSkip};
    }
  }
  if (view.banishes > 0) {
    next();
    std::snprintf(line.data(), line.size(), core::Tr("B — Bannir (%d)"), view.banishes);
    if (SkinToggleButton(line.data(), state.banishing, art) ||
        (live && ImGui::IsKeyPressed(ImGuiKey_B, false))) {
      state.banishing = !state.banishing;
    }
  } else {
    state.banishing = false;
  }
  if (state.banishing) {
    FaintText(core::Tr("Choisissez la carte à bannir."));
  }
  return asked;
}

}  // namespace

std::optional<SurvivorsOfferAsk> DrawSurvivorsOfferWindow(SurvivorsOfferState& state,
                                                          const SurvivorsOfferView& view,
                                                          const SkinArt& art) {
  std::optional<SurvivorsOfferAsk> asked;
  static WindowName name(kOfferTitle, kOfferId);
  const ImGuiViewport* screen = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(screen->GetCenter(), ImGuiCond_Always, ImVec2(0.5F, 0.5F));
  ImGui::SetNextWindowFocus();
  if (!BeginSkinnedWindow(name.Get(), nullptr, art, SkinnedTitle::kBar, kModalFlags)) {
    EndSkinnedWindow();
    return asked;
  }
  if (!view.accepts_input) {
    ImGui::BeginDisabled();
  }
  std::optional<std::uint8_t> clicked;
  const std::size_t shown = std::min(view.cards.size(), static_cast<std::size_t>(kMostOfferCards));
  for (std::size_t index = 0; index < shown; ++index) {
    if (index > 0) ImGui::SameLine();
    if (DrawCard(view.cards[index], index, art)) {
      clicked = static_cast<std::uint8_t>(index);
    }
  }
  std::optional<SurvivorsOfferAsk> charge = DrawCharges(state, view, art);
  if (!view.accepts_input) {
    ImGui::EndDisabled();
  }
  EndSkinnedWindow();
  if (!view.accepts_input) {
    return asked;
  }
  if (!clicked.has_value()) {
    clicked = PressedCard(view);
  }
  if (clicked.has_value()) {
    const SurvivorsOfferVerb verb =
        state.banishing ? SurvivorsOfferVerb::kBanish : SurvivorsOfferVerb::kPick;
    state.banishing = false;
    return SurvivorsOfferAsk{.verb = verb, .card_index = *clicked};
  }
  return charge;
}

}  // namespace ro::ui
