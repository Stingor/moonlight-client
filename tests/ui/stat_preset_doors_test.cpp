// LA FENÊTRE DES PRÉRÉGLAGES DE STATS ET SES DEUX PORTES : le bouton de la
// barre de titre du statut, et celui du volet des stats de la fiche.
//
// 🔴 UNE SEULE FENÊTRE, UN SEUL INTERRUPTEUR. Deux portes qui piloteraient
// chacune le leur ouvriraient deux fenêtres — ou une fenêtre que la seconde
// porte ne saurait plus refermer. Et fermer la fiche ou le statut ne la ferme
// pas : une application lancée se suit depuis elle.

#include <array>
#include <cstddef>
#include <string_view>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/i18n.h"
#include "ro/render/skin_pieces.h"
#include "ro/ui/character_window.h"
#include "ro/ui/stat_preset_window.h"
#include "ro/ui/status_window.h"
#include "ro/ui/ui_context.h"

using ro::render::SkinPiece;
using ro::ui::CharacterTab;
using ro::ui::CharacterWindowAction;
using ro::ui::StatPresetDoors;
using ro::ui::StatusWindowAction;

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 900.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
constexpr float kPlainPiecePixels = 8.0F;
constexpr float kStatusBackgroundWidthPixels = 302.0F;
constexpr float kStatusBackgroundHeightPixels = 115.0F;
constexpr int kSettleFrames = 4;
// Assez large pour que la fiche montre son volet des stats.
constexpr float kWideSheetPixels = 1200.0F;
constexpr float kSheetHeightPixels = 700.0F;
// Le pas du balayage : plus fin qu'un bouton dans les deux sens.
constexpr float kSweepStepXPixels = 4.0F;
constexpr float kSweepStepYPixels = 2.0F;
constexpr std::string_view kDoorLabel = "Préréglages";

[[nodiscard]] ro::ui::SkinArt PlainArt() {
  ro::ui::SkinArt art;
  for (std::size_t rank = 0; rank < ro::render::kSkinPieceCount; ++rank) {
    art.Set(static_cast<SkinPiece>(rank),
            ro::ui::SkinImage{.id = 1, .width = kPlainPiecePixels, .height = kPlainPiecePixels});
  }
  // ⚠ LE FOND DU STATUT À SA TAILLE RÉELLE : c'est lui qui donne la largeur de
  // la fenêtre, et une barre de huit pixels n'aurait pas la place du bouton.
  art.Set(SkinPiece::kStatusBackground, ro::ui::SkinImage{.id = 1,
                                                          .width = kStatusBackgroundWidthPixels,
                                                          .height = kStatusBackgroundHeightPixels});
  return art;
}

// LES DEUX FENÊTRES-PORTES ET LA FENÊTRE, dessinées dans l'ordre d'`app` :
// la fiche, le statut, puis les portes routées vers l'interrupteur unique.
struct DoorsProbe {
  ro::ui::UiContext ui;
  ro::ui::SkinArt art = PlainArt();
  ro::ui::CharacterWindowState sheet;
  ro::ui::CharacterWindowView sheet_view;
  ro::ui::CharacterHeaderView header;
  std::array<ro::ui::EquipSlotView, ro::ui::kEquipSlotCount> slots{};
  ro::ui::CharacterStatsView stats;
  ro::ui::PresetTabView presets;
  ro::ui::CharacterTitlesView titles;
  ro::ui::GuildView guild;
  ro::ui::StatusWindowState status;
  ro::ui::StatusWindowView status_view;
  ro::ui::StatPresetWindowState window;
  ro::ui::StatPresetWindowView window_view;

  CharacterWindowAction sheet_asked;
  StatusWindowAction status_asked;

  DoorsProbe() {
    sheet.open = true;
    sheet.tab = CharacterTab::kEquipment;
    status.open = true;
  }

  void Frame() {
    ui.BeginFrame(kViewport, kFrameSeconds);
    sheet_asked = ro::ui::DrawCharacterWindow(sheet, sheet_view, header, slots, stats, presets,
                                              titles, guild, {}, ro::ui::SkillPointTally{}, art);
    status_asked = ro::ui::DrawStatusWindow(status, status_view, art);
    ro::ui::RouteStatPresetDoors(window, {.from_status = status_asked.toggle_presets,
                                          .from_sheet = sheet_asked.toggle_stat_presets});
    static_cast<void>(ro::ui::DrawStatPresetWindow(window, window_view, art));
    ui.EndFrame();
  }

  void Settle() {
    for (int frame = 0; frame < kSettleFrames; ++frame) {
      Frame();
    }
  }

  // La fiche élargie jusqu'à montrer son volet des stats, et le statut posé
  // dessous : aucune des deux portes ne se cache sous l'autre fenêtre.
  void WidenSheet() {
    Settle();
    ImGui::SetWindowPos("###character", ImVec2(0.0F, 0.0F));
    ImGui::SetWindowSize("###character", ImVec2(kWideSheetPixels, kSheetHeightPixels));
    ImGui::SetWindowPos("###status", ImVec2(0.0F, kSheetHeightPixels));
    Settle();
  }

  // Un clic entier sur ce que la souris survole. Vrai si l'une des deux
  // images a demandé la porte voulue.
  template <typename Asked>
  bool Click(Asked asked) {
    ImGuiIO& io = ImGui::GetIO();
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
    Frame();
    bool seen = asked(*this);
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
    Frame();
    seen = seen || asked(*this);
    return seen;
  }
};

// Le volet des stats de la fiche, que la fiche nomme « nombres ».
[[nodiscard]] ImGuiWindow* StatsPane() {
  const ImGuiWindow* sheet = ImGui::FindWindowByName("###character");
  if (sheet == nullptr) {
    return nullptr;
  }
  for (ImGuiWindow* window : ImGui::GetCurrentContext()->Windows) {
    if (window->ParentWindow == sheet &&
        std::string_view(window->Name).find("/nombres_") != std::string_view::npos) {
      return window;
    }
  }
  return nullptr;
}

// Balaie le rectangle jusqu'à survoler le bouton `label` posé dans `host`.
[[nodiscard]] bool Hover(DoorsProbe& probe, const ImGuiWindow& host, ImVec2 from, ImVec2 to) {
  const ImGuiID wanted = ImHashStr(kDoorLabel.data(), kDoorLabel.size(), host.ID);
  ImGuiIO& io = ImGui::GetIO();
  for (float y = from.y; y < to.y; y += kSweepStepYPixels) {
    for (float x = from.x; x < to.x; x += kSweepStepXPixels) {
      io.AddMousePosEvent(x, y);
      probe.Frame();
      if (ImGui::GetHoveredID() == wanted) {
        return true;
      }
    }
  }
  return false;
}

[[nodiscard]] bool HoverSheetDoor(DoorsProbe& probe) {
  const ImGuiWindow* pane = StatsPane();
  REQUIRE(pane != nullptr);
  return Hover(probe, *pane, pane->Pos,
               ImVec2(pane->Pos.x + pane->Size.x, pane->Pos.y + pane->Size.y));
}

[[nodiscard]] bool HoverStatusDoor(DoorsProbe& probe) {
  const ImGuiWindow* status = ImGui::FindWindowByName("###status");
  REQUIRE(status != nullptr);
  return Hover(probe, *status, status->Pos,
               ImVec2(status->Pos.x + status->Size.x, status->Pos.y + status->Size.y));
}

[[nodiscard]] bool SheetDoorAsked(const DoorsProbe& probe) {
  return probe.sheet_asked.toggle_stat_presets;
}
[[nodiscard]] bool StatusDoorAsked(const DoorsProbe& probe) {
  return probe.status_asked.toggle_presets;
}

[[nodiscard]] bool PresetWindowDrawn() {
  const ImGuiWindow* window = ImGui::FindWindowByName("###stat_presets");
  return window != nullptr && window->Active;
}

}  // namespace

TEST_CASE("Préréglages de stats : le bouton de la fiche émet l'ouverture",
          "[ui][fiche][stat_presets]") {
  ro::core::UseFrench();
  DoorsProbe probe;
  probe.status.open = false;
  probe.WidenSheet();
  REQUIRE(probe.sheet.stats_shown);
  REQUIRE_FALSE(probe.window.open);
  REQUIRE(HoverSheetDoor(probe));
  CHECK(probe.Click(SheetDoorAsked));
  CHECK(probe.window.open);
  probe.Frame();
  CHECK(PresetWindowDrawn());
  // Et l'image suivante ne redemande rien.
  CHECK_FALSE(probe.sheet_asked.toggle_stat_presets);
  CHECK(probe.window.open);
}

TEST_CASE("Préréglages de stats : les deux portes pilotent le même interrupteur",
          "[ui][fiche][statut][stat_presets]") {
  ro::core::UseFrench();
  DoorsProbe probe;
  probe.WidenSheet();
  REQUIRE(probe.sheet.stats_shown);

  // Ouverte par la fiche…
  REQUIRE(HoverSheetDoor(probe));
  REQUIRE(probe.Click(SheetDoorAsked));
  REQUIRE(probe.window.open);
  // …refermée par le statut…
  REQUIRE(HoverStatusDoor(probe));
  CHECK(probe.Click(StatusDoorAsked));
  CHECK_FALSE(probe.window.open);
  // …rouverte par le statut, refermée par la fiche.
  CHECK(probe.Click(StatusDoorAsked));
  CHECK(probe.window.open);
  REQUIRE(HoverSheetDoor(probe));
  CHECK(probe.Click(SheetDoorAsked));
  CHECK_FALSE(probe.window.open);
}

TEST_CASE("Préréglages de stats : fermer une porte ne ferme pas la fenêtre",
          "[ui][fiche][statut][stat_presets]") {
  ro::core::UseFrench();
  DoorsProbe probe;
  probe.window.open = true;
  probe.Settle();
  REQUIRE(PresetWindowDrawn());
  probe.sheet.open = false;
  probe.status.open = false;
  probe.Settle();
  CHECK(probe.window.open);
  CHECK(PresetWindowDrawn());
}

TEST_CASE("Préréglages de stats : un double clic d'une même image ne bascule qu'une fois",
          "[ui][stat_presets]") {
  ro::ui::StatPresetWindowState state;
  ro::ui::RouteStatPresetDoors(state, StatPresetDoors{.from_status = true, .from_sheet = true});
  CHECK(state.open);
  ro::ui::RouteStatPresetDoors(state, StatPresetDoors{});
  CHECK(state.open);
}
