// LA PEAU D'UN CORPS, cherchée sur la palette de son sprite.
//
// ⚠ UN ENSEMBLE FIXE SE TROMPERAIT SANS RIEN DIRE : la peau du corps se
// poserait sur un vêtement, et les bras garderaient la couleur du dessin.

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/palette.h"
#include "ro/render/palette_ramps.h"

using ro::formats::kPaletteEntryCount;
using ro::formats::Palette;
using ro::render::DetectBodySkin;
using ro::render::kBodySkinTolerance;
using ro::render::kSkinLength;
using ro::render::SkinRace;
using ro::render::SkinSet;

namespace {

using Usage = std::array<int, kPaletteEntryCount>;

// Le dégradé chair des corps humains, du clair au foncé.
constexpr std::array<std::array<std::uint8_t, 3>, kSkinLength> kFlesh = {{
    {255, 225, 207},
    {255, 198, 178},
    {246, 174, 159},
    {220, 144, 132},
    {189, 115, 107},
    {158, 86, 82},
    {130, 63, 59},
}};
constexpr int kPixels = 300;

// Pose le dégradé chair à partir de `start`, chaque index peint `pixels` fois.
void PaintFlesh(Palette& palette, Usage& usage, std::size_t start, int pixels = kPixels) {
  for (std::size_t offset = 0; offset < kSkinLength; ++offset) {
    const auto& rgb = kFlesh[offset];
    palette.entries[start + offset] = {rgb[0], rgb[1], rgb[2], 255};
    usage[start + offset] = pixels;
  }
}

[[nodiscard]] std::vector<int> IndicesOf(const SkinSet& skin) {
  return {skin.indices().begin(), skin.indices().end()};
}

[[nodiscard]] std::vector<int> RunFrom(int start) {
  std::vector<int> run;
  for (std::size_t offset = 0; offset < kSkinLength; ++offset) {
    run.push_back(start + static_cast<int>(offset));
  }
  return run;
}

}  // namespace

TEST_CASE("Peau du corps : trouvée là où le sprite la range", "[render][palette][skin]") {
  for (const int start : {129, 33, 49, 17}) {
    Palette palette;
    Usage usage{};
    PaintFlesh(palette, usage, static_cast<std::size_t>(start));
    CHECK(IndicesOf(DetectBodySkin(palette, usage, SkinRace::kHuman)) == RunFrom(start));
  }
}

TEST_CASE("Peau du corps : un écart de quelques unités reste de la peau",
          "[render][palette][skin]") {
  Palette palette;
  Usage usage{};
  PaintFlesh(palette, usage, 33);
  // Un graphiste a retouché un canal d'une unité.
  palette.entries[37][2] = static_cast<std::uint8_t>(palette.entries[37][2] + 1);
  CHECK(IndicesOf(DetectBodySkin(palette, usage, SkinRace::kHuman)) == RunFrom(33));
}

TEST_CASE("Peau du corps : un corps sans dégradé chair n'a pas de peau",
          "[render][palette][skin]") {
  Palette palette;
  Usage usage{};
  // Une armure grise peinte partout.
  for (std::size_t index = 1; index < kPaletteEntryCount; ++index) {
    const auto level = static_cast<std::uint8_t>(index);
    palette.entries[index] = {level, level, level, 255};
    usage[index] = kPixels;
  }
  CHECK(DetectBodySkin(palette, usage, SkinRace::kHuman).empty());

  // Juste au-delà de la tolérance : plus de peau non plus.
  Palette near;
  Usage near_usage{};
  PaintFlesh(near, near_usage, 49);
  near.entries[49][1] = static_cast<std::uint8_t>(near.entries[49][1] - kBodySkinTolerance - 1);
  CHECK(DetectBodySkin(near, near_usage, SkinRace::kHuman).empty());
}

TEST_CASE("Peau du corps : une copie jamais peinte ne compte pas, la plus peinte l'emporte",
          "[render][palette][skin]") {
  Palette palette;
  Usage usage{};
  // La copie haute n'a aucun pixel : elle n'est pas candidate.
  PaintFlesh(palette, usage, 33);
  PaintFlesh(palette, usage, 129, 0);
  CHECK(IndicesOf(DetectBodySkin(palette, usage, SkinRace::kHuman)) == RunFrom(33));

  // Deux copies peintes : la plus peinte, puis la plus basse à égalité.
  PaintFlesh(palette, usage, 129, kPixels * 2);
  CHECK(IndicesOf(DetectBodySkin(palette, usage, SkinRace::kHuman)) == RunFrom(129));
  PaintFlesh(palette, usage, 129, kPixels);
  CHECK(IndicesOf(DetectBodySkin(palette, usage, SkinRace::kHuman)) == RunFrom(33));
}

TEST_CASE("Peau du corps : un Doram n'en a pas, une tête garde l'ensemble fixe",
          "[render][palette][skin]") {
  Palette palette;
  Usage usage{};
  PaintFlesh(palette, usage, 33);
  CHECK(DetectBodySkin(palette, usage, SkinRace::kDoram).empty());
  CHECK(IndicesOf(ro::render::HeadSkin(SkinRace::kHuman)) == RunFrom(129));
  CHECK(ro::render::HeadSkin(SkinRace::kDoram).empty());
}
