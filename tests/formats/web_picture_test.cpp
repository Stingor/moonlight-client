// UNE IMAGE VENUE DU WEB : la reconnaître, la mesurer sans la décoder, couper un
// GIF trop long, et le composer image par image comme un navigateur.
//
// ⚠ LES IMAGES D'ESSAI SONT LES NÔTRES : encodées par `EncodeAnimatedGif`, ou
// écrites octet par octet quand il faut une disposition que notre encodeur ne
// produit pas.

#include "ro/formats/web_picture.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/gif.h"

#include "support/image_builder.h"

using ro::formats::DecodeWebPicture;
using ro::formats::Image;
using ro::formats::ProbeWebPicture;
using ro::formats::SniffWebPicture;
using ro::formats::TruncateGifFrames;
using ro::formats::WebPictureFormat;

namespace {

constexpr std::size_t kChannels = 4;
constexpr int kDelayCs = 7;
constexpr std::uint32_t kMsPerCs = 10;

struct Rgba {
  std::uint8_t red = 0;
  std::uint8_t green = 0;
  std::uint8_t blue = 0;
  std::uint8_t alpha = 0;
  bool operator==(const Rgba&) const = default;
};

constexpr Rgba kClear{0, 0, 0, 0};
constexpr Rgba kRed{255, 0, 0, 255};
constexpr Rgba kBlue{0, 0, 255, 255};
constexpr Rgba kWhite{255, 255, 255, 255};

[[nodiscard]] Rgba PixelAt(const Image& image, int x, int y) {
  const std::size_t at = (static_cast<std::size_t>(y) * static_cast<std::size_t>(image.width) +
                          static_cast<std::size_t>(x)) *
                         kChannels;
  return Rgba{image.pixels[at], image.pixels[at + 1], image.pixels[at + 2], image.pixels[at + 3]};
}

[[nodiscard]] Image Filled(int width, int height, Rgba color) {
  Image image;
  image.width = width;
  image.height = height;
  for (int index = 0; index < width * height; ++index) {
    image.pixels.insert(image.pixels.end(), {color.red, color.green, color.blue, color.alpha});
  }
  return image;
}

// ── UN GIF ÉCRIT À LA MAIN ───────────────────────────────────────────────────
//
// Quatre couleurs (noir, rouge, bleu, blanc), une taille de code LZW de deux :
// les codes tiennent sur trois bits tant que le dictionnaire n'atteint pas huit
// entrées, ce qui est le cas de ces images d'un ou deux pixels.

constexpr std::uint8_t kIndexRed = 1;
constexpr std::uint8_t kIndexBlue = 2;
constexpr std::uint8_t kIndexWhite = 3;
constexpr std::uint8_t kLzwMinimumCodeSize = 2;
constexpr std::uint32_t kLzwClear = 4;
constexpr std::uint32_t kLzwEnd = 5;
constexpr std::uint32_t kLzwCodeBits = 3;

// Des codes bruts de trois bits, empaquetés poids faible d'abord, en un seul
// sous-bloc.
void AppendRawCodes(std::vector<std::uint8_t>& gif, const std::vector<std::uint32_t>& codes) {
  std::vector<std::uint8_t> packed;
  std::uint32_t bit = 0;
  for (const std::uint32_t code : codes) {
    for (std::uint32_t place = 0; place < kLzwCodeBits; ++place, ++bit) {
      if (bit / 8U >= packed.size()) {
        packed.push_back(0);
      }
      if (((code >> place) & 1U) != 0) {
        packed[bit / 8U] = static_cast<std::uint8_t>(packed[bit / 8U] | (1U << (bit % 8U)));
      }
    }
  }
  gif.push_back(kLzwMinimumCodeSize);
  gif.push_back(static_cast<std::uint8_t>(packed.size()));
  gif.insert(gif.end(), packed.begin(), packed.end());
  gif.push_back(0);
}

// Les pixels d'une image : effacement, un code par pixel, fin.
void AppendImageData(std::vector<std::uint8_t>& gif, std::initializer_list<std::uint8_t> indices) {
  std::vector<std::uint32_t> codes = {kLzwClear};
  codes.insert(codes.end(), indices.begin(), indices.end());
  codes.push_back(kLzwEnd);
  AppendRawCodes(gif, codes);
}

void AppendHeaders(std::vector<std::uint8_t>& gif, std::uint8_t disposal, int left, int width) {
  constexpr std::uint8_t kDisposalShift = 2;
  // Le contrôle : disposition, délai, sans transparence.
  gif.insert(gif.end(), {0x21, 0xF9, 0x04, static_cast<std::uint8_t>(disposal << kDisposalShift),
                         kDelayCs, 0x00, 0x00, 0x00});
  gif.insert(gif.end(), {0x2C, static_cast<std::uint8_t>(left), 0x00, 0x00, 0x00,
                         static_cast<std::uint8_t>(width), 0x00, 0x01, 0x00, 0x00});
}

void AppendFrame(std::vector<std::uint8_t>& gif, std::uint8_t disposal, int left, int width,
                 std::initializer_list<std::uint8_t> indices) {
  AppendHeaders(gif, disposal, left, width);
  AppendImageData(gif, indices);
}

// UN FLUX LZW QUI S'EMBALLE, sur une toile d'un pixel : chaque code désigne la
// chaîne que le précédent vient de créer, plus longue d'un pixel, puis la plus
// longue chaîne se répète. Quelques kilo-octets pour des millions de pixels.
constexpr std::uint32_t kLzwFirstFreeCode = kLzwClear + 2;
constexpr std::uint32_t kLzwLastGrowingCode = 0x0FFF;
constexpr std::uint32_t kLzwLongestCode = 4095;
constexpr std::size_t kSubBlockBytes = 255;

[[nodiscard]] std::vector<std::uint8_t> RunawayGif(int repeats) {
  std::vector<std::uint8_t> packed;
  std::uint64_t pending_bits = 0;
  std::uint32_t pending_count = 0;
  const auto put = [&](std::uint32_t code, std::uint32_t size) {
    pending_bits |= static_cast<std::uint64_t>(code) << pending_count;
    pending_count += size;
    while (pending_count >= 8) {
      packed.push_back(static_cast<std::uint8_t>(pending_bits & 0xFFU));
      pending_bits >>= 8U;
      pending_count -= 8;
    }
  };
  std::uint32_t size = kLzwCodeBits;
  std::uint32_t avail = kLzwFirstFreeCode;
  bool has_previous = false;
  const auto emit = [&](std::uint32_t code) {
    put(code, size);
    if (has_previous) {
      ++avail;
      if ((avail & ((1U << size) - 1U)) == 0 && avail <= kLzwLastGrowingCode) {
        ++size;
      }
    }
    has_previous = true;
  };
  put(kLzwClear, size);
  emit(0);
  while (avail <= kLzwLongestCode) {
    emit(avail);
  }
  for (int index = 0; index < repeats; ++index) {
    emit(kLzwLongestCode);
  }
  put(kLzwEnd, size);
  if (pending_count > 0) {
    packed.push_back(static_cast<std::uint8_t>(pending_bits & 0xFFU));
  }

  std::vector<std::uint8_t> gif = {
      'G',  'I',  'F',  '8',  '9',  'a',  0x01, 0x00, 0x01, 0x00, 0x81, 0x00,
      0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF,
      0xFF, 0x2C, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, kLzwMinimumCodeSize};
  for (std::size_t at = 0; at < packed.size(); at += kSubBlockBytes) {
    const std::size_t length = std::min(kSubBlockBytes, packed.size() - at);
    gif.push_back(static_cast<std::uint8_t>(length));
    gif.insert(gif.end(), packed.begin() + static_cast<std::ptrdiff_t>(at),
               packed.begin() + static_cast<std::ptrdiff_t>(at + length));
  }
  gif.push_back(0);
  gif.push_back(0x3B);
  return gif;
}

// UN PNG D'UN PIXEL DONT LE FLUX COMPRESSÉ PORTE `inflated_bytes` OCTETS : des
// blocs zlib « stockés », sans compression — l'essai n'a besoin que du
// volume décompressé, et pas de zlib.
[[nodiscard]] std::vector<std::uint8_t> SwollenPng(std::size_t inflated_bytes) {
  constexpr std::size_t kStoredBlockBytes = 65535;
  constexpr std::uint32_t kAdlerModulus = 65521;
  std::vector<std::uint8_t> zlib = {0x78, 0x01};
  std::uint32_t adler_low = 1;
  std::uint32_t adler_high = 0;
  for (std::size_t done = 0; done < inflated_bytes;) {
    const std::size_t length = std::min(kStoredBlockBytes, inflated_bytes - done);
    done += length;
    const bool last = done == inflated_bytes;
    const auto stored = static_cast<std::uint16_t>(length);
    const auto complement = static_cast<std::uint16_t>(~stored);
    zlib.insert(
        zlib.end(),
        {static_cast<std::uint8_t>(last ? 1 : 0), static_cast<std::uint8_t>(stored & 0xFFU),
         static_cast<std::uint8_t>(stored >> 8U), static_cast<std::uint8_t>(complement & 0xFFU),
         static_cast<std::uint8_t>(complement >> 8U)});
    zlib.insert(zlib.end(), length, 0);
    // Des zéros : la somme basse reste à un, la haute gagne un par octet.
    adler_high = static_cast<std::uint32_t>((adler_high + length * adler_low) % kAdlerModulus);
  }
  const std::uint32_t adler = (adler_high << 16U) | adler_low;
  for (int shift = 24; shift >= 0; shift -= 8) {
    zlib.push_back(static_cast<std::uint8_t>((adler >> static_cast<std::uint32_t>(shift)) & 0xFFU));
  }

  const auto big_endian = [](std::vector<std::uint8_t>& into, std::uint32_t value) {
    for (int shift = 24; shift >= 0; shift -= 8) {
      into.push_back(
          static_cast<std::uint8_t>((value >> static_cast<std::uint32_t>(shift)) & 0xFFU));
    }
  };
  std::vector<std::uint8_t> png = {0x89, 'P',  'N',  'G',  0x0D, 0x0A, 0x1A, 0x0A, 0x00,
                                   0x00, 0x00, 0x0D, 'I',  'H',  'D',  'R',  0x00, 0x00,
                                   0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x08, 0x06, 0x00,
                                   0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  // Le contrôle des morceaux n'est pas relu par le décodeur.
  big_endian(png, static_cast<std::uint32_t>(zlib.size()));
  png.insert(png.end(), {'I', 'D', 'A', 'T'});
  png.insert(png.end(), zlib.begin(), zlib.end());
  png.insert(png.end(), {0, 0, 0, 0, 0, 0, 0, 0, 'I', 'E', 'N', 'D', 0, 0, 0, 0});
  return png;
}

// Une toile de deux pixels sur un, sa table de quatre couleurs.
[[nodiscard]] std::vector<std::uint8_t> TwoPixelCanvas() {
  return {'G', 'I', 'F', '8', '9', 'a', 0x02, 0x00, 0x01, 0x00, 0x81, 0x00, 0x00,
          // noir, rouge, bleu, blanc
          0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF};
}

[[nodiscard]] std::vector<std::uint8_t> EncodedClip(int frame_count, int width, int height) {
  std::vector<Image> frames;
  for (int index = 0; index < frame_count; ++index) {
    Image frame = Filled(width, height, kWhite);
    // Une tache qui avance d'une colonne par image.
    for (int y = 0; y < height; ++y) {
      const std::size_t at = (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                              static_cast<std::size_t>(index % width)) *
                             kChannels;
      frame.pixels[at + 1] = 0;
      frame.pixels[at + 2] = 0;
    }
    frames.push_back(std::move(frame));
  }
  auto gif = ro::formats::EncodeAnimatedGif(frames, kDelayCs);
  REQUIRE(gif.has_value());
  return *gif;
}

}  // namespace

TEST_CASE("Image web : chaque format se reconnaît à ses premiers octets", "[formats][web]") {
  using Bytes = std::vector<std::uint8_t>;
  CHECK(SniffWebPicture(Bytes{0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A, 0}) ==
        WebPictureFormat::kPng);
  CHECK(SniffWebPicture(Bytes{0xFF, 0xD8, 0xFF, 0xE0}) == WebPictureFormat::kJpeg);
  CHECK(SniffWebPicture(Bytes{'G', 'I', 'F', '8', '9', 'a'}) == WebPictureFormat::kGif);
  CHECK(SniffWebPicture(Bytes{'G', 'I', 'F', '8', '7', 'a'}) == WebPictureFormat::kGif);
  CHECK(SniffWebPicture(Bytes{'B', 'M', 0, 0}) == WebPictureFormat::kBmp);
  CHECK(SniffWebPicture(Bytes{'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'E', 'B', 'P'}) ==
        WebPictureFormat::kWebp);
  // Un RIFF qui n'est pas une image : un son WAV.
  CHECK(SniffWebPicture(Bytes{'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', 'E'}) ==
        WebPictureFormat::kUnknown);
  CHECK(SniffWebPicture(Bytes{'<', 'h', 't', 'm', 'l', '>'}) == WebPictureFormat::kUnknown);
  CHECK(SniffWebPicture(Bytes{}) == WebPictureFormat::kUnknown);
}

TEST_CASE("Image web : un GIF se mesure et se compte sans être décodé", "[formats][web]") {
  const std::vector<std::uint8_t> gif = EncodedClip(5, 12, 8);
  const auto info = ProbeWebPicture(gif);
  REQUIRE(info.has_value());
  CHECK(info->width == 12);
  CHECK(info->height == 8);
  CHECK(info->frames == 5);

  const auto bmp = ro::test_support::BuildBmp(3, 2, std::vector<ro::test_support::BmpPixel>(6),
                                              {.bits_per_pixel = 24});
  const auto still = ProbeWebPicture(bmp);
  REQUIRE(still.has_value());
  CHECK(still->width == 3);
  CHECK(still->height == 2);
  CHECK(still->frames == 1);

  CHECK_FALSE(ProbeWebPicture(std::vector<std::uint8_t>{'x', 'y'}).has_value());
}

TEST_CASE("Image web : l'en-tête d'un WebP donne ses côtés", "[formats][web]") {
  // Étendu : la toile, largeur − 1 et hauteur − 1 sur vingt-quatre bits.
  std::vector<std::uint8_t> extended = {'R', 'I', 'F', 'F', 0,  0, 0, 0, 'W', 'E', 'B', 'P',
                                        'V', 'P', '8', 'X', 10, 0, 0, 0, 0,   0,   0,   0};
  extended.insert(extended.end(), {0x3F, 0x40, 0x00, 0xFF, 0x3F, 0x00});  // 16448 × 16384
  const auto canvas = ProbeWebPicture(extended);
  REQUIRE(canvas.has_value());
  CHECK(canvas->width == 16448);
  CHECK(canvas->height == 16384);

  // Sans perte : 14 bits de largeur − 1, puis 14 de hauteur − 1.
  std::vector<std::uint8_t> lossless = {'R', 'I', 'F', 'F', 0,   0, 0, 0, 'W', 'E', 'B',
                                        'P', 'V', 'P', '8', 'L', 0, 0, 0, 0,   0x2F};
  // largeur 300 (299), hauteur 200 (199)
  constexpr std::uint32_t kSides = 299U | (199U << 14U);
  lossless.insert(
      lossless.end(),
      {static_cast<std::uint8_t>(kSides & 0xFFU), static_cast<std::uint8_t>((kSides >> 8U) & 0xFFU),
       static_cast<std::uint8_t>((kSides >> 16U) & 0xFFU),
       static_cast<std::uint8_t>((kSides >> 24U) & 0xFFU), 0, 0, 0, 0, 0});
  const auto small = ProbeWebPicture(lossless);
  REQUIRE(small.has_value());
  CHECK(small->width == 300);
  CHECK(small->height == 200);

  // ⚠ Le WebP se MESURE ici, mais ne s'y décode pas : son décodeur est ailleurs.
  const auto decoded = DecodeWebPicture(lossless);
  REQUIRE_FALSE(decoded.has_value());
  CHECK(decoded.error().code == ro::core::ErrorCode::kUnsupportedVersion);
}

TEST_CASE("Image web : un GIF encodé revient image par image", "[formats][web]") {
  constexpr int kFrames = 4;
  constexpr int kWidth = 6;
  constexpr int kHeight = 3;
  const std::vector<std::uint8_t> gif = EncodedClip(kFrames, kWidth, kHeight);
  const auto decoded = DecodeWebPicture(gif);
  REQUIRE(decoded.has_value());
  REQUIRE(decoded->frames.size() == static_cast<std::size_t>(kFrames));
  REQUIRE(decoded->delays_ms.size() == static_cast<std::size_t>(kFrames));
  for (int index = 0; index < kFrames; ++index) {
    INFO("image " << index);
    const Image& frame = decoded->frames[static_cast<std::size_t>(index)];
    CHECK(decoded->delays_ms[static_cast<std::size_t>(index)] == kDelayCs * kMsPerCs);
    REQUIRE(frame.width == kWidth);
    REQUIRE(frame.height == kHeight);
    for (int x = 0; x < kWidth; ++x) {
      for (int y = 0; y < kHeight; ++y) {
        INFO("pixel " << x << ", " << y);
        CHECK(PixelAt(frame, x, y) == (x == index ? kRed : kWhite));
      }
    }
  }
}

TEST_CASE("Image web : la disposition 2 efface le rectangle de l'image", "[formats][web]") {
  std::vector<std::uint8_t> gif = TwoPixelCanvas();
  AppendFrame(gif, 2, 0, 2, {kIndexRed, kIndexRed});
  AppendFrame(gif, 1, 1, 1, {kIndexBlue});
  gif.push_back(0x3B);

  const auto decoded = DecodeWebPicture(gif);
  REQUIRE(decoded.has_value());
  REQUIRE(decoded->frames.size() == 2);
  CHECK(PixelAt(decoded->frames[0], 0, 0) == kRed);
  CHECK(PixelAt(decoded->frames[0], 1, 0) == kRed);
  // Le rouge de la première image est effacé : la toile y redevient transparente.
  CHECK(PixelAt(decoded->frames[1], 0, 0) == kClear);
  CHECK(PixelAt(decoded->frames[1], 1, 0) == kBlue);
}

TEST_CASE("Image web : la disposition 3 rend la toile d'avant l'image", "[formats][web]") {
  std::vector<std::uint8_t> gif = TwoPixelCanvas();
  AppendFrame(gif, 1, 0, 2, {kIndexRed, kIndexRed});
  AppendFrame(gif, 3, 1, 1, {kIndexBlue});
  AppendFrame(gif, 1, 0, 1, {kIndexWhite});
  gif.push_back(0x3B);

  const auto decoded = DecodeWebPicture(gif);
  REQUIRE(decoded.has_value());
  REQUIRE(decoded->frames.size() == 3);
  CHECK(PixelAt(decoded->frames[1], 0, 0) == kRed);
  CHECK(PixelAt(decoded->frames[1], 1, 0) == kBlue);
  // Le bleu de l'image 2 a disparu : la toile est revenue au rouge.
  CHECK(PixelAt(decoded->frames[2], 0, 0) == kWhite);
  CHECK(PixelAt(decoded->frames[2], 1, 0) == kRed);
}

TEST_CASE("Image web : un GIF se coupe après sa n-ième image", "[formats][web]") {
  const std::vector<std::uint8_t> gif = EncodedClip(6, 8, 4);
  const std::vector<std::uint8_t> three = TruncateGifFrames(gif, 3);
  CHECK(three.size() < gif.size());
  CHECK(three.back() == 0x3B);
  const auto info = ProbeWebPicture(three);
  REQUIRE(info.has_value());
  CHECK(info->frames == 3);
  const auto decoded = DecodeWebPicture(three);
  REQUIRE(decoded.has_value());
  CHECK(decoded->frames.size() == 3);

  // Assez court déjà : rendu tel quel.
  CHECK(TruncateGifFrames(gif, 6) == gif);
  CHECK(TruncateGifFrames(gif, 60) == gif);
}

TEST_CASE("Image web : un GIF coupé par la fin du flux montre ses images entières",
          "[formats][web]") {
  const std::vector<std::uint8_t> gif = EncodedClip(3, 8, 4);
  // Sans fin de flux, et la troisième image amputée de ses derniers octets.
  std::vector<std::uint8_t> cut(gif.begin(), gif.end() - 4);
  const auto info = ProbeWebPicture(cut);
  REQUIRE(info.has_value());
  CHECK(info->frames == 2);
}

TEST_CASE("Image web : une image fixe sort en RGBA, rangée du haut d'abord", "[formats][web]") {
  using ro::test_support::BmpPixel;
  const std::vector<BmpPixel> pixels = {{255, 0, 0}, {0, 0, 255}};
  const auto bmp = ro::test_support::BuildBmp(1, 2, pixels, {.bits_per_pixel = 24});
  const auto decoded = DecodeWebPicture(bmp);
  REQUIRE(decoded.has_value());
  REQUIRE(decoded->frames.size() == 1);
  CHECK(decoded->delays_ms == std::vector<std::uint32_t>{0});
  CHECK(PixelAt(decoded->frames[0], 0, 0) == kRed);
  CHECK(PixelAt(decoded->frames[0], 0, 1) == kBlue);
  CHECK(decoded->frames[0].file == ro::formats::ImageFile::kBmp);

  CHECK_FALSE(DecodeWebPicture(std::vector<std::uint8_t>{'<', 'h'}).has_value());
}

TEST_CASE("Image web : un flux LZW qui émet trop de pixels est refusé sans être décodé",
          "[formats][web]") {
  const std::vector<std::uint8_t> gif = RunawayGif(100);
  // Quelques kilo-octets, pour une toile d'un seul pixel.
  CHECK(gif.size() < 16U * 1024U);
  CHECK_FALSE(ProbeWebPicture(gif).has_value());
  CHECK_FALSE(DecodeWebPicture(gif).has_value());
  // Coupé ou non, il reste ce qu'il est.
  CHECK(TruncateGifFrames(gif, 1) == gif);
}

TEST_CASE("Image web : la disposition 3 sur la première image rend la toile vide",
          "[formats][web]") {
  std::vector<std::uint8_t> gif = TwoPixelCanvas();
  AppendFrame(gif, 3, 0, 2, {kIndexRed, kIndexRed});
  AppendFrame(gif, 1, 1, 1, {kIndexBlue});
  gif.push_back(0x3B);

  const auto decoded = DecodeWebPicture(gif);
  REQUIRE(decoded.has_value());
  REQUIRE(decoded->frames.size() == 2);
  CHECK(PixelAt(decoded->frames[0], 0, 0) == kRed);
  CHECK(PixelAt(decoded->frames[1], 0, 0) == kClear);
  CHECK(PixelAt(decoded->frames[1], 1, 0) == kBlue);
}

TEST_CASE("Image web : une image illisible en cours de flux laisse voir les précédentes",
          "[formats][web]") {
  std::vector<std::uint8_t> gif = TwoPixelCanvas();
  AppendFrame(gif, 1, 0, 2, {kIndexRed, kIndexBlue});
  // Un code au-delà du dictionnaire : le décodeur le refuserait.
  AppendHeaders(gif, 1, 0, 2);
  AppendRawCodes(gif, {kLzwClear, 7, kLzwEnd});
  gif.push_back(0x3B);

  const auto info = ProbeWebPicture(gif);
  REQUIRE(info.has_value());
  CHECK(info->frames == 1);
  const auto decoded = DecodeWebPicture(gif);
  REQUIRE(decoded.has_value());
  REQUIRE(decoded->frames.size() == 1);
  CHECK(PixelAt(decoded->frames[0], 1, 0) == kBlue);
}

TEST_CASE("Image web : une image hors de la toile", "[formats][web]") {
  // En seconde position : la première se montre seule.
  std::vector<std::uint8_t> second = TwoPixelCanvas();
  AppendFrame(second, 1, 0, 2, {kIndexRed, kIndexRed});
  AppendFrame(second, 1, 1, 2, {kIndexBlue, kIndexBlue});
  second.push_back(0x3B);
  const auto decoded = DecodeWebPicture(second);
  REQUIRE(decoded.has_value());
  CHECK(decoded->frames.size() == 1);

  // En première : rien à montrer.
  std::vector<std::uint8_t> first = TwoPixelCanvas();
  AppendFrame(first, 1, 1, 2, {kIndexBlue, kIndexBlue});
  first.push_back(0x3B);
  CHECK_FALSE(DecodeWebPicture(first).has_value());
}

TEST_CASE("Image web : soixante images au plus, et le compte s'arrête juste après",
          "[formats][web]") {
  const std::vector<std::uint8_t> gif = EncodedClip(70, 2, 2);
  const auto info = ProbeWebPicture(gif);
  REQUIRE(info.has_value());
  CHECK(info->frames == ro::formats::kWebPictureMostFrames + 1);
  const auto decoded = DecodeWebPicture(gif);
  REQUIRE(decoded.has_value());
  CHECK(decoded->frames.size() == static_cast<std::size_t>(ro::formats::kWebPictureMostFrames));
}

TEST_CASE("Image web : un JPEG aux passes innombrables est refusé avant décodage",
          "[formats][web]") {
  const auto jpeg_with_scans = [](int scans) {
    std::vector<std::uint8_t> jpeg = {0xFF, 0xD8};
    for (int index = 0; index < scans; ++index) {
      // Un début de passe minimal, puis un octet de données.
      jpeg.insert(jpeg.end(), {0xFF, 0xDA, 0x00, 0x02, 0x11});
    }
    jpeg.insert(jpeg.end(), {0xFF, 0xD9});
    return jpeg;
  };
  const auto many = DecodeWebPicture(jpeg_with_scans(200));
  REQUIRE_FALSE(many.has_value());
  CHECK(many.error().code == ro::core::ErrorCode::kInvalidArgument);
  // Peu de passes : c'est le décodeur qui refuse ce faux JPEG, pas le compte.
  const auto few = DecodeWebPicture(jpeg_with_scans(3));
  REQUIRE_FALSE(few.has_value());
  CHECK(few.error().code != ro::core::ErrorCode::kInvalidArgument);
}

TEST_CASE("Image web : un flux qui gonfle est arrêté à la mémoire permise", "[formats][web]") {
  constexpr std::size_t kInflated = 4U * 1024U * 1024U;
  constexpr std::size_t kTightBudget = 1U * 1024U * 1024U;
  const std::vector<std::uint8_t> png = SwollenPng(kInflated);
  // L'en-tête annonce un pixel : la mesure ne voit rien venir.
  const auto info = ProbeWebPicture(png);
  REQUIRE(info.has_value());
  CHECK(info->width == 1);

  const auto refused = DecodeWebPicture(png, kTightBudget);
  REQUIRE_FALSE(refused.has_value());
  CHECK(refused.error().code == ro::core::ErrorCode::kInvalidArgument);

  // Sous la borne d'usine, le même fichier se lit : c'est bien la borne qui mord.
  const auto read = DecodeWebPicture(png);
  REQUIRE(read.has_value());
  CHECK(read->frames.front().width == 1);
}
