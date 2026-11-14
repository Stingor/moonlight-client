// LES REFLETS ÉGARÉS D'UN VISAGE : ce qui rejoint la peau, et ce qui reste.
//
// ⚠ SUR UN SPRITE FABRIQUÉ EN MÉMOIRE, dont chaque pixel est posé à la main :
// un œil, un reflet au milieu de la joue, un autre collé au bord.

#include "ro/render/skin_islands.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/spr.h"

#include "support/sprite_builder.h"

namespace {

using ro::test_support::SpriteColor;

// La peau : trois tons, du clair au sombre.
constexpr std::uint8_t kSkinLight = 129;
constexpr std::uint8_t kSkinMiddle = 130;
constexpr std::uint8_t kSkinDark = 131;
constexpr std::array<std::uint8_t, 3> kSkin = {kSkinLight, kSkinMiddle, kSkinDark};
// Un reflet peint d'une couleur de cheveux, claire ; une pupille ; un blanc d'œil.
constexpr std::uint8_t kStrayLight = 200;
constexpr std::uint8_t kPupil = 2;
constexpr std::uint8_t kEyeWhite = 3;
constexpr std::int32_t kSide = 7;

constexpr std::uint8_t T = 0;
constexpr std::uint8_t S = kSkinMiddle;
constexpr std::uint8_t H = kStrayLight;
constexpr std::uint8_t K = kPupil;
constexpr std::uint8_t W = kEyeWhite;

[[nodiscard]] ro::formats::SprSprite Face(const std::vector<std::uint8_t>& indices) {
  std::vector<SpriteColor> palette(ro::formats::kPaletteEntryCount);
  palette[kSkinLight] = SpriteColor{.red = 220, .green = 180, .blue = 160};
  palette[kSkinMiddle] = SpriteColor{.red = 180, .green = 140, .blue = 120};
  palette[kSkinDark] = SpriteColor{.red = 140, .green = 100, .blue = 80};
  palette[kStrayLight] = SpriteColor{.red = 250, .green = 240, .blue = 230};
  palette[kPupil] = SpriteColor{.red = 10, .green = 10, .blue = 10};
  palette[kEyeWhite] = SpriteColor{.red = 240, .green = 240, .blue = 240};
  const std::vector<std::uint8_t> bytes = ro::test_support::BuildSpr(
      {.indexed = {{.width = kSide, .height = kSide, .indices = indices}}, .palette = palette});
  auto sprite = ro::formats::SprSprite::Parse(bytes);
  REQUIRE(sprite.has_value());
  return std::move(*sprite);
}

[[nodiscard]] std::uint8_t At(const ro::formats::SprSprite& sprite, int x, int y) {
  const ro::formats::SprIndexedFrame& frame = sprite.indexed_frames().front();
  return frame.indices[static_cast<std::size_t>(y) * static_cast<std::size_t>(frame.width) +
                       static_cast<std::size_t>(x)];
}

}  // namespace

TEST_CASE("Îlots de peau : le reflet égaré rejoint la peau, l'œil reste", "[render][peau]") {
  // clang-format off
  ro::formats::SprSprite face = Face({
      T, T, T, T, T, T, T,
      T, S, S, S, S, S, T,
      T, S, H, S, S, S, T,
      T, S, S, K, W, S, T,
      T, S, S, S, S, S, T,
      T, S, S, S, S, H, T,
      T, T, T, T, T, T, T,
  });
  // clang-format on
  CHECK(ro::render::FoldSkinIslands(face, kSkin) == 1);
  // Le reflet prend le ton de peau le plus proche en clarté : le plus clair.
  CHECK(At(face, 2, 2) == kSkinLight);
  // 🔴 L'ŒIL, ENTOURÉ DE PEAU, PORTE UNE PUPILLE SOMBRE : il reste tel quel,
  // blanc compris.
  CHECK(At(face, 3, 3) == kPupil);
  CHECK(At(face, 4, 3) == kEyeWhite);
  // Collé à la transparence, ce n'est pas un îlot : le bord du visage reste.
  CHECK(At(face, 5, 5) == kStrayLight);
}

TEST_CASE("Îlots de peau : sans ensemble de peau, rien ne bouge", "[render][peau]") {
  // clang-format off
  ro::formats::SprSprite face = Face({
      T, T, T, T, T, T, T,
      T, S, S, S, S, S, T,
      T, S, H, S, S, S, T,
      T, S, S, S, S, S, T,
      T, S, S, S, S, S, T,
      T, S, S, S, S, S, T,
      T, T, T, T, T, T, T,
  });
  // clang-format on
  CHECK(ro::render::FoldSkinIslands(face, {}) == 0);
  CHECK(At(face, 2, 2) == kStrayLight);
}
