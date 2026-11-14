// LA GRILLE D'ICÔNES DE MENU : son catalogue, ses chemins, ses cases, et ce que
// fait un clic sur une icône dont la fenêtre n'existe pas encore.

#include "ro/ui/menu_icons.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cstddef>
#include <span>
#include <string>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/align_grid.h"
#include "ro/ui/hud_frame.h"
#include "ro/ui/ui_context.h"

using ro::ui::MenuCommand;
using ro::ui::MenuIconArt;

namespace {

// LE NOMBRE DE SOMMETS PEINTS POUR L'ICÔNE DU CHAMP DE BATAILLE, sur une vraie
// image ImGui, sans GPU. ⚠ SANS BITMAP normal : le carré de secours.
[[nodiscard]] int BattleIconVertices(const ro::ui::MenuIconView& battle) {
  constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
  constexpr float kFrameSeconds = 1.0F / 60.0F;
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art;
  const ro::ui::AlignGrid grid;
  ro::ui::HudGrip grip;
  ro::ui::MenuIconsState state;
  ro::ui::MenuIconsLook look;
  const std::array<ro::ui::MenuIconView, 2> icons = {
      ro::ui::MenuIconView{.command = MenuCommand::kStatus, .ported = true}, battle};
  for (int frame = 0; frame < 2; ++frame) {
    ui.BeginFrame(kViewport, kFrameSeconds);
    static_cast<void>(ro::ui::DrawMenuIcons(state, icons, look, grid, grip, art));
    ui.EndFrame();
  }
  const ImGuiWindow* window = ImGui::FindWindowByName("##menu_icon_battle");
  REQUIRE(window != nullptr);
  return window->DrawList->VtxBuffer.Size;
}

}  // namespace

TEST_CASE("Menu d'icônes : en file, l'icône du champ de bataille porte une pastille",
          "[ui][menu_icons][battleground]") {
  ro::ui::MenuIconView battle{.command = MenuCommand::kBattleground, .ported = true};
  const int plain = BattleIconVertices(battle);
  battle.dot_lit = true;
  const int dotted = BattleIconVertices(battle);
  CHECK(dotted > plain);
}

TEST_CASE("Menu d'icônes : la pastille cède au bitmap « nouveau »",
          "[ui][menu_icons][battleground]") {
  // Le badge peint dit déjà qu'il y a quelque chose à voir.
  constexpr float kIconPixels = 32.0F;
  const ro::ui::SkinImage image{.id = 1, .width = kIconPixels, .height = kIconPixels};
  ro::ui::MenuIconView battle{.command = MenuCommand::kBattleground,
                              .normal = image,
                              .news = image,
                              .ported = true,
                              .news_lit = true};
  const int plain = BattleIconVertices(battle);
  battle.dot_lit = true;
  CHECK(BattleIconVertices(battle) == plain);
  // Sans le « nouveau », la même icône la montre.
  battle.news_lit = false;
  const int unlit = BattleIconVertices(battle);
  battle.dot_lit = false;
  CHECK(unlit > BattleIconVertices(battle));
}

TEST_CASE("Menu d'icônes : le catalogue suit l'ordre de l'énumération", "[ui][menu_icons]") {
  // 🔴 UNE ENTRÉE DÉCALÉE DONNERAIT À UNE ICÔNE LE BITMAP DE SA VOISINE, et
  // `MenuIconOf` indexe le tableau par le rang de la commande.
  std::size_t rank = 0;
  for (const ro::ui::MenuIconEntry& entry : ro::ui::MenuIconCatalogue()) {
    CHECK(static_cast<std::size_t>(entry.command) == rank);
    CHECK_FALSE(entry.stem.empty());
    ++rank;
  }
  CHECK(rank == ro::ui::kMenuCommandCount);
}

TEST_CASE("Menu d'icônes : vingt-huit cases montrées, un bouton hors grille, sept « nouveau »",
          "[ui][menu_icons]") {
  std::size_t in_grid = 0;
  std::size_t off_grid = 0;
  std::size_t with_news = 0;
  for (const ro::ui::MenuIconEntry& entry : ro::ui::MenuIconCatalogue()) {
    in_grid += entry.shown && !entry.off_grid ? 1U : 0U;
    off_grid += entry.off_grid ? 1U : 0U;
    with_news += entry.has_news_art ? 1U : 0U;
  }
  // Dix-huit icônes du jeu, la réputation comprise, l'album, l'atlas, le carnet
  // MVP, la fiche du pet, le tableau des classements, le Marché, la base de
  // données et les trois mini-jeux de ce client.
  constexpr std::size_t kShownOnThisServer = 28;
  constexpr std::size_t kWithNewsArt = 7;
  CHECK(in_grid == kShownOnThisServer);
  CHECK(off_grid == 1);
  CHECK(ro::ui::MenuIconOf(MenuCommand::kCashShopButton).off_grid);
  CHECK(ro::ui::MenuIconOf(MenuCommand::kCashShopButton).shown);
  CHECK(with_news == kWithNewsArt);
  CHECK(ro::ui::MenuIconOf(MenuCommand::kBattleground).shown);
  CHECK(ro::ui::MenuIconOf(MenuCommand::kBooking).shown);
  CHECK_FALSE(ro::ui::MenuIconOf(MenuCommand::kProbability).shown);
}

TEST_CASE(
    "Menu d'icônes : l'album, l'atlas, le pet, les classements, le Marché, la base de "
    "données et les mini-jeux viennent APRÈS celles du jeu",
    "[ui][menu_icons]") {
  // 🔴 UNE ICÔNE DE CE CLIENT INTERCALÉE DÉCALERAIT CELLES DU JEU, que le joueur
  // a dans l'œil.
  // ⚠ LE BOUTON HORS GRILLE, lui, n'a pas de case : sa place dans la liste
  // ne décale rien, et il vient tout au bout.
  const std::span<const ro::ui::MenuIconEntry> whole = ro::ui::MenuIconCatalogue();
  REQUIRE_FALSE(whole.empty());
  CHECK(whole.back().command == MenuCommand::kCashShopButton);
  const std::span<const ro::ui::MenuIconEntry> with_games = whole.first(whole.size() - 1);
  // LES MINI-JEUX EN DERNIER, un par jeu, sans bitmap du jeu ni « nouveau ».
  constexpr std::size_t kGameIcons = 3;
  REQUIRE(with_games.size() > kGameIcons);
  const std::span<const ro::ui::MenuIconEntry> games = with_games.last(kGameIcons);
  CHECK(games[0].command == MenuCommand::kRojeweled);
  CHECK(games[1].command == MenuCommand::kRoggle);
  CHECK(games[2].command == MenuCommand::kSurvivors);
  for (const ro::ui::MenuIconEntry& game : games) {
    CHECK(game.shown);
    CHECK_FALSE(game.has_news_art);
  }
  CHECK(std::string(ro::ui::MenuIconLabel(MenuCommand::kSurvivors)) == "Survivants de Midgard");
  const std::span<const ro::ui::MenuIconEntry> catalogue =
      with_games.first(with_games.size() - kGameIcons);
  // LA BASE DE DONNÉES JUSTE AVANT, sans bitmap du jeu : son art est le nôtre.
  CHECK(catalogue.back().command == MenuCommand::kDatabase);
  CHECK(catalogue.back().shown);
  CHECK_FALSE(catalogue.back().has_news_art);
  CHECK(catalogue.back().stem == "database");
  CHECK(std::string(ro::ui::MenuIconLabel(MenuCommand::kDatabase)) == "Base de données");
  REQUIRE(catalogue.size() >= 7);
  // LE MARCHÉ JUSTE AVANT, sans bitmap du jeu non plus.
  const ro::ui::MenuIconEntry& market = catalogue[catalogue.size() - 2];
  CHECK(market.command == MenuCommand::kMarket);
  CHECK(market.shown);
  CHECK_FALSE(market.has_news_art);
  CHECK(market.stem == "market");
  CHECK(std::string(ro::ui::MenuIconLabel(MenuCommand::kMarket)) == "Marché");
  // LE TABLEAU DES CLASSEMENTS AVANT LUI.
  const ro::ui::MenuIconEntry& ranking = catalogue[catalogue.size() - 3];
  CHECK(ranking.command == MenuCommand::kRankingBoard);
  CHECK(ranking.shown);
  CHECK_FALSE(ranking.has_news_art);
  CHECK(std::string(ro::ui::MenuIconLabel(MenuCommand::kRankingBoard)) ==
        "Tableau des classements");
  const ro::ui::MenuIconEntry& pet = catalogue[catalogue.size() - 4];
  CHECK(pet.command == MenuCommand::kPetWindow);
  CHECK(pet.shown);
  CHECK(ro::ui::MenuIconPath(pet.stem, MenuIconArt::kNormal) ==
        std::string(ro::render::kSkinRoot) + "menu_icon\\bt_pet.bmp");
  // L'album précède l'atlas, puis vient le carnet de chasse MVP.
  const ro::ui::MenuIconEntry& tracker = catalogue[catalogue.size() - 5];
  CHECK(tracker.command == MenuCommand::kMvpTracker);
  // SON ART EST CELUI DE `moonlight.grf`, « nouveau » compris : il s'allume
  // quand un favori entre dans sa fenêtre.
  CHECK(tracker.shown);
  CHECK(tracker.has_news_art);
  CHECK(tracker.stem == "tracker");
  CHECK(ro::ui::MenuIconPath(tracker.stem, MenuIconArt::kNews) ==
        std::string(ro::render::kSkinRoot) + "menu_icon\\bt_tracker_new.bmp");
  CHECK(std::string(ro::ui::MenuIconLabel(MenuCommand::kMvpTracker)) == "Carnet de chasse MVP");
  const ro::ui::MenuIconEntry& atlas = catalogue[catalogue.size() - 6];
  CHECK(atlas.command == MenuCommand::kCraftAtlas);
  CHECK(atlas.shown);
  const ro::ui::MenuIconEntry& album = catalogue[catalogue.size() - 7];
  CHECK(album.command == MenuCommand::kCardAlbum);
  CHECK(album.shown);
  CHECK(ro::ui::MenuIconPath(album.stem, MenuIconArt::kPressed) ==
        std::string(ro::render::kSkinRoot) + "menu_icon\\bt_album_press.bmp");
  CHECK(ro::ui::MenuIconPath(atlas.stem, MenuIconArt::kPressed) ==
        std::string(ro::render::kSkinRoot) + "menu_icon\\bt_atlas_press.bmp");
}

TEST_CASE("Menu d'icônes : les quatre chemins d'une icône", "[ui][menu_icons]") {
  const std::string root(ro::render::kSkinRoot);
  CHECK(ro::ui::MenuIconPath("mail", MenuIconArt::kNormal) == root + "menu_icon\\bt_mail.bmp");
  CHECK(ro::ui::MenuIconPath("mail", MenuIconArt::kPressed) ==
        root + "menu_icon\\bt_mail_press.bmp");
  CHECK(ro::ui::MenuIconPath("mail", MenuIconArt::kNews) == root + "menu_icon\\bt_mail_new.bmp");
  CHECK(ro::ui::MenuIconPath("mail", MenuIconArt::kNewsPressed) ==
        root + "menu_icon\\bt_mail_new_press.bmp");
}

TEST_CASE("Menu d'icônes : le bouton du Vote Shop, son bitmap et sa place", "[ui][menu_icons]") {
  const std::string root(ro::render::kSkinRoot);
  CHECK(ro::ui::MenuButtonPath(ro::ui::MenuIconOf(MenuCommand::kCashShopButton).stem) ==
        root + "basic_interface\\NC_CashShop.bmp");
  // Sous la grille, contre le bord gauche : sa droite est aux états.
  constexpr std::size_t kGridCount = 18;
  const ImVec2 grid_size = ro::ui::MenuIconsGridSize(kGridCount);
  const ImVec2 corner = ro::ui::DefaultMenuButtonCorner(kGridCount, 1.0F);
  CHECK(corner.x == ro::ui::kMenuIconsScreenMargin);
  CHECK(corner.y == ro::ui::kMenuIconsScreenMargin + grid_size.y + ro::ui::kMenuButtonGap);
  // Marge, grille et écart suivent tous l'échelle de l'interface.
  const ImVec2 doubled = ro::ui::DefaultMenuButtonCorner(kGridCount, 2.0F);
  CHECK(doubled.x == 2.0F * corner.x);
  CHECK(doubled.y == 2.0F * corner.y);
}

TEST_CASE("Menu d'icônes : le statut d'un Doram a son propre dessin", "[ui][menu_icons]") {
  CHECK(ro::ui::MenuIconStem(MenuCommand::kStatus, false) == "status");
  CHECK(ro::ui::MenuIconStem(MenuCommand::kStatus, true) == "status_doram");
  // ⚠ ET LUI SEUL : les autres ne dépendent pas du personnage.
  CHECK(ro::ui::MenuIconStem(MenuCommand::kItem, true) == "item");
}

TEST_CASE("Menu d'icônes : cinq par rangée, au pas du client", "[ui][menu_icons]") {
  const ImVec2 first = ro::ui::MenuIconCellOffset(0);
  const ImVec2 fifth = ro::ui::MenuIconCellOffset(4);
  const ImVec2 sixth = ro::ui::MenuIconCellOffset(5);
  CHECK(first.x == 0.0F);
  CHECK(first.y == ro::ui::kMenuIconNewsLift);
  CHECK(fifth.x == 4.0F * ro::ui::kMenuIconColumnPitch);
  CHECK(fifth.y == first.y);
  CHECK(sixth.x == 0.0F);
  CHECK(sixth.y == first.y + ro::ui::kMenuIconRowPitch);

  // Seize icônes : quatre rangées, cinq colonnes, la place du badge en haut.
  const ImVec2 grid = ro::ui::MenuIconsGridSize(16);
  CHECK(grid.x == 5.0F * ro::ui::kMenuIconColumnPitch);
  CHECK(grid.y == ro::ui::kMenuIconNewsLift + 4.0F * ro::ui::kMenuIconRowPitch);
  // ⚠ UNE RANGÉE INCOMPLÈTE NE RÉSERVE PAS CINQ COLONNES.
  CHECK(ro::ui::MenuIconsGridSize(3).x == 3.0F * ro::ui::kMenuIconColumnPitch);
}

TEST_CASE("Menu d'icônes : une icône portée rend sa commande", "[ui][menu_icons]") {
  ro::ui::MenuIconsState state;
  const ro::ui::MenuIconView bag{.command = MenuCommand::kItem, .ported = true};
  CHECK(ro::ui::PressMenuIcon(state, bag) == MenuCommand::kItem);
  CHECK_FALSE(state.placeholder_open);
}

TEST_CASE("Menu d'icônes : une icône en attente ouvre, puis referme, sa fenêtre",
          "[ui][menu_icons]") {
  ro::ui::MenuIconsState state;
  const ro::ui::MenuIconView mail{.command = MenuCommand::kMail, .ported = false};
  const ro::ui::MenuIconView world_map{.command = MenuCommand::kWorldMap, .ported = false};

  CHECK_FALSE(ro::ui::PressMenuIcon(state, mail).has_value());
  CHECK(state.placeholder_open);
  CHECK(state.placeholder == MenuCommand::kMail);

  // 🔴 UNE AUTRE ICÔNE EN ATTENTE REPREND LA FENÊTRE, elle ne la ferme pas : le
  // joueur est allé voir à côté.
  CHECK_FALSE(ro::ui::PressMenuIcon(state, world_map).has_value());
  CHECK(state.placeholder_open);
  CHECK(state.placeholder == MenuCommand::kWorldMap);

  // Le second clic sur la MÊME icône la referme.
  CHECK_FALSE(ro::ui::PressMenuIcon(state, world_map).has_value());
  CHECK_FALSE(state.placeholder_open);
}

TEST_CASE("Menu d'icônes : nos images ont leurs noms à elles, en PNG", "[ui][menu_icons]") {
  const std::string folder(ro::ui::kOurIconFolder);
  CHECK(folder == "data\\ui_icons\\");
  CHECK(ro::ui::OurMenuIconPath("pet", MenuIconArt::kNormal) == folder + "menu_pet.png");
  CHECK(ro::ui::OurMenuIconPath("pet", MenuIconArt::kPressed) == folder + "menu_pet_pressed.png");
  CHECK(ro::ui::OurMenuIconPath("mail", MenuIconArt::kNews) == folder + "menu_mail_new.png");
  CHECK(ro::ui::OurMenuIconPath("mail", MenuIconArt::kNewsPressed) ==
        folder + "menu_mail_new_pressed.png");
}

namespace {

// LA TAILLE D'UN BITMAP DU JEU, et celle de notre image dessinée à double densité.
constexpr float kNativeWidth = 32.0F;
constexpr float kNativeHeight = 34.0F;
constexpr ImTextureID kNativeTexture = 7;
constexpr ImTextureID kOurTexture = 9;

}  // namespace

TEST_CASE("Menu d'icônes : la nôtre d'abord, puis celle du jeu, puis le secours",
          "[ui][menu_icons]") {
  const ro::ui::SkinImage native{
      .id = kNativeTexture, .width = kNativeWidth, .height = kNativeHeight};
  const ro::ui::SkinImage ours{.id = kOurTexture,
                               .width = kNativeWidth * ro::ui::kOurIconDensity,
                               .height = kNativeHeight * ro::ui::kOurIconDensity};
  // 🔴 LA NÔTRE PREND LA PLACE DU BITMAP DU JEU, PAS PLUS : sa texture double
  // reste entière, seule sa place se divise.
  const ro::ui::SkinImage preferred = ro::ui::PreferredMenuIconArt(ours, native);
  CHECK(preferred.id == kOurTexture);
  CHECK(preferred.width == kNativeWidth);
  CHECK(preferred.height == kNativeHeight);
  // Absente, celle du jeu, telle quelle.
  const ro::ui::SkinImage fallback = ro::ui::PreferredMenuIconArt({}, native);
  CHECK(fallback.id == kNativeTexture);
  CHECK(fallback.width == kNativeWidth);
  // Ni l'une ni l'autre : vide, et la grille dessine le carré de secours.
  CHECK_FALSE(ro::ui::PreferredMenuIconArt({}, {}).usable());
}

TEST_CASE("Menu d'icônes : notre image double tient la case d'un bitmap du jeu",
          "[ui][menu_icons]") {
  constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
  constexpr float kFrameSeconds = 1.0F / 60.0F;
  const ro::ui::SkinImage ours{.id = kOurTexture,
                               .width = kNativeWidth * ro::ui::kOurIconDensity,
                               .height = kNativeHeight * ro::ui::kOurIconDensity};
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art;
  const ro::ui::AlignGrid grid;
  ro::ui::HudGrip grip;
  ro::ui::MenuIconsState state;
  ro::ui::MenuIconsLook look;
  const std::array<ro::ui::MenuIconView, 1> icons = {
      ro::ui::MenuIconView{.command = MenuCommand::kPetWindow,
                           .normal = ro::ui::PreferredMenuIconArt(ours, {}),
                           .ported = true}};
  for (int frame = 0; frame < 2; ++frame) {
    ui.BeginFrame(kViewport, kFrameSeconds);
    static_cast<void>(ro::ui::DrawMenuIcons(state, icons, look, grid, grip, art));
    ui.EndFrame();
  }
  const ImGuiWindow* window = ImGui::FindWindowByName("##menu_icon_pet");
  REQUIRE(window != nullptr);
  CHECK(window->Size.x == kNativeWidth);
  CHECK(window->Size.y == kNativeHeight);
}

TEST_CASE("Menu d'icônes : nos images « nouveau » suivent les états du catalogue",
          "[ui][menu_icons]") {
  // 🔴 LE CARNET MVP S'ALLUME, l'album et l'Atlas jamais : nos PNG livrent
  // quatre états pour le premier, deux pour les autres.
  CHECK(ro::ui::MenuIconOf(MenuCommand::kMvpTracker).has_news_art);
  CHECK_FALSE(ro::ui::MenuIconOf(MenuCommand::kCardAlbum).has_news_art);
  CHECK_FALSE(ro::ui::MenuIconOf(MenuCommand::kCraftAtlas).has_news_art);
  const std::string folder(ro::ui::kOurIconFolder);
  CHECK(ro::ui::OurMenuIconPath("tracker", MenuIconArt::kNews) == folder + "menu_tracker_new.png");
  CHECK(ro::ui::OurMenuIconPath("tracker", MenuIconArt::kNewsPressed) ==
        folder + "menu_tracker_new_pressed.png");
  CHECK(ro::ui::OurMenuIconPath("album", MenuIconArt::kPressed) ==
        folder + "menu_album_pressed.png");
  CHECK(ro::ui::OurMenuIconPath("atlas", MenuIconArt::kNormal) == folder + "menu_atlas.png");
}

namespace {

// LE BITMAP « NOUVEAU » DU JEU : son badge déborde en haut et à droite.
constexpr float kNativeNewsWidth = 35.0F;
constexpr float kNativeNewsHeight = 40.0F;
constexpr ImTextureID kOurNewsTexture = 13;

// Le rectangle où l'image de cette texture est peinte, dans la fenêtre donnée.
struct PaintedRect {
  ImVec2 min{FLT_MAX, FLT_MAX};
  ImVec2 max{-FLT_MAX, -FLT_MAX};
  bool found = false;
};

// ⚠ PAS `GetTexID()` : il assertionne sur la texture de la police, qu'aucun
// moteur n'a chargée dans un banc.
[[nodiscard]] PaintedRect PaintedRectOf(const ImGuiWindow& window, ImTextureID texture) {
  PaintedRect rect;
  const ImDrawList& list = *window.DrawList;
  for (const ImDrawCmd& command : list.CmdBuffer) {
    if (command.TexRef._TexData != nullptr || command.TexRef._TexID != texture) {
      continue;
    }
    for (unsigned int index = 0; index < command.ElemCount; ++index) {
      const ImDrawIdx vertex = list.IdxBuffer[static_cast<int>(command.IdxOffset + index)];
      const ImVec2 at = list.VtxBuffer[static_cast<int>(command.VtxOffset + vertex)].pos;
      rect.min = ImVec2(std::min(rect.min.x, at.x), std::min(rect.min.y, at.y));
      rect.max = ImVec2(std::max(rect.max.x, at.x), std::max(rect.max.y, at.y));
      rect.found = true;
    }
  }
  return rect;
}

}  // namespace

TEST_CASE("Menu d'icônes : notre « nouveau » double monte et déborde comme celui du jeu",
          "[ui][menu_icons]") {
  constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
  constexpr float kFrameSeconds = 1.0F / 60.0F;
  const ro::ui::SkinImage ours{.id = kOurTexture,
                               .width = kNativeWidth * ro::ui::kOurIconDensity,
                               .height = kNativeHeight * ro::ui::kOurIconDensity};
  const ro::ui::SkinImage our_news{.id = kOurNewsTexture,
                                   .width = kNativeNewsWidth * ro::ui::kOurIconDensity,
                                   .height = kNativeNewsHeight * ro::ui::kOurIconDensity};
  const ro::ui::SkinImage native_news{
      .id = kNativeTexture, .width = kNativeNewsWidth, .height = kNativeNewsHeight};
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art;
  const ro::ui::AlignGrid grid;
  ro::ui::HudGrip grip;
  ro::ui::MenuIconsState state;
  ro::ui::MenuIconsLook look;
  // 🔴 LA NÔTRE PASSE AVANT CELLE DU JEU, même allumée.
  const std::array<ro::ui::MenuIconView, 1> icons = {
      ro::ui::MenuIconView{.command = MenuCommand::kMvpTracker,
                           .normal = ro::ui::PreferredMenuIconArt(ours, {}),
                           .news = ro::ui::PreferredMenuIconArt(our_news, native_news),
                           .ported = true,
                           .news_lit = true}};
  for (int frame = 0; frame < 2; ++frame) {
    ui.BeginFrame(kViewport, kFrameSeconds);
    static_cast<void>(ro::ui::DrawMenuIcons(state, icons, look, grid, grip, art));
    ui.EndFrame();
  }
  const ImGuiWindow* window = ImGui::FindWindowByName("##menu_icon_tracker");
  REQUIRE(window != nullptr);
  CHECK_FALSE(PaintedRectOf(*window, kNativeTexture).found);
  CHECK_FALSE(PaintedRectOf(*window, kOurTexture).found);
  const PaintedRect painted = PaintedRectOf(*window, kOurNewsTexture);
  REQUIRE(painted.found);
  // À LA PLACE EXACTE DU BITMAP « NOUVEAU » DU JEU : 35 sur 40, monté au-dessus
  // de la case, le bas de la face aligné sur ses voisines.
  CHECK(painted.max.x - painted.min.x == kNativeNewsWidth);
  CHECK(painted.max.y - painted.min.y == kNativeNewsHeight);
  CHECK(painted.min.x == window->Pos.x);
  CHECK(painted.min.y == window->Pos.y - ro::ui::kMenuIconNewsLift);
}
