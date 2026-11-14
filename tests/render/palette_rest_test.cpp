// LE PROFIL MIXTE : le reste réglé par classe de teinte, puis les retouches.
// Voir `ro/render/palette_rest.h`.

#include "ro/render/palette_rest.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/palette.h"
#include "ro/render/hue_classes.h"
#include "ro/render/palette_ramps.h"

using ro::formats::kPaletteEntryCount;
using ro::formats::Palette;
using ro::render::ApplyRest;
using ro::render::ApplyRetouches;
using ro::render::Cover;
using ro::render::HueClassAdjusts;
using ro::render::HueClassOf;
using ro::render::MeasureRest;
using ro::render::PaletteCover;
using ro::render::PaletteRamp;
using ro::render::PaletteRetouch;
using ro::render::RampAdjust;
using ro::render::RestMeasure;

namespace {

constexpr int kChannelTop = 255;
constexpr int kTurn = 360;
constexpr int kHueSteps = 255;
constexpr int kOpaque = 255;
// La pièce de la palette d'épreuve : seize index au début.
constexpr std::uint8_t kPieceStart = 1;
constexpr std::uint8_t kPieceLength = 16;
constexpr int kPixelsPerIndex = 10;

// UNE PALETTE FRANCHEMENT COLORÉE : chaque index d'une teinte à lui, à pleine
// saturation, le tour entier parcouru. Le zéro est le transparent, en magenta.
[[nodiscard]] Palette Rainbow() {
  Palette palette;
  palette.entries[0] = {kChannelTop, 0, kChannelTop, 0};
  for (std::size_t index = 1; index < kPaletteEntryCount; ++index) {
    const int hue = static_cast<int>(index) * kTurn / kHueSteps % kTurn;
    std::array<std::uint8_t, 3> rgb{};
    const std::array<std::uint8_t, 3> red = {kChannelTop, 0, 0};
    ro::render::AdjustColor(
        red.data(),
        RampAdjust{.hue = static_cast<std::int16_t>(hue), .saturation = 100, .absolute = true},
        rgb.data());
    palette.entries[index] = {rgb[0], rgb[1], rgb[2], kOpaque};
  }
  return palette;
}

[[nodiscard]] std::array<int, kPaletteEntryCount> EveryIndexUsed() {
  std::array<int, kPaletteEntryCount> usage{};
  usage.fill(kPixelsPerIndex);
  usage[0] = 0;
  return usage;
}

[[nodiscard]] HueClassAdjusts EveryClassTurned() {
  HueClassAdjusts rest{};
  rest.fill(RampAdjust{.hue = 90});
  return rest;
}

[[nodiscard]] PaletteCover PieceCover() {
  PaletteCover cover{};
  const std::array<PaletteRamp, 1> piece{
      PaletteRamp{.start = kPieceStart, .length = kPieceLength, .ref = kPieceStart}};
  Cover(cover, piece);
  return cover;
}

}  // namespace

TEST_CASE("Reste : chaque index franchement coloré hors des pièces se règle",
          "[render][palette][rest]") {
  const Palette base = Rainbow();
  const PaletteCover cover = PieceCover();
  Palette painted = base;
  ApplyRest(painted, base, cover, EveryClassTurned());

  std::size_t changed = 0;
  std::size_t outside = 0;
  for (std::size_t index = 1; index < kPaletteEntryCount; ++index) {
    if (cover[index]) {
      // 🔴 UNE PIÈCE NE SE FAIT PAS REPEINDRE PAR LE RESTE.
      CHECK(painted.entries[index] == base.entries[index]);
      continue;
    }
    // Le magenta pur du tour ne se règle pas : voir plus bas.
    const auto& entry = base.entries[index];
    if (entry[0] == kChannelTop && entry[1] == 0 && entry[2] == kChannelTop) {
      CHECK(painted.entries[index] == entry);
      continue;
    }
    ++outside;
    if (painted.entries[index] != base.entries[index]) {
      ++changed;
    }
    // L'alpha ne bouge jamais.
    CHECK(painted.entries[index][3] == base.entries[index][3]);
  }
  // 🔴 LA COUVERTURE EST TOTALE : aucun index coloré n'échappe au reste.
  CHECK(changed == outside);
  // Le transparent reste tel quel.
  CHECK(painted.entries[0] == base.entries[0]);

  const RestMeasure measure = MeasureRest(base, EveryIndexUsed(), cover);
  CHECK(measure.total == static_cast<int>(kPaletteEntryCount - 1) * kPixelsPerIndex);
  CHECK(measure.covered == kPieceLength * kPixelsPerIndex);
  CHECK(measure.covered + measure.reachable == measure.total);
  CHECK(measure.ReachablePercent() == 100);
}

TEST_CASE("Reste : chaque couleur reçoit le réglage de SA classe", "[render][palette][rest]") {
  const Palette base = Rainbow();
  const PaletteCover cover{};
  const std::size_t chosen =
      HueClassOf(base.entries[40][0], base.entries[40][1], base.entries[40][2]);
  HueClassAdjusts rest{};
  rest[chosen] = RampAdjust{.hue = 180};
  Palette painted = base;
  ApplyRest(painted, base, cover, rest);
  for (std::size_t index = 1; index < kPaletteEntryCount; ++index) {
    const auto& entry = base.entries[index];
    const bool in_class = HueClassOf(entry[0], entry[1], entry[2]) == chosen;
    CHECK((painted.entries[index] != base.entries[index]) == in_class);
  }
}

TEST_CASE("Reste : la mesure compte les classes présentes et met les noirs à part",
          "[render][palette][rest]") {
  Palette base;
  base.entries[1] = {200, 20, 20, kOpaque};
  base.entries[2] = {100, 100, 100, kOpaque};
  base.entries[3] = {2, 2, 2, kOpaque};
  std::array<int, kPaletteEntryCount> usage{};
  usage[1] = 30;
  usage[2] = 10;
  usage[3] = 60;
  const RestMeasure measure = MeasureRest(base, usage, PaletteCover{});
  CHECK(measure.pixels[0] == 30);
  CHECK(measure.pixels[ro::render::kGreyHueClass] == 10 + 60);
  // Les noirs de contour ne se règlent pas : 40 sur 100.
  CHECK(measure.reachable == 40);
  CHECK(measure.ReachablePercent() == 40);
  CHECK(measure.mean[0] == std::array<std::uint8_t, 3>{200, 20, 20});
}

TEST_CASE("Retouches : une couleur exacte, jamais sur le zéro ni en magenta",
          "[render][palette][rest]") {
  Palette painted = Rainbow();
  const Palette before = painted;
  const std::vector<PaletteRetouch> retouches = {
      PaletteRetouch{.index = 0, .rgb = {1, 2, 3}},
      PaletteRetouch{.index = 7, .rgb = {10, 20, 30}},
      PaletteRetouch{.index = 8, .rgb = {kChannelTop, 0, kChannelTop}},
  };
  ApplyRetouches(painted, retouches);
  CHECK(painted.entries[0] == before.entries[0]);
  CHECK(painted.entries[7] == std::array<std::uint8_t, 4>{10, 20, 30, before.entries[7][3]});
  CHECK(painted.entries[8] == before.entries[8]);
  CHECK_FALSE(ro::render::RetouchAllowed(retouches[0]));
  CHECK(ro::render::RetouchAllowed(retouches[1]));
  CHECK_FALSE(ro::render::RetouchAllowed(retouches[2]));
}

TEST_CASE("Reste : une couleur d'origine en magenta pur ne se règle pas",
          "[render][palette][rest]") {
  Palette base = Rainbow();
  base.entries[5] = {kChannelTop, 0, kChannelTop, kOpaque};
  Palette painted = base;
  ApplyRest(painted, base, PaletteCover{}, EveryClassTurned());
  CHECK(painted.entries[5] == base.entries[5]);
}
