// LE MODE « PERSONNALISER » DE LA TABLÉE, image par image : ce que les gestes
// font de la mise en page, et ce qu'ils ne font PAS — choisir, créer, entrer.

#include <algorithm>
#include <string>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/character_select_window.h"
#include "ro/ui/ui_context.h"

using ro::ui::CharacterSelectAction;
using ro::ui::CharacterSelectVerb;
using ro::ui::kBanquetSeats;

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
// Plus long que la pose qu'exige le portail de la molette.
constexpr float kSettleSeconds = 0.1F;
constexpr int kSettleFrames = 5;
// Un siège qu'aucun emplacement ne remplit : il n'existe qu'en personnalisation.
constexpr int kBareSeat = 10;
// Un pas de glissement, largement au-delà du seuil d'ImGui.
constexpr float kDragStepPixels = 20.0F;
constexpr int kDragSteps = 3;

[[nodiscard]] ImVec2 SeatCentre(int seat) {
  const ro::ui::SeatFrame frame = ro::ui::SeatFrameAt(kBanquetSeats[static_cast<std::size_t>(seat)],
                                                      kViewport.width_px, kViewport.height_px);
  return ImVec2(frame.centre_x, (frame.grab_top + frame.grab_bottom) * 0.5F);
}

// Une tablée de trois emplacements — un convive, deux places libres — sur un
// compte qui peut en remplir neuf.
struct SceneProbe {
  ro::ui::UiContext ui;
  ro::ui::SkinArt art;
  ro::ui::CharacterSelectState state;
  ro::ui::CharacterSelectView view;

  SceneProbe() {
    view.slots.resize(3);
    view.slots[0].occupied = true;
    view.slots[0].name = "Aldric";
    view.slots[0].character_id = 150000;
    view.creatable_slots = 9;
  }

  CharacterSelectAction Frame(float seconds = kFrameSeconds) {
    ui.BeginFrame(kViewport, seconds);
    CharacterSelectAction action = ro::ui::DrawCharacterSelect(state, view, art);
    ui.EndFrame();
    return action;
  }

  // ⚠ LA SOURIS SE POSE AVANT L'IMAGE : ImGui décide du survol en tête
  // d'image, depuis sa file d'évènements.
  void Hover(ImVec2 where) {
    ImGui::GetIO().AddMousePosEvent(where.x, where.y);
    for (int frame = 0; frame < kSettleFrames; ++frame) {
      static_cast<void>(Frame(kSettleSeconds));
    }
  }

  // Un clic complet, et ce que l'image de la PRESSION a rendu.
  CharacterSelectAction Click(ImGuiMouseButton button = ImGuiMouseButton_Left) {
    ImGui::GetIO().AddMouseButtonEvent(button, true);
    const CharacterSelectAction pressed = Frame();
    ImGui::GetIO().AddMouseButtonEvent(button, false);
    const CharacterSelectAction released = Frame();
    return pressed.verb != CharacterSelectVerb::kNothing ? pressed : released;
  }

  CharacterSelectAction Key(ImGuiKey key) {
    ImGui::GetIO().AddKeyEvent(key, true);
    const CharacterSelectAction pressed = Frame();
    ImGui::GetIO().AddKeyEvent(key, false);
    static_cast<void>(Frame());
    return pressed;
  }
};

[[nodiscard]] bool HasAnchor(const ro::ui::SeatLayout& layout, std::string_view name) {
  return std::ranges::any_of(
      layout.anchors, [name](const ro::ui::SeatAnchor& anchor) { return anchor.name == name; });
}

}  // namespace

TEST_CASE("Personnaliser : F10 entre et sort du mode", "[ui][seats][customize]") {
  SceneProbe probe;
  static_cast<void>(probe.Frame());
  CHECK(probe.Key(ImGuiKey_F10).verb == CharacterSelectVerb::kBeginCustomizing);
  CHECK(probe.state.customizing);
  // Le panneau est là, fenêtre à part entière.
  CHECK(ImGui::FindWindowByName("Personnaliser l'écran###table_customize") != nullptr);
  CHECK(probe.Key(ImGuiKey_F10).verb == CharacterSelectVerb::kEndCustomizing);
  CHECK_FALSE(probe.state.customizing);
}

TEST_CASE("Personnaliser : F10 se tait sous une fenêtre de création", "[ui][seats][customize]") {
  // ⚠ LE JOUEUR SE TROUVERAIT DEVANT DEUX INTERACTIONS CONCURRENTES.
  SceneProbe probe;
  probe.view.dialog_open = true;
  static_cast<void>(probe.Frame());
  CHECK(probe.Key(ImGuiKey_F10).verb == CharacterSelectVerb::kNothing);
  CHECK_FALSE(probe.state.customizing);
}

TEST_CASE("Personnaliser : les quatre blocs s'enregistrent à leur place d'usine",
          "[ui][seats][customize]") {
  SceneProbe probe;
  static_cast<void>(probe.Frame());
  CHECK(HasAnchor(probe.state.layout, ro::ui::kTitleAnchor));
  CHECK(HasAnchor(probe.state.layout, ro::ui::kActionBarAnchor));
  CHECK(HasAnchor(probe.state.layout, ro::ui::kExitBarAnchor));
  // ⚠ LA PAGINATION N'EXISTE QU'AVEC PLUS D'UNE PAGE, et son ancre avec elle.
  CHECK_FALSE(HasAnchor(probe.state.layout, ro::ui::kPageBarAnchor));
  // S'enregistrer n'est pas modifier : rien à graver.
  CHECK_FALSE(probe.state.layout_dirty);
}

TEST_CASE("Personnaliser : glisser un siège le déplace, même sans convive",
          "[ui][seats][customize]") {
  SceneProbe probe;
  probe.state.customizing = true;
  const ImVec2 start = SeatCentre(kBareSeat);
  probe.Hover(start);
  ImGui::GetIO().AddMouseButtonEvent(ImGuiMouseButton_Left, true);
  static_cast<void>(probe.Frame());
  for (int step = 1; step <= kDragSteps; ++step) {
    ImGui::GetIO().AddMousePosEvent(start.x + kDragStepPixels * static_cast<float>(step), start.y);
    static_cast<void>(probe.Frame());
  }
  ImGui::GetIO().AddMouseButtonEvent(ImGuiMouseButton_Left, false);
  static_cast<void>(probe.Frame());

  const float moved =
      (probe.state.layout.seats[kBareSeat].x - kBanquetSeats[kBareSeat].x) * kViewport.width_px;
  CHECK(moved > kDragStepPixels);
  CHECK(probe.state.layout.seats[kBareSeat].y == kBanquetSeats[kBareSeat].y);
  CHECK(probe.state.layout_dirty);
}

TEST_CASE("Personnaliser : la molette tourne, Ctrl+molette grandit", "[ui][seats][customize]") {
  SceneProbe probe;
  probe.state.customizing = true;
  probe.Hover(SeatCentre(kBareSeat));
  ImGui::GetIO().AddMouseWheelEvent(0.0F, 1.0F);
  static_cast<void>(probe.Frame());
  CHECK(probe.state.layout.seats[kBareSeat].direction == 1);
  // Huit orientations en boucle : un cran en arrière depuis zéro rend sept.
  ImGui::GetIO().AddMouseWheelEvent(0.0F, -1.0F);
  static_cast<void>(probe.Frame());
  ImGui::GetIO().AddMouseWheelEvent(0.0F, -1.0F);
  static_cast<void>(probe.Frame());
  CHECK(probe.state.layout.seats[kBareSeat].direction == ro::ui::kSeatDirections - 1);

  ImGui::GetIO().AddKeyEvent(ImGuiMod_Ctrl, true);
  static_cast<void>(probe.Frame());
  ImGui::GetIO().AddMouseWheelEvent(0.0F, 1.0F);
  static_cast<void>(probe.Frame());
  ImGui::GetIO().AddKeyEvent(ImGuiMod_Ctrl, false);
  static_cast<void>(probe.Frame());
  CHECK(probe.state.layout.seats[kBareSeat].height > kBanquetSeats[kBareSeat].height);
  CHECK(probe.state.layout.seats[kBareSeat].direction == ro::ui::kSeatDirections - 1);
  CHECK(probe.state.layout_dirty);
}

TEST_CASE("Personnaliser : le clic droit ouvre la pose du siège", "[ui][seats][customize]") {
  SceneProbe probe;
  probe.state.customizing = true;
  probe.Hover(SeatCentre(kBareSeat));
  static_cast<void>(probe.Click(ImGuiMouseButton_Right));
  CHECK(probe.state.pose_seat == kBareSeat);
  CHECK_FALSE(probe.state.pose_menu_asked);
}

TEST_CASE("Personnaliser : le clic place, il ne crée pas", "[ui][seats][customize]") {
  SceneProbe probe;
  probe.state.customizing = true;
  probe.Hover(SeatCentre(1));
  CHECK(probe.Click().verb != CharacterSelectVerb::kCreate);
}

TEST_CASE("Personnaliser : Entrée n'entre pas pendant la composition", "[ui][seats][customize]") {
  SceneProbe probe;
  probe.state.selected = 0;
  probe.state.customizing = true;
  static_cast<void>(probe.Frame());
  CHECK(probe.Key(ImGuiKey_Enter).verb != CharacterSelectVerb::kEnter);
  // Hors du mode, elle entre.
  probe.state.customizing = false;
  CHECK(probe.Key(ImGuiKey_Enter).verb == CharacterSelectVerb::kEnter);
}

TEST_CASE("Tablée : une place masquée reste cliquable", "[ui][seats][customize]") {
  // 🔴 SANS CELA, MASQUER LES PLACES LIBRES SUPPRIMERAIT LA CRÉATION.
  SceneProbe probe;
  probe.state.layout.hide_free_seats = true;
  probe.Hover(SeatCentre(1));
  const CharacterSelectAction action = probe.Click();
  CHECK(action.verb == CharacterSelectVerb::kCreate);
  CHECK(action.slot == 1);
}

TEST_CASE("Tablée : hors du mode, un siège sans emplacement n'existe pas",
          "[ui][seats][customize]") {
  SceneProbe probe;
  probe.Hover(SeatCentre(kBareSeat));
  static_cast<void>(probe.Click());
  CHECK(probe.state.layout.seats[kBareSeat].x == kBanquetSeats[kBareSeat].x);
  CHECK_FALSE(probe.state.layout_dirty);
}

TEST_CASE("Tablée : les sièges suivent la mise en page vivante", "[ui][seats][customize]") {
  // Un convive déplacé se clique à sa NOUVELLE place.
  SceneProbe probe;
  probe.state.layout.seats[0] = kBanquetSeats[20];
  probe.Hover(SeatCentre(20));
  static_cast<void>(probe.Click());
  CHECK(probe.state.selected == 0);
}
