// LA PAGE « COMMANDS SETTINGS » : ce qu'un clic demande, ce qu'une page muette
// refuse, et les deux calculs que ses boutons font avant de parler au serveur.

#include "ro/ui/commands_page.h"

#include <array>
#include <cstddef>
#include <optional>

#include <imgui.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/skin_pieces.h"
#include "ro/ui/ui_context.h"

using ro::render::SkinPiece;
using ro::ui::CommandsField;
using ro::ui::CommandsPageAction;
using ro::ui::CommandsPageState;
using ro::ui::CommandsPageView;
using ro::ui::CommandsPresetView;
using ro::ui::CommandsTab;

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
constexpr float kPlainPiecePixels = 8.0F;
constexpr float kBenchWidth = 640.0F;
constexpr float kBenchHeight = 600.0F;
// La bande où tombe la première colonne du tableau : une case, son libellé.
constexpr float kFirstColumnReach = 30.0F;
// La bande des onglets, sous la barre de titre du banc, et son balayage.
constexpr float kTabBandReach = 60.0F;
constexpr float kTabScanReachX = 240.0F;
constexpr float kTabScanStepX = 8.0F;
constexpr float kTabScanStepY = 4.0F;

[[nodiscard]] ro::ui::SkinArt PlainArt() {
  ro::ui::SkinArt art;
  for (std::size_t rank = 0; rank < ro::render::kSkinPieceCount; ++rank) {
    art.Set(static_cast<SkinPiece>(rank),
            ro::ui::SkinImage{.id = 1, .width = kPlainPiecePixels, .height = kPlainPiecePixels});
  }
  return art;
}

// La page seule, dans une fenêtre de banc.
struct PageProbe {
  ro::ui::UiContext ui;
  ro::ui::SkinArt art = PlainArt();
  CommandsPageState state;
  CommandsPageView view{.told = true};

  CommandsPageAction Frame() {
    ui.BeginFrame(kViewport, kFrameSeconds);
    ImGui::SetNextWindowPos(ImVec2(0.0F, 0.0F));
    ImGui::SetNextWindowSize(ImVec2(kBenchWidth, kBenchHeight));
    ImGui::Begin("banc");
    CommandsPageAction asked = ro::ui::DrawCommandsPage(view, state, art);
    ImGui::End();
    ui.EndFrame();
    return asked;
  }

  CommandsPageAction ClickAt(float x, float y) {
    ImGuiIO& io = ImGui::GetIO();
    io.AddMousePosEvent(x, y);
    static_cast<void>(Frame());
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
    CommandsPageAction pressed = Frame();
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
    CommandsPageAction released = Frame();
    return pressed.field != CommandsField::kNone ? pressed : released;
  }

  // Clique de haut en bas le long de la première colonne jusqu'à ce qu'un
  // réglage réponde.
  CommandsPageAction FirstAnswer() {
    for (int frame = 0; frame < 4; ++frame) {
      static_cast<void>(Frame());
    }
    for (float y = 0.0F; y < kBenchHeight; y += 2.0F) {
      for (float x = 4.0F; x < kFirstColumnReach; x += 6.0F) {
        const CommandsPageAction asked = ClickAt(x, y);
        if (asked.field != CommandsField::kNone) {
          return asked;
        }
      }
    }
    return {};
  }

  // Maj + clic le long de la bande des onglets ; rend les onglets dont un lien
  // a été demandé, et l'onglet ouvert après chacun.
  struct LinkScan {
    std::optional<CommandsTab> general_opened;
    std::optional<CommandsTab> autoloot_opened;
  };
  LinkScan ScanTabLinks() {
    LinkScan seen;
    ImGuiIO& io = ImGui::GetIO();
    for (float y = 0.0F; y < kTabBandReach; y += kTabScanStepY) {
      for (float x = 0.0F; x < kTabScanReachX; x += kTabScanStepX) {
        io.AddMousePosEvent(x, y);
        static_cast<void>(Frame());
        io.AddKeyEvent(ImGuiMod_Shift, true);
        io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
        const CommandsPageAction asked = Frame();
        io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
        io.AddKeyEvent(ImGuiMod_Shift, false);
        static_cast<void>(Frame());
        if (asked.link_tab == CommandsTab::kGeneral) {
          seen.general_opened = state.tab;
        } else if (asked.link_tab == CommandsTab::kAutoloot) {
          seen.autoloot_opened = state.tab;
        }
      }
    }
    return seen;
  }
};

}  // namespace

TEST_CASE("Commands Settings : un onglet demandé s'ouvre, une fois", "[ui][commands][lien]") {
  PageProbe probe;
  static_cast<void>(probe.Frame());
  CHECK(probe.state.tab == CommandsTab::kGeneral);
  probe.state.tab_wanted = CommandsTab::kAutoloot;
  static_cast<void>(probe.Frame());
  CHECK_FALSE(probe.state.tab_wanted.has_value());
  static_cast<void>(probe.Frame());
  CHECK(probe.state.tab == CommandsTab::kAutoloot);
  // ⚠ LA DEMANDE NE S'IMPOSE PLUS : l'autre onglet se demande encore.
  probe.state.tab_wanted = CommandsTab::kGeneral;
  static_cast<void>(probe.Frame());
  static_cast<void>(probe.Frame());
  CHECK(probe.state.tab == CommandsTab::kGeneral);
}

TEST_CASE("Commands Settings : Maj + clic sur un onglet demande son lien et l'ouvre",
          "[ui][commands][lien]") {
  PageProbe probe;
  probe.view.offers_links = true;
  static_cast<void>(probe.Frame());
  const PageProbe::LinkScan seen = probe.ScanTabLinks();
  REQUIRE(seen.general_opened.has_value());
  REQUIRE(seen.autoloot_opened.has_value());
  CHECK(*seen.general_opened == CommandsTab::kGeneral);
  CHECK(*seen.autoloot_opened == CommandsTab::kAutoloot);
}

TEST_CASE("Commands Settings : sans chat, Maj + clic sur un onglet ne demande rien",
          "[ui][commands][lien]") {
  PageProbe probe;
  static_cast<void>(probe.Frame());
  const PageProbe::LinkScan seen = probe.ScanTabLinks();
  CHECK_FALSE(seen.general_opened.has_value());
  CHECK_FALSE(seen.autoloot_opened.has_value());
}

TEST_CASE("Commands Settings : la première case demande son réglage inverse", "[ui][commands]") {
  PageProbe probe;
  probe.view.show_exp = true;
  const CommandsPageAction asked = probe.FirstAnswer();
  CHECK(asked.field == CommandsField::kShowExp);
  // ⚠ LA PAGE NE COCHE RIEN ELLE-MÊME : elle demande la valeur inverse, et c'est
  // l'appelant qui la pose.
  CHECK(asked.value == 0);
  CHECK(probe.view.show_exp);
}

TEST_CASE("Commands Settings : une page que le serveur n'a pas renseignée reste sourde",
          "[ui][commands]") {
  PageProbe probe;
  probe.view.told = false;
  CHECK(probe.FirstAnswer().field == CommandsField::kNone);
}

TEST_CASE("Commands Settings : le seuil de zeny se borne et s'arrondit au pas du fil",
          "[ui][commands]") {
  CHECK(ro::ui::ClampAutolootMinZeny(-500) == 0);
  CHECK(ro::ui::ClampAutolootMinZeny(12345) == 12300);
  CHECK(ro::ui::ClampAutolootMinZeny(5000000) == ro::ui::kCommandsMinZenyHighest);
}

TEST_CASE("Commands Settings : un nom connu réécrit son preset, un neuf prend le premier libre",
          "[ui][commands]") {
  const std::array<CommandsPresetView, 2> presets = {CommandsPresetView{.slot = 1, .name = "MVP"},
                                                     CommandsPresetView{.slot = 3, .name = "Farm"}};
  CHECK(ro::ui::PresetSlotForName(presets, "Farm") == 3);
  CHECK(ro::ui::PresetSlotForName(presets, "Neuf") == 2);

  // 🔴 LES DIX EMPLACEMENTS PRIS, UN NOM NEUF N'A NULLE PART OÙ ALLER.
  std::array<CommandsPresetView, ro::ui::kCommandsPresetSlots> full{};
  for (std::size_t rank = 0; rank < full.size(); ++rank) {
    full[rank] = CommandsPresetView{.slot = static_cast<std::uint8_t>(rank + 1), .name = "x"};
  }
  CHECK(ro::ui::PresetSlotForName(full, "Neuf") == 0);
  CHECK(ro::ui::PresetSlotForName(full, "x") == 1);
}
