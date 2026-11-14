#include "ro/formats/spr.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/palette.h"

#include "support/image_builder.h"
#include "support/sprite_builder.h"

using ro::formats::ApplyPalette;
using ro::formats::DecodePalette;
using ro::formats::Image;
using ro::formats::SprSprite;
using ro::test_support::BuildPal;
using ro::test_support::BuildSpr;
using ro::test_support::PixelAt;
using ro::test_support::SprBgraFrameSpec;
using ro::test_support::SprBuildOptions;
using ro::test_support::SprIndexedFrameSpec;
using ro::test_support::SpriteColor;

namespace {

// Une palette dont chaque entrée est reconnaissable à l'œil comme au test.
// L'entrée 0 porte une VRAIE couleur, comme dans les fichiers du jeu : c'est
// elle que la convention de transparence doit écraser.
std::vector<SpriteColor> Ramp() {
  return {
      SpriteColor{.red = 0, .green = 200, .blue = 0},   // index 0 : vert vif
      SpriteColor{.red = 255, .green = 0, .blue = 0},   // 1
      SpriteColor{.red = 0, .green = 0, .blue = 255},   // 2
      SpriteColor{.red = 10, .green = 20, .blue = 30},  // 3
  };
}

// Un motif ASYMÉTRIQUE : toute inversion de rangée ou de colonne se voit.
SprIndexedFrameSpec Corner() {
  return SprIndexedFrameSpec{.width = 2, .height = 2, .indices = {1, 2, 0, 3}};
}

}  // namespace

TEST_CASE("SPR 2.1 : les index survivent à la compression par plages", "[formats][spr]") {
  SprBuildOptions options;
  options.indexed = {Corner()};
  options.palette = Ramp();

  auto sprite = SprSprite::Parse(BuildSpr(options));
  REQUIRE(sprite.has_value());
  CHECK(sprite->version() == 21);
  REQUIRE(sprite->indexed_frames().size() == 1);

  const auto& frame = sprite->indexed_frames()[0];
  CHECK(frame.width == 2);
  CHECK(frame.height == 2);
  // Y compris le zéro au milieu, qui voyage comme une plage transparente.
  CHECK(frame.indices == std::vector<std::uint8_t>{1, 2, 0, 3});
}

TEST_CASE("SPR 2.0 : les index sont écrits sans compression", "[formats][spr]") {
  SprBuildOptions options;
  options.minor = 0;
  options.indexed = {Corner()};
  options.palette = Ramp();

  auto sprite = SprSprite::Parse(BuildSpr(options));
  REQUIRE(sprite.has_value());
  CHECK(sprite->version() == 20);
  REQUIRE(sprite->indexed_frames().size() == 1);
  CHECK(sprite->indexed_frames()[0].indices == std::vector<std::uint8_t>{1, 2, 0, 3});
}

TEST_CASE("SPR : une longue plage transparente se découpe", "[formats][spr]") {
  // Un compte de transparence tient sur un octet. Une image plus large que 255
  // pixels vides oblige à plusieurs paquets, et ne traiter que le premier
  // laisserait la fin de l'image à zéro sans erreur.
  SprIndexedFrameSpec frame{.width = 300, .height = 1};
  frame.indices.assign(300, 0);
  frame.indices[299] = 3;

  SprBuildOptions options;
  options.indexed = {frame};
  options.palette = Ramp();

  auto sprite = SprSprite::Parse(BuildSpr(options));
  REQUIRE(sprite.has_value());
  REQUIRE(sprite->indexed_frames().size() == 1);
  const auto& decoded = sprite->indexed_frames()[0].indices;
  REQUIRE(decoded.size() == 300);
  CHECK(decoded[0] == 0);
  CHECK(decoded[299] == 3);
}

TEST_CASE("SPR : la palette embarquée est lue au dernier kilo-octet", "[formats][spr]") {
  SprBuildOptions options;
  options.indexed = {Corner()};
  options.palette = Ramp();

  auto sprite = SprSprite::Parse(BuildSpr(options));
  REQUIRE(sprite.has_value());
  REQUIRE(sprite->palette().has_value());
  CHECK(sprite->palette()->entries[1] == std::array<std::uint8_t, 4>{255, 0, 0, 0});
  CHECK(sprite->palette()->entries[3] == std::array<std::uint8_t, 4>{10, 20, 30, 0});
}

TEST_CASE("SPR : un fichier sans image palettisée n'a pas de palette", "[formats][spr]") {
  SprBgraFrameSpec frame{.width = 1, .height = 1, .pixels = {SpriteColor{.red = 9}}};
  SprBuildOptions options;
  options.bgra = {frame};

  auto sprite = SprSprite::Parse(BuildSpr(options));
  REQUIRE(sprite.has_value());
  CHECK_FALSE(sprite->palette().has_value());
  CHECK(sprite->bgra_frames().size() == 1);
}

TEST_CASE("SPR BGRA : rangées et composantes sont remises d'aplomb", "[formats][spr]") {
  // Le format range cette section À L'ENVERS et en ordre alpha, bleu, vert,
  // rouge. Rater le sens retourne l'image, rater l'ordre donne des rayures —
  // et aucune assertion de taille ne voit ni l'un ni l'autre.
  SprBgraFrameSpec frame{.width = 2, .height = 2};
  frame.pixels = {
      SpriteColor{.red = 255, .green = 0, .blue = 0},   // haut gauche : rouge
      SpriteColor{.red = 0, .green = 255, .blue = 0},   // haut droite : vert
      SpriteColor{.red = 0, .green = 0, .blue = 255},   // bas gauche  : bleu
      SpriteColor{.red = 10, .green = 20, .blue = 30},  // bas droite
  };
  frame.alpha = {255, 255, 255, 128};

  SprBuildOptions options;
  options.bgra = {frame};

  auto sprite = SprSprite::Parse(BuildSpr(options));
  REQUIRE(sprite.has_value());
  REQUIRE(sprite->bgra_frames().size() == 1);
  const Image& image = sprite->bgra_frames()[0];

  CHECK(PixelAt(image, 0, 0) == std::vector<std::uint8_t>{255, 0, 0, 255});
  CHECK(PixelAt(image, 1, 0) == std::vector<std::uint8_t>{0, 255, 0, 255});
  CHECK(PixelAt(image, 0, 1) == std::vector<std::uint8_t>{0, 0, 255, 255});
  CHECK(PixelAt(image, 1, 1) == std::vector<std::uint8_t>{10, 20, 30, 128});
}

TEST_CASE("ApplyPalette : l'index 0 sort NOIR transparent", "[formats][spr]") {
  // Et non « sa couleur, en transparent ». Un texel transparent participe quand
  // même au filtrage : garder le vert de l'entrée 0 dessine un halo vert autour
  // de chaque sprite dès qu'il est mis à l'échelle.
  SprBuildOptions options;
  options.indexed = {Corner()};
  options.palette = Ramp();

  auto sprite = SprSprite::Parse(BuildSpr(options));
  REQUIRE(sprite.has_value());
  REQUIRE(sprite->palette().has_value());

  const Image image = ApplyPalette(sprite->indexed_frames()[0], *sprite->palette());
  CHECK(PixelAt(image, 0, 1) == std::vector<std::uint8_t>{0, 0, 0, 0});
  // Les autres index prennent leur couleur, et sont OPAQUES : le quatrième
  // octet du fichier vaut zéro et s'y fier rendrait tout invisible.
  CHECK(PixelAt(image, 0, 0) == std::vector<std::uint8_t>{255, 0, 0, 255});
  CHECK(PixelAt(image, 1, 1) == std::vector<std::uint8_t>{10, 20, 30, 255});
}

TEST_CASE("ApplyPalette : une palette externe recolore la même image", "[formats][spr]") {
  // C'est ainsi que RO teinte cheveux et vêtements : le sprite ne porte qu'une
  // couleur par défaut. Conserver les index plutôt que des couleurs résolues
  // est ce qui rend l'opération possible sans relire le fichier.
  SprBuildOptions options;
  options.indexed = {Corner()};
  options.palette = Ramp();
  auto sprite = SprSprite::Parse(BuildSpr(options));
  REQUIRE(sprite.has_value());

  auto external = DecodePalette(
      BuildPal({SpriteColor{.red = 1}, SpriteColor{.red = 0, .green = 0, .blue = 7}}));
  REQUIRE(external.has_value());

  const Image tinted = ApplyPalette(sprite->indexed_frames()[0], *external);
  CHECK(PixelAt(tinted, 0, 0) == std::vector<std::uint8_t>{0, 0, 7, 255});
}

TEST_CASE("Palette : les quatre octets excédentaires sont en queue", "[formats][palette]") {
  // Dix .pal réels font 1028 octets. Lire depuis la fin décalerait toutes les
  // couleurs d'un cran : des teintes fausses mais plausibles, que rien d'autre
  // ne signalerait.
  const std::vector<SpriteColor> colors = Ramp();
  auto exact = DecodePalette(BuildPal(colors));
  auto padded = DecodePalette(BuildPal(colors, 4));
  REQUIRE(exact.has_value());
  REQUIRE(padded.has_value());
  CHECK(exact->entries == padded->entries);
  CHECK(padded->entries[1] == std::array<std::uint8_t, 4>{255, 0, 0, 0});
}

TEST_CASE("Palette : un fichier trop court est refusé", "[formats][palette]") {
  std::vector<std::uint8_t> truncated = BuildPal(Ramp());
  truncated.resize(1023);
  CHECK_FALSE(DecodePalette(truncated).has_value());
}

TEST_CASE("SPR tronqué : les images déjà lues sont gardées, et signalées", "[formats][spr]") {
  // Un .spr réel de l'installation déclare 21 images BGRA et s'arrête au milieu
  // de la vingtième. Refuser le fichier entier perdrait un effet complet pour
  // une image manquante ; le garder sans rien dire serait pire encore.
  SprBgraFrameSpec first{.width = 2, .height = 2};
  first.pixels.assign(4, SpriteColor{.red = 200});
  SprBgraFrameSpec second = first;

  SprBuildOptions options;
  options.bgra = {first, second};
  std::vector<std::uint8_t> spr = BuildSpr(options);
  // On ampute la seconde image de la moitié de ses pixels.
  spr.resize(spr.size() - 8);

  auto sprite = SprSprite::Parse(spr);
  REQUIRE(sprite.has_value());
  CHECK(sprite->truncated());
  CHECK(sprite->bgra_frames().size() == 1);
}

TEST_CASE("SPR tronqué : un fichier sans AUCUNE image exploitable échoue", "[formats][spr]") {
  // Le pendant du cas précédent : s'il n'y a rien à sauver, la tolérance
  // masquerait un vrai défaut au lieu de le nommer.
  SprBgraFrameSpec only{.width = 4, .height = 4};
  only.pixels.assign(16, SpriteColor{.blue = 7});

  SprBuildOptions options;
  options.bgra = {only};
  std::vector<std::uint8_t> spr = BuildSpr(options);
  spr.resize(spr.size() - 40);

  CHECK_FALSE(SprSprite::Parse(spr).has_value());
}

TEST_CASE("SPR : un compte d'images exagéré ne lit pas la palette comme des pixels",
          "[formats][spr]") {
  // 🔴 LA PALETTE N'EST PAS UNE IMAGE. Sa première entrée vaut (1, 0, 1, 0) :
  // lue comme une image 2.0, c'est « un pixel sur un pixel » suivi d'un index —
  // une image plausible, qu'un lecteur couvrant tout le fichier acceptait.
  constexpr std::size_t kIndexedCountOffset = 4;
  SprBuildOptions options;
  options.major = 2;
  options.minor = 0;
  options.indexed = {Corner(), Corner()};
  options.palette = {SpriteColor{.red = 1, .green = 0, .blue = 1}};
  std::vector<std::uint8_t> spr = BuildSpr(options);
  spr[kIndexedCountOffset] = 3;

  auto sprite = SprSprite::Parse(spr);
  REQUIRE(sprite.has_value());
  CHECK(sprite->indexed_frames().size() == 2);
  CHECK(sprite->truncated());
}

TEST_CASE("SPR : les fichiers malformés sont refusés", "[formats][spr]") {
  SprBuildOptions options;
  options.indexed = {Corner()};
  options.palette = Ramp();

  SECTION("signature absente") {
    std::vector<std::uint8_t> spr = BuildSpr(options);
    spr[0] = 'X';
    CHECK_FALSE(SprSprite::Parse(spr).has_value());
  }
  SECTION("version 3.2, non éprouvable faute de fichier réel") {
    SprBuildOptions zlib_options = options;
    zlib_options.major = 3;
    zlib_options.minor = 2;
    CHECK_FALSE(SprSprite::Parse(BuildSpr(zlib_options)).has_value());
  }
  SECTION("fichier tronqué") {
    std::vector<std::uint8_t> spr = BuildSpr(options);
    spr.resize(spr.size() / 2);
    CHECK_FALSE(SprSprite::Parse(spr).has_value());
  }
  SECTION("fichier vide") { CHECK_FALSE(SprSprite::Parse({}).has_value()); }
}
