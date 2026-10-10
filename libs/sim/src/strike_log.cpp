#include "ro/sim/strike_log.h"

#include <algorithm>
#include <cstddef>

namespace ro::sim {
namespace {

// Ce que le journal garde au moins : des centaines de coups, quand une image
// en apporte quelques-uns.
constexpr std::size_t kKeptStrikes = 256;

}  // namespace

void StrikeLog::Record(const Strike& strike) {
  // ⚠ LA PURGE SE FAIT PAR MOITIÉ, pas coup par coup : effacer en tête d'un
  // vecteur à chaque coup recopierait tout le reste à chaque fois.
  if (strikes_.size() >= kKeptStrikes * 2) {
    strikes_.erase(strikes_.begin(),
                   strikes_.begin() + static_cast<std::ptrdiff_t>(strikes_.size() - kKeptStrikes));
  }
  strikes_.push_back(RecordedStrike{.serial = next_serial_, .strike = strike});
  ++next_serial_;
}

std::span<const RecordedStrike> StrikeLog::Since(std::uint64_t serial) const {
  const auto first = std::ranges::upper_bound(
      strikes_, serial, {}, [](const RecordedStrike& kept) { return kept.serial; });
  return {first, strikes_.end()};
}

std::span<const RecordedStrike> StrikeCursor::Take(const StrikeLog& log) {
  if (seen_serial_ > log.last_serial()) {
    seen_serial_ = 0;
  }
  const std::span<const RecordedStrike> fresh = log.Since(seen_serial_);
  seen_serial_ = log.last_serial();
  return fresh;
}

}  // namespace ro::sim
