// LA MINIMAP : ce qui tient sans image, sa forme carrée, son zoom, et ses deux
// clics — le droit qui pose un marqueur, le gauche qui guide vers une
// commodité.
//
// 🔴 UNE CARTE SANS IMAGE GARDE SES MARQUEURS : c'est la règle qui se perdrait
// le plus facilement, en imbriquant le dessin des marqueurs dans le test de
// l'image. Beaucoup de cartes d'instance n'ont pas d'image, et c'est là qu'on
// cherche ses coéquipiers.

#include "ro/ui/minimap_window.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/skin_pieces.h"
#include "ro/ui/ui_context.h"

using ro::ui::MinimapAnswer;
using ro::ui::MinimapCell;
using ro::ui::MinimapTownIcon;

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
constexpr float kPlainPiecePixels = 8.0F;
constexpr int kSettleFrames = 4;
// Plus que le délai de pose du portail de la molette.
constexpr int kHoverFrames = 40;
constexpr int kWheelFrames = 12;
constexpr int kMapCells = 100;
constexpr float kPlayerCell = 50.0F;
constexpr MinimapCell kKafraCell{.x = 60, .y = 60};
constexpr MinimapCell kInnCell{.x = 20, .y = 20};
constexpr float kIconPixels = 8.0F;
constexpr float kSidePixels = 220.0F;

[[nodiscard]] ro::ui::SkinArt PlainArt() {
  ro::ui::SkinArt art;
  for (std::size_t rank = 0; rank < ro::render::kSkinPieceCount; ++rank) {
    art.Set(static_cast<ro::render::SkinPiece>(rank),
            ro::ui::SkinImage{.id = 1, .width = kPlainPiecePixels, .height = kPlainPiecePixels});
  }
  return art;
}

struct MinimapProbe {
  ro::ui::UiContext ui;
  ro::ui::SkinArt art = PlainArt();
  ro::ui::MinimapSettings settings;
  ro::ui::MinimapWindowState state;
  std::array<MinimapTownIcon, 2> town = {
      MinimapTownIcon{.cell = kKafraCell,
                      .image = {.id = 2, .width = kIconPixels, .height = kIconPixels},
                      .label = "Kafra"},
      MinimapTownIcon{.cell = kInnCell,
                      .image = {.id = 3, .width = kIconPixels, .height = kIconPixels},
                      .label = "Inn"}};
  ro::ui::MinimapView view{.map_name = "prontera",
                           .cells_width = kMapCells,
                           .cells_height = kMapCells,
                           .player_cell_x = kPlayerCell,
                           .player_cell_y = kPlayerCell,
                           .town = town};
  std::string logged;
  // L'écran des images suivantes : nul, petit, ou redimensionné en cours d'essai.
  ro::ui::Viewport viewport = kViewport;

  MinimapProbe() { settings.side_px = static_cast<int>(kSidePixels); }

  MinimapAnswer Frame() {
    ui.BeginFrame(viewport, kFrameSeconds);
    ImGui::LogToBuffer();
    MinimapAnswer answer = ro::ui::DrawMinimapWindow(settings, state, view, art);
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

  [[nodiscard]] const ImGuiWindow& Window() const {
    const ImGuiWindow* window = ImGui::FindWindowByName("###minimap");
    REQUIRE(window != nullptr);
    return *window;
  }

  // Le point de l'écran d'une cellule, au zoom 1 et sans image : la carte
  // remplit le carré, l'axe vertical inversé.
  [[nodiscard]] ImVec2 CellPoint(float cell_x, float cell_y) const {
    const ImVec2 origin = Window().DC.CursorStartPos;
    const auto cells = static_cast<float>(kMapCells);
    return {origin.x + cell_x / cells * kSidePixels,
            origin.y + (cells - cell_y) / cells * kSidePixels};
  }

  // Un clic complet ; rend la première réponse qui porte quelque chose.
  MinimapAnswer Click(ImVec2 at, ImGuiMouseButton button) {
    ImGuiIO& io = ImGui::GetIO();
    io.AddMousePosEvent(at.x, at.y);
    static_cast<void>(Frame());
    io.AddMouseButtonEvent(button, true);
    MinimapAnswer pressed = Frame();
    io.AddMouseButtonEvent(button, false);
    MinimapAnswer released = Frame();
    return pressed.guide_to.has_value() ? pressed : released;
  }
};

}  // namespace

TEST_CASE("Minimap : l'heure du serveur se montre sous la carte quand on la demande",
          "[ui][minimap]") {
  MinimapProbe probe;
  probe.view.server_clock = "16:05";
  probe.Settle();
  static_cast<void>(probe.Frame());
  // Éteinte par défaut : l'heure ne se montre pas, même connue.
  CHECK(probe.logged.find("16:05") == std::string::npos);

  probe.settings.show_server_clock = true;
  probe.Settle();
  static_cast<void>(probe.Frame());
  CHECK(probe.logged.find("Serveur 16:05") != std::string::npos);
  // Et la ligne des coordonnées reste là.
  CHECK(probe.logged.find("prontera,50,50") != std::string::npos);

  // Une heure inconnue ne laisse pas de ligne vide derrière elle.
  probe.view.server_clock = {};
  static_cast<void>(probe.Frame());
  CHECK(probe.logged.find("Serveur") == std::string::npos);
}

// 🔴 SANS LÉGENDE, le curseur ramené sous le carré n'est suivi d'aucun item :
// ImGui y voit une extension de la fenêtre par le curseur, et le signale.
TEST_CASE("Minimap : sans nom, ni coordonnées, ni heure, aucune erreur d'ImGui", "[ui][minimap]") {
  MinimapProbe probe;
  ImGuiIO& io = ImGui::GetIO();
  io.ConfigErrorRecoveryEnableAssert = false;
  io.ConfigErrorRecoveryEnableTooltip = false;
  probe.view.display_name = "Prontera";
  probe.settings.show_map_name = false;
  probe.settings.show_coords = false;
  probe.settings.show_server_clock = false;
  ImGuiContext& context = *ImGui::GetCurrentContext();
  context.DebugLogBuf.clear();
  probe.Settle();
  INFO(context.DebugLogBuf.c_str());
  CHECK(std::string(context.DebugLogBuf.c_str()).find("SetCursorPos") == std::string::npos);
  CHECK(probe.logged.find("prontera") == std::string::npos);
}

TEST_CASE("Minimap : sans image, elle dessine et garde ses marqueurs", "[ui][minimap]") {
  MinimapProbe probe;
  REQUIRE_FALSE(probe.view.map_image.usable());
  probe.Settle();
  // La ligne « carte,x,y » est là…
  static_cast<void>(probe.Frame());
  CHECK(probe.logged.find("prontera,50,50") != std::string::npos);
  // …et la commodité reste cliquable.
  const MinimapAnswer answer = probe.Click(
      probe.CellPoint(static_cast<float>(kKafraCell.x), static_cast<float>(kKafraCell.y)),
      ImGuiMouseButton_Left);
  REQUIRE(answer.guide_to.has_value());
  CHECK(answer.guide_to->x == kKafraCell.x);
  CHECK(answer.guide_to->y == kKafraCell.y);
}

TEST_CASE("Minimap : une icône cliquée guide vers SA cellule", "[ui][minimap]") {
  MinimapProbe probe;
  probe.Settle();
  const MinimapAnswer answer =
      probe.Click(probe.CellPoint(static_cast<float>(kInnCell.x), static_cast<float>(kInnCell.y)),
                  ImGuiMouseButton_Left);
  REQUIRE(answer.guide_to.has_value());
  CHECK(answer.guide_to->x == kInnCell.x);
  CHECK(answer.guide_to->y == kInnCell.y);
}

TEST_CASE("Minimap : la zone de carte reste carrée", "[ui][minimap]") {
  const ro::ui::MinimapSquareFit fit{
      .pad_x = 16.0F, .pad_y = 16.0F, .extra_height = 20.0F, .most_side = 500.0F};
  // Le côté est la moyenne des deux axes demandés.
  const ImVec2 wanted = ro::ui::FitMinimapSquare(ImVec2(300.0F, 250.0F), fit);
  CHECK(wanted.x - fit.pad_x == Catch::Approx(wanted.y - fit.pad_y - fit.extra_height));
  CHECK(wanted.x - fit.pad_x == Catch::Approx(249.0F));
  // Plancher et plafond.
  CHECK(ro::ui::FitMinimapSquare(ImVec2(10.0F, 10.0F), fit).x - fit.pad_x ==
        Catch::Approx(ro::ui::kMinimapLeastSide));
  CHECK(ro::ui::FitMinimapSquare(ImVec2(2000.0F, 2000.0F), fit).x - fit.pad_x ==
        Catch::Approx(fit.most_side));

  // La fenêtre elle-même : son contenu est un carré, lignes de texte en plus.
  MinimapProbe probe;
  probe.Settle();
  const ImGuiWindow& window = probe.Window();
  const ImVec2 padding = ImGui::GetStyle().WindowPadding;
  CHECK(window.Size.x - 2.0F * padding.x == Catch::Approx(kSidePixels));
  CHECK(probe.settings.side_px == static_cast<int>(kSidePixels));
}

TEST_CASE("Minimap : jamais plus grande que l'écran permis", "[ui][minimap]") {
  MinimapProbe probe;
  probe.settings.side_px = 5000;
  probe.Settle();
  const ImGuiWindow& window = probe.Window();
  CHECK(window.Size.y <= kViewport.height_px * ro::ui::kMinimapScreenShare + 1.0F);
  // Le côté montré est rabattu ; celui que le joueur a choisi reste.
  CHECK(probe.settings.side_px == 5000);
}

TEST_CASE("Minimap : le zoom est borné, au pas du jeu", "[ui][minimap]") {
  CHECK(ro::ui::StepMinimapZoom(1.0F, -3.0F) == Catch::Approx(ro::ui::kMinimapZoomLeast));
  CHECK(ro::ui::StepMinimapZoom(1.0F, 10.0F) == Catch::Approx(ro::ui::kMinimapZoomMost));
  CHECK(ro::ui::StepMinimapZoom(1.0F, 1.0F) == Catch::Approx(4.0F / 3.0F));
  CHECK(ro::ui::StepMinimapZoom(std::numeric_limits<float>::quiet_NaN(), 0.0F) ==
        Catch::Approx(ro::ui::kMinimapZoomLeast));

  // Et par la molette, au-dessus de la carte.
  MinimapProbe probe;
  probe.Settle();
  ImGuiIO& io = ImGui::GetIO();
  const ImVec2 center = probe.CellPoint(kPlayerCell, kPlayerCell);
  for (int frame = 0; frame < kHoverFrames; ++frame) {
    io.AddMousePosEvent(center.x, center.y);
    static_cast<void>(probe.Frame());
  }
  bool announced = false;
  for (int frame = 0; frame < kWheelFrames; ++frame) {
    io.AddMouseWheelEvent(0.0F, 1.0F);
    announced |= probe.Frame().settings_changed;
  }
  CHECK(probe.settings.zoom == Catch::Approx(ro::ui::kMinimapZoomMost));
  CHECK(announced);
  for (int frame = 0; frame < kWheelFrames; ++frame) {
    io.AddMouseWheelEvent(0.0F, -1.0F);
    static_cast<void>(probe.Frame());
  }
  CHECK(probe.settings.zoom == Catch::Approx(ro::ui::kMinimapZoomLeast));
}

TEST_CASE("Minimap : le cadrage glisse le long du bord sans rétrécir", "[ui][minimap]") {
  const ro::ui::MinimapSpan edge = ro::ui::CenteredMinimapSpan(0.1F, 0.5F);
  CHECK(edge.low == Catch::Approx(0.0F));
  CHECK(edge.high == Catch::Approx(0.5F));
  const ro::ui::MinimapSpan whole = ro::ui::CenteredMinimapSpan(0.1F, 1.0F);
  CHECK(whole.high - whole.low == Catch::Approx(1.0F));
}

TEST_CASE("Minimap : clic droit ouvre le marqueur, Entrée l'ajoute", "[ui][minimap]") {
  MinimapProbe probe;
  probe.Settle();
  // Une cellule loin des icônes et du joueur.
  const ImVec2 origin = probe.Window().DC.CursorStartPos;
  const ImVec2 at(origin.x + 66.5F, origin.y + 65.5F);
  static_cast<void>(probe.Click(at, ImGuiMouseButton_Right));
  CHECK(probe.state.memo_cell.x == 30);
  CHECK(probe.state.memo_cell.y == 70);
  CHECK(probe.logged.find("Marqueur en 30, 70") != std::string::npos);

  ImGuiIO& io = ImGui::GetIO();
  io.AddInputCharactersUTF8("Kafra");
  static_cast<void>(probe.Frame());
  io.AddKeyEvent(ImGuiKey_Enter, true);
  std::optional<ro::ui::MinimapMemoAdd> added = probe.Frame().add_memo;
  io.AddKeyEvent(ImGuiKey_Enter, false);
  const MinimapAnswer after = probe.Frame();
  if (!added.has_value()) {
    added = after.add_memo;
  }
  REQUIRE(added.has_value());
  CHECK(added->name == "Kafra");
  CHECK(added->cell.x == 30);
  CHECK(added->cell.y == 70);
}

TEST_CASE("Minimap : le plafond de marqueurs refuse l'ajout", "[ui][minimap]") {
  MinimapProbe probe;
  std::array<ro::ui::MinimapMemo, ro::ui::kMinimapMemoCap> memos{};
  probe.view.memos = memos;
  probe.Settle();
  const ImVec2 origin = probe.Window().DC.CursorStartPos;
  static_cast<void>(
      probe.Click(ImVec2(origin.x + 66.5F, origin.y + 65.5F), ImGuiMouseButton_Right));
  ImGuiIO& io = ImGui::GetIO();
  io.AddInputCharactersUTF8("Trop");
  static_cast<void>(probe.Frame());
  io.AddKeyEvent(ImGuiKey_Enter, true);
  const MinimapAnswer pressed = probe.Frame();
  io.AddKeyEvent(ImGuiKey_Enter, false);
  const MinimapAnswer released = probe.Frame();
  CHECK_FALSE(pressed.add_memo.has_value());
  CHECK_FALSE(released.add_memo.has_value());
  CHECK(probe.logged.find("Maximum atteint pour cette carte.") != std::string::npos);
}

// ── Les tombes du carnet de chasse ──────────────────────────────────────────

namespace {

// Les teintes d'une tombe, celles que la couche peint.
constexpr ImU32 kTombExactTint = IM_COL32(255, 216, 96, 235);
constexpr ImU32 kTombWindowTint = IM_COL32(232, 140, 60, 205);
constexpr MinimapCell kExactTombCell{.x = 30, .y = 30};
constexpr MinimapCell kWindowTombCell{.x = 70, .y = 40};
// Hors de la carte, donc hors du cadrage.
constexpr MinimapCell kStrayTombCell{.x = 150, .y = 50};
// L'anticrénelage rentre les sommets d'un demi-pixel : le centre n'en bouge pas.
constexpr float kCenterSlackPixels = 0.75F;

// Le centre des sommets PEINTS de cette teinte dans la fenêtre, et leur nombre.
struct TintCenter {
  ImVec2 center{0.0F, 0.0F};
  int count = 0;
};

[[nodiscard]] TintCenter CenterOf(const ImGuiWindow& window, ImU32 tint) {
  TintCenter found;
  float sum_x = 0.0F;
  float sum_y = 0.0F;
  for (const ImDrawVert& vertex : window.DrawList->VtxBuffer) {
    if (vertex.col == tint) {
      sum_x += vertex.pos.x;
      sum_y += vertex.pos.y;
      ++found.count;
    }
  }
  if (found.count > 0) {
    found.center =
        ImVec2(sum_x / static_cast<float>(found.count), sum_y / static_cast<float>(found.count));
  }
  return found;
}

}  // namespace

TEST_CASE("Minimap : la tombe d'un MVP, dorée ou orangée, à sa cellule", "[ui][minimap]") {
  MinimapProbe probe;
  const std::array<ro::ui::MinimapTomb, 3> tombs = {
      ro::ui::MinimapTomb{.cell = kExactTombCell, .exact = true},
      ro::ui::MinimapTomb{.cell = kWindowTombCell, .exact = false},
      ro::ui::MinimapTomb{.cell = kStrayTombCell, .exact = true}};
  probe.view.tombs = tombs;
  probe.Settle();
  static_cast<void>(probe.Frame());

  // DEUX LOSANGES, DEUX COULEURS : doré pour l'instant mérité, orangé pour la
  // fenêtre.
  const TintCenter exact = CenterOf(probe.Window(), kTombExactTint);
  const TintCenter window = CenterOf(probe.Window(), kTombWindowTint);
  REQUIRE(exact.count > 0);
  REQUIRE(window.count > 0);
  // 🔴 LE REPÈRE DES MÉMOS : x sur la largeur, et y COMPTÉ DEPUIS LE BAS. Et la
  // tombe hors du cadrage est OMISE : peinte, elle tirerait le centre doré.
  const ImVec2 exact_at =
      probe.CellPoint(static_cast<float>(kExactTombCell.x), static_cast<float>(kExactTombCell.y));
  CHECK(exact.center.x == Catch::Approx(exact_at.x).margin(kCenterSlackPixels));
  CHECK(exact.center.y == Catch::Approx(exact_at.y).margin(kCenterSlackPixels));
  const ImVec2 window_at =
      probe.CellPoint(static_cast<float>(kWindowTombCell.x), static_cast<float>(kWindowTombCell.y));
  CHECK(window.center.x == Catch::Approx(window_at.x).margin(kCenterSlackPixels));
  CHECK(window.center.y == Catch::Approx(window_at.y).margin(kCenterSlackPixels));
}

TEST_CASE("Minimap : sans tombe, rien de doré ni d'orangé", "[ui][minimap]") {
  MinimapProbe probe;
  probe.Settle();
  static_cast<void>(probe.Frame());
  CHECK(CenterOf(probe.Window(), kTombExactTint).count == 0);
  CHECK(CenterOf(probe.Window(), kTombWindowTint).count == 0);
}

namespace {

constexpr std::uint16_t kTombSlot = 42;

// Un clic sur une tombe, Ctrl tenu ou non ; rend ce que le clic a demandé, à
// l'image de l'appui ou à celle du relâchement.
MinimapAnswer ClickTomb(MinimapProbe& probe, ImVec2 at, bool ctrl) {
  ImGuiIO& io = ImGui::GetIO();
  io.AddKeyEvent(ImGuiMod_Ctrl, ctrl);
  io.AddMousePosEvent(at.x, at.y);
  static_cast<void>(probe.Frame());
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
  MinimapAnswer pressed = probe.Frame();
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
  const MinimapAnswer released = probe.Frame();
  io.AddKeyEvent(ImGuiMod_Ctrl, false);
  if (!pressed.guide_to.has_value()) {
    pressed.guide_to = released.guide_to;
  }
  if (!pressed.open_mvp_slot.has_value()) {
    pressed.open_mvp_slot = released.open_mvp_slot;
  }
  return pressed;
}

[[nodiscard]] ImVec2 TombPoint(const MinimapProbe& probe) {
  return probe.CellPoint(static_cast<float>(kExactTombCell.x),
                         static_cast<float>(kExactTombCell.y));
}

}  // namespace

TEST_CASE("Minimap : une tombe se survole, s'ouvre au clic et guide au Ctrl+clic",
          "[ui][minimap]") {
  MinimapProbe probe;
  const std::array<ro::ui::MinimapTomb, 1> tombs = {
      ro::ui::MinimapTomb{.cell = kExactTombCell,
                          .exact = true,
                          .slot_id = kTombSlot,
                          .label = "Baphomet (prt_maze03)",
                          .window_line = "",
                          .age_line = "Signalée il y a 5 min"}};
  probe.view.tombs = tombs;
  probe.Settle();
  const ImVec2 at = TombPoint(probe);

  SECTION("le survol dit le MVP, saute la ligne vide, et annonce les gestes") {
    ImGuiIO& io = ImGui::GetIO();
    io.AddMousePosEvent(at.x, at.y);
    static_cast<void>(probe.Frame());
    static_cast<void>(probe.Frame());
    // Le journal d'ImGui coupe ses lignes par `IM_NEWLINE`.
    const std::string expected = std::string("Baphomet (prt_maze03)") + IM_NEWLINE +
                                 "Signalée il y a 5 min" + IM_NEWLINE +
                                 "Clic : ouvrir le carnet · Ctrl+clic : y guider";
    INFO(probe.logged);
    CHECK(probe.logged.find(expected) != std::string::npos);
  }
  SECTION("le clic ouvre le carnet sur le créneau, sans guider") {
    const MinimapAnswer answer = ClickTomb(probe, at, false);
    REQUIRE(answer.open_mvp_slot.has_value());
    CHECK(*answer.open_mvp_slot == kTombSlot);
    CHECK_FALSE(answer.guide_to.has_value());
  }
  SECTION("le Ctrl+clic guide vers la tombe, sans ouvrir") {
    const MinimapAnswer answer = ClickTomb(probe, at, true);
    REQUIRE(answer.guide_to.has_value());
    CHECK(answer.guide_to->x == kExactTombCell.x);
    CHECK(answer.guide_to->y == kExactTombCell.y);
    CHECK_FALSE(answer.open_mvp_slot.has_value());
  }
}

TEST_CASE("Minimap : une tombe sans créneau ne s'ouvre pas", "[ui][minimap]") {
  MinimapProbe probe;
  const std::array<ro::ui::MinimapTomb, 1> tombs = {
      ro::ui::MinimapTomb{.cell = kExactTombCell, .exact = true, .label = "Baphomet"}};
  probe.view.tombs = tombs;
  probe.Settle();
  const ImVec2 at = TombPoint(probe);
  // L'infobulle n'annonce que le geste qui reste.
  ImGui::GetIO().AddMousePosEvent(at.x, at.y);
  static_cast<void>(probe.Frame());
  static_cast<void>(probe.Frame());
  INFO(probe.logged);
  CHECK(probe.logged.find(std::string("Baphomet") + IM_NEWLINE + "Ctrl+clic : y guider") !=
        std::string::npos);
  CHECK(probe.logged.find("Clic : ouvrir le carnet") == std::string::npos);
  const MinimapAnswer answer = ClickTomb(probe, at, false);
  CHECK_FALSE(answer.open_mvp_slot.has_value());
  CHECK_FALSE(answer.guide_to.has_value());
}

// 🔴 PEINTE PAR-DESSUS, LA TOMBE GARDE LE SURVOL : sur une commodité, le clic
// ouvre le carnet au lieu de guider vers la commodité.
TEST_CASE("Minimap : une tombe posée sur une commodité prend le clic", "[ui][minimap]") {
  MinimapProbe probe;
  const std::array<ro::ui::MinimapTomb, 1> tombs = {
      ro::ui::MinimapTomb{.cell = kKafraCell, .exact = true, .slot_id = kTombSlot}};
  probe.view.tombs = tombs;
  probe.Settle();
  const MinimapAnswer answer = ClickTomb(
      probe, probe.CellPoint(static_cast<float>(kKafraCell.x), static_cast<float>(kKafraCell.y)),
      false);
  REQUIRE(answer.open_mvp_slot.has_value());
  CHECK(*answer.open_mvp_slot == kTombSlot);
  CHECK_FALSE(answer.guide_to.has_value());
}

// La fenêtre traversante ne reçoit aucun clic : ce qui s'éprouve ici, c'est que
// la tombe reste PEINTE quand ses boutons ne sont pas posés.
TEST_CASE("Minimap : en mode traversant, une tombe se dessine encore", "[ui][minimap]") {
  MinimapProbe probe;
  probe.settings.click_through = true;
  const std::array<ro::ui::MinimapTomb, 1> tombs = {
      ro::ui::MinimapTomb{.cell = kExactTombCell, .exact = true, .slot_id = kTombSlot}};
  probe.view.tombs = tombs;
  probe.Settle();
  const MinimapAnswer answer = ClickTomb(probe, TombPoint(probe), false);
  CHECK(CenterOf(probe.Window(), kTombExactTint).count > 0);
  CHECK_FALSE(answer.open_mvp_slot.has_value());
  CHECK_FALSE(answer.guide_to.has_value());
}

// ── Les marques de quête ────────────────────────────────────────────────────

namespace {

// Les textures des marques : une par couleur, puis les quatre flèches.
constexpr ImTextureID kYellowQuestTexture = 41;
constexpr ImTextureID kGreenQuestTexture = 42;
constexpr std::array<ImTextureID, ro::ui::kMinimapQuestArrowCount> kQuestArrowTextures = {51, 52,
                                                                                          53, 54};
constexpr float kQuestIconPixels = 8.0F;
constexpr MinimapCell kInsideQuestCell{.x = 30, .y = 70};
// À droite de la carte, et au-dessus d'elle : hors du cadrage.
constexpr MinimapCell kEastQuestCell{.x = 150, .y = 50};
constexpr MinimapCell kNorthQuestCell{.x = 50, .y = 150};
constexpr std::size_t kRightArrow = 1;
constexpr std::size_t kUpArrow = 2;

[[nodiscard]] ro::ui::SkinImage QuestImage(ImTextureID id) {
  return {.id = id, .width = kQuestIconPixels, .height = kQuestIconPixels};
}

// Le rectangle couvert par une texture posée, ou rien.
//
// ⚠ PAS `GetTexID()` : il assertionne sur la texture de la police, qu'aucun
// moteur n'a chargée dans un banc.
[[nodiscard]] std::optional<std::array<ImVec2, 2>> Covered(const ImGuiWindow& window,
                                                           ImTextureID id) {
  std::optional<std::array<ImVec2, 2>> covered;
  const ImDrawList& list = *window.DrawList;
  for (const ImDrawCmd& command : list.CmdBuffer) {
    if (command.UserCallback != nullptr || command.TexRef._TexData != nullptr ||
        command.TexRef._TexID != id) {
      continue;
    }
    for (unsigned rank = 0; rank < command.ElemCount; ++rank) {
      const ImVec2 at = list.VtxBuffer[static_cast<int>(command.VtxOffset) +
                                       list.IdxBuffer[static_cast<int>(command.IdxOffset + rank)]]
                            .pos;
      if (!covered.has_value()) {
        covered = std::array<ImVec2, 2>{at, at};
      }
      (*covered)[0] = ImVec2(std::min((*covered)[0].x, at.x), std::min((*covered)[0].y, at.y));
      (*covered)[1] = ImVec2(std::max((*covered)[1].x, at.x), std::max((*covered)[1].y, at.y));
    }
  }
  return covered;
}

}  // namespace

TEST_CASE("Minimap : une marque de quête montre l'icône de SA couleur, à sa case",
          "[ui][minimap]") {
  MinimapProbe probe;
  const std::array<ro::ui::MinimapQuest, 1> quests = {
      ro::ui::MinimapQuest{.cell = kInsideQuestCell, .image = QuestImage(kYellowQuestTexture)}};
  probe.view.quests = quests;
  probe.Settle();
  static_cast<void>(probe.Frame());
  const auto covered = Covered(probe.Window(), kYellowQuestTexture);
  REQUIRE(covered.has_value());
  const ImVec2 cell = probe.CellPoint(static_cast<float>(kInsideQuestCell.x),
                                      static_cast<float>(kInsideQuestCell.y));
  CHECK(((*covered)[0].x + (*covered)[1].x) * 0.5F == Catch::Approx(cell.x));
  CHECK(((*covered)[0].y + (*covered)[1].y) * 0.5F == Catch::Approx(cell.y));
  // Huit pixels de côté, à l'échelle un de l'interface.
  CHECK((*covered)[1].x - (*covered)[0].x == Catch::Approx(kQuestIconPixels));
}

TEST_CASE("Minimap : hors du cadrage, la marque se colle au bord sous sa flèche", "[ui][minimap]") {
  MinimapProbe probe;
  for (std::size_t arrow = 0; arrow < kQuestArrowTextures.size(); ++arrow) {
    probe.view.quest_arrows[arrow] = QuestImage(kQuestArrowTextures[arrow]);
  }
  const std::array<ro::ui::MinimapQuest, 2> quests = {
      ro::ui::MinimapQuest{.cell = kEastQuestCell, .image = QuestImage(kYellowQuestTexture)},
      ro::ui::MinimapQuest{.cell = kNorthQuestCell, .image = QuestImage(kGreenQuestTexture)}};
  probe.view.quests = quests;
  probe.Settle();
  static_cast<void>(probe.Frame());
  // 🔴 LA COULEUR SE PERD : ni l'une ni l'autre icône n'est peinte, la flèche
  // de la direction l'est à leur place.
  CHECK_FALSE(Covered(probe.Window(), kYellowQuestTexture).has_value());
  CHECK_FALSE(Covered(probe.Window(), kGreenQuestTexture).has_value());
  const auto east = Covered(probe.Window(), kQuestArrowTextures[kRightArrow]);
  const auto north = Covered(probe.Window(), kQuestArrowTextures[kUpArrow]);
  REQUIRE(east.has_value());
  REQUIRE(north.has_value());
  // Collées au bord, DANS le cadre : à droite pour l'une, en haut pour l'autre.
  const ImVec2 origin = probe.Window().DC.CursorStartPos;
  CHECK((*east)[1].x == Catch::Approx(origin.x + kSidePixels));
  CHECK((*north)[0].y == Catch::Approx(origin.y));
  // Et à la hauteur, ou à l'abscisse, de leur case.
  const ImVec2 east_cell =
      probe.CellPoint(static_cast<float>(kEastQuestCell.x), static_cast<float>(kEastQuestCell.y));
  CHECK(((*east)[0].y + (*east)[1].y) * 0.5F == Catch::Approx(east_cell.y));
  // Les deux autres flèches ne servent pas.
  CHECK_FALSE(Covered(probe.Window(), kQuestArrowTextures[0]).has_value());
  CHECK_FALSE(Covered(probe.Window(), kQuestArrowTextures[3]).has_value());
}

// ── L'ÉCRAN : LA MINIMAP Y TIENT TOUJOURS ENTIÈRE ──────────────────────────
//
// 🔴 UN CLIENT NEUF voit d'abord un écran que la fenêtre ne connaît pas encore
// — nul, ou d'une autre taille que celui qui suit. La place et le côté de cette
// première image ne doivent ni rester ni s'écrire dans les réglages.

namespace {

constexpr ro::ui::Viewport kNoScreen{.width_px = 0.0F, .height_px = 0.0F};
constexpr ro::ui::Viewport kWideScreen{.width_px = 1920.0F, .height_px = 1080.0F};
constexpr ro::ui::Viewport kSmallScreen{.width_px = 640.0F, .height_px = 480.0F};
constexpr ro::ui::Viewport kTinyScreen{.width_px = 300.0F, .height_px = 200.0F};
// Une place choisie qui tient sur le grand écran, pas sur celui de l'essai.
constexpr int kChosenX = 1600;
constexpr int kChosenY = 700;
// Une place choisie qui tient sur l'écran de l'essai.
constexpr int kNearX = 100;
constexpr int kNearY = 50;
// Le glisser et la poignée : le point saisi, en cellules, loin des icônes, du
// bouton de réglages et des poignées ; puis le chemin parcouru.
constexpr float kGrabCellX = 85.0F;
constexpr float kGrabCellY = 15.0F;
constexpr ImVec2 kDragPixels{-300.0F, 100.0F};
constexpr float kGrowPixels = 40.0F;
// Le point saisi dans la poignée bas-droite, en retrait du coin.
constexpr float kGripInsetPixels = 4.0F;
constexpr int kGestureSteps = 4;

[[nodiscard]] bool WithinScreen(const ImGuiWindow& window, ro::ui::Viewport screen) {
  return window.Pos.x >= 0.0F && window.Pos.y >= 0.0F &&
         window.Pos.x + window.Size.x <= screen.width_px &&
         window.Pos.y + window.Size.y <= screen.height_px;
}

// Le coin haut-droit d'une minimap jamais glissée, à l'échelle 1.
[[nodiscard]] bool AtTopRight(const ImGuiWindow& window, ro::ui::Viewport screen) {
  return window.Pos.x + window.Size.x ==
             Catch::Approx(screen.width_px - ro::ui::kMinimapScreenMargin) &&
         window.Pos.y == Catch::Approx(ro::ui::kMinimapScreenMargin);
}

[[nodiscard]] bool Unplaced(const ro::ui::MinimapSettings& settings) {
  return ro::ui::MinimapUnplaced(settings);
}

// Bouton gauche pressé en `from`, mené jusqu'à `from + by`, relâché.
void Drag(MinimapProbe& probe, ImVec2 from, ImVec2 by) {
  ImGuiIO& io = ImGui::GetIO();
  io.AddMousePosEvent(from.x, from.y);
  static_cast<void>(probe.Frame());
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
  static_cast<void>(probe.Frame());
  for (int step = 1; step <= kGestureSteps; ++step) {
    const float done = static_cast<float>(step) / static_cast<float>(kGestureSteps);
    io.AddMousePosEvent(from.x + by.x * done, from.y + by.y * done);
    static_cast<void>(probe.Frame());
  }
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
  probe.Settle();
}

}  // namespace

TEST_CASE("Minimap : un écran inconnu à la première image ne la sort pas de l'écran",
          "[ui][minimap]") {
  MinimapProbe probe;
  probe.settings = ro::ui::MinimapSettings{};
  probe.viewport = kNoScreen;
  probe.Settle();
  // Dès la première image où l'écran est connu.
  probe.viewport = kViewport;
  static_cast<void>(probe.Frame());
  CHECK(WithinScreen(probe.Window(), kViewport));
  CHECK(AtTopRight(probe.Window(), kViewport));
  probe.Settle();
  CHECK(AtTopRight(probe.Window(), kViewport));
  // Rien de cette première image n'est devenu un choix du joueur.
  CHECK(Unplaced(probe.settings));
  CHECK(probe.settings.side_px == ro::ui::MinimapSettings{}.side_px);
}

TEST_CASE("Minimap : un écran redimensionné la garde entière, à son coin", "[ui][minimap]") {
  MinimapProbe probe;
  probe.settings = ro::ui::MinimapSettings{};
  probe.viewport = kWideScreen;
  probe.Settle();
  REQUIRE(AtTopRight(probe.Window(), kWideScreen));

  for (const ro::ui::Viewport screen : {kViewport, kSmallScreen, kTinyScreen}) {
    probe.viewport = screen;
    // À l'image même du redimensionnement, pas seulement une fois posée.
    static_cast<void>(probe.Frame());
    CHECK(WithinScreen(probe.Window(), screen));
    probe.Settle();
    CHECK(WithinScreen(probe.Window(), screen));
    CHECK(AtTopRight(probe.Window(), screen));
  }

  // Revenue sur le grand écran, elle reprend son coin ET son côté.
  probe.viewport = kWideScreen;
  probe.Settle();
  CHECK(AtTopRight(probe.Window(), kWideScreen));
  const ImVec2 padding = ImGui::GetStyle().WindowPadding;
  CHECK(probe.Window().Size.x - 2.0F * padding.x ==
        Catch::Approx(static_cast<float>(ro::ui::MinimapSettings{}.side_px)));
  CHECK(Unplaced(probe.settings));
  CHECK(probe.settings.side_px == ro::ui::MinimapSettings{}.side_px);
}

TEST_CASE("Minimap : une place choisie se garde, et se montre ramenée quand elle ne tient plus",
          "[ui][minimap]") {
  MinimapProbe probe;
  probe.settings.offset_x_px = static_cast<float>(kNearX);
  probe.settings.offset_y_px = static_cast<float>(kNearY);
  probe.Settle();
  CHECK(probe.Window().Pos.x == Catch::Approx(static_cast<float>(kNearX)));
  CHECK(probe.Window().Pos.y == Catch::Approx(static_cast<float>(kNearY)));

  // Relue d'un fichier écrit sur un plus grand écran : montrée dedans, gardée.
  probe.settings.offset_x_px = static_cast<float>(kChosenX);
  probe.settings.offset_y_px = static_cast<float>(kChosenY);
  probe.Settle();
  CHECK(WithinScreen(probe.Window(), kViewport));
  // ⚠ ET EN TRAVERSANT LES CLICS, où la fenêtre ne se déplace jamais d'elle-même.
  probe.settings.click_through = true;
  probe.Settle();
  CHECK(WithinScreen(probe.Window(), kViewport));
  CHECK(probe.settings.offset_x_px == static_cast<float>(kChosenX));
  CHECK(probe.settings.offset_y_px == static_cast<float>(kChosenY));

  // Le grand écran la rend à sa place.
  probe.viewport = kWideScreen;
  probe.Settle();
  CHECK(probe.Window().Pos.x == Catch::Approx(static_cast<float>(kChosenX)));
  CHECK(probe.Window().Pos.y == Catch::Approx(static_cast<float>(kChosenY)));
}

TEST_CASE("Minimap : le glisser et la poignée du joueur s'écrivent et restent", "[ui][minimap]") {
  MinimapProbe probe;
  probe.settings = ro::ui::MinimapSettings{};
  probe.Settle();
  const ImVec2 before = probe.Window().Pos;

  Drag(probe, probe.CellPoint(kGrabCellX, kGrabCellY), kDragPixels);
  CHECK_FALSE(Unplaced(probe.settings));
  CHECK(probe.Window().Pos.x == Catch::Approx(before.x + kDragPixels.x));
  CHECK(probe.Window().Pos.y == Catch::Approx(before.y + kDragPixels.y));

  // La poignée bas-droite, tirée en diagonale : le carré grandit d'autant.
  const ImGuiWindow& window = probe.Window();
  const ImVec2 grip(window.Pos.x + window.Size.x - kGripInsetPixels,
                    window.Pos.y + window.Size.y - kGripInsetPixels);
  const float side_before = static_cast<float>(probe.settings.side_px);
  Drag(probe, grip, ImVec2(kGrowPixels, kGrowPixels));
  CHECK(static_cast<float>(probe.settings.side_px) ==
        Catch::Approx(side_before + kGrowPixels).margin(1.0F));
  const ImVec2 padding = ImGui::GetStyle().WindowPadding;
  CHECK(probe.Window().Size.x - 2.0F * padding.x ==
        Catch::Approx(static_cast<float>(probe.settings.side_px)).margin(1.0F));
}
