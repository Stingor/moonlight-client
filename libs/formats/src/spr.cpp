#include "ro/formats/spr.h"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "ro/core/byte_reader.h"

namespace ro::formats {
namespace {

using core::ByteReader;
using core::ErrorCode;
using core::Fail;

constexpr std::string_view kSprSignature{"SP", 2};

// Seuils de version, en majeure × 10 + mineure.
//
// 2.0 sépare les deux sections d'images ; 2.1 apporte la compression par
// plages ; 2.2 élargit la longueur compressée de 16 à 32 bits ; 3.2 supprime la
// section palettisée et compresse la section BGRA par zlib.
constexpr std::int32_t kVersionTwoSections = 20;
constexpr std::int32_t kVersionRunLength = 21;
constexpr std::int32_t kVersionWideLength = 22;
constexpr std::int32_t kVersionZlibBgra = 32;

// Une image de sprite RO tient très largement dans ces bornes ; elles refusent
// un en-tête corrompu avant qu'il ne devienne une allocation démesurée.
constexpr std::int32_t kMaxDimension = 4096;
constexpr std::int32_t kMaxFrameCount = 10000;

// Décompresse les plages d'une image palettisée.
//
// Encodage minimal : un octet non nul EST un index de palette ; un octet nul
// est suivi d'un COMPTE de pixels transparents à sauter. La sortie est
// pré-remplie de zéros, donc sauter revient à ne rien écrire.
core::Result<void> ExpandRunLength(std::span<const std::uint8_t> source,
                                   std::vector<std::uint8_t>& out) {
  std::size_t written = 0;
  for (std::size_t index = 0; index < source.size(); ++index) {
    const std::uint8_t value = source[index];
    if (value != 0) {
      if (written >= out.size()) {
        return Fail(ErrorCode::kCorruptData, "plage débordant de l'image");
      }
      out[written++] = value;
      continue;
    }
    if (++index >= source.size()) {
      return Fail(ErrorCode::kUnexpectedEnd, "compte de transparence manquant");
    }
    written += source[index];
    if (written > out.size()) {
      return Fail(ErrorCode::kCorruptData, "plage transparente débordant de l'image");
    }
  }
  return {};
}

// Lit les dimensions communes aux deux sections.
core::Result<std::pair<std::int32_t, std::int32_t>> ReadFrameSize(ByteReader& reader) {
  const auto width = static_cast<std::int32_t>(reader.U16());
  const auto height = static_cast<std::int32_t>(reader.U16());
  if (!reader.ok()) {
    return Fail(ErrorCode::kUnexpectedEnd, "dimensions d'image tronquées");
  }
  if (width > kMaxDimension || height > kMaxDimension) {
    return Fail(ErrorCode::kCorruptData,
                "image de sprite " + std::to_string(width) + "x" + std::to_string(height));
  }
  return std::pair{width, height};
}

core::Result<SprIndexedFrame> ReadIndexedFrame(ByteReader& reader, std::int32_t version) {
  auto size = ReadFrameSize(reader);
  if (!size) {
    return std::unexpected(size.error());
  }

  SprIndexedFrame frame;
  frame.width = size->first;
  frame.height = size->second;
  const auto pixel_count =
      static_cast<std::size_t>(frame.width) * static_cast<std::size_t>(frame.height);
  frame.indices.assign(pixel_count, 0);

  if (version < kVersionRunLength) {
    // Avant la 2.1, les index sont écrits tels quels, sans longueur préalable.
    const std::span<const std::uint8_t> pixels = reader.Bytes(pixel_count);
    if (pixels.size() != pixel_count) {
      return Fail(ErrorCode::kUnexpectedEnd, "image palettisée tronquée");
    }
    std::copy(pixels.begin(), pixels.end(), frame.indices.begin());
    return frame;
  }

  const std::size_t length = version >= kVersionWideLength ? static_cast<std::size_t>(reader.U32())
                                                           : static_cast<std::size_t>(reader.U16());
  if (!reader.ok()) {
    return Fail(ErrorCode::kUnexpectedEnd, "longueur d'image tronquée");
  }
  const std::span<const std::uint8_t> packed = reader.Bytes(length);
  if (packed.size() != length) {
    return Fail(ErrorCode::kUnexpectedEnd, "image compressée tronquée");
  }
  if (auto expanded = ExpandRunLength(packed, frame.indices); !expanded) {
    return std::unexpected(expanded.error());
  }
  return frame;
}

core::Result<Image> ReadBgraFrame(ByteReader& reader) {
  auto size = ReadFrameSize(reader);
  if (!size) {
    return std::unexpected(size.error());
  }

  Image image;
  image.width = size->first;
  image.height = size->second;
  const auto pixel_count =
      static_cast<std::size_t>(image.width) * static_cast<std::size_t>(image.height);
  image.pixels.assign(pixel_count * 4, 0);

  const std::span<const std::uint8_t> raw = reader.Bytes(pixel_count * 4);
  if (raw.size() != pixel_count * 4) {
    return Fail(ErrorCode::kUnexpectedEnd, "image BGRA tronquée");
  }

  // ⚠ Seul endroit du format où les rangées sont à l'ENVERS, et où les
  // composantes viennent dans l'ordre alpha, bleu, vert, rouge. Rater le sens
  // des rangées retourne l'image ; rater l'ordre des composantes donne des
  // rayures colorées. Les deux passent inaperçus de toute assertion de taille.
  for (std::int32_t y = 0; y < image.height; ++y) {
    const std::size_t source_row =
        static_cast<std::size_t>(image.height - 1 - y) * static_cast<std::size_t>(image.width) * 4;
    const std::size_t destination_row =
        static_cast<std::size_t>(y) * static_cast<std::size_t>(image.width) * 4;
    for (std::int32_t x = 0; x < image.width; ++x) {
      const std::size_t source = source_row + static_cast<std::size_t>(x) * 4;
      const std::size_t destination = destination_row + static_cast<std::size_t>(x) * 4;
      image.pixels[destination + 0] = raw[source + 3];
      image.pixels[destination + 1] = raw[source + 2];
      image.pixels[destination + 2] = raw[source + 1];
      image.pixels[destination + 3] = raw[source + 0];
    }
  }
  return image;
}

}  // namespace

core::Result<SprSprite> SprSprite::Parse(std::span<const std::uint8_t> data) {
  ByteReader reader(data);
  if (reader.FixedString(kSprSignature.size()) != kSprSignature) {
    return Fail(ErrorCode::kBadSignature, "signature .spr absente");
  }
  const std::uint8_t minor = reader.U8();
  const std::uint8_t major = reader.U8();
  if (!reader.ok()) {
    return Fail(ErrorCode::kUnexpectedEnd, "en-tête .spr tronqué");
  }

  SprSprite sprite;
  sprite.version_ = static_cast<std::int32_t>(major) * 10 + static_cast<std::int32_t>(minor);
  if (sprite.version_ >= kVersionZlibBgra) {
    // Aucun fichier de l'installation n'est en 3.2 : écrire une décompression
    // zlib qu'on ne peut pas éprouver vaudrait moins qu'un refus qui se nomme.
    return Fail(ErrorCode::kUnsupportedVersion, "sprite en version 3.2 (section BGRA zlib)");
  }

  std::int32_t indexed_count = 0;
  std::int32_t bgra_count = 0;
  if (sprite.version_ >= kVersionTwoSections) {
    indexed_count = static_cast<std::int32_t>(reader.U16());
    bgra_count = static_cast<std::int32_t>(reader.U16());
  } else {
    // Avant la 2.0 il n'y a qu'une section, et les images suivent aussitôt.
    indexed_count = static_cast<std::int32_t>(reader.U16());
  }
  if (!reader.ok()) {
    return Fail(ErrorCode::kUnexpectedEnd, "compte d'images tronqué");
  }
  if (indexed_count > kMaxFrameCount || bgra_count > kMaxFrameCount) {
    return Fail(ErrorCode::kCorruptData, "compte d'images de sprite déraisonnable");
  }

  // La palette occupe le DERNIER kilo-octet du fichier ; les images ne peuvent
  // pas empiéter dessus. Un compte exagéré s'arrête donc ici plutôt que de lire
  // la palette comme si c'étaient des pixels.
  if (indexed_count > 0 && data.size() < kPaletteBytes) {
    return Fail(ErrorCode::kUnexpectedEnd, "fichier plus court que sa palette");
  }
  // ⚠ ET LE LECTEUR DES IMAGES S'ARRÊTE AVANT ELLE : tester la taille seule ne
  // l'empêchait pas de lire, sur un compte exagéré, les 1 024 octets de la
  // palette comme des pixels.
  if (indexed_count > 0) {
    const std::size_t header_end = reader.offset();
    reader = ByteReader(data.first(data.size() - kPaletteBytes));
    if (!reader.Seek(header_end)) {
      return Fail(ErrorCode::kUnexpectedEnd, "en-tête .spr empiétant sur la palette");
    }
  }

  // Une image illisible ARRÊTE la lecture sans perdre les précédentes : voir
  // SprSprite::truncated(). La première erreur est conservée, parce qu'elle
  // devient l'échec du fichier si rien n'a pu être lu.
  std::optional<core::Error> first_error;

  sprite.indexed_frames_.reserve(static_cast<std::size_t>(indexed_count));
  for (std::int32_t index = 0; index < indexed_count && !first_error.has_value(); ++index) {
    auto frame = ReadIndexedFrame(reader, sprite.version_);
    if (!frame) {
      first_error = frame.error();
      break;
    }
    sprite.indexed_frames_.push_back(std::move(*frame));
  }

  sprite.bgra_frames_.reserve(static_cast<std::size_t>(bgra_count));
  for (std::int32_t index = 0; index < bgra_count && !first_error.has_value(); ++index) {
    auto frame = ReadBgraFrame(reader);
    if (!frame) {
      first_error = frame.error();
      break;
    }
    sprite.bgra_frames_.push_back(std::move(*frame));
  }

  if (first_error.has_value()) {
    if (sprite.indexed_frames_.empty() && sprite.bgra_frames_.empty()) {
      return std::unexpected(*first_error);
    }
    sprite.truncated_ = true;
  }

  if (indexed_count > 0) {
    auto palette = DecodePalette(data.subspan(data.size() - kPaletteBytes));
    if (!palette) {
      return std::unexpected(palette.error());
    }
    sprite.palette_ = *palette;
  }
  return sprite;
}

Image ApplyPalette(const SprIndexedFrame& frame, const Palette& palette) {
  Image image;
  image.width = frame.width;
  image.height = frame.height;
  image.pixels.resize(frame.indices.size() * 4);

  for (std::size_t pixel = 0; pixel < frame.indices.size(); ++pixel) {
    const std::uint8_t index = frame.indices[pixel];
    const std::size_t offset = pixel * 4;
    if (index == 0) {
      // Noir transparent, et non la couleur de l'entrée 0 : voir spr.h.
      image.pixels[offset + 0] = 0;
      image.pixels[offset + 1] = 0;
      image.pixels[offset + 2] = 0;
      image.pixels[offset + 3] = 0;
      continue;
    }
    image.pixels[offset + 0] = palette.entries[index][0];
    image.pixels[offset + 1] = palette.entries[index][1];
    image.pixels[offset + 2] = palette.entries[index][2];
    image.pixels[offset + 3] = 255;
  }
  return image;
}

}  // namespace ro::formats
