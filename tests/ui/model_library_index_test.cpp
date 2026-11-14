// LA NAVIGATION DE LA BIBLIOTHÈQUE DE MODÈLES : l'arbre des dossiers, le
// filtre, et les cases de la grille qui se voient et se demandent.

#include "ro/ui/model_library_index.h"

#include <cstddef>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using ro::ui::BuildModelLibraryTree;
using ro::ui::FilterModelFiles;
using ro::ui::GridColumns;
using ro::ui::GridSpan;
using ro::ui::ModelFolder;
using ro::ui::ModelLibraryTree;
using ro::ui::ModelPassesFilter;
using ro::ui::ModelStem;
using ro::ui::VignetteRequestOrder;
using ro::ui::VisibleGridCells;

namespace {

const std::vector<std::string> kFiles = {
    "pont.rsm",
    "대만\\대만_절벽1.rsm",
    "대만\\대만_절벽2.rsm",
    "대만\\집\\지붕.rsm2",
    "프론테라\\Fountain.rsm",
};

const ModelFolder& Child(const ModelLibraryTree& tree, std::size_t parent, std::size_t rank) {
  return tree.folders[tree.folders[parent].children[rank]];
}

}  // namespace

TEST_CASE("Bibliothèque : l'arbre des dossiers compte les modèles de chacun",
          "[ui][model_library]") {
  const ModelLibraryTree tree = BuildModelLibraryTree(kFiles);
  REQUIRE(tree.folders.size() == 4);
  const ModelFolder& root = tree.folders[0];
  CHECK(root.prefix.empty());
  CHECK(root.model_count == kFiles.size());
  REQUIRE(root.children.size() == 2);
  const ModelFolder& taiwan = Child(tree, 0, 0);
  CHECK(taiwan.name == "대만");
  CHECK(taiwan.prefix == "대만\\");
  // Ses deux modèles, et celui de son sous-dossier.
  CHECK(taiwan.model_count == 3);
  REQUIRE(taiwan.children.size() == 1);
  const ModelFolder& house = tree.folders[taiwan.children[0]];
  CHECK(house.prefix == "대만\\집\\");
  CHECK(house.model_count == 1);
  CHECK(Child(tree, 0, 1).model_count == 1);
}

TEST_CASE("Bibliothèque : le filtre veut chaque mot, sans égard à la casse",
          "[ui][model_library]") {
  CHECK(ModelPassesFilter("프론테라\\Fountain.rsm", ""));
  CHECK(ModelPassesFilter("프론테라\\Fountain.rsm", "fount"));
  CHECK(ModelPassesFilter("프론테라\\Fountain.rsm", "RSM  fount"));
  CHECK_FALSE(ModelPassesFilter("프론테라\\Fountain.rsm", "fount rsm2"));
  CHECK(ModelPassesFilter("대만\\대만_절벽1.rsm", "절벽"));
}

TEST_CASE("Bibliothèque : un dossier montre ses modèles et ceux de ses sous-dossiers",
          "[ui][model_library]") {
  CHECK(FilterModelFiles(kFiles, "", "") == std::vector<std::size_t>{0, 1, 2, 3, 4});
  CHECK(FilterModelFiles(kFiles, "대만\\", "") == std::vector<std::size_t>{1, 2, 3});
  CHECK(FilterModelFiles(kFiles, "대만\\", "rsm2") == std::vector<std::size_t>{3});
  CHECK(FilterModelFiles(kFiles, "프론테라\\", "pont").empty());
}

TEST_CASE("Grille : les colonnes qui tiennent, une au moins", "[ui][model_library]") {
  // 3 cases de 100 et 2 écarts de 10 font 320.
  CHECK(GridColumns(320.0F, 100.0F, 10.0F) == 3);
  CHECK(GridColumns(319.0F, 100.0F, 10.0F) == 2);
  CHECK(GridColumns(20.0F, 100.0F, 10.0F) == 1);
}

TEST_CASE("Grille : seules les rangées visibles se posent", "[ui][model_library]") {
  // 25 cases en 4 colonnes : 7 rangées de 50 pixels.
  const GridSpan top = VisibleGridCells(25, 4, 50.0F, 0.0F, 100.0F);
  CHECK(top.first == 0);
  CHECK(top.end == 8);
  // Défilée de 75 pixels : la rangée 1 à moitié, la 2 entière, la 3 à moitié.
  const GridSpan middle = VisibleGridCells(25, 4, 50.0F, 75.0F, 100.0F);
  CHECK(middle.first == 4);
  CHECK(middle.end == 16);
  // Au bout, la dernière rangée est incomplète.
  const GridSpan bottom = VisibleGridCells(25, 4, 50.0F, 300.0F, 100.0F);
  CHECK(bottom.first == 24);
  CHECK(bottom.end == 25);
  CHECK(VisibleGridCells(0, 4, 50.0F, 0.0F, 100.0F).empty());
}

TEST_CASE("Grille : les vignettes visibles se demandent d'abord, puis leurs voisines",
          "[ui][model_library]") {
  // 3 colonnes ; les rangées 2 et 3 se voient (cases 6 à 11).
  const GridSpan visible{.first = 6, .end = 12};
  const std::vector<std::size_t> order = VignetteRequestOrder(visible, 30, 3, 1, 1, 100);
  const std::vector<std::size_t> expected = {6, 7, 8, 9, 10, 11, 12, 13, 14, 5, 4, 3};
  CHECK(order == expected);
  // Le plafond coupe les voisines avant les visibles.
  CHECK(VignetteRequestOrder(visible, 30, 3, 1, 1, 4) == std::vector<std::size_t>{6, 7, 8, 9});
  // Au bout de la liste, rien au-delà.
  CHECK(VignetteRequestOrder(GridSpan{.first = 27, .end = 30}, 30, 3, 2, 0, 100) ==
        std::vector<std::size_t>{27, 28, 29});
}

TEST_CASE("Bibliothèque : une case écrit le nom du modèle, sans dossier ni extension",
          "[ui][model_library]") {
  CHECK(ModelStem("대만\\집\\지붕.rsm2") == "지붕");
  CHECK(ModelStem("pont.rsm") == "pont");
  CHECK(ModelStem(".rsm") == ".rsm");
}
