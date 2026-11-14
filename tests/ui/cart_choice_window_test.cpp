#include "ro/ui/cart_choice_window.h"

#include <array>
#include <cstdint>
#include <string_view>

#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/ui_context.h"

using ro::ui::CartChoiceButtonAt;
using ro::ui::CartChoiceButtonOrigin;
using ro::ui::CartChoiceWindowHeight;

TEST_CASE("Liste des charrettes : cinq boutons par rangée, décalés type par type", "[ui][cart]") {
  // Le type un monte en haut de sa case, le type trois garde la place commune.
  CHECK(CartChoiceButtonOrigin(0, 1).x == 0.0F);
  CHECK(CartChoiceButtonOrigin(0, 1).y == 15.0F);
  CHECK(CartChoiceButtonOrigin(2, 3).x == 140.0F);
  CHECK(CartChoiceButtonOrigin(2, 3).y == 28.0F);
  CHECK(CartChoiceButtonOrigin(3, 4).x == 228.0F);
  CHECK(CartChoiceButtonOrigin(4, 5).x == 295.0F);
  CHECK(CartChoiceButtonOrigin(4, 5).y == 23.0F);
  // Le sixième commence la seconde rangée, quatre-vingt-deux pixels plus bas.
  CHECK(CartChoiceButtonOrigin(5, 6).x == 13.0F);
  CHECK(CartChoiceButtonOrigin(5, 6).y == 110.0F);
  CHECK(CartChoiceButtonOrigin(0, 12).y == 5.0F);
  CHECK(CartChoiceButtonOrigin(1, 13).x == 74.0F);
}

TEST_CASE("Liste des charrettes : la fenêtre grandit d'une rangée par cinq boutons", "[ui][cart]") {
  CHECK(CartChoiceWindowHeight(0) == 98.0F);
  CHECK(CartChoiceWindowHeight(3) == 98.0F);
  CHECK(CartChoiceWindowHeight(5) == 98.0F);
  CHECK(CartChoiceWindowHeight(6) == 196.0F);
  CHECK(CartChoiceWindowHeight(10) == 196.0F);
}

TEST_CASE("Liste des charrettes : là où deux boutons se recouvrent, le centre le plus proche",
          "[ui][cart]") {
  constexpr std::array<std::uint8_t, 3> kTypes = {1, 2, 3};
  // Le premier seul, à gauche.
  CHECK(CartChoiceButtonAt(kTypes, ImVec2(10.0F, 50.0F)) == 0);
  // Entre soixante-dix et cent, les deux premiers se recouvrent : celui dont
  // le centre est le plus proche, le premier à quatre-vingts, le second à
  // quatre-vingt-dix-huit.
  CHECK(CartChoiceButtonAt(kTypes, ImVec2(80.0F, 50.0F)) == 0);
  CHECK(CartChoiceButtonAt(kTypes, ImVec2(98.0F, 50.0F)) == 1);
  // Au-dessus de toute case : rien.
  CHECK_FALSE(CartChoiceButtonAt(kTypes, ImVec2(10.0F, 2.0F)).has_value());
  // Plus bas que le type un, le type trois commence plus bas lui aussi.
  CHECK(CartChoiceButtonAt(kTypes, ImVec2(150.0F, 20.0F)) == 1);
  CHECK(CartChoiceButtonAt(kTypes, ImVec2(150.0F, 120.0F)) == 2);
}

TEST_CASE("Liste des charrettes : un clic au milieu d'une charrette la choisit", "[ui][cart]") {
  // 🔴 LA ZONE CLIQUABLE DU CORPS DEVIENT ACTIVE AU CLIC, et la fenêtre ne se
  // disait plus survolée : un clic posé sur une image ne choisissait rien.
  constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
  constexpr std::array<std::uint8_t, 3> kTypes = {1, 2, 3};
  constexpr float kFrameSeconds = 1.0F / 60.0F;
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art;
  ro::ui::CartChoiceWindowState state;
  state.open = true;
  const ro::ui::CartChoiceView view{.title = "Liste du cart", .types = kTypes};
  const auto frame = [&] {
    ui.BeginFrame(kViewport, kFrameSeconds);
    const ro::ui::CartChoiceWindowAction action = ro::ui::DrawCartChoiceWindow(state, view, art);
    ui.EndFrame();
    return action;
  };
  static_cast<void>(frame());
  static_cast<void>(frame());
  const ImGuiWindow* window = ImGui::FindWindowByName("Liste du cart###cart_list");
  REQUIRE(window != nullptr);
  const float scale = ImGui::GetStyle().FontScaleMain;
  const ImVec2 origin = CartChoiceButtonOrigin(1, kTypes[1]);
  const float half = ro::ui::kCartChoiceButtonSide / 2.0F;
  ImGuiIO& io = ImGui::GetIO();
  io.AddMousePosEvent(window->Pos.x + (origin.x + half) * scale,
                      window->Pos.y + (origin.y + half) * scale);
  static_cast<void>(frame());
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
  static_cast<void>(frame());
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
  const ro::ui::CartChoiceWindowAction released = frame();
  REQUIRE(released.chosen.has_value());
  CHECK(*released.chosen == kTypes[1]);
}

TEST_CASE("Liste des charrettes : le titre est celui de la table de messages, dans sa langue",
          "[ui][cart][langue]") {
  // 🔴 CE N'EST PAS UN TEXTE À NOUS : le client le tire de sa table de messages,
  // que le joueur voit traduite. Un titre écrit en dur restait « Cart List »
  // dans toutes les langues.
  constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
  constexpr std::array<std::uint8_t, 1> kTypes = {1};
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art;
  ro::ui::CartChoiceWindowState state;
  state.open = true;
  const ro::ui::CartChoiceView view{.title = "Liste du cart", .types = kTypes};
  constexpr float kFrameSeconds = 1.0F / 60.0F;
  ui.BeginFrame(kViewport, kFrameSeconds);
  static_cast<void>(ro::ui::DrawCartChoiceWindow(state, view, art));
  ui.EndFrame();
  const ImGuiWindow* window = ImGui::FindWindowByName("Liste du cart###cart_list");
  REQUIRE(window != nullptr);
  CHECK(std::string_view(window->Name) == "Liste du cart###cart_list");
}
