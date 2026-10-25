#include "survivors/arena_measures.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace moonclient {
namespace {

constexpr double kMedianPct = 50.0;
constexpr double kHighPct = 95.0;
constexpr double kWholePct = 100.0;
constexpr double kHalfBin = 0.5;

// Le rang (à partir de 1) du centile `percent` parmi `count` valeurs.
[[nodiscard]] std::size_t RankOf(std::size_t count, double percent) {
  const auto rank =
      static_cast<std::size_t>(std::ceil(percent * static_cast<double>(count) / kWholePct));
  return std::clamp<std::size_t>(rank, 1, count);
}

// Le centile de la partie entière : le milieu de la classe qui porte son
// rang, jamais au-delà du maximum ; le maximum pour le débord.
[[nodiscard]] double WholePercentile(const std::vector<std::uint32_t>& bins, std::size_t samples,
                                     double max_ms, double percent) {
  const std::size_t rank = RankOf(samples, percent);
  std::size_t seen = 0;
  for (std::size_t bin = 0; bin + 1 < bins.size(); ++bin) {
    seen += bins[bin];
    if (seen >= rank) {
      return std::min((static_cast<double>(bin) + kHalfBin) * kWholeBinMs, max_ms);
    }
  }
  return max_ms;
}

}  // namespace

double NearestRankPercentile(std::vector<double>& samples, double percent) {
  if (samples.empty()) return 0.0;
  const std::size_t rank = RankOf(samples.size(), percent);
  const auto nth = samples.begin() + static_cast<std::ptrdiff_t>(rank - 1);
  std::nth_element(samples.begin(), nth, samples.end());
  return *nth;
}

ArenaMeasures::ArenaMeasures() { Reset(); }

void ArenaMeasures::Record(ArenaMeasure measure, double elapsed_ms) {
  Track& track = tracks_[static_cast<std::size_t>(measure)];
  // Une durée négative ou NaN compte pour zéro.
  const double kept_ms = elapsed_ms > 0.0 ? elapsed_ms : 0.0;
  track.window[track.next] = kept_ms;
  track.next = (track.next + 1) % kMeasureWindowFrames;
  track.filled = std::min(track.filled + 1, kMeasureWindowFrames);
  // Bornée avant la conversion : une durée énorme tombe dans le débord.
  const double bin = std::min(kept_ms / kWholeBinMs, static_cast<double>(kWholeBins));
  ++track.bins[static_cast<std::size_t>(bin)];
  ++track.samples;
  track.max_ms = std::max(track.max_ms, kept_ms);
}

MeasureSummary ArenaMeasures::Window(ArenaMeasure measure) const {
  const Track& track = tracks_[static_cast<std::size_t>(measure)];
  std::vector<double> samples(track.window.begin(),
                              track.window.begin() + static_cast<std::ptrdiff_t>(track.filled));
  MeasureSummary summary{.samples = samples.size()};
  if (samples.empty()) return summary;
  summary.max_ms = *std::max_element(samples.begin(), samples.end());
  summary.p50_ms = NearestRankPercentile(samples, kMedianPct);
  summary.p95_ms = NearestRankPercentile(samples, kHighPct);
  return summary;
}

MeasureSummary ArenaMeasures::Whole(ArenaMeasure measure) const {
  const Track& track = tracks_[static_cast<std::size_t>(measure)];
  MeasureSummary summary{.max_ms = track.max_ms, .samples = track.samples};
  if (track.samples == 0) return summary;
  summary.p50_ms = WholePercentile(track.bins, track.samples, track.max_ms, kMedianPct);
  summary.p95_ms = WholePercentile(track.bins, track.samples, track.max_ms, kHighPct);
  return summary;
}

void ArenaMeasures::Reset() {
  for (Track& track : tracks_) {
    track = Track();
    // Une classe par kWholeBinMs, plus le débord.
    track.bins.assign(kWholeBins + 1, 0);
  }
}

}  // namespace moonclient
