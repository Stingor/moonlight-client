#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "ro/formats/image.h"

namespace ro::test_support {

struct BmpPixel {
  std::uint8_t red = 0;
  std::uint8_t green = 0;
  std::uint8_t blue = 0;
};

struct BmpBuildOptions {
  // 8 (palettisé), 24 ou 32.
  std::uint16_t bits_per_pixel = 8;
  // Un BMP est presque toujours rangé de bas en haut ; une hauteur négative
  // signale l'inverse. Les deux existent, et confondre les deux retourne
  // l'image sans qu'aucune assertion de taille ne s'en aperçoive.
  bool bottom_up = true;
  // Écrite telle quelle dans l'en-tête : sert à fabriquer des fichiers que le
  // décodeur DOIT refuser.
  std::uint32_t compression = 0;
  std::uint32_t info_header_size = 40;
};

// Construit un BMP. `pixels` est rangé de HAUT en BAS, width × height entrées ;
// le constructeur se charge de l'inversion et du remplissage des rangées.
//
// En 8 bits, la palette est déduite des couleurs distinctes (256 au plus).
[[nodiscard]] std::vector<std::uint8_t> BuildBmp(std::int32_t width, std::int32_t height,
                                                 const std::vector<BmpPixel>& pixels,
                                                 const BmpBuildOptions& options = {});

struct TgaPixel {
  std::uint8_t red = 0;
  std::uint8_t green = 0;
  std::uint8_t blue = 0;
  // Un vrai canal, contrairement au BMP : le décodeur doit le transmettre.
  std::uint8_t alpha = 255;
};

struct TgaBuildOptions {
  // 8 (niveaux de gris), 24 ou 32.
  std::uint8_t bits_per_pixel = 32;
  // Compression par plages. Le constructeur produit à la fois des paquets
  // répétés et des paquets littéraux : n'en gérer qu'un des deux passerait
  // inaperçu si le fichier d'essai ne contenait que l'autre.
  bool rle = false;
  // Sans le bit correspondant, le fichier est rangé de BAS en haut — le cas de
  // loin le plus répandu dans les données du jeu.
  bool top_down = false;
  // Écrits tels quels : servent à fabriquer des fichiers que le décodeur DOIT
  // refuser. Sans surcharge, le type découle de la profondeur et de `rle`.
  std::uint8_t image_type_override = 0;
  std::uint8_t colormap_type = 0;
  // Champ identifiant, glissé ENTRE l'en-tête et les pixels. Sauter sa longueur
  // variable est le piège classique du format.
  std::string identifier;
};

// Construit un TGA. `pixels` est rangé de HAUT en BAS, width × height entrées ;
// le constructeur se charge de l'inversion et de la compression éventuelle.
//
// En 8 bits, seule la composante rouge est écrite (image en niveaux de gris).
[[nodiscard]] std::vector<std::uint8_t> BuildTga(std::int32_t width, std::int32_t height,
                                                 const std::vector<TgaPixel>& pixels,
                                                 const TgaBuildOptions& options = {});

// Le pixel (x, y) d'une image décodée, en RGBA : quatre octets qu'un essai
// compare d'un bloc.
[[nodiscard]] inline std::vector<std::uint8_t> PixelAt(const formats::Image& image, std::int32_t x,
                                                       std::int32_t y) {
  constexpr std::size_t kRgbaBytes = 4;
  const std::size_t offset = (static_cast<std::size_t>(y) * static_cast<std::size_t>(image.width) +
                              static_cast<std::size_t>(x)) *
                             kRgbaBytes;
  return {image.pixels[offset], image.pixels[offset + 1], image.pixels[offset + 2],
          image.pixels[offset + 3]};
}

}  // namespace ro::test_support
