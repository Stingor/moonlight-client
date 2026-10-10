// LE DÉCODAGE D'UNE IMAGE WEBP PAR LIBWEBP.

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <utility>

#include <webp/decode.h>
#include <webp/demux.h>

#include "web/webp_picture.h"

namespace moonclient {
namespace {

constexpr std::size_t kChannels = 4;

// 🔴 LA GARDE DU DÉCODAGE, celle des images du web : un en-tête de quelques
// octets peut annoncer un gigaoctet de pixels.
constexpr std::size_t kMostDecodedBytes = ro::formats::kWebPictureMostDecodedBytes;
constexpr auto kMostSide = static_cast<int>(ro::formats::kWebPictureMostSide);

struct AnimDecoderFree {
  void operator()(WebPAnimDecoder* decoder) const { WebPAnimDecoderDelete(decoder); }
};
using AnimDecoder = std::unique_ptr<WebPAnimDecoder, AnimDecoderFree>;

[[nodiscard]] std::unexpected<ro::core::Error> Refused(const char* why) {
  return ro::core::Fail(ro::core::ErrorCode::kUnsupportedVersion, std::string("libwebp : ") + why);
}

// LA PREMIÈRE IMAGE D'UN WEBP ANIMÉ, recomposée sur son canevas : le décodeur
// simple refuse un fichier animé.
[[nodiscard]] bool DecodeFirstFrame(std::span<const std::uint8_t> bytes,
                                    ro::formats::Image& image) {
  WebPAnimDecoderOptions options;
  if (WebPAnimDecoderOptionsInit(&options) == 0) {
    return false;
  }
  options.color_mode = MODE_RGBA;
  options.use_threads = 0;
  const WebPData data{.bytes = bytes.data(), .size = bytes.size()};
  const AnimDecoder decoder(WebPAnimDecoderNew(&data, &options));
  if (!decoder) {
    return false;
  }
  std::uint8_t* canvas = nullptr;
  int timestamp_ms = 0;
  if (WebPAnimDecoderGetNext(decoder.get(), &canvas, &timestamp_ms) == 0 || canvas == nullptr) {
    return false;
  }
  // Le canevas appartient au décodeur : on le recopie avant qu'il ne meure.
  std::memcpy(image.pixels.data(), canvas, image.pixels.size());
  return true;
}

}  // namespace

ro::core::Result<ro::formats::DecodedWebPicture> DecodeWebpPicture(
    std::span<const std::uint8_t> bytes) {
  // ⚠ LE WEBP SEUL : les autres formats passent par `stb`.
  if (ro::formats::SniffWebPicture(bytes) != ro::formats::WebPictureFormat::kWebp) {
    return ro::core::Fail(ro::core::ErrorCode::kBadSignature, "libwebp : seul le WebP passe ici");
  }
  // 🔴 LA TAILLE SE LIT DANS L'EN-TÊTE ET SE JUGE AVANT TOUTE ALLOCATION : le
  // décodeur animé réserve son canevas entier dès sa création.
  WebPBitstreamFeatures features;
  if (WebPGetFeatures(bytes.data(), bytes.size(), &features) != VP8_STATUS_OK) {
    return Refused("WebP illisible");
  }
  if (features.width <= 0 || features.height <= 0) {
    return Refused("taille illisible");
  }
  if (features.width > kMostSide || features.height > kMostSide ||
      static_cast<std::size_t>(features.width) * static_cast<std::size_t>(features.height) *
              kChannels >
          kMostDecodedBytes) {
    return ro::core::Fail(ro::core::ErrorCode::kInvalidArgument,
                          "libwebp : image trop grande pour être décodée sans risque");
  }

  ro::formats::Image image;
  image.width = features.width;
  image.height = features.height;
  const std::size_t stride = static_cast<std::size_t>(features.width) * kChannels;
  image.pixels.resize(stride * static_cast<std::size_t>(features.height));
  if (features.has_animation != 0) {
    if (!DecodeFirstFrame(bytes, image)) {
      return Refused("première image illisible");
    }
  } else if (WebPDecodeRGBAInto(bytes.data(), bytes.size(), image.pixels.data(),
                                image.pixels.size(), static_cast<int>(stride)) == nullptr) {
    return Refused("pixels illisibles");
  }

  ro::formats::DecodedWebPicture decoded;
  decoded.frames.push_back(std::move(image));
  decoded.delays_ms.push_back(0);
  return decoded;
}

}  // namespace moonclient
