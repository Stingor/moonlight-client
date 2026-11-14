// LE FILET DE L'AFFICHAGE, vu de la fenêtre qui doit le montrer.
//
// 🔴 CE QU'IL MESURE : que la boîte PARAÎT. Elle est la seule issue d'un
// réglage qui se défait tout seul au bout de quinze secondes ; personne ne peut
// répondre à une question qui n'est pas dessinée, et le silence vaut « non ».

#include <span>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/options_window.h"
#include "ro/ui/ui_context.h"

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
// La clé ImGui de la boîte, telle que `options_window.cpp` la pose.
constexpr const char* kDisplayBoxId = "##garder_affichage";

const ro::ui::SkinArt kNoArt;

// Une image de jeu, panneau ouvert, et ce que la fenêtre en rapporte.
ro::ui::OptionsWindowAction OneFrame(ro::ui::UiContext& ui, ro::ui::OptionsWindowState& state) {
  ui.BeginFrame(kViewport, kFrameSeconds);
  const ro::ui::OptionsWindowAction asked =
      ro::ui::DrawOptionsWindow(state, std::span<const ro::ui::OptionSectionView>(), kNoArt);
  ui.EndFrame();
  return asked;
}

// ⚠ `WasActive` ET NON `Active` : le second ne vaut que PENDANT l'image.
[[nodiscard]] bool BoxShown() {
  const ImGuiWindow* box = ImGui::FindWindowByName(kDisplayBoxId);
  return box != nullptr && box->WasActive;
}

}  // namespace

TEST_CASE("Filet d'affichage : la boîte paraît dès qu'un retour est armé", "[ui][options]") {
  ro::ui::UiContext ui;
  ro::ui::OptionsWindowState state;
  state.open = true;

  // Rien d'armé : rien à montrer.
  static_cast<void>(OneFrame(ui, state));
  CHECK_FALSE(BoxShown());

  // Le filet s'arme. ⚠ DEUX IMAGES : un surgissant d'ImGui s'ouvre à la
  // première et se dessine à la seconde.
  state.revert_seconds = 15;
  static_cast<void>(OneFrame(ui, state));
  static_cast<void>(OneFrame(ui, state));
  CHECK(BoxShown());
}
