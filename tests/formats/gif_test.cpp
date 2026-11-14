// L'encodeur GIF, jugé par un lecteur écrit à part.
//
// 🔴 CE QU'ON VÉRIFIE N'EST PAS « LE FICHIER COMMENCE PAR GIF89a » : c'est que
// les octets se REDÉCOMPRESSENT en l'image qu'on a donnée. Un contrôle
// d'en-tête laisserait passer un flux LZW dont la largeur de code dérive d'un
// bit au bout de quelques centaines de codes — la panne exacte qui ne se voit
// qu'après un tiers de l'animation.
//
// ⚠ ET UN LECTEUR DE CE DÉPÔT NE PROUVE PAS QU'UN LECTEUR DU MONDE SUIVRA. Le
// fichier a donc été relu une fois par Pillow, hors de cette suite : six images
// de 64 sur 64, boucle infinie, délais conformes, ZÉRO pixel en désaccord sur
// 24 576. C'est ce relevé-là qui tranche la largeur de code, et non une lecture
// de la spécification.

#include "ro/formats/gif.h"

#include <catch2/catch_test_macros.hpp>

#include "support/gif_reader.h"

namespace {

constexpr int kDelayCs = 10;

// ⚠ DES COULEURS QUI SURVIVENT À L'ARRONDI. La quantification passe par une
// grille de 5 bits par canal ; seules les valeurs dont les bits de poids faible
// recopient ceux de poids fort en ressortent identiques. 0 et 255 en sont, et
// c'est ce qui permet d'exiger l'ÉGALITÉ plutôt qu'un voisinage.
struct Rgb {
  std::uint8_t red = 0;
  std::uint8_t green = 0;
  std::uint8_t blue = 0;
};

constexpr Rgb kBlack{0, 0, 0};
constexpr Rgb kWhite{255, 255, 255};
constexpr Rgb kRed{255, 0, 0};
constexpr Rgb kGreen{0, 255, 0};
constexpr Rgb kBlue{0, 0, 255};

[[nodiscard]] ro::formats::Image Filled(int width, int height, Rgb color) {
  ro::formats::Image image;
  image.width = width;
  image.height = height;
  image.pixels.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4U);
  for (std::size_t at = 0; at < image.pixels.size(); at += 4) {
    image.pixels[at + 0] = color.red;
    image.pixels[at + 1] = color.green;
    image.pixels[at + 2] = color.blue;
    image.pixels[at + 3] = 255;
  }
  return image;
}

void Paint(ro::formats::Image& image, int x, int y, Rgb color) {
  const std::size_t at = (static_cast<std::size_t>(y) * static_cast<std::size_t>(image.width) +
                          static_cast<std::size_t>(x)) *
                         4U;
  image.pixels[at + 0] = color.red;
  image.pixels[at + 1] = color.green;
  image.pixels[at + 2] = color.blue;
  image.pixels[at + 3] = 255;
}

[[nodiscard]] bool SamePicture(const ro::formats::Image& left, const ro::formats::Image& right) {
  if (left.width != right.width || left.height != right.height) {
    return false;
  }
  for (std::size_t at = 0; at < left.pixels.size(); at += 4) {
    if (left.pixels[at] != right.pixels[at] || left.pixels[at + 1] != right.pixels[at + 1] ||
        left.pixels[at + 2] != right.pixels[at + 2]) {
      return false;
    }
  }
  return true;
}

[[nodiscard]] ro::testing::GifClip Encoded(std::span<const ro::formats::Image> frames) {
  const auto bytes = ro::formats::EncodeAnimatedGif(frames, kDelayCs);
  REQUIRE(bytes.has_value());
  const auto clip = ro::testing::ReadGif(*bytes);
  REQUIRE(clip.has_value());
  return *clip;
}

}  // namespace

TEST_CASE("GIF : chaque image se redécompresse à l'identique", "[formats][gif]") {
  // Trois images bien distinctes, peu de couleurs : la coupe médiane leur donne
  // à chacune son entrée, donc l'aller-retour doit être EXACT.
  std::vector<ro::formats::Image> frames = {Filled(8, 4, kRed), Filled(8, 4, kGreen),
                                            Filled(8, 4, kBlue)};
  Paint(frames[0], 1, 1, kWhite);
  Paint(frames[1], 5, 2, kBlack);
  Paint(frames[2], 7, 3, kWhite);

  const ro::testing::GifClip clip = Encoded(frames);
  REQUIRE(clip.frames.size() == frames.size());
  CHECK(clip.width == 8);
  CHECK(clip.height == 4);
  for (std::size_t at = 0; at < frames.size(); ++at) {
    CHECK(SamePicture(clip.frames[at].canvas, frames[at]));
    CHECK(clip.frames[at].delay_centiseconds == kDelayCs);
  }
}

TEST_CASE("GIF : seule la boîte de ce qui bouge est écrite", "[formats][gif]") {
  // 🔴 C'EST LE GAIN DE TOUT LE FORMAT ICI. Un écran de jeu immobile mis dans un
  // GIF entier à dix images par seconde ne se poste nulle part.
  std::vector<ro::formats::Image> frames = {Filled(16, 16, kBlue), Filled(16, 16, kBlue)};
  Paint(frames[1], 9, 5, kRed);

  const ro::testing::GifClip clip = Encoded(frames);
  REQUIRE(clip.frames.size() == 2);
  // La première porte tout : il n'y a rien dessous.
  CHECK(clip.frames[0].width == 16);
  CHECK(clip.frames[0].height == 16);
  // La seconde ne porte QUE le pixel qui a changé.
  CHECK(clip.frames[1].left == 9);
  CHECK(clip.frames[1].top == 5);
  CHECK(clip.frames[1].width == 1);
  CHECK(clip.frames[1].height == 1);
  // Et la toile montre quand même l'image entière.
  CHECK(SamePicture(clip.frames[1].canvas, frames[1]));
}

TEST_CASE("GIF : une image immobile brûle quand même son délai", "[formats][gif]") {
  // ⚠ SAUTER UNE IMAGE INCHANGÉE AVANCERAIT TOUT LE RESTE : une pause dans la
  // scène deviendrait un saut, et le clip mentirait sur sa durée.
  const std::vector<ro::formats::Image> frames = {Filled(8, 8, kGreen), Filled(8, 8, kGreen),
                                                  Filled(8, 8, kGreen)};
  const ro::testing::GifClip clip = Encoded(frames);
  REQUIRE(clip.frames.size() == 3);
  for (const ro::testing::GifFrame& frame : clip.frames) {
    CHECK(frame.delay_centiseconds == kDelayCs);
    CHECK(SamePicture(frame.canvas, frames[0]));
  }
  CHECK(clip.frames[1].width == 1);
  CHECK(clip.frames[2].width == 1);
}

TEST_CASE("GIF : il boucle", "[formats][gif]") {
  // Sans l'extension de boucle, l'animation joue une fois et s'arrête sur sa
  // dernière image — ce qui passe pour une capture ratée.
  const std::vector<ro::formats::Image> frames = {Filled(4, 4, kRed), Filled(4, 4, kBlue)};
  CHECK(Encoded(frames).loops_forever);
}

TEST_CASE("GIF : un flux long garde ses largeurs de code", "[formats][gif]") {
  // 🔴 LE CAS QUI NE SE VOIT QU'À L'USAGE. Le dictionnaire LZW grandit, et la
  // largeur des codes avec lui ; un désaccord d'un seul bit entre celui qui
  // écrit et celui qui lit ne se manifeste qu'après plusieurs centaines de
  // codes — donc jamais sur une image de huit pixels sur quatre.
  //
  // Un damier irrégulier de soixante-quatre sur soixante-quatre engendre
  // largement de quoi franchir le premier élargissement.
  constexpr int kSide = 64;
  std::vector<ro::formats::Image> frames;
  for (int number = 0; number < 2; ++number) {
    ro::formats::Image frame = Filled(kSide, kSide, kBlack);
    for (int y = 0; y < kSide; ++y) {
      for (int x = 0; x < kSide; ++x) {
        // Une suite déterministe mais sans période courte : le LZW ne doit pas
        // pouvoir tout replier sur une poignée de codes.
        const int mixed = (x * 7 + y * 13 + number * 5) % 5;
        const Rgb color = mixed == 0   ? kBlack
                          : mixed == 1 ? kWhite
                          : mixed == 2 ? kRed
                          : mixed == 3 ? kGreen
                                       : kBlue;
        Paint(frame, x, y, color);
      }
    }
    frames.push_back(std::move(frame));
  }

  const ro::testing::GifClip clip = Encoded(frames);
  REQUIRE(clip.frames.size() == 2);
  CHECK(SamePicture(clip.frames[0].canvas, frames[0]));
  CHECK(SamePicture(clip.frames[1].canvas, frames[1]));
}

TEST_CASE("GIF : plus de couleurs que la palette n'en tient", "[formats][gif]") {
  // La table globale n'a que 255 entrées d'image. Au-delà, l'encodeur doit
  // rendre un fichier VALIDE et approximatif, jamais un échec.
  ro::formats::Image frame = Filled(32, 32, kBlack);
  for (int y = 0; y < 32; ++y) {
    for (int x = 0; x < 32; ++x) {
      Paint(frame, x, y,
            Rgb{static_cast<std::uint8_t>(x * 8), static_cast<std::uint8_t>(y * 8),
                static_cast<std::uint8_t>((x + y) * 4)});
    }
  }
  const std::vector<ro::formats::Image> frames = {frame};
  const ro::testing::GifClip clip = Encoded(frames);
  REQUIRE(clip.frames.size() == 1);
  CHECK(clip.nonblack_palette_entries <= ro::formats::kGifPictureColors);
  CHECK(clip.frames[0].width == 32);
}

TEST_CASE("GIF : ce qui ne s'encode pas se refuse", "[formats][gif]") {
  CHECK_FALSE(ro::formats::EncodeAnimatedGif({}, kDelayCs).has_value());

  // 🔴 DEUX TAILLES DANS UN MÊME CLIP : le format n'a qu'une toile, et laisser
  // passer produirait un fichier que les lecteurs interprètent chacun à leur
  // façon. C'est le cas que le recadrage en cours d'enregistrement ferait
  // arriver.
  const std::vector<ro::formats::Image> mixed = {Filled(8, 8, kRed), Filled(8, 9, kRed)};
  const auto refused = ro::formats::EncodeAnimatedGif(mixed, kDelayCs);
  REQUIRE_FALSE(refused.has_value());
  CHECK(refused.error().code == ro::core::ErrorCode::kInvalidArgument);
}
