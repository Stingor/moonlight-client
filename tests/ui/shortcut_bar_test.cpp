// La géométrie des barres de raccourcis : ce qui décide de la taille d'une
// fenêtre et de la place de chaque case.
//
// ⚠ CES RÉGLAGES VIENNENT D'UN FICHIER QUE LE JOUEUR PEUT ÉCRIRE. Zéro colonne
// divise par zéro au placement, vingt cases débordent sur la barre suivante :
// les bornes ne sont pas un confort, et elles ne se voient pas à la relecture.

#include "ro/ui/shortcut_bar.h"

#include <array>
#include <cstddef>
#include <cstdint>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/ui_context.h"

using ro::ui::CenteredBarLeft;
using ro::ui::GridOfBar;
using ro::ui::HudRect;
using ro::ui::HudSiblings;
using ro::ui::kShortcutBarCount;
using ro::ui::kShortcutSlotsPerBar;
using ro::ui::kShortcutSlotTotal;
using ro::ui::ShortcutBarGrid;
using ro::ui::ShortcutBarLayout;
using ro::ui::ShortcutBarsLayout;
using ro::ui::ShortcutSlotIndex;
using ro::ui::ShortcutSlotUnder;
using ro::ui::SnappedAmongBars;

TEST_CASE("Barres : une rangée pleine tient sur une ligne", "[ui][barre]") {
  const ShortcutBarGrid grid = GridOfBar(ShortcutBarLayout{});
  CHECK(grid.shown == kShortcutSlotsPerBar);
  CHECK(grid.columns == kShortcutSlotsPerBar);
  CHECK(grid.rows == 1);
  CHECK(grid.across() == kShortcutSlotsPerBar);
}

TEST_CASE("Barres : la dernière ligne peut être incomplète", "[ui][barre]") {
  // Sept cases sur quatre colonnes font DEUX lignes, pas une et trois quarts.
  const ShortcutBarGrid grid = GridOfBar(ShortcutBarLayout{.columns = 4, .shown_slots = 7});
  CHECK(grid.rows == 2);
  CHECK(grid.across() == 4);
}

TEST_CASE("Barres : une seule colonne fait une barre VERTICALE", "[ui][barre]") {
  // ⚠ RIEN DE SPÉCIAL À ÉCRIRE POUR ÇA, et c'est l'intérêt : la barre verticale
  // est la grille à une colonne, donc elle est gratuite.
  const ShortcutBarGrid grid = GridOfBar(ShortcutBarLayout{.columns = 1, .shown_slots = 6});
  CHECK(grid.columns == 1);
  CHECK(grid.rows == 6);
  CHECK(grid.across() == 1);
}

TEST_CASE("Barres : une largeur plus grande que le compte ne ment pas", "[ui][barre]") {
  // Trois cases sur douze colonnes tiennent sur TROIS cases de large : une
  // fenêtre taillée sur douze laisserait neuf cases de vide à cliquer.
  const ShortcutBarGrid grid = GridOfBar(ShortcutBarLayout{.columns = 12, .shown_slots = 3});
  CHECK(grid.rows == 1);
  CHECK(grid.across() == 3);
}

TEST_CASE("Barres : les réglages hors bornes sont ramenés", "[ui][barre]") {
  // 🔴 ZÉRO COLONNE DIVISERAIT PAR ZÉRO au placement, et vingt cases
  // déborderaient sur la barre suivante. Le fichier de réglages se corrige à la
  // main : ces valeurs-là arriveront.
  const ShortcutBarGrid none = GridOfBar(ShortcutBarLayout{.columns = 0, .shown_slots = 0});
  CHECK(none.columns == 1);
  CHECK(none.shown == 1);
  CHECK(none.rows == 1);

  const ShortcutBarGrid huge = GridOfBar(ShortcutBarLayout{.columns = 99, .shown_slots = 99});
  CHECK(huge.columns == kShortcutSlotsPerBar);
  CHECK(huge.shown == kShortcutSlotsPerBar);
  CHECK(huge.rows == 1);

  const ShortcutBarGrid below = GridOfBar(ShortcutBarLayout{.columns = -4, .shown_slots = -1});
  CHECK(below.columns == 1);
  CHECK(below.shown == 1);
}

TEST_CASE("Barres : l'index d'une case est GLOBAL", "[ui][barre]") {
  // 🔴 C'EST CE QUE LA DEMANDE PORTE, et le seul moyen qu'une case désigne la
  // même chose des deux côtés. Un rang dans sa barre ferait pointer la case 3 de
  // la barre 5 sur la case 3 de la première.
  CHECK(ShortcutSlotIndex(0, 0) == 0);
  CHECK(ShortcutSlotIndex(0, kShortcutSlotsPerBar - 1) == kShortcutSlotsPerBar - 1);
  CHECK(ShortcutSlotIndex(1, 0) == kShortcutSlotsPerBar);
  CHECK(ShortcutSlotIndex(kShortcutBarCount - 1, kShortcutSlotsPerBar - 1) ==
        kShortcutSlotTotal - 1);
}

TEST_CASE("Barres : les huit partent avec la même apparence", "[ui][barre]") {
  // ⚠ CE QUE CET ESSAI TIENT : les défauts vivent désormais dans l'en-tête,
  // parce que le fichier de réglages lit la valeur présente à la déclaration.
  // Un défaut recopié dans le dessin en ferait une seconde source, et le jour où
  // les deux divergeraient, c'est la moitié des joueurs — ceux dont le fichier
  // n'a pas la clé — qui verrait l'autre.
  const ShortcutBarsLayout layout;
  for (const ShortcutBarLayout& bar : layout.bars) {
    CHECK(bar.empty_color == ro::ui::kDefaultSlotEmptyColor);
    CHECK(bar.border_color == ro::ui::kDefaultSlotBorderColor);
    CHECK(bar.border_thickness == ro::ui::kDefaultSlotBorderThickness);
    CHECK(bar.key_color == ro::ui::kDefaultSlotLabelColor);
    CHECK(bar.count_color == ro::ui::kDefaultSlotLabelColor);
    CHECK(bar.countdown_color == ro::ui::kDefaultCountdownColor);
    CHECK(bar.shadow_color == ro::ui::kDefaultSlotShadowColor);
    // Le contour par défaut, comme le jeu : lisible sur n'importe quelle icône.
    CHECK(bar.shadow_kind == static_cast<int>(ro::ui::SlotTextShadow::kHalo));
    CHECK(bar.key_scale == 1.0F);
    CHECK(bar.count_scale == 1.0F);
  }
}

TEST_CASE("Barres : une case sans cadre reste possible", "[ui][barre]") {
  // ⚠ ZÉRO EST UNE VALEUR UTILE, pas une valeur à border : des cases jointives
  // sans bordure est une disposition que des joueurs préfèrent. Le dessin la
  // lit comme « ne rien peindre », et c'est ce que la borne basse du curseur
  // doit laisser atteindre.
  CHECK(ro::ui::kLargestBorderThickness > ro::ui::kDefaultSlotBorderThickness);
  ShortcutBarLayout bar;
  bar.border_thickness = 0.0F;
  CHECK(bar.border_thickness == 0.0F);
}

TEST_CASE("Barres : seule la première est montrée au départ", "[ui][barre]") {
  // Huit barres vides à l'écran d'une première partie diraient que le client est
  // cassé. Celle-ci est celle du jeu, avec ses touches.
  const ShortcutBarsLayout layout;
  CHECK(layout.bars[0].shown);
  for (std::size_t bar = 1; bar < kShortcutBarCount; ++bar) {
    CHECK_FALSE(layout.bars[bar].shown);
  }
  CHECK(layout.show_keys);
  // L'aimant entre barres est allumé d'office, comme celui des jauges.
  CHECK(layout.sticky);
}

// ── LA PLACE D'UNE BARRE ─────────────────────────────────────────────────────

TEST_CASE("Barres : centrer pose la barre au milieu de la largeur", "[ui][barre]") {
  constexpr float kScreenWidth = 1920.0F;
  constexpr float kBarWidth = 400.0F;
  CHECK(CenteredBarLeft(0.0F, kScreenWidth, kBarWidth) == 760.0F);
  // L'écran commence où le cadre de la vue commence, et un demi-pixel se tronque :
  // une barre sur un demi-pixel floute ses icônes.
  CHECK(CenteredBarLeft(100.0F, 1000.0F, 301.0F) == 449.0F);
}

TEST_CASE("Barres : une barre traînée s'accroche à ses sœurs affichées", "[ui][barre]") {
  std::array<HudRect, 2> frames{HudRect{.x = 100.0F, .y = 500.0F, .width = 200.0F, .height = 40.0F},
                                HudRect{}};
  std::array<bool, 2> shown{true, true};
  const HudSiblings bars{.frames = frames, .shown = shown, .mine = 1};
  const ImVec2 size(100.0F, 40.0F);

  SECTION("bout à bout, et au même haut") {
    const ImVec2 posed = SnappedAmongBars(ImVec2(306.0F, 503.0F), size, bars);
    CHECK(posed.x == 300.0F);
    CHECK(posed.y == 500.0F);
  }
  SECTION("au même bord gauche") {
    const ImVec2 posed = SnappedAmongBars(ImVec2(96.0F, 300.0F), size, bars);
    CHECK(posed.x == 100.0F);
    CHECK(posed.y == 300.0F);
  }
  SECTION("trop loin, rien ne bouge") {
    const ImVec2 posed = SnappedAmongBars(ImVec2(700.0F, 200.0F), size, bars);
    CHECK(posed.x == 700.0F);
    CHECK(posed.y == 200.0F);
  }
  SECTION("une barre éteinte n'aimante rien") {
    shown[0] = false;
    const ImVec2 posed = SnappedAmongBars(ImVec2(306.0F, 503.0F), size, bars);
    CHECK(posed.x == 306.0F);
    CHECK(posed.y == 503.0F);
  }
}

// ── CE QUE LA SOURIS TRAVERSE ────────────────────────────────────────────────
//
// 🔴 CE CALCUL DÉCIDE DE CE QUE LA BARRE PREND AU JEU. Faux d'une gouttière, il
// avale un clic de déplacement entre deux icônes, ou rend sourde une case qui
// devait lancer. Rien de tout cela ne se voit à la relecture : cela se mesure à
// la souris, en jeu, case par case — sauf ici.

TEST_CASE("Barres : le centre d'une case rend son rang", "[ui][barre]") {
  const ShortcutBarGrid grid = GridOfBar(ShortcutBarLayout{.columns = 4, .shown_slots = 8});
  // Côté 32, écart 2 : la deuxième case commence à 34, la deuxième ligne aussi.
  CHECK(ShortcutSlotUnder(grid, 16.0F, 16.0F, 32.0F, 2.0F) == std::size_t{0});
  CHECK(ShortcutSlotUnder(grid, 50.0F, 16.0F, 32.0F, 2.0F) == std::size_t{1});
  CHECK(ShortcutSlotUnder(grid, 16.0F, 50.0F, 32.0F, 2.0F) == std::size_t{4});
  CHECK(ShortcutSlotUnder(grid, 118.0F, 50.0F, 32.0F, 2.0F) == std::size_t{7});
}

TEST_CASE("Barres : l'écart entre deux cases n'est pas une case", "[ui][barre]") {
  // Sans le reste, une barre espacée prendrait la souris sur toute sa surface —
  // et le clic de déplacement se perdrait entre deux icônes.
  const ShortcutBarGrid grid = GridOfBar(ShortcutBarLayout{.columns = 4, .shown_slots = 8});
  CHECK_FALSE(ShortcutSlotUnder(grid, 33.0F, 16.0F, 32.0F, 2.0F).has_value());
  CHECK_FALSE(ShortcutSlotUnder(grid, 16.0F, 33.0F, 32.0F, 2.0F).has_value());
}

TEST_CASE("Barres : sans espacement, les cases sont jointives", "[ui][barre]") {
  // ⚠ LE CAS OÙ IL N'Y A PLUS DE GOUTTIÈRE : le reste vaut alors toujours moins
  // que le côté, et aucun point de la grille ne doit se perdre.
  const ShortcutBarGrid grid = GridOfBar(ShortcutBarLayout{.columns = 4, .shown_slots = 8});
  CHECK(ShortcutSlotUnder(grid, 31.9F, 0.0F, 32.0F, 0.0F) == std::size_t{0});
  CHECK(ShortcutSlotUnder(grid, 32.0F, 0.0F, 32.0F, 0.0F) == std::size_t{1});
}

TEST_CASE("Barres : à droite de la dernière colonne, rien", "[ui][barre]") {
  // 🔴 L'ERREUR QUI PASSERAIT INAPERÇUE : sans garde de colonne, le point posé
  // juste après la dernière colonne retombe sur la PREMIÈRE case de la ligne
  // suivante, qui est une vraie case. La barre répondrait donc à côté d'elle.
  const ShortcutBarGrid grid = GridOfBar(ShortcutBarLayout{.columns = 4, .shown_slots = 8});
  CHECK_FALSE(ShortcutSlotUnder(grid, 150.0F, 16.0F, 32.0F, 2.0F).has_value());
}

TEST_CASE("Barres : hors de la barre, rien", "[ui][barre]") {
  const ShortcutBarGrid grid = GridOfBar(ShortcutBarLayout{.columns = 4, .shown_slots = 8});
  CHECK_FALSE(ShortcutSlotUnder(grid, -1.0F, 16.0F, 32.0F, 2.0F).has_value());
  CHECK_FALSE(ShortcutSlotUnder(grid, 16.0F, -1.0F, 32.0F, 2.0F).has_value());
  // Au-delà des cases MONTRÉES : la neuvième existe dans la barre, mais elle
  // n'est pas dessinée — rien ne l'y attend.
  CHECK_FALSE(ShortcutSlotUnder(grid, 16.0F, 84.0F, 32.0F, 2.0F).has_value());
}

TEST_CASE("Barres : une case traversante est un réglage PAR BARRE", "[ui][barre]") {
  // Le verrou est commun aux huit ; celui-ci ne l'est pas. Une barre de parade
  // que l'on regarde sans y toucher n'a rien à prendre à la souris, pendant que
  // celle des soins doit répondre au clic.
  const ShortcutBarsLayout layout;
  for (std::size_t bar = 0; bar < kShortcutBarCount; ++bar) {
    CHECK_FALSE(layout.bars[bar].click_through);
  }
}

// ── LE BLOC CTRL ─────────────────────────────────────────────────────────────
//
// 🔴 CTRL + DÉPLACEMENT EMMÈNE LES BARRES QUI SE TOUCHENT, comme les cadres de
// HUD. Ce sont des fenêtres qu'ImGui déplace lui-même : le geste ne s'éprouve
// qu'en traînant pour de bon, image par image.

namespace {

constexpr ro::ui::Viewport kBlockViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kBlockFrameSeconds = 1.0F / 60.0F;
constexpr int kBlockSettleFrames = 4;
constexpr int kBlockDragSteps = 5;
constexpr float kBlockStepX = 10.0F;
constexpr float kBlockStepY = -6.0F;

struct BarBlockProbe {
  ro::ui::UiContext ui;
  ro::ui::SkinArt art;
  ShortcutBarsLayout layout;
  std::array<ro::ui::ShortcutSlotView, kShortcutSlotTotal> slots{};

  BarBlockProbe() {
    layout.locked = false;
    layout.bars[1].shown = true;
    for (int frame = 0; frame < kBlockSettleFrames; ++frame) {
      Frame();
    }
  }

  ro::ui::ShortcutBarsAction last;

  void Frame() {
    ui.BeginFrame(kBlockViewport, kBlockFrameSeconds);
    // L'ordre du jeu : les barres entre elles, puis les fenêtres entre elles.
    ro::ui::SnapMovingWindow(ro::ui::AlignGrid{}, ro::ui::SnapMovingShortcutBar(layout));
    last = ro::ui::DrawShortcutBars(layout, slots, art);
    ui.EndFrame();
  }

  [[nodiscard]] static ImGuiWindow& Bar(const char* id) {
    ImGuiWindow* window = ImGui::FindWindowByName(id);
    REQUIRE(window != nullptr);
    return *window;
  }

  // Traîne la première barre depuis son centre, Ctrl tenu ou non.
  void Drag(bool ctrl) {
    ImGuiIO& io = ImGui::GetIO();
    const ImGuiWindow& first = Bar("###shortcut_bar");
    ImVec2 mouse(first.Pos.x + first.Size.x * 0.5F, first.Pos.y + first.Size.y * 0.5F);
    io.AddMousePosEvent(mouse.x, mouse.y);
    Frame();
    io.AddKeyEvent(ImGuiMod_Ctrl, ctrl);
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
    Frame();
    for (int step = 0; step < kBlockDragSteps; ++step) {
      mouse = ImVec2(mouse.x + kBlockStepX, mouse.y + kBlockStepY);
      io.AddMousePosEvent(mouse.x, mouse.y);
      Frame();
    }
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
    io.AddKeyEvent(ImGuiMod_Ctrl, false);
    Frame();
  }
};

}  // namespace

TEST_CASE("Barres : CTRL emmène en bloc la barre qui touche", "[ui][barre]") {
  BarBlockProbe probe;
  ImGuiWindow& first = BarBlockProbe::Bar("###shortcut_bar");
  ImGuiWindow& second = BarBlockProbe::Bar("###shortcut_bar2");
  // La seconde posée juste au-dessus de la première : elles se touchent.
  ImGui::SetWindowPos(&second, ImVec2(first.Pos.x, first.Pos.y - second.Size.y), ImGuiCond_Always);
  probe.Frame();
  const ImVec2 first_before = first.Pos;
  const ImVec2 second_before = second.Pos;

  SECTION("Ctrl tenu, les deux partent du même écart") {
    probe.Drag(true);
    const float moved_x = first.Pos.x - first_before.x;
    const float moved_y = first.Pos.y - first_before.y;
    CHECK(moved_x != 0.0F);
    // ⚠ LA SŒUR QUI SUIT N'EST PAS UNE VOISINE : prise pour cible, elle
    // retiendrait la barre saisie contre sa place de l'image d'avant.
    CHECK(moved_y == kBlockStepY * static_cast<float>(kBlockDragSteps));
    CHECK(second.Pos.x - second_before.x == moved_x);
    CHECK(second.Pos.y - second_before.y == moved_y);
  }
  SECTION("sans Ctrl, la sœur reste où elle est") {
    probe.Drag(false);
    CHECK(first.Pos.x != first_before.x);
    CHECK(second.Pos.x == second_before.x);
    CHECK(second.Pos.y == second_before.y);
  }
}

// ── LE CLIC DROIT DÉCRIT ─────────────────────────────────────────────────────
//
// 🔴 UNE COMPÉTENCE AUSSI, et sans Ctrl : c'est le geste du jeu sur sa barre.
// Une case qui ne rendait rien obligeait à passer par le grimoire.

namespace {

// Un clic droit enfoncé puis relâché dans la première case de la première
// barre, verrouillée ; rend la case dont la description est demandée.
int RightClickFirstSlot(const ro::ui::ShortcutSlotView& slot) {
  BarBlockProbe probe;
  probe.layout.locked = true;
  probe.slots[0] = slot;
  probe.Frame();
  ImGuiIO& io = ImGui::GetIO();
  const ImVec2 inside = BarBlockProbe::Bar("###shortcut_bar").DC.CursorStartPos;
  io.AddMousePosEvent(inside.x + 2.0F, inside.y + 2.0F);
  probe.Frame();
  io.AddMouseButtonEvent(ImGuiMouseButton_Right, true);
  probe.Frame();
  const int asked = probe.last.describe;
  io.AddMouseButtonEvent(ImGuiMouseButton_Right, false);
  probe.Frame();
  return asked;
}

}  // namespace

TEST_CASE("Barres : le clic droit décrit ce que la case porte", "[ui][barre]") {
  constexpr std::uint16_t kHealSkillId = 28;
  constexpr std::uint32_t kRedPotionItemId = 501;
  CHECK(RightClickFirstSlot(ro::ui::ShortcutSlotView{.skill_id = kHealSkillId}) == 0);
  CHECK(RightClickFirstSlot(ro::ui::ShortcutSlotView{.item_id = kRedPotionItemId}) == 0);
  CHECK(RightClickFirstSlot(ro::ui::ShortcutSlotView{}) < 0);
}
