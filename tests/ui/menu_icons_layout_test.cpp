// LES ICÔNES DU MENU COMME CADRES LIBRES : leur case d'origine, puis ce que la
// souris leur fait, sur de vraies images ImGui — cliquées verrouillées,
// déplacées déverrouillées, jamais retaillées, aimantées entre elles.

#include <algorithm>
#include <cstddef>
#include <optional>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/hud_frame.h"
#include "ro/ui/menu_icons.h"
#include "ro/ui/ui_context.h"

namespace {

using ro::ui::MenuCommand;

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;

// Le rang du statut, du sac et de l'équipement parmi les icônes montrées.
constexpr std::size_t kStatusRank = 0;
constexpr std::size_t kEquipmentRank = 1;

// Loin de toute icône et de tout bord : là où aucun aimant n'atteint.
constexpr ImVec2 kFarAway(300.0F, 400.0F);

// Le haut de la première icône accrochée, sous le bas de la fenêtre : la grille
// commence quatre pixels plus haut que lui.
constexpr float kDockedIconTop = ro::ui::kMenuIconsDockTop - ro::ui::kMenuGridAboveDock;

// UNE GRILLE D'ICÔNES QUI TOURNE : un vrai contexte ImGui, sans GPU.
//
// ⚠ AUCUN BITMAP : chaque icône prend le carré de secours, de taille connue.
// C'est le carré, pas le dessin, qui se déplace et se clique.
struct MenuBench {
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art;
  const ro::ui::AlignGrid grid;
  ro::ui::HudGrip grip;
  ro::ui::MenuIconsState state;
  ro::ui::MenuIconsLook look;
  std::vector<ro::ui::MenuIconView> icons;
  // Les cases de la grille : le bouton hors grille n'en prend pas.
  std::size_t grid_count = 0;
  std::optional<MenuCommand> clicked;
  bool moved = false;

  // `dock` : la grille accrochée et ouverte DÈS LA PREMIÈRE IMAGE, comme au
  // lancement d'un jeu dont la fenêtre « Basic Info » est déjà allumée.
  explicit MenuBench(std::optional<ImVec2> dock = std::nullopt) {
    state.dock = dock;
    state.grid_open = dock.has_value();
    for (const ro::ui::MenuIconEntry& entry : ro::ui::MenuIconCatalogue()) {
      if (entry.shown) {
        icons.push_back(ro::ui::MenuIconView{.command = entry.command, .ported = true});
        grid_count += entry.off_grid ? 0U : 1U;
      }
    }
    Frame();  // les cadres prennent leur place
  }

  void Frame() {
    ui.BeginFrame(kViewport, kFrameSeconds);
    const ro::ui::MenuIconsAnswer answer =
        ro::ui::DrawMenuIcons(state, icons, look, grid, grip, art);
    ui.EndFrame();
    if (answer.clicked.has_value()) {
      clicked = answer.clicked;
    }
    moved = moved || answer.moved;
  }

  // ⚠ LA SOURIS SE POSE AVANT L'IMAGE : ImGui décide du survol en tête de
  // `NewFrame`, depuis la file d'évènements.
  void MoveTo(ImVec2 where) {
    ImGui::GetIO().AddMousePosEvent(where.x, where.y);
    Frame();
  }
  void Press() {
    ImGui::GetIO().AddMouseButtonEvent(ImGuiMouseButton_Left, true);
    Frame();
  }
  void Release() {
    ImGui::GetIO().AddMouseButtonEvent(ImGuiMouseButton_Left, false);
    Frame();
  }
  void Drag(ImVec2 from, ImVec2 to) {
    MoveTo(from);
    Press();
    MoveTo(to);
    Release();
  }

  [[nodiscard]] static float Scale() { return ImGui::GetStyle().FontScaleMain; }
  [[nodiscard]] float Side() const { return ro::ui::kMenuIconFallbackSide * Scale(); }
  [[nodiscard]] ImVec2 Corner(std::size_t rank) const {
    return ro::ui::DefaultMenuIconCorner(rank, Scale());
  }
  [[nodiscard]] ImVec2 ButtonCorner() const {
    return ro::ui::DefaultMenuButtonCorner(grid_count, Scale());
  }
  [[nodiscard]] ImVec2 Middle(std::size_t rank) const {
    const ImVec2 corner = Corner(rank);
    return {corner.x + Side() * 0.5F, corner.y + Side() * 0.5F};
  }
  [[nodiscard]] ro::ui::MenuIconPlace& Place(MenuCommand command) {
    return look.places[static_cast<std::size_t>(command)];
  }
  // OÙ L'ICÔNE EST À L'ÉCRAN : sa place retenue est ancrée au bord le plus
  // proche, et ne se lit en pixels d'écran qu'à travers son ancre.
  [[nodiscard]] ImVec2 OnScreen(MenuCommand command) {
    const ro::ui::MenuIconPlace& place = Place(command);
    const ro::ui::HudRect shown =
        ro::ui::HudOnScreen(ro::ui::HudRect{.x = place.x,
                                            .y = place.y,
                                            .width = Side(),
                                            .height = Side(),
                                            .across = place.across,
                                            .down = place.down},
                            ImVec2(kViewport.width_px, kViewport.height_px));
    return {shown.x, shown.y};
  }
};

}  // namespace

TEST_CASE("Icônes libres : la case d'origine est la grille du jeu, au coin haut-gauche",
          "[ui][menu_icons]") {
  const ImVec2 first = ro::ui::DefaultMenuIconCorner(0, 1.0F);
  CHECK(first.x == ro::ui::kMenuIconsScreenMargin);
  // ⚠ LA PLACE DU BADGE EST RÉSERVÉE AU-DESSUS : le « nouveau » déborde en haut.
  CHECK(first.y == ro::ui::kMenuIconsScreenMargin + ro::ui::kMenuIconNewsLift);
  const ImVec2 seventh = ro::ui::DefaultMenuIconCorner(6, 1.0F);
  CHECK(seventh.x == first.x + ro::ui::kMenuIconColumnPitch);
  CHECK(seventh.y == first.y + ro::ui::kMenuIconRowPitch);
  // L'échelle de l'interface agrandit la grille entière, marge comprise.
  const ImVec2 doubled = ro::ui::DefaultMenuIconCorner(0, 2.0F);
  CHECK(doubled.x == 2.0F * first.x);
  // Les états se rangent à sa droite : le bord droit d'une rangée pleine,
  // cinq colonnes au pas de 42.
  CHECK(ro::ui::MenuIconsHomeRight(1.0F) ==
        ro::ui::kMenuIconsScreenMargin + 5.0F * ro::ui::kMenuIconColumnPitch);
}

TEST_CASE("Icônes libres : verrouillées, elles se cliquent et ne bougent pas", "[ui][menu_icons]") {
  MenuBench bench;
  REQUIRE(bench.look.locked);
  bench.Drag(bench.Middle(kStatusRank), kFarAway);
  CHECK(bench.clicked == MenuCommand::kStatus);
  CHECK_FALSE(bench.moved);
  CHECK_FALSE(ro::ui::MenuIconPlaced(bench.Place(MenuCommand::kStatus)));
}

TEST_CASE("Icônes libres : une icône masquée ne se clique plus, ses voisines restent",
          "[ui][menu_icons]") {
  MenuBench bench;
  bench.Place(MenuCommand::kStatus).hidden = true;
  bench.Frame();
  bench.MoveTo(bench.Middle(kStatusRank));
  bench.Press();
  bench.Release();
  CHECK_FALSE(bench.clicked.has_value());
  // ⚠ L'ÉQUIPEMENT N'A PAS PRIS LA CASE LIBÉRÉE : il est toujours à la sienne.
  bench.MoveTo(bench.Middle(kEquipmentRank));
  bench.Press();
  bench.Release();
  CHECK(bench.clicked == MenuCommand::kEquipment);
}

TEST_CASE("Icônes libres : déverrouillées, elles se déplacent et n'ouvrent rien",
          "[ui][menu_icons]") {
  MenuBench bench;
  bench.look.locked = false;
  bench.Frame();
  // 🔴 SEMÉES DÈS LE DÉVERROUILLAGE, à leur case d'origine : sans place écrite,
  // le bloc CTRL n'aurait rien à emporter.
  CHECK(bench.moved);
  const ImVec2 corner = bench.Corner(kEquipmentRank);
  CHECK(bench.OnScreen(MenuCommand::kEquipment).x == corner.x);
  CHECK(bench.OnScreen(MenuCommand::kEquipment).y == corner.y);

  const ImVec2 grab = bench.Middle(kStatusRank);
  bench.Drag(grab, kFarAway);
  CHECK_FALSE(bench.clicked.has_value());
  const ImVec2 status = bench.OnScreen(MenuCommand::kStatus);
  CHECK(status.x == kFarAway.x - bench.Side() * 0.5F);
  CHECK(status.y == kFarAway.y - bench.Side() * 0.5F);
}

TEST_CASE("Icônes libres : le coin d'une icône la déplace, il ne la retaille pas",
          "[ui][menu_icons]") {
  // 🔴 LA PRISE DU COIN BAS-DROIT RETAILLE UN CADRE DE HUD. Une icône a la taille
  // de son dessin : saisie là, elle doit partir entière.
  MenuBench bench;
  bench.look.locked = false;
  bench.Frame();
  const ImVec2 corner = bench.Corner(kStatusRank);
  const ImVec2 grab(corner.x + bench.Side() - 1.0F, corner.y + bench.Side() - 1.0F);
  bench.Drag(grab, ImVec2(grab.x + 400.0F, grab.y + 300.0F));
  CHECK(bench.OnScreen(MenuCommand::kStatus).x == corner.x + 400.0F);
  CHECK(bench.OnScreen(MenuCommand::kStatus).y == corner.y + 300.0F);
}

TEST_CASE("Icônes libres : une icône posée près d'une autre s'y aimante", "[ui][menu_icons]") {
  MenuBench bench;
  bench.look.locked = false;
  bench.Frame();
  // Le statut juste sous le coin de l'équipement, à quatre pixels près.
  const ImVec2 target = bench.Corner(kEquipmentRank);
  constexpr float kAlmost = 4.0F;
  const ImVec2 grab = bench.Middle(kStatusRank);
  const ImVec2 drop(target.x + kAlmost + bench.Side() * 0.5F,
                    target.y + bench.Side() + kAlmost + bench.Side() * 0.5F);
  bench.Drag(grab, drop);
  CHECK(bench.OnScreen(MenuCommand::kStatus).x == target.x);
  CHECK(bench.OnScreen(MenuCommand::kStatus).y == target.y + bench.Side());
}

TEST_CASE("Icônes libres : le bouton du Vote Shop se tient contre la grille et s'y clique",
          "[ui][menu_icons]") {
  MenuBench bench;
  const ImVec2 home = bench.ButtonCorner();
  bench.MoveTo(ImVec2(home.x + bench.Side() * 0.5F, home.y + bench.Side() * 0.5F));
  bench.Press();
  bench.Release();
  CHECK(bench.clicked == MenuCommand::kCashShopButton);

  // 🔴 IL NE DÉCALE AUCUNE CASE : le statut est toujours à la première.
  bench.clicked.reset();
  bench.MoveTo(bench.Middle(kStatusRank));
  bench.Press();
  bench.Release();
  CHECK(bench.clicked == MenuCommand::kStatus);
}

TEST_CASE("Icônes libres : masqué, le bouton du Vote Shop ne se clique plus", "[ui][menu_icons]") {
  MenuBench bench;
  bench.Place(MenuCommand::kCashShopButton).hidden = true;
  const ImVec2 home = bench.ButtonCorner();
  bench.MoveTo(ImVec2(home.x + bench.Side() * 0.5F, home.y + bench.Side() * 0.5F));
  bench.Press();
  bench.Release();
  CHECK_FALSE(bench.clicked.has_value());
}

namespace {

constexpr ImTextureID kTrackerNormalTexture = 41;
constexpr ImTextureID kTrackerNewsTexture = 42;
constexpr float kTrackerBitmapPixels = 32.0F;

// L'image posée par ce bitmap dans une liste de l'image dessinée, ou non.
//
// ⚠ PAS `GetTexID()` : il assertionne sur la texture de la police, qu'aucun
// moteur n'a chargée dans un banc.
[[nodiscard]] bool Painted(ImTextureID id) {
  const ImDrawData* drawn = ImGui::GetDrawData();
  REQUIRE(drawn != nullptr);
  for (const ImDrawList* list : drawn->CmdLists) {
    for (const ImDrawCmd& command : list->CmdBuffer) {
      if (command.UserCallback == nullptr && command.TexRef._TexData == nullptr &&
          command.TexRef._TexID == id && command.ElemCount > 0) {
        return true;
      }
    }
  }
  return false;
}

}  // namespace

TEST_CASE("Icônes libres : le carnet MVP signalé prend son art « nouveau »", "[ui][menu_icons]") {
  MenuBench bench;
  const auto tracker =
      std::ranges::find(bench.icons, MenuCommand::kMvpTracker, &ro::ui::MenuIconView::command);
  REQUIRE(tracker != bench.icons.end());
  tracker->normal = ro::ui::SkinImage{
      .id = kTrackerNormalTexture, .width = kTrackerBitmapPixels, .height = kTrackerBitmapPixels};
  tracker->news = ro::ui::SkinImage{
      .id = kTrackerNewsTexture, .width = kTrackerBitmapPixels, .height = kTrackerBitmapPixels};
  bench.Frame();
  CHECK(Painted(kTrackerNormalTexture));
  CHECK_FALSE(Painted(kTrackerNewsTexture));
  // UN FAVORI ENTRE DANS SA FENÊTRE : l'icône s'allume.
  tracker->news_lit = true;
  bench.Frame();
  CHECK(Painted(kTrackerNewsTexture));
  CHECK_FALSE(Painted(kTrackerNormalTexture));
}

TEST_CASE("Icônes accrochées : la grille suit la fenêtre Basic Info, les places rangées restent",
          "[ui][menu_icons]") {
  MenuBench bench;
  // Une place rangée par le joueur, loin de l'accroche.
  bench.Place(MenuCommand::kStatus).x = kFarAway.x;
  bench.Place(MenuCommand::kStatus).y = kFarAway.y;
  const ImVec2 dock(100.0F, 200.0F);
  bench.state.dock = dock;
  bench.state.grid_open = true;
  bench.moved = false;
  bench.Frame();
  // La première icône tombe à (9, 23) sous le coin d'accroche, et s'y clique.
  const float scale = MenuBench::Scale();
  const ImVec2 first(dock.x + ro::ui::kMenuIconsDockLeft * scale + bench.Side() * 0.5F,
                     dock.y + kDockedIconTop * scale + bench.Side() * 0.5F);
  bench.MoveTo(first);
  bench.Press();
  bench.Release();
  CHECK(bench.clicked == MenuCommand::kStatus);
  // La place rangée n'a pas bougé : elle reviendra quand la fenêtre s'éteindra.
  CHECK(bench.Place(MenuCommand::kStatus).x == kFarAway.x);
  CHECK(bench.Place(MenuCommand::kStatus).y == kFarAway.y);
  CHECK_FALSE(bench.moved);
}

TEST_CASE("Icônes accrochées : une icône masquée ne laisse pas de trou", "[ui][menu_icons]") {
  MenuBench bench;
  // L'équipement masqué : l'icône d'après prend sa case, la deuxième.
  bench.Place(MenuCommand::kEquipment).hidden = true;
  const ImVec2 dock(100.0F, 200.0F);
  bench.state.dock = dock;
  bench.state.grid_open = true;
  bench.Frame();
  const float scale = MenuBench::Scale();
  const ImVec2 second(dock.x + (ro::ui::kMenuIconsDockLeft + ro::ui::kMenuIconColumnPitch) * scale +
                          bench.Side() * 0.5F,
                      dock.y + kDockedIconTop * scale + bench.Side() * 0.5F);
  bench.MoveTo(second);
  bench.Press();
  bench.Release();
  CHECK(bench.clicked == MenuCommand::kItem);
}

TEST_CASE("Icônes accrochées : fermée d'abord, la grille s'ouvre par son bandeau",
          "[ui][menu_icons]") {
  MenuBench bench;
  const ImVec2 dock(100.0F, 200.0F);
  bench.state.dock = dock;
  bench.Frame();
  const float scale = MenuBench::Scale();
  // Fermée, la case du statut ne répond pas : l'icône n'est pas dessinée.
  const ImVec2 first(dock.x + ro::ui::kMenuIconsDockLeft * scale + bench.Side() * 0.5F,
                     dock.y + kDockedIconTop * scale + bench.Side() * 0.5F);
  bench.MoveTo(first);
  bench.Press();
  bench.Release();
  CHECK_FALSE(bench.clicked.has_value());
  // Le bandeau, juste sous le bas de la fenêtre, l'ouvre.
  const float band_middle =
      ro::ui::kMenuGridBandTop - ro::ui::kMenuGridAboveDock + ro::ui::kMenuGridBandHeight * 0.5F;
  bench.MoveTo(
      ImVec2(dock.x + ro::ui::kMenuGridBandWidth * 0.5F * scale, dock.y + band_middle * scale));
  bench.Press();
  bench.Release();
  CHECK(bench.state.grid_open);
}

TEST_CASE("Icônes accrochées : le panneau de la grille reste derrière les icônes",
          "[ui][menu_icons]") {
  // 🔴 LE PANNEAU ET LES ICÔNES NAISSENT DANS LA MÊME IMAGE, le panneau d'abord,
  // comme au lancement d'un jeu dont la fenêtre « Basic Info » est allumée.
  // ImGui pose au fond toute fenêtre neuve de cette famille : sans correction,
  // les icônes, nées après, passent derrière le panneau.
  MenuBench bench(ImVec2(100.0F, 200.0F));
  bench.Frame();
  const ImGuiContext& context = *ImGui::GetCurrentContext();
  const auto rank_of = [&context](const char* name) {
    const ImGuiWindow* window = ImGui::FindWindowByName(name);
    REQUIRE(window != nullptr);
    for (int rank = 0; rank < context.Windows.Size; ++rank) {
      if (context.Windows[rank] == window) {
        return rank;
      }
    }
    return -1;
  };
  // Dessiné d'abord, il est en dessous : il n'assombrit ni ne couvre rien.
  CHECK(rank_of("##grille_accrochee") < rank_of("##menu_icon_status"));
}
