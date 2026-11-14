#include "ro/render/effect_color_tally.h"

#include <array>
#include <cstdint>
#include <optional>

#include <catch2/catch_test_macros.hpp>

namespace {

using ro::render::EffectColorTally;
using ro::render::EffectDye;
using Rgb = std::array<std::uint8_t, 3>;

// Une image d'un texel par couleur donnée, opaque.
ro::formats::Image Texels(std::initializer_list<std::array<std::uint8_t, 4>> texels) {
  ro::formats::Image image;
  image.width = static_cast<std::int32_t>(texels.size());
  image.height = 1;
  for (const auto& texel : texels) {
    image.pixels.insert(image.pixels.end(), texel.begin(), texel.end());
  }
  return image;
}

constexpr std::array<std::uint8_t, 4> kWhiteTint = {255, 255, 255, 255};

}  // namespace

TEST_CASE("Le compte d'une image rouge rend du rouge à plein éclat") {
  EffectColorTally tally;
  const ro::formats::Image dim_red = Texels({{128, 0, 0, 255}});
  tally.Add(dim_red, kWhiteTint);
  CHECK(tally.Color() == std::optional(Rgb{255, 0, 0}));
}

TEST_CASE("La teinte du quad colore la texture grise") {
  EffectColorTally tally;
  const ro::formats::Image grey = Texels({{200, 200, 200, 255}});
  tally.Add(grey, std::array<std::uint8_t, 4>{0, 255, 0, 255});
  CHECK(tally.Color() == std::optional(Rgb{0, 255, 0}));
}

TEST_CASE("Un texel transparent ou noir ne compte pas") {
  EffectColorTally tally;
  const ro::formats::Image mixed = Texels({{0, 0, 255, 0}, {0, 0, 0, 255}, {255, 0, 0, 255}});
  tally.Add(mixed, kWhiteTint);
  CHECK(tally.Color() == std::optional(Rgb{255, 0, 0}));
}

TEST_CASE("Rien de compté ne rend aucune couleur") {
  EffectColorTally tally;
  CHECK_FALSE(tally.Color().has_value());
  tally.Add(Texels({{0, 0, 0, 255}}), kWhiteTint);
  CHECK_FALSE(tally.Color().has_value());
}

TEST_CASE("Seule la teinture de la sonde compte, et la remise repart de zéro") {
  EffectColorTally tally;
  EffectDye probe;
  probe.lightness_scale = 1.001F;
  tally.Reset(probe);
  CHECK(tally.Counts(probe));
  CHECK_FALSE(tally.Counts(std::nullopt));
  CHECK_FALSE(tally.Counts(EffectDye{}));
  tally.Add(Texels({{255, 0, 0, 255}}), kWhiteTint);
  tally.NoteImage();
  CHECK(tally.images_counted() == 1);
  tally.Reset(EffectDye{});
  CHECK(tally.images_counted() == 0);
  CHECK_FALSE(tally.Color().has_value());
}
