#include "ro/formats/bmp.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/error.h"
#include "ro/formats/image.h"

#include "support/image_builder.h"

using ro::core::ErrorCode;
using ro::formats::ApplyMagentaColorKey;
using ro::formats::ApplyTextureColorKey;
using ro::formats::DecodeBmp;
using ro::formats::DecodeImage;
using ro::formats::Image;
using ro::formats::ImageFile;
using ro::formats::MattePaperBackground;
using ro::formats::ReduceToInkShape;
using ro::formats::RemapForDarkPaper;
using ro::test_support::BmpBuildOptions;
using ro::test_support::BmpPixel;
using ro::test_support::BuildBmp;

namespace {

// Un damier 2 × 2 dont les quatre pixels sont distincts : n'importe quelle
// inversion de rangée, de colonne ou de canal se voit immédiatement.
//
//   rouge  vert
//   bleu   blanc
const std::vector<BmpPixel> kQuad = {
    {255, 0, 0},
    {0, 255, 0},
    {0, 0, 255},
    {255, 255, 255},
};

void CheckQuad(const Image& image) {
  REQUIRE(image.width == 2);
  REQUIRE(image.height == 2);
  REQUIRE(image.pixels.size() == 2 * 2 * 4);

  const auto pixel_at = [&image](std::size_t x, std::size_t y) {
    const std::size_t offset = (y * 2 + x) * 4;
    return std::vector<std::uint8_t>(
        image.pixels.begin() + static_cast<std::ptrdiff_t>(offset),
        image.pixels.begin() + static_cast<std::ptrdiff_t>(offset + 4));
  };

  CHECK(pixel_at(0, 0) == std::vector<std::uint8_t>{255, 0, 0, 255});
  CHECK(pixel_at(1, 0) == std::vector<std::uint8_t>{0, 255, 0, 255});
  CHECK(pixel_at(0, 1) == std::vector<std::uint8_t>{0, 0, 255, 255});
  CHECK(pixel_at(1, 1) == std::vector<std::uint8_t>{255, 255, 255, 255});
}

}  // namespace

TEST_CASE("BMP 8 bits palettisé", "[formats][bmp]") {
  // La forme réellement employée par les textures de terrain : les 70 relevées
  // sur cinq cartes officielles sont toutes en 8 bits non compressés.
  auto image = DecodeBmp(BuildBmp(2, 2, kQuad, BmpBuildOptions{.bits_per_pixel = 8}));
  REQUIRE(image.has_value());
  CheckQuad(*image);
}

TEST_CASE("BMP 24 et 32 bits", "[formats][bmp]") {
  SECTION("24 bits") {
    auto image = DecodeBmp(BuildBmp(2, 2, kQuad, BmpBuildOptions{.bits_per_pixel = 24}));
    REQUIRE(image.has_value());
    CheckQuad(*image);
  }

  SECTION("32 bits : le quatrième octet n'est PAS un alpha") {
    // En BI_RGB il est officiellement inutilisé et vaut souvent zéro. Le
    // prendre pour un alpha rendrait toute l'image transparente.
    auto image = DecodeBmp(BuildBmp(2, 2, kQuad, BmpBuildOptions{.bits_per_pixel = 32}));
    REQUIRE(image.has_value());
    CheckQuad(*image);
  }
}

TEST_CASE("BMP : le sens des rangées", "[formats][bmp]") {
  // Un BMP de bas en haut et son équivalent de haut en bas doivent produire la
  // MÊME image. Rien d'autre que la comparaison pixel à pixel ne le vérifie :
  // les dimensions sont identiques dans les deux cas.
  auto bottom_up = DecodeBmp(BuildBmp(2, 2, kQuad, BmpBuildOptions{.bottom_up = true}));
  auto top_down = DecodeBmp(BuildBmp(2, 2, kQuad, BmpBuildOptions{.bottom_up = false}));
  REQUIRE(bottom_up.has_value());
  REQUIRE(top_down.has_value());

  CheckQuad(*bottom_up);
  CheckQuad(*top_down);
  CHECK(bottom_up->pixels == top_down->pixels);
}

TEST_CASE("BMP : le remplissage des rangées est ignoré", "[formats][bmp]") {
  // Une largeur de 3 en 24 bits fait 9 octets par rangée, complétés à 12. Se
  // tromper de pas décale toutes les rangées suivantes en diagonale.
  const std::vector<BmpPixel> row = {{10, 20, 30}, {40, 50, 60}, {70, 80, 90},
                                     {11, 22, 33}, {44, 55, 66}, {77, 88, 99}};
  auto image = DecodeBmp(BuildBmp(3, 2, row, BmpBuildOptions{.bits_per_pixel = 24}));
  REQUIRE(image.has_value());
  REQUIRE(image->width == 3);
  REQUIRE(image->height == 2);

  CHECK(image->pixels[0] == 10);
  CHECK(image->pixels[1] == 20);
  CHECK(image->pixels[2] == 30);
  // Premier pixel de la seconde rangée : 3 pixels × 4 octets plus loin.
  CHECK(image->pixels[12] == 11);
  CHECK(image->pixels[13] == 22);
  CHECK(image->pixels[14] == 33);
}

TEST_CASE("BMP : formats refusés explicitement", "[formats][bmp]") {
  SECTION("compressé") {
    auto image = DecodeBmp(BuildBmp(2, 2, kQuad, BmpBuildOptions{.compression = 1}));
    REQUIRE_FALSE(image.has_value());
    CHECK(image.error().code == ErrorCode::kUnsupportedVersion);
  }

  SECTION("profondeur non gérée") {
    auto image = DecodeBmp(BuildBmp(2, 2, kQuad, BmpBuildOptions{.bits_per_pixel = 4}));
    REQUIRE_FALSE(image.has_value());
    CHECK(image.error().code == ErrorCode::kUnsupportedVersion);
  }

  SECTION("en-tête d'information trop court") {
    auto image = DecodeBmp(BuildBmp(2, 2, kQuad, BmpBuildOptions{.info_header_size = 12}));
    REQUIRE_FALSE(image.has_value());
    CHECK(image.error().code == ErrorCode::kUnsupportedVersion);
  }

  SECTION("hauteur au plus petit entier 32 bits") {
    // Sa négation déborde : le refus doit venir AVANT, sans comportement
    // indéfini — c'est ce que l'essai sous UBSan vérifie.
    constexpr std::size_t kHeightOffset = 22;
    std::vector<std::uint8_t> bmp = BuildBmp(2, 2, kQuad);
    bmp[kHeightOffset] = 0x00;
    bmp[kHeightOffset + 1] = 0x00;
    bmp[kHeightOffset + 2] = 0x00;
    bmp[kHeightOffset + 3] = 0x80;
    auto image = DecodeBmp(bmp);
    REQUIRE_FALSE(image.has_value());
    CHECK(image.error().code == ErrorCode::kCorruptData);
  }

  SECTION("signature invalide") {
    std::vector<std::uint8_t> bmp = BuildBmp(2, 2, kQuad);
    bmp[1] = 'X';
    auto image = DecodeBmp(bmp);
    REQUIRE_FALSE(image.has_value());
    CHECK(image.error().code == ErrorCode::kBadSignature);
  }
}

TEST_CASE("BMP : un en-tête V5 se lit comme un V3", "[formats][bmp]") {
  // Les en-têtes plus longs que 40 octets ajoutent des champs qu'on n'utilise
  // pas ; l'offset des pixels reste la seule chose qui compte.
  auto image = DecodeBmp(BuildBmp(2, 2, kQuad, BmpBuildOptions{.info_header_size = 124}));
  REQUIRE(image.has_value());
  CheckQuad(*image);
}

TEST_CASE("ApplyMagentaColorKey", "[formats][bmp]") {
  const std::vector<BmpPixel> with_magenta = {
      {255, 0, 255},
      {255, 0, 0},
      {0, 255, 0},
      {254, 0, 255},  // presque magenta : ne doit PAS être touché
  };
  auto image = DecodeBmp(BuildBmp(2, 2, with_magenta, BmpBuildOptions{.bits_per_pixel = 24}));
  REQUIRE(image.has_value());

  ApplyMagentaColorKey(*image);
  CHECK(image->pixels[3] == 0);     // magenta pur : transparent
  CHECK(image->pixels[7] == 255);   // rouge : intact
  CHECK(image->pixels[11] == 255);  // vert : intact
  CHECK(image->pixels[15] == 255);  // presque magenta : intact
}

TEST_CASE("ApplyMagentaColorKey : seul l'alpha tombe, la couleur reste", "[formats][bmp]") {
  // 🔴 LA CLÉ NE REPEINT RIEN : le pixel effacé garde son magenta, et c'est le
  // téléversement qui le remplit, parce que c'est lui qui choisit le filtrage.
  // Repeindre ici doublerait un travail que le téléversement écrase.
  const std::vector<BmpPixel> with_magenta = {
      {255, 0, 255},
      {255, 0, 0},
      {0, 255, 0},
      {254, 0, 255},
  };
  auto image = DecodeBmp(BuildBmp(2, 2, with_magenta, BmpBuildOptions{.bits_per_pixel = 24}));
  REQUIRE(image.has_value());
  ApplyMagentaColorKey(*image);

  CHECK(image->pixels == std::vector<std::uint8_t>{255, 0, 255, 0,  //
                                                   255, 0, 0, 255,  //
                                                   0, 255, 0, 255,  //
                                                   254, 0, 255, 255});
}

TEST_CASE("ApplyTextureColorKey : le magenta à cinq bits près devient un texel noir et vide",
          "[formats][bmp]") {
  // Le jeu téléverse ces textures en cinq bits par canal : tout ce qui y tombe
  // sur le magenta disparaît, et le texel vide qu'il écrit est NOIR.
  const std::vector<BmpPixel> near_magenta = {
      {255, 0, 255},  // magenta pur
      {248, 7, 248},  // le plus loin que la clé attrape
      {247, 0, 255},  // un cran de rouge en dessous : opaque
      {255, 8, 255},  // un cran de vert au-dessus : opaque
  };
  auto image = DecodeBmp(BuildBmp(2, 2, near_magenta, BmpBuildOptions{.bits_per_pixel = 24}));
  REQUIRE(image.has_value());
  ApplyTextureColorKey(*image);

  CHECK(image->pixels == std::vector<std::uint8_t>{0, 0, 0, 0,        //
                                                   0, 0, 0, 0,        //
                                                   247, 0, 255, 255,  //
                                                   255, 8, 255, 255});
}

TEST_CASE("ApplyEffectTextureColorKey : le magenta à cinq bits part, le noir reste peint",
          "[formats][bmp]") {
  // Hors d'un `.str`, le jeu ne demande pas la clé noire : le fond noir d'un
  // éclair reste un texel opaque, que l'addition fera disparaître.
  const std::vector<BmpPixel> pixels = {
      {0, 0, 0},      // noir : opaque
      {248, 7, 248},  // magenta en cinq bits : transparent
      {247, 0, 255},  // un cran de rouge en dessous : opaque
      {255, 255, 255},
  };
  auto image =
      DecodeImage("eclair.bmp", BuildBmp(2, 2, pixels, BmpBuildOptions{.bits_per_pixel = 24}));
  REQUIRE(image.has_value());
  ro::formats::ApplyEffectTextureColorKey(*image);

  constexpr std::uint8_t kOpaque = 255;
  CHECK(image->pixels[3] == kOpaque);
  CHECK(image->pixels[7] == 0);
  CHECK(image->pixels[11] == kOpaque);
  CHECK(image->pixels[15] == kOpaque);
}

TEST_CASE("ApplyInterfaceColorKey : le presque-magenta d'une minimap part, sa couleur reste",
          "[formats][bmp]") {
  const std::vector<BmpPixel> pixels = {
      {255, 2, 255},  // le vide de xmas_fild01 : transparent
      {0, 0, 0},      // noir : opaque
      {247, 0, 255},  // un cran de rouge sous la clé : opaque
      {255, 0, 255},
  };
  auto image =
      DecodeImage("minimap.bmp", BuildBmp(2, 2, pixels, BmpBuildOptions{.bits_per_pixel = 24}));
  REQUIRE(image.has_value());
  ro::formats::ApplyInterfaceColorKey(*image);

  CHECK(image->pixels == std::vector<std::uint8_t>{255, 2, 255, 0,    //
                                                   0, 0, 0, 255,      //
                                                   247, 0, 255, 255,  //
                                                   255, 0, 255, 0});
}

TEST_CASE("DecodeImage : l'image garde le format de son fichier", "[formats][bmp]") {
  const std::vector<BmpPixel> pixel = {{1, 2, 3}};
  auto image = DecodeImage("Motif.BMP", BuildBmp(1, 1, pixel));
  REQUIRE(image.has_value());
  CHECK(image->file == ImageFile::kBmp);
  // Un décodeur appelé seul ne sait pas d'où viennent ses octets.
  CHECK(DecodeBmp(BuildBmp(1, 1, pixel))->file == ImageFile::kOther);
}

TEST_CASE("BMP : aucune troncature ne fait planter le décodeur", "[formats][bmp]") {
  const std::vector<std::uint8_t> complete = BuildBmp(4, 4, std::vector<BmpPixel>(16, {1, 2, 3}));
  for (std::size_t length = 0; length <= complete.size(); ++length) {
    const std::vector<std::uint8_t> truncated(
        complete.begin(), complete.begin() + static_cast<std::ptrdiff_t>(length));
    auto image = DecodeBmp(truncated);
    if (image.has_value()) {
      CHECK(image->pixels.size() ==
            static_cast<std::size_t>(image->width) * static_cast<std::size_t>(image->height) * 4);
    }
  }
  SUCCEED("aucune troncature n'a provoqué de lecture hors bornes");
}

namespace {

// UNE ILLUSTRATION SUR PAPIER : du blanc, et ce qu'on y peint pixel par pixel.
struct Paper {
  Image image;

  Paper(int width, int height) {
    image.width = width;
    image.height = height;
    image.pixels.assign(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4,
                        255);
  }
  void Paint(int x, int y, std::uint8_t grey) {
    std::uint8_t* pixel = At(x, y);
    pixel[0] = grey;
    pixel[1] = grey;
    pixel[2] = grey;
  }
  [[nodiscard]] std::uint8_t* At(int x, int y) {
    const auto row = static_cast<std::size_t>(y) * static_cast<std::size_t>(image.width);
    return image.pixels.data() + (row + static_cast<std::size_t>(x)) * 4;
  }
};

// Un canal posé sur du blanc sous son opacité : ce que la fenêtre montrera.
[[nodiscard]] int OnWhite(std::uint8_t channel, std::uint8_t alpha) {
  constexpr int kFull = 255;
  return (channel * alpha + kFull * (kFull - alpha) + kFull / 2) / kFull;
}

constexpr std::uint8_t kDark = 30;
constexpr std::uint8_t kShadowGrey = 200;

}  // namespace

TEST_CASE("Détourage : le papier tombe, l'objet reste", "[formats][matte]") {
  Paper paper(7, 7);
  for (int y = 2; y <= 4; ++y) {
    for (int x = 2; x <= 4; ++x) {
      paper.Paint(x, y, kDark);
    }
  }
  MattePaperBackground(paper.image);
  CHECK(paper.At(0, 0)[3] == 0);
  CHECK(paper.At(6, 6)[3] == 0);
  CHECK(paper.At(3, 3)[3] == 255);
  CHECK(paper.At(3, 3)[0] == kDark);
}

TEST_CASE("Détourage : une ombre isolée devient translucide", "[formats][matte]") {
  // UN AMAS DE GRIS QUE SEUL LE PAPIER ENTOURE : une couche posée sur le blanc,
  // qui devient un noir translucide — et redonne son gris sur du blanc.
  Paper paper(8, 5);
  paper.Paint(3, 2, kShadowGrey);
  paper.Paint(4, 2, kShadowGrey);
  MattePaperBackground(paper.image);
  const std::uint8_t* shadow = paper.At(3, 2);
  CHECK(shadow[3] == 255 - kShadowGrey);
  CHECK(shadow[0] == 0);
  CHECK(OnWhite(shadow[0], shadow[3]) == kShadowGrey);
}

TEST_CASE("Détourage : une partie claire de l'objet reste opaque", "[formats][matte]") {
  // ⚠ UN GRIS QUI TOUCHE L'OBJET EN EST UNE PARTIE : l'acier d'une jambière, la
  // face d'une pièce. Le traiter en ombre le viderait.
  Paper paper(7, 7);
  for (int y = 2; y <= 4; ++y) {
    for (int x = 2; x <= 4; ++x) {
      paper.Paint(x, y, kDark);
    }
  }
  paper.Paint(3, 3, kShadowGrey);
  MattePaperBackground(paper.image);
  CHECK(paper.At(3, 3)[3] == 255);
  CHECK(paper.At(3, 3)[0] == kShadowGrey);
}

TEST_CASE("Détourage : le liseré perd son blanc, et l'image revient sur du blanc",
          "[formats][matte]") {
  // LE LISERÉ D'UN OBJET, mi-chemin entre son trait et le papier : posé tel quel
  // sur un fond sombre, il ferait un halo clair. Sa couleur se nettoie du blanc
  // qui s'y mêlait, et sur du blanc il redonne son gris.
  constexpr std::uint8_t kFringe = 230;
  Paper paper(7, 7);
  paper.Paint(3, 3, kDark);
  paper.Paint(2, 3, kFringe);
  MattePaperBackground(paper.image);
  const std::uint8_t* fringe = paper.At(2, 3);
  CHECK(fringe[3] > 0);
  CHECK(fringe[3] < 255);
  CHECK(fringe[0] < kFringe);
  const int shown = OnWhite(fringe[0], fringe[3]);
  CHECK(shown >= kFringe - 1);
  CHECK(shown <= kFringe + 1);
}

TEST_CASE("Retourné pour un papier sombre : le blanc tombe, un trait noir devient blanc",
          "[formats][matte]") {
  // UN CADRE D'INTERFACE : du blanc, un trait noir, et le magenta du client.
  Paper paper(3, 1);
  paper.Paint(1, 0, 0);
  paper.At(2, 0)[3] = 0;
  RemapForDarkPaper(paper.image);
  CHECK(paper.At(0, 0)[3] == 0);
  CHECK(paper.At(1, 0)[0] == 255);
  CHECK(paper.At(1, 0)[3] == 255);
  CHECK(paper.At(2, 0)[3] == 0);
}

TEST_CASE("Réduit à sa forme : l'encre pleine, le liseré adouci, le papier tombe",
          "[formats][matte]") {
  // UN LOGO D'UNE ENCRE BLEUE lissé contre du blanc : le papier, un liseré
  // pâle, trois pixels d'encre, un plus foncé, et le magenta du client.
  constexpr std::array<std::uint8_t, 3> kInk = {88, 101, 242};
  constexpr std::array<std::uint8_t, 3> kFringe = {159, 167, 247};
  constexpr std::array<std::uint8_t, 3> kDeeper = {60, 70, 230};
  Paper paper(7, 1);
  const auto paint = [&paper](int x, const std::array<std::uint8_t, 3>& color) {
    std::copy(color.begin(), color.end(), paper.At(x, 0));
  };
  paint(1, kFringe);
  paint(2, kInk);
  paint(3, kInk);
  paint(4, kInk);
  paint(5, kDeeper);
  paper.At(6, 0)[3] = 0;
  ReduceToInkShape(paper.image);
  CHECK(paper.At(0, 0)[3] == 0);
  CHECK(paper.At(1, 0)[3] > 0);
  CHECK(paper.At(1, 0)[3] < 255);
  CHECK(paper.At(3, 0)[3] == 255);
  CHECK(paper.At(5, 0)[3] == 255);
  CHECK(paper.At(6, 0)[3] == 0);
  // ⚠ TOUT EST BLANC : c'est la teinte du dessin qui donnera la couleur.
  for (int x = 0; x < 7; ++x) {
    CHECK(paper.At(x, 0)[0] == 255);
    CHECK(paper.At(x, 0)[1] == 255);
    CHECK(paper.At(x, 0)[2] == 255);
  }
}
