#include "ro/render/window_fit.h"

#include <algorithm>
#include <cstdint>

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
  // En entiers : le côté qui borne vaut alors exactement la place, sans perdre un
  // pixel à l'arrondi d'un facteur.
  const std::int64_t width_at_room_height =
      static_cast<std::int64_t>(wanted.width) * room.height / wanted.height;
  Resolution fitted = room;
  if (width_at_room_height <= room.width) {
    fitted.width = static_cast<int>(width_at_room_height);
  } else {
    fitted.height =
        static_cast<int>(static_cast<std::int64_t>(wanted.height) * room.width / wanted.width);
  }
  // Le plancher, sans jamais dépasser la place : sur un écran plus petit que
  // lui, c'est la place qui l'emporte.
  fitted.width = std::min(std::max(fitted.width, kSmallestWindow.width), room.width);
  fitted.height = std::min(std::max(fitted.height, kSmallestWindow.height), room.height);
  return fitted;
}

}  // namespace ro::render
