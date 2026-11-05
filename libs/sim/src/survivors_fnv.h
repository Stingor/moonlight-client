#pragma once

#include <cstddef>
#include <cstdint>

#include "ro/sim/survivors_geometry.h"

namespace ro::sim {

// FNV-1a, octet par octet : le même résultat sur toute machine. Sert à constater
// entre clients ce qu'ils ne peuvent pas se montrer en entier.
class FnvDigest {
 public:
  void Add(std::uint64_t value) noexcept {
    for (std::size_t index = 0; index < kValueBytes; ++index) {
      hash_ ^= (value >> (index * kByteBits)) & 0xFFU;
      hash_ *= kPrime;
    }
  }
  void AddSigned(std::int64_t value) noexcept { Add(static_cast<std::uint64_t>(value)); }
  void AddPoint(FieldPoint point) noexcept {
    AddSigned(point.x);
    AddSigned(point.y);
  }
  [[nodiscard]] std::uint64_t value() const noexcept { return hash_; }

 private:
  static constexpr std::size_t kValueBytes = 8;
  static constexpr std::size_t kByteBits = 8;
  // Le décalage de FNV-1a sur 64 bits, et son multiplicateur.
  static constexpr std::uint64_t kOffset = 14695981039346656037ULL;
  static constexpr std::uint64_t kPrime = 1099511628211ULL;

  std::uint64_t hash_ = kOffset;
};

}  // namespace ro::sim
