#include "ro/sim/npc_illustration.h"

namespace ro::sim {
namespace {

// Le type qui efface toutes les illustrations.
constexpr std::uint8_t kClearType = 255;

}  // namespace

std::optional<IllustrationPlace> IllustrationPlaceOf(std::uint8_t type) noexcept {
  switch (type) {
    case static_cast<std::uint8_t>(IllustrationPlace::kBottomLeft):
      return IllustrationPlace::kBottomLeft;
    case static_cast<std::uint8_t>(IllustrationPlace::kBottomMiddle):
      return IllustrationPlace::kBottomMiddle;
    case static_cast<std::uint8_t>(IllustrationPlace::kBottomRight):
      return IllustrationPlace::kBottomRight;
    case static_cast<std::uint8_t>(IllustrationPlace::kCenteredWindow):
      return IllustrationPlace::kCenteredWindow;
    case static_cast<std::uint8_t>(IllustrationPlace::kCenteredFloating):
      return IllustrationPlace::kCenteredFloating;
    case kClearType:
      return std::nullopt;
    default:
      return IllustrationPlace::kBottomMiddle;
  }
}

}  // namespace ro::sim
