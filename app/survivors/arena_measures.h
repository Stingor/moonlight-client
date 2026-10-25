#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace moonclient {

// LES MESURES DE L'ARÈNE (U24) : ce que coûte chaque part d'une image, en
// millisecondes. La pause les montre sur une fenêtre glissante des dernières
// images ; la fin de partie écrit au journal du client celles de la partie
// entière. Rien de ce que la simulation relit n'en dépend : les flottants y
// sont permis.
enum class ArenaMeasure : std::uint8_t {
  kSimulation,
  kComposition,
  kScene,
  kEffects,
  kHud,
};
inline constexpr std::size_t kArenaMeasureCount = 5;

// Les images de la fenêtre glissante : cinq secondes à 60 images par seconde.
inline constexpr std::size_t kMeasureWindowFrames = 300;
// La partie entière se range en kWholeBins classes de kWholeBinMs, jusqu'à
// 50 ms ; au-delà, une classe de débord. Son maximum reste exact.
inline constexpr double kWholeBinMs = 0.05;
inline constexpr std::size_t kWholeBins = 1000;

struct MeasureSummary {
  double p50_ms = 0.0;
  double p95_ms = 0.0;
  double max_ms = 0.0;
  std::size_t samples = 0;
};

// Le centile au rang le plus proche : la valeur de rang ⌈p × n / 100⌉ dans
// l'ordre croissant. `samples` est réordonné ; 0 pour une liste vide.
[[nodiscard]] double NearestRankPercentile(std::vector<double>& samples, double percent);

class ArenaMeasures {
 public:
  ArenaMeasures();
  void Record(ArenaMeasure measure, double elapsed_ms);
  // Les dernières kMeasureWindowFrames mesures.
  [[nodiscard]] MeasureSummary Window(ArenaMeasure measure) const;
  // Toutes les mesures depuis Reset ; p50 et p95 au milieu de leur classe.
  [[nodiscard]] MeasureSummary Whole(ArenaMeasure measure) const;
  void Reset();

 private:
  struct Track {
    std::array<double, kMeasureWindowFrames> window = {};
    std::size_t next = 0;
    std::size_t filled = 0;
    std::vector<std::uint32_t> bins;  // la dernière est le débord
    std::size_t samples = 0;
    double max_ms = 0.0;
  };
  std::array<Track, kArenaMeasureCount> tracks_;
};

}  // namespace moonclient
