#pragma once

#include <algorithm>
#include <cstddef>

namespace moonclient {

// LE RELEVÉ DU DÉCOR REPOSÉ par l'éditeur de carte : combien de fois, combien
// de temps en tout et au pire, combien de lots recuits. Un glissé repose le
// décor à chaque image ; le journal en reçoit un bilan par seconde, pas une
// ligne par image.
struct ReplaceTally {
  // La durée d'un bilan, en millisecondes.
  static constexpr double kPeriodMs = 1000.0;

  std::size_t count = 0;
  double total_ms = 0.0;
  double worst_ms = 0.0;
  std::size_t baked_batches = 0;
  double first_ms = 0.0;

  // Retient un repos qui a duré `elapsed_ms` et recuit `baked` lots, à
  // l'instant `now_ms`. Vrai quand le bilan couvre sa période : l'appelant le
  // journalise, puis le remet à zéro.
  bool Add(double elapsed_ms, std::size_t baked, double now_ms) {
    if (count == 0) {
      first_ms = now_ms;
    }
    ++count;
    total_ms += elapsed_ms;
    worst_ms = std::max(worst_ms, elapsed_ms);
    baked_batches += baked;
    return now_ms - first_ms >= kPeriodMs;
  }
  [[nodiscard]] double average_ms() const {
    return count == 0 ? 0.0 : total_ms / static_cast<double>(count);
  }
};

}  // namespace moonclient
