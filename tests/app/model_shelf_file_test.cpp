// LES FAVORIS ET LES RÉCENTS DE LA BIBLIOTHÈQUE DE MODÈLES : les gestes les
// rangent, le fichier les garde tels quels, et un fichier abîmé se refuse.

#include "world/model_shelf_file.h"

#include <cstddef>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using moonclient::ModelShelf;
using moonclient::ReadModelShelf;
using moonclient::RememberRecent;
using moonclient::ToggleFavorite;
using moonclient::WriteModelShelf;

namespace {

constexpr float kCellPx = 128.0F;

}  // namespace

TEST_CASE("Étagère de modèles : un favori se bascule, la liste reste triée", "[app][map_editor]") {
  ModelShelf shelf;
  CHECK(ToggleFavorite(shelf, "대만\\대만_절벽1.rsm"));
  CHECK(ToggleFavorite(shelf, "pont.rsm"));
  CHECK(shelf.favorites == std::vector<std::string>{"pont.rsm", "대만\\대만_절벽1.rsm"});
  CHECK(ToggleFavorite(shelf, "pont.rsm"));
  CHECK(shelf.favorites == std::vector<std::string>{"대만\\대만_절벽1.rsm"});
}

TEST_CASE("Étagère de modèles : le dernier posé passe en tête, sans doublon ni débord",
          "[app][map_editor]") {
  ModelShelf shelf;
  RememberRecent(shelf, "a.rsm");
  RememberRecent(shelf, "b.rsm");
  RememberRecent(shelf, "a.rsm");
  CHECK(shelf.recents == std::vector<std::string>{"a.rsm", "b.rsm"});
  for (std::size_t rank = 0; rank < ModelShelf::kRecentLimit; ++rank) {
    RememberRecent(shelf, std::to_string(rank) + ".rsm");
  }
  CHECK(shelf.recents.size() == ModelShelf::kRecentLimit);
  CHECK(shelf.recents.front() == std::to_string(ModelShelf::kRecentLimit - 1) + ".rsm");
}

TEST_CASE("Étagère de modèles : le fichier fait l'aller-retour", "[app][map_editor]") {
  ModelShelf shelf;
  static_cast<void>(ToggleFavorite(shelf, "대만\\대만_절벽1.rsm"));
  static_cast<void>(ToggleFavorite(shelf, "pont.rsm"));
  RememberRecent(shelf, "프론테라\\Fountain.rsm");
  RememberRecent(shelf, "pont.rsm");
  shelf.cell_px = kCellPx;
  const auto read = ReadModelShelf(WriteModelShelf(shelf));
  REQUIRE(read.has_value());
  CHECK(*read == shelf);
}

TEST_CASE("Étagère de modèles : la lecture range ce qu'un humain a écrit de travers",
          "[app][map_editor]") {
  const auto read = ReadModelShelf(
      "favorites: [z.rsm, a.rsm, z.rsm]\n"
      "recents: [b.rsm, a.rsm, b.rsm]\n"
      "cell_px: -4\n");
  REQUIRE(read.has_value());
  CHECK(read->favorites == std::vector<std::string>{"a.rsm", "z.rsm"});
  CHECK(read->recents == std::vector<std::string>{"b.rsm", "a.rsm"});
  CHECK(read->cell_px == 0.0F);
  // Un texte vide est une étagère vide ; un texte que le YAML refuse, une
  // erreur — le fichier ne se réécrira pas.
  REQUIRE(ReadModelShelf("").has_value());
  CHECK(ReadModelShelf("").value() == ModelShelf{});
  CHECK_FALSE(ReadModelShelf("favorites: [a.rsm\n").has_value());
}
