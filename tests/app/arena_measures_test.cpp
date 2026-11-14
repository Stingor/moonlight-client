// LES MESURES DE L'ARÈNE : centiles au rang le plus proche sur la fenêtre
// glissante, et sur la partie entière par classes.

#include "survivors/arena_measures.h"

#include <cstddef>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using Catch::Approx;
using moonclient::ArenaMeasure;
using moonclient::ArenaMeasures;
using moonclient::kMeasureWindowFrames;
using moonclient::kWholeBinMs;
using moonclient::NearestRankPercentile;

namespace {

constexpr std::size_t kHundred = 100;
constexpr double kMedianPct = 50.0;
constexpr double kHighPct = 95.0;
constexpr double kSlowFrameMs = 80.0;

}  // namespace

TEST_CASE("NearestRankPercentile : le rang ⌈p × n / 100⌉", "[app][survivors]") {
  std::vector<double> empty;
  CHECK(NearestRankPercentile(empty, kMedianPct) == 0.0);
  // 1 à 100 dans le désordre : p50 vaut 50, p95 vaut 95, p100 vaut 100.
  std::vector<double> samples;
  for (std::size_t value = kHundred; value >= 1; --value) {
    samples.push_back(static_cast<double>(value));
  }
  CHECK(NearestRankPercentile(samples, kMedianPct) == 50.0);
  CHECK(NearestRankPercentile(samples, kHighPct) == 95.0);
  CHECK(NearestRankPercentile(samples, 100.0) == 100.0);
  std::vector<double> single{3.0};
  CHECK(NearestRankPercentile(single, kHighPct) == 3.0);
}

TEST_CASE("ArenaMeasures : la fenêtre ne garde que les dernières images", "[app][survivors]") {
  ArenaMeasures measures;
  CHECK(measures.Window(ArenaMeasure::kScene).samples == 0);
  for (std::size_t frame = 0; frame < kMeasureWindowFrames; ++frame) {
    measures.Record(ArenaMeasure::kScene, kSlowFrameMs);
  }
  for (std::size_t frame = 0; frame < kMeasureWindowFrames; ++frame) {
    measures.Record(ArenaMeasure::kScene, 1.0);
  }
  const moonclient::MeasureSummary window = measures.Window(ArenaMeasure::kScene);
  CHECK(window.samples == kMeasureWindowFrames);
  CHECK(window.max_ms == 1.0);
  CHECK(window.p95_ms == 1.0);
  // La partie entière garde les images lentes, au débord, et son maximum.
  const moonclient::MeasureSummary whole = measures.Whole(ArenaMeasure::kScene);
  CHECK(whole.samples == 2 * kMeasureWindowFrames);
  CHECK(whole.max_ms == kSlowFrameMs);
  CHECK(whole.p95_ms == kSlowFrameMs);
  CHECK(whole.p50_ms == Approx(1.0).margin(kWholeBinMs));
  // Les autres mesures n'ont rien vu.
  CHECK(measures.Whole(ArenaMeasure::kHud).samples == 0);
  measures.Reset();
  CHECK(measures.Whole(ArenaMeasure::kScene).samples == 0);
}

TEST_CASE("ArenaMeasures : les centiles de la partie entière, à la classe près",
          "[app][survivors]") {
  ArenaMeasures measures;
  for (std::size_t value = 1; value <= kHundred; ++value) {
    measures.Record(ArenaMeasure::kSimulation, static_cast<double>(value) / 10.0);
  }
  const moonclient::MeasureSummary whole = measures.Whole(ArenaMeasure::kSimulation);
  CHECK(whole.p50_ms == Approx(5.0).margin(kWholeBinMs));
  CHECK(whole.p95_ms == Approx(9.5).margin(kWholeBinMs));
  CHECK(whole.max_ms == Approx(10.0));
  const moonclient::MeasureSummary window = measures.Window(ArenaMeasure::kSimulation);
  CHECK(window.p50_ms == Approx(5.0));
  CHECK(window.p95_ms == Approx(9.5));
}
