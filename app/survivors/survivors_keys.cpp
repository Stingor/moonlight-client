#include "survivors/survivors_keys.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace moonclient {
namespace {

enum class Toward : std::uint8_t { kNorth, kWest, kSouth, kEast };

struct MoveKey {
  std::uint32_t scancode = 0;
  Toward toward = Toward::kNorth;
};

// Un bit par entrée, à son index : deux touches d'une même direction (W et la
// flèche du haut) se tiennent et se relâchent chacune pour soi.
constexpr std::array<MoveKey, 8> kMoveKeys{{
    {.scancode = kScancodeW, .toward = Toward::kNorth},
    {.scancode = kScancodeA, .toward = Toward::kWest},
    {.scancode = kScancodeS, .toward = Toward::kSouth},
    {.scancode = kScancodeD, .toward = Toward::kEast},
    {.scancode = kScancodeUp, .toward = Toward::kNorth},
    {.scancode = kScancodeLeft, .toward = Toward::kWest},
    {.scancode = kScancodeDown, .toward = Toward::kSouth},
    {.scancode = kScancodeRight, .toward = Toward::kEast},
}};

[[nodiscard]] std::optional<std::size_t> IndexOf(std::uint32_t scancode) noexcept {
  for (std::size_t index = 0; index < kMoveKeys.size(); ++index) {
    if (kMoveKeys[index].scancode == scancode) return index;
  }
  return std::nullopt;
}

[[nodiscard]] constexpr std::uint8_t BitOf(std::size_t index) noexcept {
  return static_cast<std::uint8_t>(1U << index);
}

}  // namespace

bool IsMoveKey(std::uint32_t scancode) noexcept { return IndexOf(scancode).has_value(); }

bool MoveKeys::Take(std::uint32_t scancode, bool down) noexcept {
  const std::optional<std::size_t> index = IndexOf(scancode);
  if (!index) return false;
  if (down) {
    held_ = static_cast<std::uint8_t>(held_ | BitOf(*index));
  } else {
    held_ = static_cast<std::uint8_t>(held_ & ~BitOf(*index));
  }
  return true;
}

std::uint8_t MoveKeys::Heading() const noexcept {
  std::array<bool, 4> toward = {};
  for (std::size_t index = 0; index < kMoveKeys.size(); ++index) {
    if ((held_ & BitOf(index)) != 0) {
      toward[static_cast<std::size_t>(kMoveKeys[index].toward)] = true;
    }
  }
  const auto axis = [&toward](Toward plus, Toward minus) {
    return (toward[static_cast<std::size_t>(plus)] ? 1 : 0) -
           (toward[static_cast<std::size_t>(minus)] ? 1 : 0);
  };
  // y vers le nord, comme la simulation ; un vecteur nul rend kNoHeading.
  const ro::sim::FieldPoint delta{axis(Toward::kEast, Toward::kWest) * ro::sim::kSubunitsPerCell,
                                  axis(Toward::kNorth, Toward::kSouth) * ro::sim::kSubunitsPerCell};
  return ro::sim::HeadingOf(delta, ro::sim::kNoHeading);
}

std::uint8_t HeadingOf(const MoveKeys& keys, const MouseAim& mouse) noexcept {
  if (mouse.held) return ro::sim::HeadingOf(mouse.target - mouse.hero, ro::sim::kNoHeading);
  return keys.Heading();
}

MoveKeyLabels LabelsOf(const ScancodeNamer& name_of) {
  return MoveKeyLabels{.north = name_of(kScancodeW),
                       .west = name_of(kScancodeA),
                       .south = name_of(kScancodeS),
                       .east = name_of(kScancodeD)};
}

}  // namespace moonclient
