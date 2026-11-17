#include "ro/ui/survivors_offer_window.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>

#include "ro/core/i18n.h"
#include "ro/ui/skin_look.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"
#include "ro/ui/window_name.h"

#include "survivors_codex_link.h"
#include "survivors_text.h"
#include "ui_text.h"

namespace ro::ui {
namespace {

inline constexpr const char* kOfferTitle = core::TrText("Niveau supérieur !");
inline constexpr const char* kOfferId = "survivors_offer";

constexpr float kCardWidth = 190.0F;
constexpr float kCardHeight = 250.0F;
constexpr float kIconSide = 64.0F;
constexpr float kRecipeIconSide = 16.0F;
// La carte est posée sur le corps clair : « NEW! » s'y écrit en or sombre.
constexpr ImU32 kNewColor = kSkinGoldTextColor;
constexpr ImU32 kEvolveColor = kSkinEvolveTextColor;
constexpr ImU32 kObjectiveColor = kSkinGoldTextColor;
constexpr std::size_t kLineSize = 96;
constexpr float kVoteBarHeight = 6.0F;
constexpr ImGuiWindowFlags kModalFlags =
    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
    ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings;

constexpr std::array<ImGuiKey, kMostOfferCards> kRowKeys = {ImGuiKey_1, ImGuiKey_2, ImGuiKey_3,
                                                            ImGuiKey_4};
constexpr std::array<ImGuiKey, kMostOfferCards> kPadKeys = {ImGuiKey_Keypad1, ImGuiKey_Keypad2,
                                                            ImGuiKey_Keypad3, ImGuiKey_Keypad4};

// La carte dont la touche est pressée à cette image, ou rien.
[[nodiscard]] std::optional<std::uint8_t> PressedCard(const SurvivorsOfferView& view) {
  if (view.typing) return std::nullopt;
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

// LES RECETTES DE L'OFFRE : ce qu'elle complète, puis ce qu'elle fait évoluer
// et avec quoi. L'autre ingrédient, puis « » » et l'évolué ; une entrée que le
// Codex n'a pas vue n'est qu'une silhouette et « ??? ».
void DrawRecipes(std::span<const SurvivorsOfferRecipe> recipes) {
  if (recipes.empty()) return;
  ImGui::PushTextWrapPos(0.0F);
  for (const SurvivorsOfferRecipe& recipe : recipes) {
    if (recipe.completes) {
      ColoredText(kObjectiveColor, recipe.union_of_two ? core::Tr("Complète un combo :")
                                                       : core::Tr("Complète une évolution :"));
    } else if (recipe.card_offer) {
      ColoredText(kEvolveColor, core::Tr("Fait évoluer :"));
    } else {
      ColoredText(kEvolveColor,
                  recipe.union_of_two ? core::Tr("Combo avec :") : core::Tr("Évolue avec :"));
    }
    DrawCodexLink(recipe.partner, kRecipeIconSide);
    ImGui::TextUnformatted("»");
    ImGui::SameLine();
    DrawCodexLink(recipe.result, kRecipeIconSide);
  }
  ImGui::PopTextWrapPos();
}

// Une carte ; vrai quand elle est cliquée.
// Le bouton dit ce que le clic fera : prendre la carte, ou la bannir quand le
// bannissement est armé.
[[nodiscard]] bool DrawCard(const SurvivorsOfferCard& card, std::size_t index, bool banishing,
                            const SkinArt& art) {
  std::array<char, kLineSize> line = {};
  ImGui::PushID(static_cast<int>(index));
  ImGui::BeginChild("##card", ImVec2(kCardWidth, kCardHeight), ImGuiChildFlags_Borders,
                    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
  // Le texte défile dans son cadre, au-dessus du bouton : il ne passe jamais dessous.
  const ImGuiStyle& style = ImGui::GetStyle();
  const float body_height =
      kCardHeight - 2.0F * style.WindowPadding.y - SkinButtonHeight(art) - style.ItemSpacing.y;
  ImGui::BeginChild("##body", ImVec2(0.0F, body_height));
  if (card.icon != 0) {
    const float wide = card.icon_size.x > 0.0F ? card.icon_size.x : kIconSide;
    const float high = card.icon_size.y > 0.0F ? card.icon_size.y : kIconSide;
    const float scale = std::min(kIconSide / wide, kIconSide / high);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                         (ImGui::GetContentRegionAvail().x - wide * scale) * 0.5F);
    ImGui::Image(card.icon, ImVec2(wide * scale, high * scale));
  }
  ImGui::TextWrapped("%s", card.name);
  if (card.for_hero != nullptr) {
    std::snprintf(line.data(), line.size(), core::Tr("Pour %s"), card.for_hero);
    ColoredText(kObjectiveColor, line.data());
  }
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
  if (card.objective) {
    ImGui::SameLine();
    ColoredText(kObjectiveColor, core::Tr("Objectif"));
  }
  if (card.for_hero != nullptr) {
    std::snprintf(line.data(), line.size(),
                  card.my_vote ? core::Tr("%d voix, dont la vôtre") : core::Tr("%d voix"),
                  card.votes);
    FaintText(line.data());
  }
  DrawRecipes(card.recipes);
  ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
  ImGui::TextWrapped("%s", SurvivorsText(card.description));
  ImGui::PopStyleColor();
  // Une description trop longue fait défiler le texte : l'ascenseur se voit.
  PaintWindowScrollbar(art);
  ImGui::EndChild();
  std::snprintf(line.data(), line.size(),
                banishing ? core::Tr("%d — Bannir") : core::Tr("%d — Prendre"),
                static_cast<int>(index + 1));
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
  // Dans la garde, les boutons sont grisés et les touches se taisent aussi ;
  // pendant une saisie, seules les touches se taisent.
  const bool live = view.accepts_input && !view.typing;
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
  if (view.charges_withheld) {
    FaintText(core::Tr("Charges Kafra : en partie libre seulement."));
  }
  return asked;
}

// EN COOP : la minuterie du vote, et les héros qu'on peut relever au prix de cette
// montée de niveau.
[[nodiscard]] std::optional<SurvivorsOfferAsk> DrawRaises(const SurvivorsOfferView& view,
                                                          const SkinArt& art) {
  std::optional<SurvivorsOfferAsk> asked;
  if (view.voters == 0) return asked;
  ImGui::ProgressBar(std::clamp(view.vote_time_left, 0.0F, 1.0F), ImVec2(-1.0F, kVoteBarHeight),
                     "");
  std::array<char, kLineSize> line = {};
  for (const SurvivorsOfferRaise& raise : view.raises) {
    ImGui::PushID(raise.hero_index);
    std::snprintf(line.data(), line.size(),
                  raise.my_vote ? core::Tr("Relever %s (%d voix, dont la vôtre)")
                                : core::Tr("Relever %s (%d voix)"),
                  raise.name, raise.votes);
    if (SkinButton(line.data(), art)) {
      asked = SurvivorsOfferAsk{.verb = SurvivorsOfferVerb::kRaise, .card_index = raise.hero_index};
    }
    ImGui::PopID();
  }
  if (!view.raises.empty()) {
    std::snprintf(line.data(), line.size(),
                  core::Tr("Une feuille part, et cette montée de niveau avec elle. Feuilles : %d"),
                  view.leaves);
    FaintText(line.data());
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
  if (!view.typing) ImGui::SetNextWindowFocus();
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
    if (DrawCard(view.cards[index], index, state.banishing, art)) {
      clicked = static_cast<std::uint8_t>(index);
    }
  }
  std::optional<SurvivorsOfferAsk> charge = DrawCharges(state, view, art);
  if (const std::optional<SurvivorsOfferAsk> raise = DrawRaises(view, art)) charge = raise;
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
