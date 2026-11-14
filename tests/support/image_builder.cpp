#include "support/image_builder.h"

#include <algorithm>
#include <array>
#include <map>

namespace ro::test_support {
namespace {

void AppendU16(std::vector<std::uint8_t>& out, std::uint16_t value) {
  out.push_back(static_cast<std::uint8_t>(value & 0xFFU));
  out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFFU));
}

void AppendU32(std::vector<std::uint8_t>& out, std::uint32_t value) {
  for (unsigned shift = 0; shift < 32; shift += 8) {
    out.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFFU));
  }
}

void PutU32(std::vector<std::uint8_t>& out, std::size_t at, std::uint32_t value) {
  for (unsigned index = 0; index < 4; ++index) {
    out[at + index] = static_cast<std::uint8_t>((value >> (index * 8)) & 0xFFU);
  }
}

constexpr std::uint8_t kTgaTrueColor = 2;
constexpr std::uint8_t kTgaGrayscale = 3;
// Les types compressés valent le type simple augmenté de 8.
constexpr std::uint8_t kTgaRleOffset = 8;
constexpr std::uint8_t kTgaDescriptorTopDown = 0x20;
// Un paquet, répété ou littéral, couvre au plus 128 pixels.
constexpr std::size_t kMaxRun = 128;

void AppendTgaPixel(std::vector<std::uint8_t>& out, const TgaPixel& pixel,
                    std::size_t bytes_per_pixel) {
  if (bytes_per_pixel == 1) {
    out.push_back(pixel.red);
    return;
  }
  // Le TGA range ses composantes en bleu, vert, rouge, puis alpha.
  out.push_back(pixel.blue);
  out.push_back(pixel.green);
  out.push_back(pixel.red);
  if (bytes_per_pixel == 4) {
    out.push_back(pixel.alpha);
  }
}

bool SamePixel(const std::vector<std::uint8_t>& row, std::size_t left, std::size_t right,
               std::size_t bytes_per_pixel) {
  return std::equal(row.begin() + static_cast<std::ptrdiff_t>(left * bytes_per_pixel),
                    row.begin() + static_cast<std::ptrdiff_t>((left + 1) * bytes_per_pixel),
                    row.begin() + static_cast<std::ptrdiff_t>(right * bytes_per_pixel));
}

// Compresse une rangée déjà sérialisée. Les pixels identiques consécutifs
// deviennent un paquet répété, le reste des paquets littéraux — donc un fichier
// d'essai exerce toujours les deux branches du décodeur.
void AppendRleRow(std::vector<std::uint8_t>& out, const std::vector<std::uint8_t>& row,
                  std::size_t width, std::size_t bytes_per_pixel) {
  std::size_t index = 0;
  while (index < width) {
    std::size_t run = 1;
    while (index + run < width && run < kMaxRun &&
           SamePixel(row, index, index + run, bytes_per_pixel)) {
      ++run;
    }

    if (run >= 2) {
      out.push_back(static_cast<std::uint8_t>(0x80U | (run - 1)));
      const auto start = static_cast<std::ptrdiff_t>(index * bytes_per_pixel);
      out.insert(out.end(), row.begin() + start,
                 row.begin() + start + static_cast<std::ptrdiff_t>(bytes_per_pixel));
      index += run;
      continue;
    }

    // Paquet littéral : on avance tant que le pixel suivant ne démarre pas une
    // répétition, qu'il serait plus économique d'encoder à part.
    std::size_t literal = 1;
    while (index + literal < width && literal < kMaxRun &&
           !(index + literal + 1 < width &&
             SamePixel(row, index + literal, index + literal + 1, bytes_per_pixel))) {
      ++literal;
    }
    out.push_back(static_cast<std::uint8_t>(literal - 1));
    const auto start = static_cast<std::ptrdiff_t>(index * bytes_per_pixel);
    out.insert(out.end(), row.begin() + start,
               row.begin() + start + static_cast<std::ptrdiff_t>(literal * bytes_per_pixel));
    index += literal;
  }
}

}  // namespace

std::vector<std::uint8_t> BuildBmp(std::int32_t width, std::int32_t height,
                                   const std::vector<BmpPixel>& pixels,
                                   const BmpBuildOptions& options) {
  // Palette déduite des couleurs distinctes, dans l'ordre de première
  // apparition — un ordre stable rend les attentes des tests lisibles.
  std::map<std::array<std::uint8_t, 3>, std::uint8_t> palette_index;
  std::vector<std::array<std::uint8_t, 3>> palette;
  if (options.bits_per_pixel == 8) {
    for (const BmpPixel& pixel : pixels) {
      const std::array<std::uint8_t, 3> key{pixel.red, pixel.green, pixel.blue};
      if (!palette_index.contains(key) && palette.size() < 256) {
        palette_index.emplace(key, static_cast<std::uint8_t>(palette.size()));
        palette.push_back(key);
      }
    }
  }

  const std::size_t palette_bytes = options.bits_per_pixel == 8 ? 256 * 4 : 0;
  const std::size_t row_stride =
      ((static_cast<std::size_t>(width) * options.bits_per_pixel + 31) / 32) * 4;
  const std::size_t pixel_offset = 14 + options.info_header_size + palette_bytes;

  std::vector<std::uint8_t> bmp;
  bmp.push_back('B');
  bmp.push_back('M');
  AppendU32(bmp, 0);  // taille du fichier, corrigée à la fin
  AppendU32(bmp, 0);  // champs réservés
  AppendU32(bmp, static_cast<std::uint32_t>(pixel_offset));

  AppendU32(bmp, options.info_header_size);
  AppendU32(bmp, static_cast<std::uint32_t>(width));
  AppendU32(bmp, static_cast<std::uint32_t>(options.bottom_up ? height : -height));
  AppendU16(bmp, 1);  // plans
  AppendU16(bmp, options.bits_per_pixel);
  AppendU32(bmp, options.compression);
  AppendU32(bmp, 0);  // taille de l'image
  AppendU32(bmp, 2835);
  AppendU32(bmp, 2835);
  AppendU32(bmp, 0);  // couleurs utilisées : 0 = toutes
  AppendU32(bmp, 0);
  // Un en-tête plus long que 40 octets se complète de zéros (V4, V5) ; un plus
  // court, annoncé pour être refusé, n'a rien à combler.
  if (options.info_header_size > 40) {
    bmp.insert(bmp.end(), options.info_header_size - 40, 0);
  }

  if (options.bits_per_pixel == 8) {
    for (std::size_t index = 0; index < 256; ++index) {
      if (index < palette.size()) {
        bmp.push_back(palette[index][2]);  // bleu
        bmp.push_back(palette[index][1]);  // vert
        bmp.push_back(palette[index][0]);  // rouge
      } else {
        bmp.insert(bmp.end(), 3, 0);
      }
      bmp.push_back(0);
    }
  }

  for (std::int32_t y = 0; y < height; ++y) {
    // La première rangée écrite est la DERNIÈRE de l'image quand le fichier est
    // rangé de bas en haut.
    const std::int32_t source_y = options.bottom_up ? height - 1 - y : y;
    const std::size_t row_start = bmp.size();

    for (std::int32_t x = 0; x < width; ++x) {
      const BmpPixel& pixel =
          pixels[static_cast<std::size_t>(source_y) * static_cast<std::size_t>(width) +
                 static_cast<std::size_t>(x)];
      if (options.bits_per_pixel == 8) {
        const std::array<std::uint8_t, 3> key{pixel.red, pixel.green, pixel.blue};
        const auto found = palette_index.find(key);
        bmp.push_back(found == palette_index.end() ? 0 : found->second);
      } else {
        bmp.push_back(pixel.blue);
        bmp.push_back(pixel.green);
        bmp.push_back(pixel.red);
        if (options.bits_per_pixel == 32) {
          bmp.push_back(0);
        }
      }
    }
    // Soustraction bornée : une profondeur non gérée écrit plus d'octets que
    // le pas calculé, et `row_stride - written` déborderait en size_t. Ces
    // fichiers-là existent pour être REFUSÉS par le décodeur, pas pour être
    // corrects — le constructeur ne doit pas planter en les fabriquant.
    const std::size_t written = bmp.size() - row_start;
    if (row_stride > written) {
      bmp.insert(bmp.end(), row_stride - written, 0);
    }
  }

  PutU32(bmp, 2, static_cast<std::uint32_t>(bmp.size()));
  return bmp;
}

std::vector<std::uint8_t> BuildTga(std::int32_t width, std::int32_t height,
                                   const std::vector<TgaPixel>& pixels,
                                   const TgaBuildOptions& options) {
  const std::size_t bytes_per_pixel = options.bits_per_pixel / 8U;

  std::uint8_t image_type = options.image_type_override;
  if (image_type == 0) {
    const std::uint8_t base = options.bits_per_pixel == 8 ? kTgaGrayscale : kTgaTrueColor;
    image_type = options.rle ? static_cast<std::uint8_t>(base + kTgaRleOffset) : base;
  }

  std::vector<std::uint8_t> tga;
  tga.push_back(static_cast<std::uint8_t>(options.identifier.size()));
  tga.push_back(options.colormap_type);
  tga.push_back(image_type);
  tga.insert(tga.end(), 5, 0);  // description de la palette
  AppendU16(tga, 0);            // origine en X
  AppendU16(tga, 0);            // origine en Y
  AppendU16(tga, static_cast<std::uint16_t>(width));
  AppendU16(tga, static_cast<std::uint16_t>(height));
  tga.push_back(options.bits_per_pixel);
  tga.push_back(options.top_down ? kTgaDescriptorTopDown : 0);
  tga.insert(tga.end(), options.identifier.begin(), options.identifier.end());

  for (std::int32_t y = 0; y < height; ++y) {
    // La première rangée écrite est la DERNIÈRE de l'image quand le fichier est
    // rangé de bas en haut.
    const std::int32_t source_y = options.top_down ? y : height - 1 - y;

    std::vector<std::uint8_t> row;
    for (std::int32_t x = 0; x < width; ++x) {
      const TgaPixel& pixel =
          pixels[static_cast<std::size_t>(source_y) * static_cast<std::size_t>(width) +
                 static_cast<std::size_t>(x)];
      AppendTgaPixel(row, pixel, bytes_per_pixel);
    }

    if (options.rle) {
      AppendRleRow(tga, row, static_cast<std::size_t>(width), bytes_per_pixel);
    } else {
      tga.insert(tga.end(), row.begin(), row.end());
    }
  }

  return tga;
}

}  // namespace ro::test_support
