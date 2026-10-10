#include "ro/sim/packed_position.h"

namespace ro::sim {
namespace {

// Les masques et décalages sont ceux de RBUFPOS et WBUFPOS, dans
// src/map/clif.cpp. Ils sont nommés plutôt que répétés parce qu'ils
// n'apparaissent pas au même endroit à la lecture et à l'écriture, et qu'une
// paire désaccordée décalerait tout un personnage sans erreur visible.
constexpr std::uint16_t kLowBitsPerCoordinate = 2;  // ce que x laisse au second octet
constexpr std::uint16_t kHighBitsOfY = 4;           // ce que y laisse au troisième
constexpr std::uint8_t kDirectionMask = 0x0F;
constexpr std::uint8_t kSixBitMask = 0x3F;
constexpr std::uint8_t kFourBitMask = 0x0F;
constexpr std::uint8_t kTwoBitMask = 0x03;

}  // namespace

CellPosition UnpackPosition(const std::array<std::uint8_t, kPackedPositionBytes>& packed) {
  CellPosition position;
  position.x = static_cast<std::uint16_t>((packed[0] << 2) | (packed[1] >> 6));
  position.y = static_cast<std::uint16_t>(((packed[1] & kSixBitMask) << 4) | (packed[2] >> 4));
  position.direction = static_cast<std::uint8_t>(packed[2] & kDirectionMask);
  return position;
}

std::array<std::uint8_t, kPackedPositionBytes> PackPosition(CellPosition position) {
  return {
      static_cast<std::uint8_t>(position.x >> kLowBitsPerCoordinate),
      static_cast<std::uint8_t>((position.x << 6) | ((position.y >> kHighBitsOfY) & kSixBitMask)),
      static_cast<std::uint8_t>((position.y << 4) | (position.direction & kDirectionMask)),
  };
}

CellMovement UnpackMovement(const std::array<std::uint8_t, kPackedMovementBytes>& packed) {
  CellMovement movement;
  movement.from.x = static_cast<std::uint16_t>((packed[0] << 2) | (packed[1] >> 6));
  movement.from.y = static_cast<std::uint16_t>(((packed[1] & kSixBitMask) << 4) | (packed[2] >> 4));
  movement.to.x = static_cast<std::uint16_t>(((packed[2] & kFourBitMask) << 6) | (packed[3] >> 2));
  movement.to.y = static_cast<std::uint16_t>(((packed[3] & kTwoBitMask) << 8) | packed[4]);
  movement.sub_x = static_cast<std::uint8_t>(packed[5] >> 4);
  movement.sub_y = static_cast<std::uint8_t>(packed[5] & kFourBitMask);
  return movement;
}

std::array<std::uint8_t, kPackedMovementBytes> PackMovement(const CellMovement& movement) {
  return {
      static_cast<std::uint8_t>(movement.from.x >> kLowBitsPerCoordinate),
      static_cast<std::uint8_t>((movement.from.x << 6) |
                                ((movement.from.y >> kHighBitsOfY) & kSixBitMask)),
      static_cast<std::uint8_t>((movement.from.y << 4) | ((movement.to.x >> 6) & kFourBitMask)),
      static_cast<std::uint8_t>((movement.to.x << 2) | ((movement.to.y >> 8) & kTwoBitMask)),
      static_cast<std::uint8_t>(movement.to.y),
      static_cast<std::uint8_t>((movement.sub_x << 4) | (movement.sub_y & kFourBitMask)),
  };
}

}  // namespace ro::sim
