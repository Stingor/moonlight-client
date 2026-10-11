// LE DÉCODAGE D'UNE IMAGE WEBP PAR LIBWEBP.

#include <algorithm>
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
constexpr auto kMostFrames = static_cast<std::size_t>(ro::formats::kWebPictureMostFrames);

struct AnimDecoderFree {
  void operator()(WebPAnimDecoder* decoder) const { WebPAnimDecoderDelete(decoder); }
};
using AnimDecoder = std::unique_ptr<WebPAnimDecoder, AnimDecoderFree>;

[[nodiscard]] std::unexpected<ro::core::Error> Refused(const char* why) {
  return ro::core::Fail(ro::core::ErrorCode::kUnsupportedVersion, std::string("libwebp : ") + why);
}

[[nodiscard]] std::unexpected<ro::core::Error> TooLarge() {
  return ro::core::Fail(ro::core::ErrorCode::kInvalidArgument,
                        "libwebp : image trop grande pour être décodée sans risque");
}

// Les octets d'UNE image de cette taille, ou 0 quand elle sort des bornes.
[[nodiscard]] std::size_t FrameBytes(int width, int height) {
  if (width <= 0 || height <= 0 || width > kMostSide || height > kMostSide) {
    return 0;
  }
  const std::size_t bytes =
      static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * kChannels;
  return bytes > kMostDecodedBytes ? 0 : bytes;
}

[[nodiscard]] ro::formats::Image BlankImage(int width, int height, std::size_t frame_bytes) {
  ro::formats::Image image;
  image.width = width;
  image.height = height;
  image.pixels.resize(frame_bytes);
  return image;
}

// UN WEBP ANIMÉ, image par image, chacune recomposée sur le canevas.
//
// 🔴 LES IMAGES SE BORNENT COMME CELLES D'UN GIF : `kWebPictureMostFrames` au
// plus, et toutes ensemble sous `kWebPictureMostDecodedBytes`. Celles qui
// dépassent sont laissées, pas refusées : l'animation se coupe plus tôt.
[[nodiscard]] ro::core::Result<ro::formats::DecodedWebPicture> DecodeAnimation(
    std::span<const std::uint8_t> bytes) {
  WebPAnimDecoderOptions options;
  if (WebPAnimDecoderOptionsInit(&options) == 0) {
    return Refused("décodeur animé indisponible");
  }
  options.color_mode = MODE_RGBA;
  options.use_threads = 0;
  const WebPData data{.bytes = bytes.data(), .size = bytes.size()};
  const AnimDecoder decoder(WebPAnimDecoderNew(&data, &options));
  if (!decoder) {
    return Refused("animation illisible");
  }
  WebPAnimInfo info;
  if (WebPAnimDecoderGetInfo(decoder.get(), &info) == 0 || info.frame_count == 0) {
    return Refused("animation illisible");
  }
  const int width = static_cast<int>(info.canvas_width);
  const int height = static_cast<int>(info.canvas_height);
  const std::size_t frame_bytes = FrameBytes(width, height);
  if (frame_bytes == 0) {
    return TooLarge();
  }
  const std::size_t frames_kept = std::min(
      {static_cast<std::size_t>(info.frame_count), kMostFrames, kMostDecodedBytes / frame_bytes});

  ro::formats::DecodedWebPicture decoded;
  decoded.frames.reserve(frames_kept);
  decoded.delays_ms.reserve(frames_kept);
  int previous_end_ms = 0;
  while (decoded.frames.size() < frames_kept && WebPAnimDecoderHasMoreFrames(decoder.get()) != 0) {
    std::uint8_t* canvas = nullptr;
    int end_ms = 0;
    if (WebPAnimDecoderGetNext(decoder.get(), &canvas, &end_ms) == 0 || canvas == nullptr) {
      break;
    }
    // Le canevas appartient au décodeur, qui le réécrit à l'image suivante.
    ro::formats::Image image = BlankImage(width, height, frame_bytes);
    std::memcpy(image.pixels.data(), canvas, frame_bytes);
    decoded.frames.push_back(std::move(image));
    // Le décodeur rend l'instant où l'image FINIT ; sa durée est l'écart.
    decoded.delays_ms.push_back(static_cast<std::uint32_t>(std::max(0, end_ms - previous_end_ms)));
    previous_end_ms = end_ms;
  }
  if (decoded.frames.empty()) {
    return Refused("première image illisible");
  }
  return decoded;
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
  const std::size_t frame_bytes = FrameBytes(features.width, features.height);
  if (frame_bytes == 0) {
    return TooLarge();
  }
  if (features.has_animation != 0) {
    return DecodeAnimation(bytes);
  }

  ro::formats::Image image = BlankImage(features.width, features.height, frame_bytes);
  const std::size_t stride = static_cast<std::size_t>(features.width) * kChannels;
  if (WebPDecodeRGBAInto(bytes.data(), bytes.size(), image.pixels.data(), image.pixels.size(),
                         static_cast<int>(stride)) == nullptr) {
    return Refused("pixels illisibles");
  }
  ro::formats::DecodedWebPicture decoded;
  decoded.frames.push_back(std::move(image));
  decoded.delays_ms.push_back(0);
  return decoded;
}

}  // namespace moonclient
