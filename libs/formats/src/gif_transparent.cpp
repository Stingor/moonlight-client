#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "ro/formats/gif.h"

#include "gif_internal.h"

namespace ro::formats {
namespace {

using gif_internal::CellOf;
using gif_internal::kOpaqueAlpha;
using gif_internal::kTransparentIndex;

// Disposition 2 — « remettre le fond » —, plus le drapeau de transparence. Le
// fond est l'index transparent : la toile redevient vide sous chaque image.
constexpr std::uint8_t kRestoreBackgroundAndTransparent =
    (2 << gif_internal::kDisposalShift) | gif_internal::kTransparentFlag;

// Le rectangle de ce qu'une image peint.
struct Painted {
  int left = 0;
  int top = 0;
  int width = 0;
  int height = 0;
};

// ⚠ RIEN DE PEINT : une image d'un pixel transparent, qui brûle son délai —
// la sauter avancerait le reste du clip.
[[nodiscard]] Painted PaintedArea(std::span<const std::uint8_t> indices, int width, int height) {
  int left = width;
  int top = height;
  int right = -1;
  int bottom = -1;
  for (int y = 0; y < height; ++y) {
    const std::size_t row = static_cast<std::size_t>(y) * static_cast<std::size_t>(width);
    for (int x = 0; x < width; ++x) {
      if (indices[row + static_cast<std::size_t>(x)] == kTransparentIndex) {
        continue;
      }
      left = std::min(left, x);
      right = std::max(right, x);
      top = std::min(top, y);
      bottom = std::max(bottom, y);
    }
  }
  if (right < 0) {
    return Painted{.left = 0, .top = 0, .width = 1, .height = 1};
  }
  return Painted{.left = left, .top = top, .width = right - left + 1, .height = bottom - top + 1};
}

}  // namespace

core::Result<std::vector<std::uint8_t>> EncodeTransparentGif(
    std::span<const Image> frames, std::span<const int> delays_centiseconds) {
  if (frames.empty()) {
    return core::Fail(core::ErrorCode::kInvalidArgument, "aucune image à encoder");
  }
  if (delays_centiseconds.size() != frames.size()) {
    return core::Fail(core::ErrorCode::kInvalidArgument, "un délai par image");
  }
  const int width = frames.front().width;
  const int height = frames.front().height;
  if (width <= 0 || height <= 0) {
    return core::Fail(core::ErrorCode::kInvalidArgument, "image de taille nulle");
  }
  const auto expected = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4U;
  for (const Image& frame : frames) {
    if (frame.width != width || frame.height != height || frame.pixels.size() != expected) {
      return core::Fail(core::ErrorCode::kInvalidArgument,
                        "les images d'un GIF ont toutes la même taille");
    }
  }

  const gif_internal::Palette palette = gif_internal::BuildPalette(frames, /*opaque_only=*/true);

  std::vector<std::uint8_t> out;
  gif_internal::PutHeader(out, width, height, palette);

  const auto count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
  std::vector<std::uint8_t> indices(count);
  std::vector<std::uint8_t> piece;
  for (std::size_t number = 0; number < frames.size(); ++number) {
    const Image& frame = frames[number];
    for (std::size_t at = 0; at < count; ++at) {
      const std::uint8_t* pixel = &frame.pixels[at * 4];
      indices[at] = pixel[3] < kOpaqueAlpha ? kTransparentIndex
                                            : palette.of_cell[CellOf(pixel[0], pixel[1], pixel[2])];
    }

    const Painted area = PaintedArea(indices, width, height);
    piece.resize(static_cast<std::size_t>(area.width) * static_cast<std::size_t>(area.height));
    for (int y = 0; y < area.height; ++y) {
      const std::size_t row =
          static_cast<std::size_t>(area.top + y) * static_cast<std::size_t>(width) +
          static_cast<std::size_t>(area.left);
      std::copy_n(indices.begin() + static_cast<std::ptrdiff_t>(row), area.width,
                  piece.begin() + static_cast<std::ptrdiff_t>(y) * area.width);
    }

    gif_internal::PutGraphicControl(out, kRestoreBackgroundAndTransparent,
                                    delays_centiseconds[number]);
    gif_internal::PutImage(out, area.left, area.top, area.width, area.height, piece);
  }

  out.push_back(gif_internal::kTrailer);
  return out;
}

}  // namespace ro::formats
