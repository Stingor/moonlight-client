#include "ro/formats/tga.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/image.h"

#include "support/image_builder.h"

using ro::formats::DecodeImage;
using ro::formats::DecodeTga;
using ro::test_support::BuildTga;
using ro::test_support::PixelAt;
using ro::test_support::TgaBuildOptions;
using ro::test_support::TgaPixel;

namespace {

// Quatre couleurs distinctes : toute permutation de canaux ou de rangées se
// voit. Rangées de HAUT en bas.
const std::vector<TgaPixel> kQuad = {
    TgaPixel{.red = 255, .green = 0, .blue = 0, .alpha = 255},
    TgaPixel{.red = 0, .green = 255, .blue = 0, .alpha = 255},
    TgaPixel{.red = 0, .green = 0, .blue = 255, .alpha = 255},
    TgaPixel{.red = 10, .green = 20, .blue = 30, .alpha = 255},
};

}  // namespace

TEST_CASE("TGA 32 bits : les canaux arrivent dans le bon ordre", "[formats][tga]") {
  // Le fichier range bleu, vert, rouge, alpha ; l'image sort en RGBA. Une
  // inversion rouge/bleu laisse toutes les tailles correctes.
  auto image = DecodeTga(BuildTga(2, 2, kQuad));
  REQUIRE(image.has_value());
  CHECK(image->width == 2);
  CHECK(image->height == 2);

  CHECK(PixelAt(*image, 0, 0) == std::vector<std::uint8_t>{255, 0, 0, 255});
  CHECK(PixelAt(*image, 1, 0) == std::vector<std::uint8_t>{0, 255, 0, 255});
  CHECK(PixelAt(*image, 0, 1) == std::vector<std::uint8_t>{0, 0, 255, 255});
  CHECK(PixelAt(*image, 1, 1) == std::vector<std::uint8_t>{10, 20, 30, 255});
}

TEST_CASE("TGA : les deux sens de rangement donnent la même image", "[formats][tga]") {
  // Le sens dépend d'un seul bit de l'en-tête. Le confondre retourne l'image
  // verticalement sans qu'aucune dimension ne change — exactement le défaut qui
  // avait mis le terrain en miroir.
  auto bottom_up = DecodeTga(BuildTga(2, 2, kQuad, TgaBuildOptions{.top_down = false}));
  auto top_down = DecodeTga(BuildTga(2, 2, kQuad, TgaBuildOptions{.top_down = true}));
  REQUIRE(bottom_up.has_value());
  REQUIRE(top_down.has_value());

  CHECK(bottom_up->pixels == top_down->pixels);
  // Et cette image commune est bien dans le sens attendu : rouge en haut à
  // gauche, comme kQuad l'énonce.
  CHECK(PixelAt(*bottom_up, 0, 0) == std::vector<std::uint8_t>{255, 0, 0, 255});
}

TEST_CASE("TGA 24 bits : l'image est opaque", "[formats][tga]") {
  auto image = DecodeTga(BuildTga(2, 2, kQuad, TgaBuildOptions{.bits_per_pixel = 24}));
  REQUIRE(image.has_value());
  CHECK(PixelAt(*image, 0, 0) == std::vector<std::uint8_t>{255, 0, 0, 255});
  CHECK(PixelAt(*image, 1, 1) == std::vector<std::uint8_t>{10, 20, 30, 255});
}

TEST_CASE("TGA 8 bits : le niveau de gris se répand sur les trois canaux", "[formats][tga]") {
  const std::vector<TgaPixel> greys = {TgaPixel{.red = 0}, TgaPixel{.red = 80},
                                       TgaPixel{.red = 160}, TgaPixel{.red = 255}};

  auto image = DecodeTga(BuildTga(2, 2, greys, TgaBuildOptions{.bits_per_pixel = 8}));
  REQUIRE(image.has_value());
  CHECK(PixelAt(*image, 1, 0) == std::vector<std::uint8_t>{80, 80, 80, 255});
  CHECK(PixelAt(*image, 1, 1) == std::vector<std::uint8_t>{255, 255, 255, 255});
}

TEST_CASE("TGA compressé : le résultat est identique au non compressé", "[formats][tga]") {
  // L'image mêle une longue plage uniforme et des pixels tous différents, pour
  // que le fichier contienne à la fois des paquets répétés et des littéraux.
  std::vector<TgaPixel> pixels;
  for (int index = 0; index < 8; ++index) {
    pixels.push_back(TgaPixel{.red = 7, .green = 7, .blue = 7, .alpha = 255});
  }
  for (int index = 0; index < 8; ++index) {
    pixels.push_back(TgaPixel{.red = static_cast<std::uint8_t>(index * 10),
                              .green = static_cast<std::uint8_t>(200 - index),
                              .blue = static_cast<std::uint8_t>(index),
                              .alpha = 255});
  }

  const std::vector<std::uint8_t> plain = BuildTga(4, 4, pixels);
  const std::vector<std::uint8_t> packed = BuildTga(4, 4, pixels, TgaBuildOptions{.rle = true});
  // La compression doit réellement avoir eu lieu, sinon le test ne prouve rien.
  REQUIRE(packed.size() < plain.size());

  auto reference = DecodeTga(plain);
  auto compressed = DecodeTga(packed);
  REQUIRE(reference.has_value());
  REQUIRE(compressed.has_value());
  CHECK(compressed->pixels == reference->pixels);
}

TEST_CASE("TGA : le champ identifiant est enjambé", "[formats][tga]") {
  // Sa longueur est variable et précède les pixels. L'ignorer décale toute
  // l'image d'autant d'octets, ce qui produit des couleurs plausibles mais
  // fausses.
  auto image =
      DecodeTga(BuildTga(2, 2, kQuad, TgaBuildOptions{.identifier = "fabrique par un outil"}));
  REQUIRE(image.has_value());
  CHECK(PixelAt(*image, 0, 0) == std::vector<std::uint8_t>{255, 0, 0, 255});
}

TEST_CASE("TGA : un canal alpha entièrement nul vaut opaque", "[formats][tga]") {
  // Certains exportateurs laissent le canal à zéro. Le prendre au mot ferait
  // disparaître la texture entière, sans erreur ni trace.
  std::vector<TgaPixel> transparent = kQuad;
  for (TgaPixel& pixel : transparent) {
    pixel.alpha = 0;
  }

  auto image = DecodeTga(BuildTga(2, 2, transparent));
  REQUIRE(image.has_value());
  CHECK(PixelAt(*image, 0, 0) == std::vector<std::uint8_t>{255, 0, 0, 255});
  CHECK(PixelAt(*image, 1, 1) == std::vector<std::uint8_t>{10, 20, 30, 255});
}

TEST_CASE("TGA : un alpha partiel est transmis tel quel", "[formats][tga]") {
  // Le contrepoint du cas précédent : dès qu'UN pixel est visible, les zéros
  // des autres sont un vrai découpage et doivent survivre. C'est lui qui
  // découpe le feuillage.
  std::vector<TgaPixel> cut_out = kQuad;
  cut_out[0].alpha = 0;
  cut_out[1].alpha = 128;

  auto image = DecodeTga(BuildTga(2, 2, cut_out));
  REQUIRE(image.has_value());
  CHECK(PixelAt(*image, 0, 0)[3] == 0);
  CHECK(PixelAt(*image, 1, 0)[3] == 128);
  CHECK(PixelAt(*image, 1, 1)[3] == 255);
}

TEST_CASE("TGA : les formes non gérées sont refusées", "[formats][tga]") {
  SECTION("image à palette") {
    auto image = DecodeTga(BuildTga(2, 2, kQuad, TgaBuildOptions{.colormap_type = 1}));
    CHECK_FALSE(image.has_value());
  }
  SECTION("type d'image inconnu") {
    auto image = DecodeTga(BuildTga(2, 2, kQuad, TgaBuildOptions{.image_type_override = 1}));
    CHECK_FALSE(image.has_value());
  }
  SECTION("profondeur non gérée") {
    auto image = DecodeTga(BuildTga(2, 2, kQuad, TgaBuildOptions{.bits_per_pixel = 16}));
    CHECK_FALSE(image.has_value());
  }
  SECTION("fichier vide") {
    auto image = DecodeTga({});
    CHECK_FALSE(image.has_value());
  }
}

TEST_CASE("TGA : un fichier tronqué ne déborde pas", "[formats][tga]") {
  SECTION("pixels tronqués") {
    std::vector<std::uint8_t> tga = BuildTga(2, 2, kQuad);
    tga.resize(tga.size() - 5);
    auto image = DecodeTga(tga);
    CHECK_FALSE(image.has_value());
  }
  SECTION("paquet RLE tronqué") {
    std::vector<std::uint8_t> tga = BuildTga(2, 2, kQuad, TgaBuildOptions{.rle = true});
    tga.resize(tga.size() - 3);
    auto image = DecodeTga(tga);
    CHECK_FALSE(image.has_value());
  }
  SECTION("en-tête seul") {
    std::vector<std::uint8_t> tga = BuildTga(2, 2, kQuad);
    tga.resize(18);
    auto image = DecodeTga(tga);
    CHECK_FALSE(image.has_value());
  }
  SECTION("image RLE géante annoncée sur quelques octets") {
    // Aucun paquet ne rend plus de 128 pixels : 16384 × 16384 ne tient pas dans
    // ce qui suit l'en-tête, et le refus doit précéder toute allocation.
    constexpr std::uint8_t kLow = 0x00;
    constexpr std::uint8_t kHigh = 0x40;  // 16384 en petit-boutiste
    constexpr std::size_t kWidthOffset = 12;
    std::vector<std::uint8_t> tga = BuildTga(2, 2, kQuad, TgaBuildOptions{.rle = true});
    tga[kWidthOffset] = kLow;
    tga[kWidthOffset + 1] = kHigh;
    tga[kWidthOffset + 2] = kLow;
    tga[kWidthOffset + 3] = kHigh;
    auto image = DecodeTga(tga);
    REQUIRE_FALSE(image.has_value());
    CHECK(image.error().code == ro::core::ErrorCode::kUnexpectedEnd);
    // Le refus vient de la confrontation AVANT l'allocation, et non du premier
    // paquet manquant, qui ne se lit qu'après avoir tout réservé.
    CHECK(image.error().detail.find("annoncée") != std::string::npos);
  }
}

TEST_CASE("TGA compressé : une image uniforme au taux maximal passe", "[formats][tga]") {
  // Le contrepoint du refus précédent : une rangée de 128 pixels identiques
  // tient en UN paquet répété, le meilleur taux possible, pile à la borne.
  constexpr std::int32_t kWidest = 128;
  const std::vector<TgaPixel> uniform(static_cast<std::size_t>(kWidest),
                                      TgaPixel{.red = 9, .green = 8, .blue = 7, .alpha = 255});
  auto image = DecodeTga(BuildTga(kWidest, 1, uniform, TgaBuildOptions{.rle = true}));
  REQUIRE(image.has_value());
  CHECK(PixelAt(*image, kWidest - 1, 0) == std::vector<std::uint8_t>{9, 8, 7, 255});
}

TEST_CASE("DecodeImage : l'extension choisit le décodeur", "[formats][image]") {
  const std::vector<std::uint8_t> tga = BuildTga(2, 2, kQuad);

  SECTION("extension en minuscules") {
    auto image = DecodeImage("herbe.tga", tga);
    REQUIRE(image.has_value());
    CHECK(image->width == 2);
  }
  SECTION("extension en majuscules") {
    // Les fichiers RSW et GND citent leurs textures dans une casse quelconque.
    auto image = DecodeImage("HERBE.TGA", tga);
    REQUIRE(image.has_value());
    CHECK(image->width == 2);
  }
  SECTION("un chemin qui contient des points") {
    auto image = DecodeImage("data\\texture\\v2.5\\herbe.tga", tga);
    CHECK(image.has_value());
  }
  SECTION("format encore non géré") {
    auto image = DecodeImage("nuage.png", tga);
    CHECK_FALSE(image.has_value());
  }
  SECTION("nom sans extension") {
    auto image = DecodeImage("herbe", tga);
    CHECK_FALSE(image.has_value());
  }
}
