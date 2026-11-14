#include "ro/render/post_fx.h"

#include <array>
#include <cstdint>
#include <cstdlib>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/formats/image.h"
#include "ro/render/post_fx_reference.h"

using Catch::Approx;
using ro::formats::Image;
using ro::render::Clamped;
using ro::render::ColorPassConstants;
using ro::render::ColorPassConstantsOf;
using ro::render::ColorPassReference;
using ro::render::FxaaReference;
using ro::render::PostFxFilter;
using ro::render::PostFxPreset;
using ro::render::PostFxSettings;
using ro::render::PresetSettings;

namespace {

constexpr std::size_t kChannels = 4;

Image Uniform(std::int32_t side, std::array<std::uint8_t, 3> rgb) {
  Image image;
  image.width = side;
  image.height = side;
  image.pixels.reserve(static_cast<std::size_t>(side * side) * kChannels);
  for (std::int32_t pixel = 0; pixel < side * side; ++pixel) {
    image.pixels.insert(image.pixels.end(), {rgb[0], rgb[1], rgb[2], 255});
  }
  return image;
}

std::array<int, 3> At(const Image& image, std::int32_t x, std::int32_t y) {
  const std::size_t at = (static_cast<std::size_t>(y) * static_cast<std::size_t>(image.width) +
                          static_cast<std::size_t>(x)) *
                         kChannels;
  return {image.pixels[at], image.pixels[at + 1], image.pixels[at + 2]};
}

// Une valeur calculée à la main, à un niveau près : l'arrondi du dernier bit
// d'un flottant peut faire tomber un x,5 d'un côté ou de l'autre.
bool Near(const std::array<int, 3>& got, const std::array<int, 3>& expected) {
  for (std::size_t channel = 0; channel < got.size(); ++channel) {
    if (std::abs(got[channel] - expected[channel]) > 1) {
      return false;
    }
  }
  return true;
}

// Le pixel du milieu d'une image unie passée par la passe de couleur.
std::array<int, 3> Graded(const PostFxSettings& settings, std::array<std::uint8_t, 3> rgb) {
  constexpr std::int32_t kSide = 9;
  const Image source = Uniform(kSide, rgb);
  const ColorPassConstants constants = ColorPassConstantsOf(settings, 0, kSide, kSide);
  return At(ColorPassReference(source, constants), kSide / 2, kSide / 2);
}

}  // namespace

TEST_CASE("Post-traitement : les défauts ne changent rien", "[render][postfx]") {
  const PostFxSettings neutral;
  CHECK_FALSE(neutral.enabled);
  CHECK(neutral.brightness == 0.0F);
  CHECK(neutral.contrast == 1.0F);
  CHECK(neutral.gamma == 1.0F);
  CHECK(neutral.saturation == 1.0F);
  CHECK(neutral.temperature == 0.0F);
  CHECK(neutral.filter == static_cast<int>(PostFxFilter::kNone));
  CHECK(neutral.vignette == 0.0F);
  CHECK(neutral.grain == 0.0F);
  CHECK(neutral.aberration == 0.0F);
  CHECK(neutral.sharpen == 0.0F);
  CHECK_FALSE(neutral.fxaa);
  CHECK(neutral.fxaa_strength == Approx(0.35F));

  // Une image quelconque ressort octet pour octet.
  CHECK(Graded(neutral, {200, 100, 50}) == std::array<int, 3>{200, 100, 50});
  CHECK(Graded(neutral, {0, 255, 7}) == std::array<int, 3>{0, 255, 7});
}

TEST_CASE("Post-traitement : les préréglages nommés", "[render][postfx]") {
  const PostFxSettings night = PresetSettings(PostFxPreset::kNight);
  CHECK(night.enabled);
  CHECK(night.brightness == Approx(-0.06F));
  CHECK(night.contrast == Approx(1.08F));
  CHECK(night.saturation == Approx(0.85F));
  CHECK(night.temperature == Approx(-0.25F));
  CHECK(night.vignette == Approx(0.35F));
  CHECK(night.gamma == 1.0F);

  const PostFxSettings cinema = PresetSettings(PostFxPreset::kCinema);
  CHECK(cinema.contrast == Approx(1.15F));
  CHECK(cinema.saturation == Approx(1.12F));
  CHECK(cinema.vignette == Approx(0.30F));
  CHECK(cinema.sharpen == Approx(0.30F));
  CHECK(cinema.aberration == Approx(0.20F));
  CHECK(cinema.fxaa);
  // La force n'est pas nommée par le préréglage : elle garde son défaut.
  CHECK(cinema.fxaa_strength == Approx(0.35F));

  const PostFxSettings retro = PresetSettings(PostFxPreset::kRetro);
  CHECK(retro.contrast == Approx(1.10F));
  CHECK(retro.saturation == Approx(1.20F));
  CHECK(retro.grain == Approx(0.35F));
  CHECK(retro.vignette == Approx(0.45F));

  const PostFxSettings black_and_white = PresetSettings(PostFxPreset::kBlackAndWhite);
  CHECK(black_and_white.filter == static_cast<int>(PostFxFilter::kGrayscale));
  CHECK(black_and_white.saturation == 1.0F);

  // Le neutre allume, et ne règle rien d'autre.
  PostFxSettings neutral = PresetSettings(PostFxPreset::kNeutral);
  CHECK(neutral.enabled);
  neutral.enabled = false;
  CHECK(Graded(neutral, {10, 20, 30}) == std::array<int, 3>{10, 20, 30});
}

TEST_CASE("Post-traitement : un fichier hors plage est ramené", "[render][postfx]") {
  PostFxSettings wild;
  wild.brightness = 3.0F;
  wild.contrast = -1.0F;
  wild.gamma = 0.0F;
  wild.saturation = 9.0F;
  wild.temperature = -4.0F;
  wild.filter = 42;
  wild.vignette = 2.0F;
  wild.grain = -1.0F;
  wild.aberration = 5.0F;
  wild.sharpen = -3.0F;
  wild.fxaa_strength = 7.0F;
  const PostFxSettings tamed = Clamped(wild);
  CHECK(tamed.brightness == 0.5F);
  CHECK(tamed.contrast == 0.5F);
  CHECK(tamed.gamma == 0.5F);
  CHECK(tamed.saturation == 2.0F);
  CHECK(tamed.temperature == -1.0F);
  CHECK(tamed.filter == static_cast<int>(PostFxFilter::kColorblind));
  CHECK(tamed.vignette == 1.0F);
  CHECK(tamed.grain == 0.0F);
  CHECK(tamed.aberration == 1.0F);
  CHECK(tamed.sharpen == 0.0F);
  CHECK(tamed.fxaa_strength == 1.0F);

  wild.filter = -2;
  CHECK(Clamped(wild).filter == static_cast<int>(PostFxFilter::kNone));
}

TEST_CASE("Post-traitement : les constantes de la passe de couleur", "[render][postfx]") {
  PostFxSettings settings;
  settings.brightness = 0.1F;
  settings.contrast = 1.2F;
  settings.gamma = 2.0F;
  settings.saturation = 0.5F;
  settings.temperature = -0.5F;
  settings.filter = static_cast<int>(PostFxFilter::kSepia);
  settings.vignette = 0.25F;
  settings.grain = 0.75F;
  settings.aberration = 0.4F;
  settings.sharpen = 0.6F;
  constexpr std::uint32_t kNowMs = 70'000;
  const ColorPassConstants constants = ColorPassConstantsOf(settings, kNowMs, 800, 600);
  CHECK(constants.grade == std::array<float, 4>{0.1F, 1.2F, 0.5F, 0.5F});
  CHECK(constants.tone == std::array<float, 4>{-0.5F, 2.0F, 0.25F, 0.75F});
  // L'horloge se replie sur seize bits : 70 000 ms donne 4,464 s.
  CHECK(constants.effects[0] == Approx(0.4F));
  CHECK(constants.effects[1] == Approx(0.6F));
  CHECK(constants.effects[2] == Approx(static_cast<float>(kNowMs & 0xFFFFU) / 1000.0F));
  CHECK(constants.texel[0] == Approx(1.0F / 800.0F));
  CHECK(constants.texel[1] == Approx(1.0F / 600.0F));

  // Un gamma nul, venu d'un fichier trafiqué, se planche à un dixième.
  settings.gamma = 0.0F;
  CHECK(ColorPassConstantsOf(settings, 0, 1, 1).grade[2] == Approx(10.0F));
}

TEST_CASE("Post-traitement : l'étalonnage, calculé à la main", "[render][postfx]") {
  PostFxSettings settings;
  settings.enabled = true;

  settings.brightness = 0.2F;
  // 100 + 0,2 × 255 = 151.
  CHECK(Near(Graded(settings, {100, 100, 100}), {151, 151, 151}));
  settings.brightness = 0.0F;

  settings.gamma = 2.0F;
  // √(64 / 255) × 255 = 127,75.
  CHECK(Near(Graded(settings, {64, 64, 64}), {128, 128, 128}));
  settings.gamma = 1.0F;

  settings.contrast = 2.0F;
  // (0,2 − 0,5) × 2 + 0,5 = −0,1, borné à zéro ; (0,8 − 0,5) × 2 + 0,5 = 1,1, borné à un.
  CHECK(Near(Graded(settings, {51, 204, 128}), {0, 255, 129}));
  settings.contrast = 1.0F;

  settings.saturation = 0.0F;
  // 0,299 × 200 + 0,587 × 100 + 0,114 × 50 = 124,2.
  CHECK(Near(Graded(settings, {200, 100, 50}), {124, 124, 124}));
  settings.saturation = 1.0F;

  settings.temperature = 1.0F;
  // Un dixième de plus au rouge, un de moins au bleu : ± 25,5.
  CHECK(Near(Graded(settings, {100, 100, 100}), {125, 100, 75}));
}

TEST_CASE("Post-traitement : les filtres", "[render][postfx]") {
  PostFxSettings settings;
  settings.enabled = true;
  settings.filter = static_cast<int>(PostFxFilter::kGrayscale);
  CHECK(Near(Graded(settings, {200, 100, 50}), {124, 124, 124}));
  settings.filter = static_cast<int>(PostFxFilter::kSepia);
  // La luminance d'un gris est lui-même : 100 × (1,07 ; 0,74 ; 0,43).
  CHECK(Near(Graded(settings, {100, 100, 100}), {107, 74, 43}));
  settings.filter = static_cast<int>(PostFxFilter::kNegative);
  CHECK(Near(Graded(settings, {200, 100, 50}), {55, 155, 205}));
  settings.filter = static_cast<int>(PostFxFilter::kColorblind);
  CHECK(Near(Graded(settings, {200, 100, 50}), {210, 90, 55}));
  // ⚠ BORNÉ : 250 × 1,05 dépasserait.
  CHECK(Near(Graded(settings, {250, 250, 250}), {255, 225, 255}));
}

TEST_CASE("Post-traitement : la vignette épargne le centre", "[render][postfx]") {
  PostFxSettings settings;
  settings.enabled = true;
  settings.vignette = 1.0F;
  constexpr std::int32_t kSide = 101;
  const Image source = Uniform(kSide, {200, 200, 200});
  const Image shaded = ColorPassReference(source, ColorPassConstantsOf(settings, 0, kSide, kSide));
  CHECK(At(shaded, kSide / 2, kSide / 2) == std::array<int, 3>{200, 200, 200});
  // Au coin, la distance au centre vaut ≈ 0,7 : la rampe de 0,8 à 0,35 en est à
  // t ≈ 0,21, soit t² (3 − 2t) ≈ 0,11 de la lumière.
  const std::array<int, 3> corner = At(shaded, 0, 0);
  CHECK(corner[0] > 15);
  CHECK(corner[0] < 30);
}

TEST_CASE("Post-traitement : netteté et aberration laissent une image unie", "[render][postfx]") {
  PostFxSettings settings;
  settings.enabled = true;
  settings.sharpen = 1.0F;
  settings.aberration = 1.0F;
  CHECK(Graded(settings, {90, 140, 210}) == std::array<int, 3>{90, 140, 210});
}

TEST_CASE("Post-traitement : la netteté creuse une arête", "[render][postfx]") {
  // Une moitié gauche sombre, une moitié droite claire.
  constexpr std::int32_t kSide = 8;
  Image source = Uniform(kSide, {60, 60, 60});
  for (std::int32_t y = 0; y < kSide; ++y) {
    for (std::int32_t x = kSide / 2; x < kSide; ++x) {
      const std::size_t at =
          (static_cast<std::size_t>(y) * kSide + static_cast<std::size_t>(x)) * kChannels;
      source.pixels[at] = 180;
      source.pixels[at + 1] = 180;
      source.pixels[at + 2] = 180;
    }
  }
  PostFxSettings settings;
  settings.enabled = true;
  settings.sharpen = 1.0F;
  const Image sharpened =
      ColorPassReference(source, ColorPassConstantsOf(settings, 0, kSide, kSide));
  // Le dernier pixel sombre s'assombrit encore, le premier clair s'éclaircit.
  CHECK(At(sharpened, kSide / 2 - 1, kSide / 2)[0] < 60);
  CHECK(At(sharpened, kSide / 2, kSide / 2)[0] > 180);
  // Loin de l'arête, rien ne bouge.
  CHECK(At(sharpened, 0, kSide / 2)[0] == 60);
}

TEST_CASE("Post-traitement : l'anticrénelage laisse les zones plates", "[render][postfx]") {
  const Image flat = Uniform(6, {30, 120, 240});
  CHECK(FxaaReference(flat, 1.0F).pixels == flat.pixels);
}

TEST_CASE("Post-traitement : l'anticrénelage adoucit une arête en escalier", "[render][postfx]") {
  // Une marche diagonale : clair au-dessus de la diagonale, sombre dessous.
  constexpr std::int32_t kSide = 8;
  Image source = Uniform(kSide, {20, 20, 20});
  for (std::int32_t y = 0; y < kSide; ++y) {
    for (std::int32_t x = y; x < kSide; ++x) {
      const std::size_t at =
          (static_cast<std::size_t>(y) * kSide + static_cast<std::size_t>(x)) * kChannels;
      source.pixels[at] = 230;
      source.pixels[at + 1] = 230;
      source.pixels[at + 2] = 230;
    }
  }
  // À force nulle, l'image ressort intacte.
  CHECK(FxaaReference(source, 0.0F).pixels == source.pixels);
  // À force pleine, un pixel de la marche prend une valeur INTERMÉDIAIRE.
  const Image smoothed = FxaaReference(source, 1.0F);
  bool softened = false;
  for (std::int32_t y = 1; y + 1 < kSide; ++y) {
    const int value = At(smoothed, y, y)[0];
    CHECK(value >= 20);
    CHECK(value <= 230);
    softened = softened || (value > 20 && value < 230);
  }
  CHECK(softened);
}
