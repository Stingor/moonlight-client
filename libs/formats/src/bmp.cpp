#include "ro/formats/bmp.h"

#include <string>
#include <string_view>

#include "ro/core/byte_reader.h"

namespace ro::formats {
namespace {

using core::ByteReader;
using core::ErrorCode;
using core::Fail;

constexpr std::string_view kBmpSignature{"BM", 2};

// BITMAPFILEHEADER puis BITMAPINFOHEADER.
constexpr std::size_t kFileHeaderSize = 14;
constexpr std::uint32_t kInfoHeaderSize = 40;

// Compression BI_RGB : aucune. Les autres valeurs (RLE8, RLE4, BITFIELDS) sont
// refusées — elles n'apparaissent pas dans les données du jeu.
constexpr std::uint32_t kCompressionNone = 0;

// Les textures du jeu font 256 × 256 ; le plafond laisse une marge très large
// tout en refusant une dimension corrompue avant qu'elle ne devienne une
// allocation démesurée.
constexpr std::int32_t kMaxDimension = 16384;

}  // namespace

core::Result<Image> DecodeBmp(std::span<const std::uint8_t> data) {
  ByteReader reader(data);

  const std::span<const std::uint8_t> signature_bytes = reader.Bytes(kBmpSignature.size());
  const std::string_view signature(reinterpret_cast<const char*>(signature_bytes.data()),
                                   signature_bytes.size());
  if (signature != kBmpSignature) {
    return Fail(ErrorCode::kBadSignature, "en-tête BMP attendu « BM »");
  }

  reader.Skip(8);  // taille du fichier et champs réservés
  const std::uint32_t pixel_offset = reader.U32();
  const std::uint32_t info_header_size = reader.U32();
  if (!reader.ok()) {
    return Fail(ErrorCode::kUnexpectedEnd, "en-tête BMP tronqué");
  }
  if (info_header_size < kInfoHeaderSize) {
    // BITMAPCOREHEADER (12 octets) et consorts : formats antérieurs qui
    // n'apparaissent pas dans les données du jeu.
    return Fail(ErrorCode::kUnsupportedVersion,
                "en-tête d'information BMP de " + std::to_string(info_header_size) + " octets");
  }

  const std::int32_t width = reader.I32();
  const std::int32_t raw_height = reader.I32();
  reader.Skip(2);  // plans
  const std::uint16_t bits_per_pixel = reader.U16();
  const std::uint32_t compression = reader.U32();
  reader.Skip(4);  // taille de l'image, souvent nulle en BI_RGB
  reader.Skip(8);  // résolutions
  const std::uint32_t palette_entries_declared = reader.U32();
  if (!reader.ok()) {
    return Fail(ErrorCode::kUnexpectedEnd, "en-tête d'information BMP tronqué");
  }

  if (compression != kCompressionNone) {
    return Fail(ErrorCode::kUnsupportedVersion,
                "BMP compressé (méthode " + std::to_string(compression) + ")");
  }
  if (bits_per_pixel != 8 && bits_per_pixel != 24 && bits_per_pixel != 32) {
    return Fail(ErrorCode::kUnsupportedVersion,
                "BMP en " + std::to_string(bits_per_pixel) + " bits par pixel");
  }

  // Une hauteur négative signale une image rangée de haut en bas.
  const bool bottom_up = raw_height >= 0;
  // ⚠ LA NÉGATION SE FAIT EN 64 BITS : celle du plus petit entier 32 bits
  // déborde, avant même que la borne ci-dessous ait pu la refuser.
  const std::int64_t height64 =
      raw_height >= 0 ? std::int64_t{raw_height} : -std::int64_t{raw_height};
  if (height64 > kMaxDimension) {
    return Fail(ErrorCode::kCorruptData,
                "dimensions BMP " + std::to_string(width) + "x" + std::to_string(raw_height));
  }
  const auto height = static_cast<std::int32_t>(height64);
  if (width <= 0 || height <= 0 || width > kMaxDimension || height > kMaxDimension) {
    return Fail(ErrorCode::kCorruptData,
                "dimensions BMP " + std::to_string(width) + "x" + std::to_string(raw_height));
  }

  // Palette : présente seulement en 8 bits. Un compte déclaré nul signifie
  // « toutes les couleurs représentables », soit 256.
  std::vector<std::uint8_t> palette;
  if (bits_per_pixel == 8) {
    const std::uint32_t entries = palette_entries_declared == 0 ? 256 : palette_entries_declared;
    if (entries > 256) {
      return Fail(ErrorCode::kCorruptData,
                  "palette BMP de " + std::to_string(entries) + " entrées");
    }
    if (!reader.Seek(kFileHeaderSize + info_header_size)) {
      return Fail(ErrorCode::kUnexpectedEnd, "palette BMP hors du fichier");
    }
    const std::span<const std::uint8_t> raw_palette = reader.Bytes(entries * 4);
    if (!reader.ok()) {
      return Fail(ErrorCode::kUnexpectedEnd, "palette BMP tronquée");
    }
    palette.assign(raw_palette.begin(), raw_palette.end());
  }

  // Chaque rangée est complétée à un multiple de quatre octets.
  const std::size_t row_stride = ((static_cast<std::size_t>(width) * bits_per_pixel + 31) / 32) * 4;
  if (!reader.Seek(pixel_offset)) {
    return Fail(ErrorCode::kCorruptData, "offset des pixels BMP hors du fichier");
  }
  const std::span<const std::uint8_t> rows =
      reader.Bytes(row_stride * static_cast<std::size_t>(height));
  if (!reader.ok()) {
    return Fail(ErrorCode::kUnexpectedEnd, "pixels BMP tronqués");
  }

  Image image;
  image.width = width;
  image.height = height;
  image.pixels.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4);

  for (std::int32_t y = 0; y < height; ++y) {
    // Rangée source : la dernière du fichier correspond au haut de l'image
    // quand celle-ci est rangée de bas en haut.
    const std::size_t source_row =
        bottom_up ? static_cast<std::size_t>(height - 1 - y) : static_cast<std::size_t>(y);
    const std::uint8_t* source = rows.data() + source_row * row_stride;
    std::uint8_t* destination =
        image.pixels.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(width) * 4;

    for (std::int32_t x = 0; x < width; ++x) {
      std::uint8_t blue = 0;
      std::uint8_t green = 0;
      std::uint8_t red = 0;
      std::uint8_t alpha = 255;

      switch (bits_per_pixel) {
        case 8: {
          const std::size_t index = static_cast<std::size_t>(source[x]) * 4;
          if (index + 2 < palette.size()) {
            blue = palette[index];
            green = palette[index + 1];
            red = palette[index + 2];
          }
          break;
        }
        case 24: {
          const std::uint8_t* pixel = source + static_cast<std::size_t>(x) * 3;
          blue = pixel[0];
          green = pixel[1];
          red = pixel[2];
          break;
        }
        default: {  // 32 bits
          const std::uint8_t* pixel = source + static_cast<std::size_t>(x) * 4;
          blue = pixel[0];
          green = pixel[1];
          red = pixel[2];
          // Le quatrième octet d'un BMP 32 bits BI_RGB est officiellement
          // inutilisé et vaut souvent zéro ; le prendre pour un alpha rendrait
          // l'image entièrement transparente.
          break;
        }
      }

      std::uint8_t* out = destination + static_cast<std::size_t>(x) * 4;
      out[0] = red;
      out[1] = green;
      out[2] = blue;
      out[3] = alpha;
    }
  }

  return image;
}

}  // namespace ro::formats
