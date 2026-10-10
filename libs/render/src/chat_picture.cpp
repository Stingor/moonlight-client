#include "ro/render/chat_picture.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

#include "ro/render/card_thumbnail_pixels.h"

namespace ro::render {
namespace {

constexpr std::size_t kChannels = 4;

[[nodiscard]] std::size_t FrameBytes(ThumbnailSize size) {
  return static_cast<std::size_t>(size.width) * static_cast<std::size_t>(size.height) * kChannels;
}

// LA TAILLE D'UNE ANIMATION RÉDUITE : le côté le plus long tient dans
// `most_side_animated`, puis il baisse, pixel par pixel, tant que toutes les
// images ensemble pèsent plus que `most_animated_bytes`. Le rapport des côtés
// reste celui de la source.
[[nodiscard]] ThumbnailSize AnimatedSize(int width, int height, std::size_t frame_count,
                                         const ChatPictureLimits& limits) {
  int most_side = std::max(1, limits.most_side_animated);
  ThumbnailSize size = FitThumbnail(width, height, most_side, most_side);
  while (most_side > 1 && FrameBytes(size) * frame_count > limits.most_animated_bytes) {
    --most_side;
    size = FitThumbnail(width, height, most_side, most_side);
  }
  return size;
}

}  // namespace

core::Result<ChatPicture> PrepareChatPicture(std::span<const std::uint8_t> bytes,
                                             const ChatPictureLimits& limits) {
  const auto info = formats::ProbeWebPicture(bytes);
  if (!info) {
    return std::unexpected(info.error());
  }
  // 🔴 LA GARDE, AVANT LE DÉCODAGE : `stb_image` décode à pleine taille, et un
  // en-tête de quelques octets peut annoncer un gigaoctet de pixels.
  const auto kept_frames =
      static_cast<std::size_t>(std::clamp(info->frames, 1, std::max(1, limits.most_frames)));
  const std::size_t decoded_bytes = static_cast<std::size_t>(info->width) *
                                    static_cast<std::size_t>(info->height) * kChannels *
                                    kept_frames;
  if (decoded_bytes > limits.most_decoded_bytes) {
    return core::Fail(core::ErrorCode::kInvalidArgument,
                      "image du chat trop grande pour être décodée sans risque");
  }

  // Un GIF trop long est coupé AVANT le décodage : ses images en trop ne
  // coûtent rien.
  std::vector<std::uint8_t> truncated;
  std::span<const std::uint8_t> decodable = bytes;
  if (formats::SniffWebPicture(bytes) == formats::WebPictureFormat::kGif &&
      static_cast<std::size_t>(info->frames) > kept_frames) {
    truncated = formats::TruncateGifFrames(bytes, static_cast<int>(kept_frames));
    decodable = truncated;
  }
  // La mémoire du décodeur, bornée en proportion des pixels permis : un flux
  // qui gonfle au-delà est refusé pendant le décodage.
  auto decoded = formats::DecodeWebPicture(
      decodable, limits.most_decoded_bytes * formats::kWebPictureWorkingFactor);
  if (!decoded) {
    return std::unexpected(decoded.error());
  }
  return FitChatPicture(std::move(*decoded), limits);
}

core::Result<ChatPicture> FitChatPicture(formats::DecodedWebPicture decoded,
                                         const ChatPictureLimits& limits) {
  if (decoded.frames.empty()) {
    return core::Fail(core::ErrorCode::kInvalidArgument, "image du chat : aucune image");
  }
  const auto most_frames = static_cast<std::size_t>(std::max(1, limits.most_frames));
  if (decoded.frames.size() > most_frames) {
    decoded.frames.resize(most_frames);
  }
  decoded.delays_ms.resize(decoded.frames.size(), 0);

  const formats::Image& first = decoded.frames.front();
  if (first.width <= 0 || first.height <= 0) {
    return core::Fail(core::ErrorCode::kCorruptData, "image du chat : image vide");
  }
  for (const formats::Image& frame : decoded.frames) {
    if (frame.width != first.width || frame.height != first.height ||
        frame.pixels.size() != FrameBytes({.width = first.width, .height = first.height})) {
      return core::Fail(core::ErrorCode::kCorruptData,
                        "image du chat : images d'animation de tailles différentes");
    }
  }

  const bool animated = decoded.frames.size() > 1;
  const int most_still = std::max(1, limits.most_side_still);
  const ThumbnailSize size =
      animated ? AnimatedSize(first.width, first.height, decoded.frames.size(), limits)
               : FitThumbnail(first.width, first.height, most_still, most_still);

  ChatPicture picture;
  picture.delays_ms = std::move(decoded.delays_ms);
  picture.frames.reserve(decoded.frames.size());
  for (formats::Image& frame : decoded.frames) {
    if (frame.width == size.width && frame.height == size.height) {
      picture.frames.push_back(std::move(frame));
    } else {
      picture.frames.push_back(ShrinkImage(frame, size));
    }
  }
  return picture;
}

}  // namespace ro::render
