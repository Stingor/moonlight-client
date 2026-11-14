// LE MAJ + CLIC DANS L'ARBRE DES SUCCÈS : une racine ou une sous-ligne rend sa
// place, dont `app` pose le lien dans le chat, et la choisit au passage.

#include <array>
#include <optional>
#include <string_view>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/achievement_window.h"
#include "ro/ui/skin_art.h"
#include "ro/ui/ui_context.h"

namespace {

using ro::ui::AchievementAction;
using ro::ui::AchievementTabHit;

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
constexpr int kSettleFrames = 4;
constexpr const char* kWindowName = "###achievements";
// L'onglet Aventure : six sous-onglets.
constexpr int kAdventure = ro::ui::AchievementTabOfMajor(2);
// Le balayage descend l'arbre à cette distance du bord gauche de la fenêtre :
// dans l'arbre, à droite de l'ascenseur des sous-lignes.
constexpr float kScanX = 60.0F;
constexpr float kScanStepY = 5.0F;

const std::array<std::string_view, 6> kAdventureWords = {
    "Rune Midgarts", "Schwartzvald", "Arunafeltz", "Nouveau Monde", "Localisation", "Donjon"};

// La fenêtre seule, ouverte sur l'onglet Aventure.
struct TreeProbe {
  ro::ui::UiContext ui;
  ro::ui::SkinArt skin;
  ro::ui::AchievementWindowState state;
  ro::ui::AchievementView view;

  TreeProbe() {
    state.open = true;
    view.words.window_title = "Succès";
    view.words.subtabs[static_cast<std::size_t>(kAdventure)] = kAdventureWords;
    view.tab = kAdventure;
  }

  AchievementAction Frame() {
    ui.BeginFrame(kViewport, kFrameSeconds);
    AchievementAction asked = ro::ui::DrawAchievementWindow(state, view, skin);
    ui.EndFrame();
    return asked;
  }

  void Settle() {
    for (int frame = 0; frame < kSettleFrames; ++frame) {
      static_cast<void>(Frame());
    }
  }

  // Un clic en ce point, Maj tenue ou non, l'arbre ramené sur Aventure
  // entière. Rend la réponse de l'image de l'appui.
  AchievementAction ClickAt(float x, float y, bool shift) {
    state.tab = kAdventure;
    state.subtab = ro::ui::kAchievementWholeTab;
    state.subtab_scroll = 0;
    ImGuiIO& io = ImGui::GetIO();
    io.AddMousePosEvent(x, y);
    static_cast<void>(Frame());
    io.AddKeyEvent(ImGuiMod_Shift, shift);
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
    AchievementAction pressed = Frame();
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
    io.AddKeyEvent(ImGuiMod_Shift, false);
    static_cast<void>(Frame());
    return pressed;
  }
};

}  // namespace

TEST_CASE("Succès : Maj + clic sur une sous-ligne demande son lien et la choisit",
          "[ui][succes][lien]") {
  TreeProbe probe;
  probe.Settle();
  const ImGuiWindow* window = ImGui::FindWindowByName(kWindowName);
  REQUIRE(window != nullptr);
  const float left = window->Pos.x;
  const float top = window->Pos.y;
  const float bottom = window->Pos.y + window->Size.y;

  std::optional<AchievementTabHit> first_subtab;
  std::optional<AchievementTabHit> adventure_root;
  for (float y = top; y < bottom; y += kScanStepY) {
    const AchievementAction asked = probe.ClickAt(left + kScanX, y, true);
    if (!asked.link_tab.has_value()) {
      continue;
    }
    if (asked.link_tab->subtab != ro::ui::kAchievementWholeTab && !first_subtab.has_value()) {
      first_subtab = asked.link_tab;
      // ⚠ LE MAJ + CLIC CHOISIT AU PASSAGE ce qu'il désigne.
      CHECK(probe.state.tab == kAdventure);
      CHECK(probe.state.subtab == asked.link_tab->subtab);
      CHECK(asked.tab_changed);
    }
    if (asked.link_tab->tab == kAdventure &&
        asked.link_tab->subtab == ro::ui::kAchievementWholeTab) {
      adventure_root = asked.link_tab;
    }
  }
  REQUIRE(first_subtab.has_value());
  CHECK(first_subtab->tab == kAdventure);
  // En descendant, la première sous-ligne touchée est la première montrée.
  CHECK(first_subtab->subtab == 0);
  // La racine garde son lien d'onglet entier.
  CHECK(adventure_root.has_value());
}

TEST_CASE("Succès : un clic sans Maj dans l'arbre ne demande aucun lien", "[ui][succes][lien]") {
  TreeProbe probe;
  probe.Settle();
  const ImGuiWindow* window = ImGui::FindWindowByName(kWindowName);
  REQUIRE(window != nullptr);
  bool chosen = false;
  for (float y = window->Pos.y; y < window->Pos.y + window->Size.y; y += kScanStepY) {
    const AchievementAction asked = probe.ClickAt(window->Pos.x + kScanX, y, false);
    CHECK_FALSE(asked.link_tab.has_value());
    chosen = chosen || asked.tab_changed;
  }
  // Le clic, lui, a bien touché l'arbre.
  CHECK(chosen);
}
