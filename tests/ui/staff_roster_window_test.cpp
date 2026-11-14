// La fenêtre « Joueurs en ligne » du staff, dessinée hors jeu : fermée elle ne
// dessine rien, chaque phase et chaque vue se dessinent sans rien demander, et
// le clic droit sur une ligne rend son joueur.

#include "ro/ui/staff_roster_window.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/skin_pieces.h"
#include "ro/ui/ui_context.h"

using ro::render::SkinPiece;
using ro::ui::StaffRosterAnswer;
using ro::ui::StaffRosterPhase;
using ro::ui::StaffRosterRow;
using ro::ui::StaffRosterTab;

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
constexpr float kPlainPiecePixels = 8.0F;
constexpr const char* kWindowName = "###staff_roster";
constexpr std::uint32_t kOnlyAccount = 2000123;

[[nodiscard]] ro::ui::SkinArt PlainArt() {
  ro::ui::SkinArt art;
  for (std::size_t rank = 0; rank < ro::render::kSkinPieceCount; ++rank) {
    art.Set(static_cast<SkinPiece>(rank),
            ro::ui::SkinImage{.id = 1, .width = kPlainPiecePixels, .height = kPlainPiecePixels});
  }
  return art;
}

// Une ligne qui porte tout ce que la fenêtre sait dire d'un joueur.
[[nodiscard]] StaffRosterRow BusyRow() {
  StaffRosterRow row;
  row.account_id = kOnlyAccount;
  row.char_id = 150042;
  row.name = "Tobias";
  row.guild = "Cendre";
  row.party = "Veilleurs";
  row.job_name = "Sniper";
  row.base_level = 96;
  row.job_level = 64;
  row.map = "gl_cas02";
  row.instance_id = 4;
  row.staff = true;
  row.vending = true;
  row.hidden = true;
  row.away = true;
  row.jailed = true;
  row.online_s = 22800;
  row.character_s = 120;
  row.idle_s = 900;
  row.action_age_s = {30U, 1U, std::nullopt, 21600U, std::nullopt, std::nullopt, 600U};
  row.client = ro::ui::StaffRosterClientKind::kMoonclient;
  row.patch_index = 41;
  row.revision = "906828f0fabcdef0";
  row.tree_modified = true;
  row.client_outdated = true;
  row.machine_group = 1;
  row.cheat_cadence = 3;
  row.mute_minutes = 34;
  return row;
}

struct RosterProbe {
  ro::ui::UiContext ui;
  ro::ui::SkinArt art = PlainArt();
  ro::ui::StaffRosterWindowState state;
  ro::ui::StaffRosterView view;

  RosterProbe() {
    state.open = true;
    state.filters.hide_offline_shops = false;
    view.phase = StaffRosterPhase::kReady;
    view.refresh_period_s = 10;
    view.rows.push_back(BusyRow());
  }

  StaffRosterAnswer Frame() {
    ui.BeginFrame(kViewport, kFrameSeconds);
    const StaffRosterAnswer answer = ro::ui::DrawStaffRosterWindow(state, view, art);
    ui.EndFrame();
    return answer;
  }
};

// Clique en ce point, et rend ce que la fenêtre a demandé au relâchement.
[[nodiscard]] StaffRosterAnswer ClickAt(RosterProbe& probe, ImVec2 at, ImGuiMouseButton button) {
  ImGuiIO& io = ImGui::GetIO();
  io.AddMousePosEvent(at.x, at.y);
  static_cast<void>(probe.Frame());
  io.AddMouseButtonEvent(button, true);
  static_cast<void>(probe.Frame());
  io.AddMouseButtonEvent(button, false);
  return probe.Frame();
}

// Le balayage descend la fenêtre par sa marge gauche, là où commencent une
// ligne de la liste et un bouton du corps.
constexpr float kScanStepPixels = 6.0F;
constexpr float kScanLeftInsetPixels = 40.0F;

}  // namespace

TEST_CASE("Joueurs en ligne : fermée, rien ne se dessine", "[ui][staff_roster]") {
  RosterProbe probe;
  probe.state.open = false;
  for (int frame = 0; frame < 3; ++frame) {
    const StaffRosterAnswer answer = probe.Frame();
    CHECK_FALSE(answer.refresh);
    CHECK_FALSE(answer.menu_account_id.has_value());
  }
  CHECK(ImGui::FindWindowByName(kWindowName) == nullptr);
}

TEST_CASE("Joueurs en ligne : chaque phase se dessine sans rien redemander", "[ui][staff_roster]") {
  RosterProbe probe;
  for (const StaffRosterPhase phase :
       {StaffRosterPhase::kAsking, StaffRosterPhase::kSilent, StaffRosterPhase::kDenied,
        StaffRosterPhase::kUnreadable, StaffRosterPhase::kReady}) {
    probe.view.phase = phase;
    for (int frame = 0; frame < 3; ++frame) {
      CHECK_FALSE(probe.Frame().refresh);
    }
  }
  CHECK(ImGui::FindWindowByName(kWindowName) != nullptr);
  CHECK(probe.state.open);
}

TEST_CASE("Joueurs en ligne : les deux vues se dessinent, liste pleine ou vide",
          "[ui][staff_roster]") {
  RosterProbe probe;
  for (const StaffRosterTab tab : {StaffRosterTab::kPlayers, StaffRosterTab::kMaps}) {
    probe.state.tab = tab;
    for (int frame = 0; frame < 3; ++frame) {
      CHECK_FALSE(probe.Frame().refresh);
    }
  }
  CHECK(probe.state.shown_order.size() == 1);
  probe.view.rows.clear();
  probe.view.refreshing = true;
  for (int frame = 0; frame < 3; ++frame) {
    CHECK_FALSE(probe.Frame().refresh);
  }
  CHECK(probe.state.shown_order.empty());
}

TEST_CASE("Joueurs en ligne : la recherche retire de la liste ce qu'elle ne lit pas",
          "[ui][staff_roster]") {
  RosterProbe probe;
  static_cast<void>(probe.Frame());
  CHECK(probe.state.shown_order.size() == 1);
  const std::string needle = "geffen";
  needle.copy(probe.state.search.data(), needle.size());
  static_cast<void>(probe.Frame());
  CHECK(probe.state.shown_order.empty());
}

TEST_CASE("Joueurs en ligne : demandée au premier plan, elle le prend une fois",
          "[ui][staff_roster]") {
  RosterProbe probe;
  probe.state.raise = true;
  static_cast<void>(probe.Frame());
  CHECK_FALSE(probe.state.raise);
}

TEST_CASE("Joueurs en ligne : le clic droit sur une ligne rend son joueur", "[ui][staff_roster]") {
  RosterProbe probe;
  static_cast<void>(probe.Frame());
  static_cast<void>(probe.Frame());
  const ImGuiWindow* window = ImGui::FindWindowByName(kWindowName);
  REQUIRE(window != nullptr);
  // La seule ligne de la liste est quelque part sous le titre : le balayage
  // descend jusqu'à elle, et tout ce qu'il touche d'autre ne rend personne.
  std::optional<std::uint32_t> asked;
  for (float y = window->Pos.y; y < window->Pos.y + window->Size.y && !asked.has_value();
       y += kScanStepPixels) {
    asked = ClickAt(probe, ImVec2(window->Pos.x + kScanLeftInsetPixels, y), ImGuiMouseButton_Right)
                .menu_account_id;
  }
  REQUIRE(asked.has_value());
  CHECK(*asked == kOnlyAccount);
}

TEST_CASE("Joueurs en ligne : serveur muet, la commande reste à un clic", "[ui][staff_roster]") {
  RosterProbe probe;
  probe.view.phase = StaffRosterPhase::kSilent;
  probe.view.rows.clear();
  static_cast<void>(probe.Frame());
  static_cast<void>(probe.Frame());
  const ImGuiWindow* window = ImGui::FindWindowByName(kWindowName);
  REQUIRE(window != nullptr);
  // Le bouton du corps vient avant ceux du pied : le balayage s'arrête sur lui.
  bool said = false;
  for (float y = window->Pos.y; y < window->Pos.y + window->Size.y && !said; y += kScanStepPixels) {
    const StaffRosterAnswer answer =
        ClickAt(probe, ImVec2(window->Pos.x + kScanLeftInsetPixels, y), ImGuiMouseButton_Left);
    said = answer.say_command;
    CHECK_FALSE(answer.refresh);
  }
  CHECK(said);
  // Hors du silence, rien ne propose la commande.
  probe.view.phase = StaffRosterPhase::kDenied;
  for (float y = window->Pos.y; y < window->Pos.y + window->Size.y; y += kScanStepPixels) {
    const StaffRosterAnswer answer =
        ClickAt(probe, ImVec2(window->Pos.x + kScanLeftInsetPixels, y), ImGuiMouseButton_Left);
    CHECK_FALSE(answer.say_command);
  }
}
