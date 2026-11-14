// LA TRAÎNÉE COLORÉE (effet 166) : la couleur que chaque état lui donne, ce
// qu'elle fait du corps qui la porte, quand elle sème ses copies et comment
// elles s'effacent.

#include "ro/render/afterimage_blur.h"

#include <array>
#include <cstdint>
#include <optional>

#include <catch2/catch_test_macros.hpp>

namespace {

using ro::render::AfterimageBlur;
using ro::render::Motion;

constexpr std::array<std::uint8_t, 4> kWhite = {255, 255, 255, 255};
constexpr std::uint16_t kEnergyCoat = 31;
constexpr std::uint16_t kOverthrust = 25;
constexpr std::uint16_t kTwoHandQuicken = 2;
constexpr std::uint16_t kExplosionSpirits = 86;
constexpr std::uint16_t kProvoke = 0;

}  // namespace

TEST_CASE("chaque état donne sa couleur à la traînée", "[afterimage_blur]") {
  CHECK(ro::render::AfterimageBlurStateColor(kEnergyCoat) ==
        std::array<float, 3>{180.0F, 180.0F, 250.0F});
  CHECK(ro::render::AfterimageBlurStateColor(kOverthrust) ==
        std::array<float, 3>{250.0F, 150.0F, 150.0F});
  CHECK(ro::render::AfterimageBlurStateColor(kTwoHandQuicken) ==
        std::array<float, 3>{200.0F, 200.0F, 0.0F});
  CHECK(ro::render::AfterimageBlurStateColor(kExplosionSpirits) ==
        std::array<float, 3>{-1.0F, -1.0F, -1.0F});
  CHECK_FALSE(ro::render::AfterimageBlurStateColor(kProvoke).has_value());
}

TEST_CASE("Energy Coat teint le corps en bleu et garde son alpha", "[afterimage_blur]") {
  const AfterimageBlur blur{{180.0F, 180.0F, 250.0F}, 7};
  const std::array<std::uint8_t, 4> translucent = {255, 255, 255, 90};
  CHECK(ro::render::AfterimageBlurCarrierTint(translucent, blur, 0) ==
        std::array<std::uint8_t, 4>{180, 180, 250, 90});
}

TEST_CASE("sans état, la place de naissance fait la couleur, octet par octet",
          "[afterimage_blur]") {
  // Troncature vers zéro, puis l'octet faible : −37,6 donne −37, soit 0xDB ;
  // 300,9 donne 300, soit 44. Et −1 laisse la composante telle quelle.
  const AfterimageBlur blur{{-37.6F, 300.9F, -1.4F}, 0};
  CHECK(ro::render::AfterimageBlurCarrierTint(kWhite, blur, 0) ==
        std::array<std::uint8_t, 4>{0xDB, 44, 255, 255});
  // La copie ne reçoit pas non plus la composante −1 : elle reste blanche.
  CHECK(ro::render::AfterimageGhostTint(blur) == std::array<std::uint8_t, 4>{0xDB, 44, 255, 180});
}

TEST_CASE("le rouge −1 fait clignoter le corps entre le rose et le blanc", "[afterimage_blur]") {
  const AfterimageBlur blur{{-1.0F, -1.0F, -1.0F}, 3};
  CHECK(ro::render::AfterimageBlurCarrierTint(kWhite, blur, 14) ==
        std::array<std::uint8_t, 4>{250, 250, 250, 255});
  CHECK(ro::render::AfterimageBlurCarrierTint(kWhite, blur, 15) ==
        std::array<std::uint8_t, 4>{250, 200, 200, 255});
  // Et il ne sème rien, même en marchant.
  CHECK_FALSE(ro::render::AfterimageBlurSowsAt(blur, Motion::kWalk));
}

TEST_CASE("la traînée ne sème qu'en mouvement, tous les cinq tics", "[afterimage_blur]") {
  AfterimageBlur blur{{180.0F, 180.0F, 250.0F}, 10};
  CHECK(ro::render::AfterimageBlurSowsAt(blur, Motion::kWalk));
  CHECK(ro::render::AfterimageBlurSowsAt(blur, Motion::kAttack));
  CHECK(ro::render::AfterimageBlurSowsAt(blur, Motion::kCast));
  // Immobile, rien : c'est ce qui faisait croire que l'effet ne montrait rien.
  CHECK_FALSE(ro::render::AfterimageBlurSowsAt(blur, Motion::kIdle));
  CHECK_FALSE(ro::render::AfterimageBlurSowsAt(blur, Motion::kReady));
  CHECK_FALSE(ro::render::AfterimageBlurSowsAt(blur, Motion::kSit));
  blur.age_ticks = 11;
  CHECK_FALSE(ro::render::AfterimageBlurSowsAt(blur, Motion::kWalk));
  // L'âge repart de zéro à 9998 : ce tic-là sème, comme le premier.
  blur.age_ticks = ro::render::kAfterimageBlurAgeWrapTicks;
  CHECK(ro::render::AfterimageBlurSowsAt(blur, Motion::kWalk));
}

TEST_CASE("une image qui enjambe le tic du semis sème quand même", "[afterimage_blur]") {
  // Le client met la traînée à jour à chaque tic ; une image qui en saute un
  // ne doit pas perdre la copie de l'âge multiple de cinq qu'elle a franchi.
  AfterimageBlur blur;
  blur.color = {200.0F, 200.0F, 0.0F};
  blur.age_ticks = 6;
  CHECK(ro::render::AfterimageBlurSowsSince(blur, 4U, Motion::kAttack));
  CHECK_FALSE(ro::render::AfterimageBlurSowsSince(blur, 5U, Motion::kAttack));
  CHECK_FALSE(ro::render::AfterimageBlurSowsSince(blur, 4U, Motion::kIdle));
  blur.age_ticks = 9;
  CHECK_FALSE(ro::render::AfterimageBlurSowsSince(blur, 6U, Motion::kWalk));
  blur.age_ticks = 11;
  CHECK(ro::render::AfterimageBlurSowsSince(blur, 8U, Motion::kWalk));
  // Sans image précédente, ou quand l'effet renaît plus jeune, seul l'âge
  // courant compte.
  blur.age_ticks = 10;
  CHECK(ro::render::AfterimageBlurSowsSince(blur, std::nullopt, Motion::kWalk));
  blur.age_ticks = 3;
  CHECK_FALSE(ro::render::AfterimageBlurSowsSince(blur, 40U, Motion::kWalk));
}

TEST_CASE("une copie part de 180 d'alpha et en perd huit par tic", "[afterimage_blur]") {
  CHECK(ro::render::AfterimageGhostAlphaAt(0) == std::uint8_t{180});
  CHECK(ro::render::AfterimageGhostAlphaAt(1) == std::uint8_t{172});
  CHECK(ro::render::AfterimageGhostAlphaAt(22) == std::uint8_t{4});
  CHECK_FALSE(ro::render::AfterimageGhostAlphaAt(23).has_value());
  CHECK(ro::render::kAfterimageGhostLifeTicks == 23);
}
