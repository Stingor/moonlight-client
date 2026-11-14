// Les pièces du skin RO, lues depuis un client synthétique.

#include "ro/render/skin_pieces.h"

#include <algorithm>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/vfs/encoding.h"

#include "support/grf_builder.h"
#include "support/image_builder.h"
#include "support/temp_directory.h"

using ro::render::LoadSkinPieces;
using ro::render::SkinPiece;
using ro::render::SkinPiecePath;
using ro::render::SkinPieces;
using ro::test_support::BmpPixel;
using ro::test_support::BuildBmp;
using ro::test_support::GrfFileSpec;
using ro::test_support::TempDirectory;
using ro::vfs::Vfs;

namespace {

// Un carré de deux sur deux : trois pixels colorés, un magenta.
//
// ⚠ LE MAGENTA EST LA TRANSPARENCE DES TEXTURES DU JEU, antérieure au canal
// alpha. Une pièce chargée sans cette conversion montre des coins rose vif là
// où le client ne montre rien.
constexpr BmpPixel kMagenta{.red = 255, .green = 0, .blue = 255};
constexpr BmpPixel kOpaque{.red = 10, .green = 20, .blue = 30};

[[nodiscard]] std::vector<std::uint8_t> SmallPiece() {
  return BuildBmp(2, 2, {kOpaque, kMagenta, kOpaque, kOpaque});
}

// Le chemin complet d'une pièce, tel que NOS SOURCES l'écrivent : en UTF-8.
[[nodiscard]] std::string PathOf(SkinPiece piece) {
  return std::string(ro::render::kSkinRoot) + std::string(SkinPiecePath(piece));
}

// Le même chemin, tel que L'ARCHIVE le range.
//
// ⚠ UN GRF NOMME SES ENTRÉES EN CP949, et c'est toute la raison d'être de cette
// fonction. Nos sources écrivent le dossier d'interface en coréen UTF-8 — seule
// forme lisible — et le VFS convertit à la lecture. Un essai qui rangerait les
// octets UTF-8 dans l'archive n'y retrouverait rien, et accuserait le chargeur.
[[nodiscard]] std::string ArchivePathOf(SkinPiece piece) {
  auto cp949 = ro::vfs::Utf8ToCp949(PathOf(piece));
  REQUIRE(cp949.has_value());
  return *cp949;
}

}  // namespace

TEST_CASE("Skin : les pièces se lisent, et le magenta disparaît", "[render][skin]") {
  const TempDirectory client;
  client.WriteFile("habillage.grf",
                   ro::test_support::BuildGrf({
                       GrfFileSpec{ArchivePathOf(SkinPiece::kTitleBarLeft), SmallPiece()},
                       GrfFileSpec{ArchivePathOf(SkinPiece::kTitleBarMiddle), SmallPiece()},
                   }));

  auto vfs = Vfs::Mount({.root = client.path(), .archives = {"habillage.grf"}});
  REQUIRE(vfs.has_value());
  const SkinPieces pieces = LoadSkinPieces(*vfs);

  const ro::formats::Image* left = pieces.Get(SkinPiece::kTitleBarLeft);
  REQUIRE(left != nullptr);
  CHECK(left->width == 2);
  CHECK(left->height == 2);
  // Le second pixel de la première rangée était magenta : il est devenu
  // transparent, et le premier est resté opaque.
  CHECK(left->pixels[3] == 255);
  CHECK(left->pixels[7] == 0);

  CHECK(pieces.Get(SkinPiece::kTitleBarMiddle) != nullptr);
}

TEST_CASE("Skin : une pièce absente se dit, et n'empêche rien", "[render][skin]") {
  // ⚠ REFUSER DE DÉMARRER POUR UN BITMAP DE BORDURE serait hors de proportion.
  // Ce qui manque se DIT, une fois, et l'interface se rabat sur un aplat : une
  // fenêtre nue s'explique alors, au lieu de passer pour une panne de rendu.
  const TempDirectory client;
  client.WriteFile("habillage.grf",
                   ro::test_support::BuildGrf({
                       GrfFileSpec{ArchivePathOf(SkinPiece::kTitleBarLeft), SmallPiece()},
                   }));

  auto vfs = Vfs::Mount({.root = client.path(), .archives = {"habillage.grf"}});
  REQUIRE(vfs.has_value());
  const SkinPieces pieces = LoadSkinPieces(*vfs);

  CHECK(pieces.Get(SkinPiece::kTitleBarLeft) != nullptr);
  CHECK(pieces.Get(SkinPiece::kTitleBarRight) == nullptr);
  CHECK_FALSE(pieces.complete());
  // Tout ce qui manque est nommé, par son chemin complet.
  CHECK(pieces.missing.size() == ro::render::kSkinPieceCount - 1);
  CHECK(std::ranges::find(pieces.missing, PathOf(SkinPiece::kTitleBarRight)) !=
        pieces.missing.end());
}

TEST_CASE("Skin : chaque pièce a son chemin, et aucun ne se répète", "[render][skin]") {
  // Un chemin recopié d'une entrée à l'autre ferait dessiner deux fois le même
  // morceau, et le défaut ne se verrait qu'à l'œil, sur une fenêtre.
  std::vector<std::string_view> paths;
  for (std::size_t rank = 0; rank < ro::render::kSkinPieceCount; ++rank) {
    const std::string_view path = SkinPiecePath(static_cast<SkinPiece>(rank));
    CHECK_FALSE(path.empty());
    paths.push_back(path);
  }
  std::ranges::sort(paths);
  CHECK(std::ranges::adjacent_find(paths) == paths.end());
}
