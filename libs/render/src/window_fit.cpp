#include "ro/render/window_fit.h"

#include <algorithm>

namespace ro::render {

namespace {

[[nodiscard]] bool Empty(Resolution size) noexcept { return size.width <= 0 || size.height <= 0; }

}  // namespace

Resolution FitWindowSize(Resolution wanted, Resolution room) noexcept {
  if (Empty(wanted) || Empty(room)) {
    return wanted;
  }
  if (wanted.width <= room.width && wanted.height <= room.height) {
    return wanted;
  }
  const float wide = static_cast<float>(wanted.width);
  const float tall = static_cast<float>(wanted.height);
  const float factor = std::min(static_cast<float>(room.width) * kFittedRoomShare / wide,
                                static_cast<float>(room.height) * kFittedRoomShare / tall);
  Resolution fitted{.width = static_cast<int>(wide * factor),
                    .height = static_cast<int>(tall * factor)};
  // Le plancher, sans jamais dépasser la place : sur un écran plus petit que
  // lui, c'est la place qui l'emporte.
  fitted.width = std::min(std::max(fitted.width, kSmallestWindow.width), room.width);
  fitted.height = std::min(std::max(fitted.height, kSmallestWindow.height), room.height);
  return fitted;
}

}  // namespace ro::render
