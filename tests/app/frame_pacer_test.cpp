// LE PLAFOND D'IMAGES : des échéances cumulées, sur une horloge injectée.

#include "options/frame_pacer.h"

#include <cstdint>
#include <optional>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using moonclient::FramePacer;
using moonclient::kNanosecondsPerSecond;

namespace {

constexpr int kHighRefresh = 144;
constexpr std::uint64_t kDrawingNs = 2'000'000;
constexpr std::uint64_t kStartNs = 5'000'000'000;
constexpr int kSecondsSimulated = 10;
constexpr double kNanosecondsPerMillisecond = 1'000'000.0;
constexpr double kExpectedFrameMs = 1000.0 / kHighRefresh;

// Le temps qu'il est quand l'image part : l'échéance si elle est à venir,
// maintenant sinon.
std::uint64_t StartOfNextFrame(FramePacer& pacer, std::uint64_t now_ns, int limit) {
  const std::optional<std::uint64_t> next_ns = pacer.NextFrameStartNs(now_ns, limit);
  return next_ns.has_value() ? *next_ns : now_ns;
}

}  // namespace

TEST_CASE("Plafond d'images : 144 tient 6,944 ms par image, sans dérive",
          "[app][display][frame_limit]") {
  FramePacer pacer;
  std::uint64_t now_ns = kStartNs;
  // Le premier appel pose l'ancre, APRÈS le premier dessin.
  const std::uint64_t anchor_ns = now_ns + kDrawingNs;
  const int frames = kHighRefresh * kSecondsSimulated;
  for (int frame = 0; frame < frames; ++frame) {
    now_ns = StartOfNextFrame(pacer, now_ns + kDrawingNs, kHighRefresh);
  }
  const double mean_ms =
      static_cast<double>(now_ns - anchor_ns) / kNanosecondsPerMillisecond / frames;
  CHECK(mean_ms == Catch::Approx(kExpectedFrameMs).epsilon(1e-9));
  // ⚠ AUCUNE ERREUR NE S'ACCUMULE : dix secondes pleines, à la nanoseconde.
  CHECK(now_ns - anchor_ns == kSecondsSimulated * kNanosecondsPerSecond);
}

TEST_CASE("Plafond d'images : un retard de plus d'une image recale l'échéance",
          "[app][display][frame_limit]") {
  FramePacer pacer;
  std::uint64_t now_ns = StartOfNextFrame(pacer, kStartNs, kHighRefresh);
  now_ns = StartOfNextFrame(pacer, now_ns + kDrawingNs, kHighRefresh);

  // Un accroc de cinquante millisecondes : l'image suivante part aussitôt…
  constexpr std::uint64_t kHitchNs = 50'000'000;
  const std::uint64_t late_ns = now_ns + kHitchNs;
  CHECK_FALSE(pacer.NextFrameStartNs(late_ns, kHighRefresh).has_value());
  // … et la suivante une image plus tard, non en rafale pour rattraper.
  const std::optional<std::uint64_t> next_ns =
      pacer.NextFrameStartNs(late_ns + kDrawingNs, kHighRefresh);
  REQUIRE(next_ns.has_value());
  CHECK(*next_ns == late_ns + kNanosecondsPerSecond / kHighRefresh);
}

TEST_CASE("Plafond d'images : un retard de moins d'une image garde la cadence",
          "[app][display][frame_limit]") {
  FramePacer pacer;
  const std::uint64_t anchor_ns = kStartNs;
  static_cast<void>(pacer.NextFrameStartNs(anchor_ns, kHighRefresh));
  // L'échéance de la deuxième image est manquée d'une milliseconde : rien à
  // attendre, et la troisième garde sa place sur la grille.
  const std::uint64_t second_ns = anchor_ns + 2 * kNanosecondsPerSecond / kHighRefresh;
  constexpr std::uint64_t kSlipNs = 1'000'000;
  CHECK_FALSE(pacer.NextFrameStartNs(second_ns + kSlipNs, kHighRefresh).has_value());
  const std::optional<std::uint64_t> next_ns =
      pacer.NextFrameStartNs(second_ns + kSlipNs + kDrawingNs, kHighRefresh);
  REQUIRE(next_ns.has_value());
  CHECK(*next_ns == anchor_ns + 3 * kNanosecondsPerSecond / kHighRefresh);
}

TEST_CASE("Plafond d'images : sans plafond, aucune attente", "[app][display][frame_limit]") {
  FramePacer pacer;
  CHECK_FALSE(pacer.NextFrameStartNs(kStartNs, 0).has_value());
  CHECK_FALSE(pacer.NextFrameStartNs(kStartNs + kDrawingNs, 0).has_value());
}
