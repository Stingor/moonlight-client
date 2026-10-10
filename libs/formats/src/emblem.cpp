#include "ro/formats/emblem.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <limits>
#include <string>

#include <stb_image.h>
#include <zlib.h>

#include "ro/core/byte_reader.h"
#include "ro/core/byte_writer.h"
#include "ro/formats/bmp.h"

namespace ro::formats {
namespace {

using core::ByteReader;
using core::ErrorCode;
using core::Fail;

// ── L'EN-TÊTE D'UN BMP, aux décalages où le serveur et le jeu le lisent ──────
constexpr std::size_t kBmpHeaderBytes = 54;
constexpr std::size_t kDeclaredSizeAt = 2;
constexpr std::size_t kPixelOffsetAt = 10;
constexpr std::size_t kInfoSizeAt = 14;
constexpr std::size_t kWidthAt = 18;
constexpr std::size_t kHeightAt = 22;
constexpr std::size_t kPlanesAt = 26;
constexpr std::size_t kDepthAt = 28;
constexpr std::size_t kCompressionAt = 30;
constexpr std::size_t kImageSizeAt = 34;
constexpr std::uint32_t kInfoHeaderBytes = 40;
constexpr std::uint16_t kTrueColorDepth = 24;
constexpr std::uint16_t kPaletteDepth = 8;
constexpr std::uint32_t kNoCompression = 0;

constexpr std::size_t kBytesPerPixel = 3;
constexpr std::size_t kEmblemRowBytes = static_cast<std::size_t>(kEmblemSide) * kBytesPerPixel;

// ── LA FENÊTRE DU COMPTE DE TRANSPARENCE ─────────────────────────────────────
//
// Six octets qui valent deux pixels magenta consécutifs, lus en trois mots de
// quatre octets qui se chevauchent — c'est la lecture du serveur.
constexpr std::size_t kTransparencyWindow = 3;
constexpr std::size_t kWordBytes = 4;
constexpr std::int32_t kMagentaWordFirst = static_cast<std::int32_t>(0xFFFF00FFU);
constexpr std::int32_t kMagentaWordSecond = 0x00FFFF00;
constexpr std::int32_t kMagentaWordThird = static_cast<std::int32_t>(0xFF00FFFFU);
// Le serveur part d'UN, pas de zéro : le compte n'est jamais nul.
constexpr int kTransparencyFirstCount = 1;
constexpr int kTransparencyScale = 300;

// ── LES OCTETS D'UNE COULEUR `0xRRGGBB` ──────────────────────────────────────
constexpr unsigned kRedShift = 16;
constexpr unsigned kGreenShift = 8;
constexpr std::uint32_t kChannelMask = 0xFFU;
constexpr std::uint8_t kOpaque = 255;

// LE GIF COMMENCE PAR SA VERSION : « GIF87a » ou « GIF89a ».
constexpr std::string_view kGifSignature = "GIF8";

// CE QU'UN EMBLÈME DÉCOMPRESSÉ PEUT PESER au plus. Le serveur le borne à 1800
// octets ; la marge laisse passer un en-tête bavard sans laisser gonfler un
// flux piégé.
constexpr std::size_t kInflatedMost = 64U * 1024U;

[[nodiscard]] std::uint32_t U32At(std::span<const std::uint8_t> bytes, std::size_t at) {
  ByteReader reader(bytes);
  reader.Skip(at);
  return reader.U32();
}

[[nodiscard]] std::int32_t I32At(std::span<const std::uint8_t> bytes, std::size_t at) {
  ByteReader reader(bytes);
  reader.Skip(at);
  return reader.I32();
}

[[nodiscard]] std::uint16_t U16At(std::span<const std::uint8_t> bytes, std::size_t at) {
  ByteReader reader(bytes);
  reader.Skip(at);
  return reader.U16();
}

// Une taille annoncée qu'on accepte de décoder : voir `kEmblemMostSide`.
[[nodiscard]] bool FitsEmblem(int width, int height) {
  return width > 0 && height > 0 && width <= kEmblemMostSide && height <= kEmblemMostSide;
}

[[nodiscard]] std::uint32_t PackedColor(std::uint8_t red, std::uint8_t green, std::uint8_t blue) {
  return (std::uint32_t{red} << kRedShift) | (std::uint32_t{green} << kGreenShift) | blue;
}

}  // namespace

EmblemVerdict CheckEmblemBmp(std::span<const std::uint8_t> bmp) {
  EmblemVerdict verdict;
  verdict.bytes = bmp.size();
  const auto fail = [&verdict](EmblemFault fault) {
    verdict.fault = fault;
    return verdict;
  };
  if (bmp.size() < kBmpHeaderBytes) {
    return fail(EmblemFault::kTooShort);
  }
  if (bmp[0] != 'B' || bmp[1] != 'M') {
    return fail(EmblemFault::kNotBmp);
  }
  if (U32At(bmp, kDeclaredSizeAt) != bmp.size()) {
    return fail(EmblemFault::kDeclaredSize);
  }
  if (U32At(bmp, kPixelOffsetAt) >= bmp.size()) {
    return fail(EmblemFault::kPixelsOutside);
  }
  verdict.width = I32At(bmp, kWidthAt);
  const std::int32_t raw_height = I32At(bmp, kHeightAt);
  // ⚠ LA NÉGATION SE FAIT EN 64 BITS : celle du plus petit entier déborde.
  const std::int64_t height = raw_height < 0 ? -std::int64_t{raw_height} : std::int64_t{raw_height};
  verdict.height = height > std::numeric_limits<std::int32_t>::max()
                       ? std::numeric_limits<std::int32_t>::max()
                       : static_cast<std::int32_t>(height);
  if (verdict.width != kEmblemSide || verdict.height != kEmblemSide) {
    return fail(EmblemFault::kWrongSize);
  }
  const std::uint16_t depth = U16At(bmp, kDepthAt);
  if (depth != kTrueColorDepth && depth != kPaletteDepth) {
    return fail(EmblemFault::kDepth);
  }
  if (U32At(bmp, kCompressionAt) != kNoCompression) {
    return fail(EmblemFault::kCompressed);
  }
  if (bmp.size() > kEmblemMostBytes) {
    return fail(EmblemFault::kTooHeavy);
  }
  return verdict;
}

int EmblemTransparencyPercent(std::span<const std::uint8_t> bmp) {
  if (bmp.size() < kBmpHeaderBytes) {
    return 0;
  }
  const std::uint32_t offset = U32At(bmp, kPixelOffsetAt);
  if (offset >= bmp.size()) {
    return 0;
  }
  int count = kTransparencyFirstCount;
  std::array<std::int32_t, kTransparencyWindow> window = {};
  // ⚠ LE RANG DANS LA FENÊTRE SE LIT SUR L'INDEX ABSOLU, comme chez le serveur,
  // et non sur l'écart à l'origine des pixels.
  for (std::size_t at = offset; at + kWordBytes <= bmp.size(); ++at) {
    const std::size_t slot = at % kTransparencyWindow;
    window[slot] = I32At(bmp, at);
    if (slot == kTransparencyWindow - 1 && window[0] == kMagentaWordFirst &&
        window[1] == kMagentaWordSecond && window[2] == kMagentaWordThird) {
      ++count;
    }
  }
  return count * kTransparencyScale / static_cast<int>(bmp.size() - offset);
}

std::vector<std::uint8_t> EncodeEmblemBmp(const EmblemPixels& pixels) {
  constexpr std::size_t kPixelBytes = kEmblemRowBytes * static_cast<std::size_t>(kEmblemSide);
  constexpr std::size_t kFileBytes = kBmpHeaderBytes + kPixelBytes;
  std::vector<std::uint8_t> bytes;
  bytes.reserve(kFileBytes);
  core::ByteWriter out(bytes);
  out.U8('B');
  out.U8('M');
  out.U32(static_cast<std::uint32_t>(kFileBytes));
  out.U32(0);  // deux champs réservés
  out.U32(static_cast<std::uint32_t>(kBmpHeaderBytes));
  out.U32(kInfoHeaderBytes);
  out.I32(kEmblemSide);
  // Une hauteur POSITIVE : les rangées vont de bas en haut, la forme ordinaire.
  out.I32(kEmblemSide);
  out.U16(1);  // un plan
  out.U16(kTrueColorDepth);
  out.U32(kNoCompression);
  out.U32(static_cast<std::uint32_t>(kPixelBytes));
  out.U32(0);  // résolutions et palette : sans objet en 24 bits
  out.U32(0);
  out.U32(0);
  out.U32(0);
  for (int row = kEmblemSide - 1; row >= 0; --row) {
    for (int column = 0; column < kEmblemSide; ++column) {
      const std::uint32_t color =
          pixels[static_cast<std::size_t>(row) * kEmblemSide + static_cast<std::size_t>(column)];
      out.U8(static_cast<std::uint8_t>(color & kChannelMask));
      out.U8(static_cast<std::uint8_t>((color >> kGreenShift) & kChannelMask));
      out.U8(static_cast<std::uint8_t>((color >> kRedShift) & kChannelMask));
    }
  }
  static_assert(kInfoSizeAt + sizeof(std::uint32_t) == kWidthAt);
  static_assert(kPlanesAt + sizeof(std::uint16_t) == kDepthAt);
  static_assert(kCompressionAt + sizeof(std::uint32_t) == kImageSizeAt);
  return bytes;
}

core::Result<EmblemPixels> EmblemPixelsFromBmp(std::span<const std::uint8_t> bmp) {
  if (bmp.size() < kBmpHeaderBytes || bmp[0] != 'B' || bmp[1] != 'M') {
    return Fail(ErrorCode::kBadSignature, "emblème : pas un BMP");
  }
  const std::int32_t raw_height = I32At(bmp, kHeightAt);
  const std::int64_t height = raw_height < 0 ? -std::int64_t{raw_height} : std::int64_t{raw_height};
  const std::uint16_t depth = U16At(bmp, kDepthAt);
  if (I32At(bmp, kWidthAt) != kEmblemSide || height != kEmblemSide ||
      (depth != kTrueColorDepth && depth != kPaletteDepth)) {
    return Fail(ErrorCode::kUnsupportedVersion, "emblème : ni 24 × 24, ni 24 bits ou 256 couleurs");
  }
  auto image = DecodeBmp(bmp);
  if (!image) {
    return std::unexpected(image.error());
  }
  EmblemPixels pixels = {};
  for (std::size_t index = 0; index < kEmblemPixelCount; ++index) {
    const std::uint8_t* rgba = image->pixels.data() + index * kWordBytes;
    pixels[index] = PackedColor(rgba[0], rgba[1], rgba[2]);
  }
  return pixels;
}

EmblemPixels EmblemPixelsFromImage(const Image& image) {
  EmblemPixels pixels;
  pixels.fill(kEmblemClearColor);
  if (image.width <= 0 || image.height <= 0 ||
      image.pixels.size() < static_cast<std::size_t>(image.width) *
                                static_cast<std::size_t>(image.height) * kWordBytes) {
    return pixels;
  }
  // Négatif quand l'image est plus grande que l'emblème : elle est rognée.
  const int left = (kEmblemSide - image.width) / 2;
  const int top = (kEmblemSide - image.height) / 2;
  for (int y = 0; y < image.height; ++y) {
    const int row = y + top;
    if (row < 0 || row >= kEmblemSide) {
      continue;
    }
    for (int x = 0; x < image.width; ++x) {
      const int column = x + left;
      if (column < 0 || column >= kEmblemSide) {
        continue;
      }
      const std::uint8_t* rgba = image.pixels.data() + (static_cast<std::size_t>(y) *
                                                            static_cast<std::size_t>(image.width) +
                                                        static_cast<std::size_t>(x)) *
                                                           kWordBytes;
      if (rgba[3] == 0) {
        continue;
      }
      pixels[static_cast<std::size_t>(row) * kEmblemSide + static_cast<std::size_t>(column)] =
          PackedColor(rgba[0], rgba[1], rgba[2]);
    }
  }
  return pixels;
}

core::Result<Image> DecodeEmblemImage(std::span<const std::uint8_t> data) {
  if (data.size() >= kGifSignature.size() &&
      std::equal(kGifSignature.begin(), kGifSignature.end(), data.begin())) {
    if (data.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
      return Fail(ErrorCode::kUnsupportedVersion, "emblème GIF trop grand");
    }
    int width = 0;
    int height = 0;
    int channels = 0;
    // ⚠ L'EN-TÊTE D'ABORD, sans rien allouer : voir `kEmblemMostSide`.
    if (stbi_info_from_memory(data.data(), static_cast<int>(data.size()), &width, &height,
                              &channels) == 0) {
      return Fail(ErrorCode::kCorruptData, "emblème GIF : en-tête illisible");
    }
    if (!FitsEmblem(width, height)) {
      return Fail(ErrorCode::kUnsupportedVersion, "emblème GIF de " + std::to_string(width) +
                                                      " × " + std::to_string(height) +
                                                      " pixels : refusé sans décodage");
    }
    stbi_uc* rgba = stbi_load_from_memory(data.data(), static_cast<int>(data.size()), &width,
                                          &height, &channels, static_cast<int>(kWordBytes));
    if (rgba == nullptr || width <= 0 || height <= 0) {
      const char* why = stbi_failure_reason();
      stbi_image_free(rgba);
      return Fail(ErrorCode::kCorruptData,
                  std::string("emblème GIF illisible : ") + (why != nullptr ? why : "sans motif"));
    }
    Image image;
    image.width = width;
    image.height = height;
    image.pixels.assign(rgba, rgba + static_cast<std::size_t>(width) *
                                         static_cast<std::size_t>(height) * kWordBytes);
    stbi_image_free(rgba);
    return image;
  }
  if (data.size() >= kHeightAt + sizeof(std::int32_t)) {
    const std::int64_t width = I32At(data, kWidthAt);
    const std::int64_t height = std::abs(static_cast<std::int64_t>(I32At(data, kHeightAt)));
    if (width > kEmblemMostSide || height > kEmblemMostSide) {
      return Fail(ErrorCode::kUnsupportedVersion, "emblème BMP de " + std::to_string(width) +
                                                      " × " + std::to_string(height) +
                                                      " pixels : refusé sans décodage");
    }
  }
  auto image = DecodeBmp(data);
  if (!image) {
    return std::unexpected(image.error());
  }
  ApplyMagentaColorKey(*image);
  return image;
}

core::Result<std::vector<std::uint8_t>> InflateEmblem(std::span<const std::uint8_t> compressed) {
  if (compressed.empty()) {
    return Fail(ErrorCode::kUnexpectedEnd, "emblème : flux zlib vide");
  }
  std::vector<std::uint8_t> output(kInflatedMost);
  z_stream stream{};
  if (inflateInit(&stream) != Z_OK) {
    return Fail(ErrorCode::kCorruptData, "emblème : inflateInit a échoué");
  }
  // zlib ne modifie pas l'entrée, mais son API ne le dit pas dans le type.
  stream.next_in = const_cast<Bytef*>(static_cast<const Bytef*>(compressed.data()));
  stream.avail_in = static_cast<uInt>(compressed.size());
  stream.next_out = output.data();
  stream.avail_out = static_cast<uInt>(output.size());
  const int status = inflate(&stream, Z_FINISH);
  const uLong produced = stream.total_out;
  inflateEnd(&stream);
  // ⚠ UN FLUX QUI N'EST PAS FINI QUAND LA PLACE MANQUE EST REFUSÉ, pas tronqué :
  // un BMP coupé passerait le contrôle de signature et s'afficherait à moitié.
  if (status != Z_STREAM_END) {
    return Fail(ErrorCode::kCorruptData, "emblème : flux zlib incomplet ou trop gros");
  }
  output.resize(produced);
  return output;
}

}  // namespace ro::formats
