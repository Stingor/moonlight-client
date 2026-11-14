// LA BOULE D'ÉNERGIE DE LA BAGUETTE, hors de la troupe : sa couleur, son
// disque fait en code, son sprite, et les pièces d'un tir.

#include "ro/render/wand_orb.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <glm/glm.hpp>

#include "ro/formats/act.h"
#include "ro/formats/spr.h"
#include "ro/render/effect_frame.h"
#include "ro/render/element_glow.h"

using Catch::Approx;
using ro::render::WandOrbPart;
using ro::render::WandOrbPiece;

namespace {

constexpr glm::vec3 kShooter{0.0F, 10.0F, 0.0F};
constexpr glm::vec3 kTarget{0.0F, 10.0F, -40.0F};
// Le vol d'un trait de baguette à pleine portée : douze tics, trois fois.
constexpr std::int32_t kWandFlightTicks = 36;
// Un vol bien plus long que le plafond d'étincelles n'en sème.
constexpr std::int32_t kVeryLongFlightTicks = 1000;
constexpr std::array<std::uint8_t, 3> kFireRgb = {255, 140, 40};
constexpr std::size_t kRgbaChannels = 4;
constexpr std::size_t kAlphaChannel = 3;

std::vector<WandOrbPiece> PiecesOf(std::int32_t flight_ticks) {
  std::uint32_t generator = 1;
  return ro::render::WandOrbPieces(kShooter, kTarget, flight_ticks, kFireRgb, generator);
}

std::size_t CountOf(const std::vector<WandOrbPiece>& pieces, WandOrbPart part) {
  return static_cast<std::size_t>(std::ranges::count_if(
      pieces, [part](const WandOrbPiece& piece) { return piece.part == part; }));
}

}  // namespace

TEST_CASE("boule de baguette : la couleur de l'élément, l'arcane sans lui", "[wand_orb]") {
  CHECK(ro::render::WandOrbRgb(ro::render::ElementHue::kFire) ==
        ro::render::ElementHueRgb(ro::render::ElementHue::kFire));
  CHECK(ro::render::WandOrbRgb(ro::render::ElementHue::kWater) ==
        ro::render::ElementHueRgb(ro::render::ElementHue::kWater));
  CHECK(ro::render::WandOrbRgb(std::nullopt) == ro::render::kWandOrbArcaneRgb);
  // LE CŒUR TIRE VERS LE BLANC sans perdre sa couleur : chaque canal monte, et
  // le plus faible reste le plus faible.
  const std::array<std::uint8_t, 3> core = ro::render::WandOrbCoreRgb(kFireRgb);
  for (std::size_t channel = 0; channel < core.size(); ++channel) {
    CHECK(core[channel] >= kFireRgb[channel]);
  }
  CHECK(core[2] < core[0]);
}

TEST_CASE("boule de baguette : le disque est plein au centre et s'évanouit au bord", "[wand_orb]") {
  const ro::formats::Image disc = ro::render::WandOrbDiscImage();
  REQUIRE(disc.width == ro::render::kWandOrbDiscPixels);
  REQUIRE(disc.height == ro::render::kWandOrbDiscPixels);
  const auto alpha_at = [&disc](std::int32_t column, std::int32_t row) {
    return disc.pixels[(static_cast<std::size_t>(row * disc.width + column)) * kRgbaChannels +
                       kAlphaChannel];
  };
  const std::int32_t middle = ro::render::kWandOrbDiscPixels / 2;
  CHECK(alpha_at(middle, middle) > 240);
  CHECK(alpha_at(0, 0) == 0);
  CHECK(alpha_at(0, middle) < 10);
  // Du centre au bord, l'alpha ne remonte jamais.
  for (std::int32_t column = middle; column + 1 < disc.width; ++column) {
    CHECK(alpha_at(column + 1, middle) <= alpha_at(column, middle));
  }
}

TEST_CASE("boule de baguette : une action par pièce, l'attente sur une image vide", "[wand_orb]") {
  const ro::render::LoadedActorSprite sprite = ro::render::WandOrbSprite();
  REQUIRE(sprite.sprite.bgra_frames().size() == 1);
  const std::vector<ro::formats::ActAction>& actions = sprite.animation.actions();
  REQUIRE(actions.size() == ro::render::kWandOrbPartCount);
  const auto action = [&actions](WandOrbPart part) {
    return actions[static_cast<std::size_t>(ro::render::WandOrbActionOf(part))];
  };
  REQUIRE(action(WandOrbPart::kCore).frames.size() == 1);
  CHECK(action(WandOrbPart::kCore).frames.front().layers.front().scale_x ==
        Approx(ro::render::kWandOrbCoreDiameterPixels /
               static_cast<float>(ro::render::kWandOrbDiscPixels)));
  // LE HALO PULSE : ses images n'ont pas toutes la même taille.
  const ro::formats::ActAction halo = action(WandOrbPart::kHalo);
  REQUIRE(halo.frames.size() == static_cast<std::size_t>(ro::render::kWandOrbHaloPulseFrames));
  const auto [smallest, largest] = std::ranges::minmax_element(
      halo.frames, {},
      [](const ro::formats::ActFrame& frame) { return frame.layers.front().scale_x; });
  CHECK(largest->layers.front().scale_x > smallest->layers.front().scale_x);
  // L'ÉTINCELLE RÉTRÉCIT, L'ÉCLAT S'ÉLARGIT, et tous deux attendent sur une
  // image vide.
  const ro::formats::ActAction spark = action(WandOrbPart::kSpark);
  const ro::formats::ActAction burst = action(WandOrbPart::kBurst);
  CHECK(spark.frames.front().layers.empty());
  CHECK(burst.frames.front().layers.empty());
  CHECK(spark.frames.back().layers.front().scale_x < spark.frames[1].layers.front().scale_x);
  CHECK(burst.frames.back().layers.front().scale_x > burst.frames[1].layers.front().scale_x);
}

TEST_CASE("boule de baguette : seul le cœur porte l'ombre", "[wand_orb]") {
  const std::vector<WandOrbPiece> pieces = PiecesOf(kWandFlightTicks);
  REQUIRE(CountOf(pieces, WandOrbPart::kCore) == 1);
  REQUIRE(CountOf(pieces, WandOrbPart::kHalo) == 1);
  REQUIRE(CountOf(pieces, WandOrbPart::kBurst) == 1);
  for (const WandOrbPiece& piece : pieces) {
    CHECK(piece.casts_shadow == (piece.part == WandOrbPart::kCore));
  }
}

TEST_CASE("boule de baguette : le cœur et le halo volent ensemble jusqu'à la cible", "[wand_orb]") {
  for (const WandOrbPiece& piece : PiecesOf(kWandFlightTicks)) {
    const bool flies = piece.part == WandOrbPart::kCore || piece.part == WandOrbPart::kHalo;
    CHECK(piece.flies_to.has_value() == flies);
    if (flies) {
      CHECK(piece.birth == kShooter);
      CHECK(*piece.flies_to == kTarget);
      CHECK(piece.life_ticks == kWandFlightTicks);
      CHECK(piece.hidden_ticks == 0);
    }
  }
}

TEST_CASE("boule de baguette : les étincelles naissent sur le vol, en nombre borné", "[wand_orb]") {
  const std::vector<WandOrbPiece> pieces = PiecesOf(kWandFlightTicks);
  const std::size_t sparks = CountOf(pieces, WandOrbPart::kSpark);
  CHECK(sparks > 0);
  CHECK(sparks <= static_cast<std::size_t>(ro::render::kWandOrbSparkCountMax));
  for (const WandOrbPiece& piece : pieces) {
    if (piece.part != WandOrbPart::kSpark) {
      continue;
    }
    // Elle attend que la boule passe, puis vit ses images en s'effaçant.
    CHECK(piece.hidden_ticks > 0);
    CHECK(piece.hidden_ticks < kWandFlightTicks);
    CHECK(piece.life_ticks - piece.hidden_ticks == ro::render::kWandOrbSparkFrames);
    CHECK(piece.fade_out_ticks == ro::render::kWandOrbSparkFrames);
    CHECK_FALSE(piece.loops_frames);
    // Près du point où passe la boule à son tic.
    const float share =
        static_cast<float>(piece.hidden_ticks) / static_cast<float>(kWandFlightTicks);
    const glm::vec3 passing = kShooter + (kTarget - kShooter) * share;
    CHECK(glm::length(piece.birth - passing) <=
          ro::render::kWandOrbSparkJitterUnits * std::sqrt(3.0F) + 1e-4F);
  }
  // UN VOL TRÈS LONG ne sème pas plus que le plafond.
  CHECK(CountOf(PiecesOf(kVeryLongFlightTicks), WandOrbPart::kSpark) ==
        static_cast<std::size_t>(ro::render::kWandOrbSparkCountMax));
  // Un vol d'un tic n'en sème aucune.
  CHECK(CountOf(PiecesOf(1), WandOrbPart::kSpark) == 0);
}

TEST_CASE("boule de baguette : l'éclat attend l'arrivée et dure moins de 250 ms", "[wand_orb]") {
  for (const WandOrbPiece& piece : PiecesOf(kWandFlightTicks)) {
    if (piece.part != WandOrbPart::kBurst) {
      continue;
    }
    CHECK(piece.birth == kTarget);
    CHECK(piece.hidden_ticks == kWandFlightTicks);
    const std::int32_t shown_ticks = piece.life_ticks - piece.hidden_ticks;
    CHECK(shown_ticks == ro::render::kWandOrbBurstFrames);
    CHECK(static_cast<float>(shown_ticks * ro::render::kEffectTickMs) <
          ro::render::kWandOrbBurstCeilingMs);
    CHECK(piece.fade_out_ticks == shown_ticks);
  }
}

TEST_CASE("formats : un sprite et une animation faits en code", "[wand_orb]") {
  ro::formats::Image image;
  image.width = 1;
  image.height = 1;
  image.pixels = {1, 2, 3, 4};
  const ro::formats::SprSprite sprite = ro::formats::SprSprite::FromRgbaFrames({image});
  CHECK(sprite.indexed_frames().empty());
  REQUIRE(sprite.bgra_frames().size() == 1);
  CHECK(sprite.bgra_frames().front().pixels == image.pixels);
  CHECK_FALSE(sprite.palette().has_value());
  const ro::formats::ActAnimation animation =
      ro::formats::ActAnimation::FromActions(std::vector<ro::formats::ActAction>(2));
  CHECK(animation.actions().size() == 2);
  CHECK(animation.sound_files().empty());
  CHECK_FALSE(animation.anchors_dropped());
}
