#include "ro/core/pcg32.h"

#include <cstddef>
#include <cstdint>
#include <span>

namespace ro::core {

namespace {

// La sortie de 32 bits : xorshift du haut de l'état, puis rotation par ses
// cinq bits de tête.
constexpr unsigned kXorShift = 18;
constexpr unsigned kOutputShift = 27;
constexpr unsigned kRotationShift = 59;
constexpr std::uint32_t kRotationMask = 31;
constexpr std::uint64_t kLargestDraw = 0xFFFFFFFFULL;

}  // namespace

Pcg32::Pcg32(std::uint64_t seed, std::uint64_t stream) noexcept
    : state_(0), increment_((stream << 1U) | 1U) {
  (void)Next();
  state_ += seed;
  (void)Next();
}

std::uint32_t Pcg32::Next() noexcept {
  const std::uint64_t old = state_;
  state_ = old * kPcg32Multiplier + increment_;
  const auto xorshifted = static_cast<std::uint32_t>(((old >> kXorShift) ^ old) >> kOutputShift);
  const auto rotation = static_cast<std::uint32_t>(old >> kRotationShift);
  return (xorshifted >> rotation) | (xorshifted << ((0U - rotation) & kRotationMask));
}

std::uint32_t Pcg32::Below(std::uint32_t bound) noexcept {
  if (bound == 0) return 0;
  // Les sorties sous le seuil biaiseraient le reste de la division : on les
  // rejette. Le seuil vaut 2^32 mod bound, calculé en arithmétique 32 bits.
  const std::uint32_t threshold = (0U - bound) % bound;
  for (;;) {
    const std::uint32_t draw = Next();
    if (draw >= threshold) return draw % bound;
  }
}

std::int32_t Pcg32::Between(std::int32_t least, std::int32_t most) noexcept {
  if (most < least) return least;
  const std::uint64_t span =
      static_cast<std::uint64_t>(static_cast<std::int64_t>(most) - least) + 1U;
  if (span > kLargestDraw) return static_cast<std::int32_t>(Next());
  const std::int64_t offset = Below(static_cast<std::uint32_t>(span));
  return static_cast<std::int32_t>(least + offset);
}

bool Pcg32::Chance(std::uint32_t numerator, std::uint32_t denominator) noexcept {
  if (denominator == 0) return false;
  return Below(denominator) < numerator;
}

std::size_t Pcg32::PickWeighted(std::span<const std::uint32_t> weights) noexcept {
  std::uint64_t total = 0;
  for (const std::uint32_t weight : weights) total += weight;
  if (total == 0 || total > kLargestDraw) return weights.size();
  const std::uint32_t draw = Below(static_cast<std::uint32_t>(total));
  std::uint64_t running = 0;
  for (std::size_t index = 0; index < weights.size(); ++index) {
    running += weights[index];
    if (running > draw) return index;
  }
  return weights.size();
}

}  // namespace ro::core
