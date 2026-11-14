// LA GRILLE NE GARDE AUCUNE BANDE MORTE.
//
// 🔴 CE QUE CES ESSAIS TIENNENT : la largeur laissée aux CASES est un multiple
// entier du côté d'une case. Ce n'est pas de l'esthétique — une bande plus
// étroite qu'une case porte le fond pavé du sac, ressemble donc à une colonne,
// et rien ne peut jamais y être posé.
//
// ⚠ ILS PILOTENT LES VRAIES FENÊTRES, sur plusieurs images, parce que la faute
// n'était dans aucun calcul : elle était dans la MESURE que le calage relit.
// Le rectangle de l'enfant contient la gouttière de l'ascenseur, si bien qu'une
// fenêtre calée sur sept cases n'en logeait que six. Aucun essai de fonction
// pure ne pouvait l'attraper.

#include <cstddef>
#include <span>
#include <string>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/cart_window.h"
#include "ro/ui/inventory_window.h"
#include "ro/ui/skin_art.h"
#include "ro/ui/ui_context.h"

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;

// Le calage se fait sur la mesure de l'image PRÉCÉDENTE : il faut donc laisser
// passer quelques images avant de lire une taille arrêtée.
constexpr int kFramesToSettle = 8;

// Assez d'objets pour déborder d'une fenêtre à sa taille d'ouverture, et assez
// peu pour n'en pas remplir la moitié.
constexpr int kEnoughToOverflow = 74;
constexpr int kTooFewToOverflow = 3;

struct GridMeasure {
  float usable_width = 0.0F;
  float scrollbar_width = 0.0F;
  // CE QUE LA GRILLE PORTE, et ce qu'elle MONTRE. ⚠ LES DEUX : le placement
  // libre se juge sur le rapport entre les deux, pas sur l'un des deux.
  float content_height = 0.0F;
  float visible_height = 0.0F;
  bool found = false;
};

[[nodiscard]] std::vector<ro::ui::InventoryCellView> Cells(const std::vector<std::string>& names) {
  std::vector<ro::ui::InventoryCellView> cells;
  cells.reserve(names.size());
  for (std::size_t rank = 0; rank < names.size(); ++rank) {
    ro::ui::InventoryCellView cell;
    // La place serveur commence à deux, et elle sert d'identité ImGui.
    cell.client_slot = static_cast<std::uint16_t>(rank + 2);
    cell.name = names[rank];
    cell.item_id = static_cast<std::uint32_t>(501 + rank);
    cell.amount = 1;
    cells.push_back(cell);
  }
  return cells;
}

[[nodiscard]] std::vector<std::string> Names(int count) {
  std::vector<std::string> names;
  names.reserve(static_cast<std::size_t>(count));
  for (int rank = 0; rank < count; ++rank) {
    names.push_back("objet " + std::to_string(rank));
  }
  return names;
}

// La grille vit dans un enfant nommé « <fenêtre>/grille_XXXXXXXX ». On lit sa
// taille et la gouttière que l'ascenseur lui prend.
[[nodiscard]] GridMeasure MeasureGrid(const char* window_id) {
  const std::string wanted = std::string(window_id) + "/grille";
  const ImGuiContext& context = *ImGui::GetCurrentContext();
  for (const ImGuiWindow* window : context.Windows) {
    if (std::string(window->Name).find(wanted) == std::string::npos) {
      continue;
    }
    return GridMeasure{.usable_width = window->Size.x - window->ScrollbarSizes.x,
                       .scrollbar_width = window->ScrollbarSizes.x,
                       .content_height = window->ContentSize.y,
                       .visible_height = window->Size.y,
                       .found = true};
  }
  return GridMeasure{};
}

[[nodiscard]] GridMeasure DrawInventoryFor(int item_count, bool tabs_vertical = true,
                                           bool free_layout = false) {
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art;
  ro::ui::InventoryWindowState state;
  state.open = true;
  state.tabs_vertical = tabs_vertical;
  state.free_layout = free_layout;
  const std::vector<std::string> names = Names(item_count);
  const std::vector<ro::ui::InventoryCellView> cells = Cells(names);

  GridMeasure measure;
  for (int frame = 0; frame < kFramesToSettle; ++frame) {
    ui.BeginFrame(kViewport, kFrameSeconds);
    (void)ro::ui::DrawInventoryWindow(state, {}, std::span<const ro::ui::InventoryCellView>(cells),
                                      ro::ui::InventoryTally{}, art);
    measure = MeasureGrid("###inventory");
    ui.EndFrame();
  }
  return measure;
}

[[nodiscard]] GridMeasure DrawCartFor(int item_count) {
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art;
  ro::ui::CartWindowState state;
  state.open = true;
  const std::vector<std::string> names = Names(item_count);
  const std::vector<ro::ui::InventoryCellView> cells = Cells(names);
  const ro::ui::CartTally tally{.item_count = item_count, .capacity = 100};

  GridMeasure measure;
  for (int frame = 0; frame < kFramesToSettle; ++frame) {
    ui.BeginFrame(kViewport, kFrameSeconds);
    (void)ro::ui::DrawCartWindow(state, {}, std::span<const ro::ui::InventoryCellView>(cells),
                                 tally, art);
    measure = MeasureGrid("###cart");
    ui.EndFrame();
  }
  return measure;
}

// Le reste de la largeur au-delà du dernier multiple entier du côté d'une case.
[[nodiscard]] float DeadBand(float usable_width) {
  const auto whole = static_cast<int>(usable_width / ro::ui::kItemCellPixels);
  return usable_width - static_cast<float>(whole) * ro::ui::kItemCellPixels;
}

}  // namespace

TEST_CASE("Sac : l'ascenseur ne laisse pas de bande morte", "[ui][sac]") {
  // 🔴 LA FAUTE MESURÉE : 224 px d'enfant calés sur sept cases, moins 14 px de
  // gouttière, ne logeaient que six cases — et les 18 px restants portaient le
  // fond pavé contre l'ascenseur, sans jamais rien pouvoir contenir.
  const GridMeasure measure = DrawInventoryFor(kEnoughToOverflow);
  REQUIRE(measure.found);
  // L'essai ne vaut que s'il y a bien un ascenseur : sans lui, il ne mesure
  // rien de ce qu'il prétend tenir.
  REQUIRE(measure.scrollbar_width > 0.0F);
  CHECK(DeadBand(measure.usable_width) == 0.0F);
}

TEST_CASE("Sac : sans ascenseur non plus, la grille tombe juste", "[ui][sac]") {
  // ⚠ L'AUTRE MOITIÉ DE LA RÈGLE. La gouttière entre et sort du chrome selon
  // que l'ascenseur paraît ; le calage doit tomber juste dans les deux états,
  // sans quoi la correction ne ferait que déplacer la bande.
  const GridMeasure measure = DrawInventoryFor(kTooFewToOverflow);
  REQUIRE(measure.found);
  CHECK(measure.scrollbar_width == 0.0F);
  CHECK(DeadBand(measure.usable_width) == 0.0F);
}

TEST_CASE("Sac : onglets couchés, la grille tombe juste aussi", "[ui][sac]") {
  // 🔴 LA BANDE CHANGE D'AXE, DONC LE CHROME AUSSI. Debout, elle prend de la
  // LARGEUR à la grille ; couchée, de la HAUTEUR — et c'est la largeur que le
  // calage sur les cases surveille. Une mesure restée sur l'ancien axe laisserait
  // la bande morte que ces essais existent pour interdire.
  const GridMeasure measure = DrawInventoryFor(kEnoughToOverflow, /*tabs_vertical=*/false);
  REQUIRE(measure.found);
  REQUIRE(measure.scrollbar_width > 0.0F);
  CHECK(DeadBand(measure.usable_width) == 0.0F);

  // ⚠ ET LA GRILLE COUCHÉE EST PLUS LARGE QUE LA DEBOUT : la bande ne lui prend
  // plus sa colonne de gauche. Sans cette comparaison, l'essai passerait aussi
  // sur une disposition qui n'aurait pas bougé.
  const GridMeasure standing = DrawInventoryFor(kEnoughToOverflow);
  REQUIRE(standing.found);
  CHECK(measure.usable_width > standing.usable_width);
}

TEST_CASE("Sac : en placement libre, la grille offre des cases OÙ POSER", "[ui][sac]") {
  // 🔴 LE DÉFAUT QUE CET ESSAI TIENT, ET IL S'EST VU EN JEU : la grille ne
  // dessinait des cases que pour les OBJETS. Un sac de trois objets n'offrait
  // donc que trois cibles — on ne pouvait qu'ÉCHANGER deux objets, jamais poser
  // sur une place libre, alors que c'est tout l'objet du réglage. Le fond pavé
  // qu'on voyait dessous n'est qu'un décor : il ne reçoit rien.
  const GridMeasure roomy = DrawInventoryFor(kTooFewToOverflow, /*tabs_vertical=*/true,
                                             /*free_layout=*/true);
  REQUIRE(roomy.found);
  // La grille couvre ce qu'elle montre, à moins d'une case près — le reliquat
  // que le calage n'a pas pu donner à une ligne entière.
  CHECK(roomy.content_height + ro::ui::kItemCellPixels > roomy.visible_height);
  // 🔴 ET PAS UNE CASE DE PLUS : un contenu plus haut que ce qui se voit ouvre
  // un ascenseur qui n aurait rien à faire défiler, et cet ascenseur mange la
  // poignée de redimensionnement du coin bas droit.
  CHECK(roomy.scrollbar_width == 0.0F);

  // ⚠ ET RIEN NE CHANGE QUAND LE RÉGLAGE EST ÉTEINT : trois objets tiennent sur
  // une ligne, et la grille s'arrête là. Sans cette moitié-ci, l'essai passerait
  // aussi sur une fenêtre qui remplirait TOUJOURS sa zone.
  const GridMeasure tight = DrawInventoryFor(kTooFewToOverflow);
  REQUIRE(tight.found);
  CHECK(tight.content_height == ro::ui::kItemCellPixels);
  CHECK(roomy.content_height > tight.content_height);
}

TEST_CASE("Charrette : l'ascenseur ne laisse pas de bande morte", "[ui][charrette]") {
  // Elle porte la MÊME grille que le sac, donc la même faute et la même règle.
  const GridMeasure measure = DrawCartFor(kEnoughToOverflow);
  REQUIRE(measure.found);
  REQUIRE(measure.scrollbar_width > 0.0F);
  CHECK(DeadBand(measure.usable_width) == 0.0F);
}
