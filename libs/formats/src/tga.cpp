#include "ro/formats/tga.h"

#include <algorithm>
#include <string>

#include "ro/core/byte_reader.h"

namespace ro::formats {
namespace {

using core::ByteReader;
using core::ErrorCode;
using core::Fail;

constexpr std::size_t kHeaderSize = 18;

// Types d'image. Les variantes indexées (1 et 9) n'apparaissent pas dans les
// données du jeu et sont refusées.
constexpr std::uint8_t kTypeTrueColor = 2;
constexpr std::uint8_t kTypeGrayscale = 3;
constexpr std::uint8_t kTypeTrueColorRle = 10;
constexpr std::uint8_t kTypeGrayscaleRle = 11;

// Bit 5 du descripteur : image rangée de haut en bas.
constexpr std::uint8_t kDescriptorTopDown = 0x20;

// Les textures du jeu ne dépassent pas 1024 ; le plafond refuse une dimension
// corrompue avant qu'elle ne devienne une allocation démesurée.
constexpr std::int32_t kMaxDimension = 16384;

// Un paquet RLE encode au plus 128 pixels.
constexpr std::uint8_t kRunLengthMask = 0x7F;
constexpr std::uint8_t kRunLengthFlag = 0x80;

// Écrit un pixel RGBA depuis `bytes_per_pixel` octets sources.
void WritePixel(std::uint8_t* out, const std::uint8_t* source, std::size_t bytes_per_pixel) {
  if (bytes_per_pixel == 1) {
    // Niveaux de gris : la même valeur sur les trois canaux.
    out[0] = source[0];
    out[1] = source[0];
    out[2] = source[0];
    out[3] = 255;
    return;
  }
  // Le TGA range ses composantes en bleu, vert, rouge, puis alpha.
  out[0] = source[2];
  out[1] = source[1];
  out[2] = source[0];
  out[3] = bytes_per_pixel == 4 ? source[3] : 255;
}

// Décompresse les paquets RLE en un tampon de pixels bruts.
core::Result<std::vector<std::uint8_t>> ExpandRle(ByteReader& reader, std::size_t pixel_count,
                                                  std::size_t bytes_per_pixel) {
  // ⚠ LA TAILLE ANNONCÉE SE CONFRONTE AUX OCTETS AVANT D'ALLOUER. Le meilleur
  // paquet — une tête et un pixel répété — rend au plus `kRunLengthMask + 1`
  // pixels. Un en-tête de dix-huit octets qui annonce 16384 × 16384 pixels
  // réserverait sinon un gigaoctet avant d'échouer au premier paquet.
  const std::size_t best_packet_bytes = 1 + bytes_per_pixel;
  const std::size_t most_packets = (reader.remaining() + best_packet_bytes - 1) / best_packet_bytes;
  if (pixel_count > most_packets * (static_cast<std::size_t>(kRunLengthMask) + 1)) {
    return Fail(ErrorCode::kUnexpectedEnd, "paquets RLE trop courts pour l'image annoncée");
  }
  std::vector<std::uint8_t> raw(pixel_count * bytes_per_pixel);
  std::size_t written = 0;

  while (written < pixel_count) {
    const std::uint8_t packet = reader.U8();
    if (!reader.ok()) {
      return Fail(ErrorCode::kUnexpectedEnd, "paquet RLE tronqué");
    }
    const std::size_t run = static_cast<std::size_t>(packet & kRunLengthMask) + 1;
    if (written + run > pixel_count) {
      return Fail(ErrorCode::kCorruptData, "paquet RLE débordant de l'image");
    }

    if ((packet & kRunLengthFlag) != 0) {
      // Répétition : un seul pixel, répété `run` fois.
      const std::span<const std::uint8_t> pixel = reader.Bytes(bytes_per_pixel);
      if (pixel.size() != bytes_per_pixel) {
        return Fail(ErrorCode::kUnexpectedEnd, "pixel RLE tronqué");
      }
      for (std::size_t index = 0; index < run; ++index) {
        std::copy(pixel.begin(), pixel.end(),
                  raw.begin() + static_cast<std::ptrdiff_t>((written + index) * bytes_per_pixel));
      }
    } else {
      // Littéral : `run` pixels distincts à la suite.
      const std::span<const std::uint8_t> block = reader.Bytes(run * bytes_per_pixel);
      if (block.size() != run * bytes_per_pixel) {
        return Fail(ErrorCode::kUnexpectedEnd, "bloc RLE tronqué");
      }
      std::copy(block.begin(), block.end(),
                raw.begin() + static_cast<std::ptrdiff_t>(written * bytes_per_pixel));
    }
    written += run;
  }
  return raw;
}

// Un alpha nul PARTOUT signale un canal laissé vide par l'outil d'export, pas
// une image entièrement transparente. La prendre au mot ferait disparaître la
// texture entière ; on la traite comme opaque.
bool AlphaIsEntirelyZero(const std::vector<std::uint8_t>& pixels) {
  for (std::size_t offset = 3; offset < pixels.size(); offset += 4) {
    if (pixels[offset] != 0) {
      return false;
    }
  }
  return !pixels.empty();
}

}  // namespace

core::Result<Image> DecodeTga(std::span<const std::uint8_t> data) {
  ByteReader reader(data);

  const std::uint8_t id_length = reader.U8();
  const std::uint8_t colormap_type = reader.U8();
  const std::uint8_t image_type = reader.U8();
  reader.Skip(5);  // description de la palette, inutilisée ici
  reader.Skip(4);  // origine
  const std::int32_t width = reader.U16();
  const std::int32_t height = reader.U16();
  const std::uint8_t bits_per_pixel = reader.U8();
  const std::uint8_t descriptor = reader.U8();
  if (!reader.ok()) {
    return Fail(ErrorCode::kUnexpectedEnd, "en-tête TGA tronqué");
  }

  if (colormap_type != 0) {
    return Fail(ErrorCode::kUnsupportedVersion, "TGA à palette");
  }
  const bool compressed = image_type == kTypeTrueColorRle || image_type == kTypeGrayscaleRle;
  if (image_type != kTypeTrueColor && image_type != kTypeGrayscale && !compressed) {
    return Fail(ErrorCode::kUnsupportedVersion, "type d'image TGA " + std::to_string(image_type));
  }
  if (bits_per_pixel != 8 && bits_per_pixel != 24 && bits_per_pixel != 32) {
    return Fail(ErrorCode::kUnsupportedVersion,
                "TGA en " + std::to_string(bits_per_pixel) + " bits par pixel");
  }
  if (width <= 0 || height <= 0 || width > kMaxDimension || height > kMaxDimension) {
    return Fail(ErrorCode::kCorruptData,
                "dimensions TGA " + std::to_string(width) + "x" + std::to_string(height));
  }

  // Le champ identifiant précède les pixels et n'a pas de longueur fixe.
  if (!reader.Seek(kHeaderSize + id_length)) {
    return Fail(ErrorCode::kUnexpectedEnd, "champ identifiant TGA hors du fichier");
  }

  const std::size_t bytes_per_pixel = bits_per_pixel / 8U;
  const auto pixel_count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);

  std::vector<std::uint8_t> raw;
  if (compressed) {
    auto expanded = ExpandRle(reader, pixel_count, bytes_per_pixel);
    if (!expanded) {
      return std::unexpected(expanded.error());
    }
    raw = std::move(*expanded);
  } else {
    const std::span<const std::uint8_t> block = reader.Bytes(pixel_count * bytes_per_pixel);
    if (block.size() != pixel_count * bytes_per_pixel) {
      return Fail(ErrorCode::kUnexpectedEnd, "pixels TGA tronqués");
    }
    raw.assign(block.begin(), block.end());
  }

  Image image;
  image.width = width;
  image.height = height;
  image.pixels.resize(pixel_count * 4);

  // Sans le bit 5, le fichier est rangé de BAS en haut.
  const bool top_down = (descriptor & kDescriptorTopDown) != 0;
  for (std::int32_t y = 0; y < height; ++y) {
    const std::size_t source_row =
        top_down ? static_cast<std::size_t>(y) : static_cast<std::size_t>(height - 1 - y);
    const std::uint8_t* source =
        raw.data() + source_row * static_cast<std::size_t>(width) * bytes_per_pixel;
    std::uint8_t* destination =
        image.pixels.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(width) * 4;

    for (std::int32_t x = 0; x < width; ++x) {
      WritePixel(destination + static_cast<std::size_t>(x) * 4,
                 source + static_cast<std::size_t>(x) * bytes_per_pixel, bytes_per_pixel);
    }
  }

  if (bytes_per_pixel == 4 && AlphaIsEntirelyZero(image.pixels)) {
    for (std::size_t offset = 3; offset < image.pixels.size(); offset += 4) {
      image.pixels[offset] = 255;
    }
  }

  return image;
}

}  // namespace ro::formats
