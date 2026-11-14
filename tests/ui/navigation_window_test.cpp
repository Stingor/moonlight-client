// LA NAVIGATION : la saisie qui part à la frappe, le résultat unique qui se
// choisit seul, « Y aller » qui désigne SA ligne, des pastilles qui filtrent
// sans relancer la recherche, et le bandeau d'un itinéraire introuvable.

#include "ro/ui/navigation_window.h"

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/skin_pieces.h"
#include "ro/ui/ui_context.h"

using ro::ui::NavigationAnswer;
using ro::ui::NavigationEntryView;
using ro::ui::NavigationFilter;
using ro::ui::NavigationGroupView;
using ro::ui::NavigationKind;

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
constexpr float kPlainPiecePixels = 8.0F;
constexpr int kSettleFrames = 4;
constexpr float kScanStep = 4.0F;
constexpr int kGraphMaps = 1301;

[[nodiscard]] ro::ui::SkinArt PlainArt() {
  ro::ui::SkinArt art;
  for (std::size_t rank = 0; rank < ro::render::kSkinPieceCount; ++rank) {
    art.Set(static_cast<ro::render::SkinPiece>(rank),
            ro::ui::SkinImage{.id = 1, .width = kPlainPiecePixels, .height = kPlainPiecePixels});
  }
  return art;
}

struct NavigationProbe {
  ro::ui::UiContext ui;
  ro::ui::SkinArt art = PlainArt();
  ro::ui::NavigationWindowState state;
  std::array<NavigationEntryView, 2> maps = {NavigationEntryView{.kind = NavigationKind::kMap,
                                                                 .name = "Prontera",
                                                                 .map_name = "prontera",
                                                                 .map_label = "Prontera"},
                                             NavigationEntryView{.kind = NavigationKind::kMap,
                                                                 .name = "Prontera Field",
                                                                 .map_name = "prt_fild08",
                                                                 .map_label = "Prontera Field"}};
  std::array<NavigationEntryView, 1> poring = {NavigationEntryView{.kind = NavigationKind::kMonster,
                                                                   .name = "Poring",
                                                                   .map_name = "prt_fild08",
                                                                   .map_label = "Prontera Field",
                                                                   .level = 1,
                                                                   .amount = 60}};
  std::array<NavigationGroupView, 2> groups = {
      NavigationGroupView{.name = "Prontera", .kind = NavigationKind::kMap, .entries = maps},
      NavigationGroupView{.name = "Poring", .kind = NavigationKind::kMonster, .entries = poring}};
  ro::ui::NavigationView view{.groups = groups, .results_serial = 1, .graph_map_count = kGraphMaps};
  std::string logged;

  NavigationProbe() { state.open = true; }

  NavigationAnswer Frame() {
    ui.BeginFrame(kViewport, kFrameSeconds);
    ImGui::LogToBuffer();
    NavigationAnswer answer = ro::ui::DrawNavigationWindow(state, view, art);
    logged = ImGui::GetCurrentContext()->LogBuffer.c_str();
    ImGui::LogFinish();
    ui.EndFrame();
    return answer;
  }

  void Settle() {
    for (int frame = 0; frame < kSettleFrames; ++frame) {
      static_cast<void>(Frame());
    }
  }
};

[[nodiscard]] ImGuiWindow* WindowNamed(std::string_view part) {
  for (ImGuiWindow* window : ImGui::GetCurrentContext()->Windows) {
    if (std::string_view(window->Name).find(part) != std::string_view::npos) {
      return window;
    }
  }
  return nullptr;
}

// Balaie une fenêtre jusqu'à survoler l'item voulu.
[[nodiscard]] bool HoverItem(NavigationProbe& probe, const ImGuiWindow& window, ImGuiID item) {
  ImGuiIO& io = ImGui::GetIO();
  const ImVec2 low = window.Pos;
  const ImVec2 high(window.Pos.x + window.Size.x, window.Pos.y + window.Size.y);
  for (float y = low.y; y < high.y; y += kScanStep) {
    for (float x = low.x; x < high.x; x += kScanStep) {
      io.AddMousePosEvent(x, y);
      static_cast<void>(probe.Frame());
      if (ImGui::GetCurrentContext()->HoveredId == item) {
        return true;
      }
    }
  }
  return false;
}

// Un clic gauche là où la souris se trouve ; rend les deux réponses fusionnées.
NavigationAnswer Click(NavigationProbe& probe) {
  ImGuiIO& io = ImGui::GetIO();
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
  NavigationAnswer pressed = probe.Frame();
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
  NavigationAnswer released = probe.Frame();
  if (!released.go_to) {
    released.go_to = pressed.go_to;
  }
  if (!released.term) {
    released.term = pressed.term;
  }
  return released;
}

}  // namespace

TEST_CASE("Navigation : fermée, elle ne dessine rien", "[ui][navigation]") {
  NavigationProbe probe;
  probe.state.open = false;
  static_cast<void>(probe.Frame());
  CHECK(ImGui::FindWindowByName("###navigation") == nullptr);
}

TEST_CASE("Navigation : la frappe émet le terme, sans attendre Entrée", "[ui][navigation]") {
  NavigationProbe probe;
  probe.Settle();
  ImGuiWindow* window = ImGui::FindWindowByName("###navigation");
  REQUIRE(window != nullptr);
  REQUIRE(HoverItem(probe, *window, window->GetID("##navi_term")));
  static_cast<void>(Click(probe));
  ImGui::GetIO().AddInputCharactersUTF8("Po");
  const NavigationAnswer typed = probe.Frame();
  REQUIRE(typed.term.has_value());
  CHECK(*typed.term == "Po");
}

TEST_CASE("Navigation : taper lève le bornage", "[ui][navigation]") {
  NavigationProbe probe;
  ro::ui::OpenNavigationSearch(probe.state, "Warp Agent", false, "prontera");
  probe.Settle();
  CHECK(probe.logged.find("Limité à cette carte :") != std::string::npos);
  ImGuiWindow* window = ImGui::FindWindowByName("###navigation");
  REQUIRE(window != nullptr);
  REQUIRE(HoverItem(probe, *window, window->GetID("##navi_term")));
  static_cast<void>(Click(probe));
  ImGui::GetIO().AddInputCharactersUTF8("x");
  static_cast<void>(probe.Frame());
  CHECK(probe.state.bound_map.empty());
}

TEST_CASE("Navigation : le résultat unique se sélectionne tout seul", "[ui][navigation]") {
  NavigationProbe probe;
  probe.view.groups = std::span(probe.groups).subspan(1);
  probe.Settle();
  CHECK(probe.state.selected_group == 0);
  CHECK(probe.state.selected_entry == 0);
}

TEST_CASE("Navigation : « Y aller » désigne la ligne choisie", "[ui][navigation]") {
  NavigationProbe probe;
  probe.view.groups = std::span(probe.groups).subspan(1);
  probe.Settle();
  ImGuiWindow* detail = WindowNamed("##navi_detail");
  REQUIRE(detail != nullptr);
  REQUIRE(HoverItem(probe, *detail, detail->GetID("Y aller")));
  const NavigationAnswer clicked = Click(probe);
  REQUIRE(clicked.go_to.has_value());
  CHECK(clicked.go_to->group == 0);
  CHECK(clicked.go_to->entry == 0);
}

TEST_CASE("Navigation : les pastilles filtrent sans relancer la recherche", "[ui][navigation]") {
  NavigationProbe probe;
  probe.Settle();
  // Deux groupes visibles : rien ne se choisit seul.
  CHECK(probe.state.selected_group == -1);
  ImGuiWindow* window = ImGui::FindWindowByName("###navigation");
  REQUIRE(window != nullptr);
  REQUIRE(HoverItem(probe, *window, window->GetID("Monstres")));
  const NavigationAnswer clicked = Click(probe);
  CHECK(probe.state.filter == NavigationFilter::kMonsters);
  CHECK_FALSE(clicked.term.has_value());
  static_cast<void>(probe.Frame());
  // Le filtre ne laisse qu'un monstre : il se choisit seul, et c'est bien lui.
  CHECK(probe.state.selected_group == 1);
  CHECK(probe.state.selected_entry == 0);
  CHECK(probe.logged.find("Prontera Field") == std::string::npos);
}

TEST_CASE("Navigation : de nouveaux résultats oublient la sélection", "[ui][navigation]") {
  NavigationProbe probe;
  probe.state.selected_group = 0;
  probe.state.selected_entry = 1;
  probe.state.seen_serial = probe.view.results_serial;
  static_cast<void>(probe.Frame());
  CHECK(probe.state.selected_entry == 1);
  ++probe.view.results_serial;
  static_cast<void>(probe.Frame());
  CHECK(probe.state.selected_group == -1);
}

TEST_CASE("Navigation : aucun chemin se dit en toutes lettres", "[ui][navigation]") {
  NavigationProbe probe;
  probe.view.route.no_path = true;
  probe.Settle();
  CHECK(probe.logged.find("Aucun chemin jusqu'à cette destination.") != std::string::npos);
  CHECK(probe.logged.find("Aucune liaison connue ne mène jusque-là.") != std::string::npos);

  // Déjà sur place n'est pas un échec.
  probe.view.route.already_here = true;
  static_cast<void>(probe.Frame());
  CHECK(probe.logged.find("Vous êtes déjà sur cette carte.") != std::string::npos);
  CHECK(probe.logged.find("Aucun chemin") == std::string::npos);
}

TEST_CASE("Navigation : l'itinéraire nomme ses étapes et le prochain passage", "[ui][navigation]") {
  NavigationProbe probe;
  const std::array<ro::ui::NavigationRouteStep, 2> steps = {
      ro::ui::NavigationRouteStep{
          .map_label = "Prontera", .passage = "Parler à Kafra Employee", .current = true},
      ro::ui::NavigationRouteStep{.map_label = "Izlude"}};
  probe.view.route.steps = steps;
  probe.view.route.guiding = true;
  probe.view.route.distance_cells = 42;
  probe.Settle();
  CHECK(probe.logged.find("Izlude") != std::string::npos);
  CHECK(probe.logged.find("Prochain passage : Parler à Kafra Employee") != std::string::npos);
  CHECK(probe.logged.find("Distance estimée : 42 cellules") != std::string::npos);
}

TEST_CASE("Navigation : sans données, elle le dit", "[ui][navigation]") {
  NavigationProbe probe;
  probe.view.graph_map_count = 0;
  probe.Settle();
  CHECK(probe.logged.find("Les données de navigation ne sont pas chargées.") != std::string::npos);
}

TEST_CASE("Navigation : les noms de monstre se lisent", "[ui][navigation]") {
  CHECK(std::string_view(ro::ui::NavigationElementName(1)) == "Eau");
  CHECK(std::string_view(ro::ui::NavigationSizeName(2)) == "Grande");
  CHECK(std::string_view(ro::ui::NavigationRaceName(7)) == "Demi-humain");
  CHECK(std::string_view(ro::ui::NavigationRaceName(42)) == "?");
}
