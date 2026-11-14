// LE DESSIN ENTRE DEUX TICS : le poids, l'angle par le plus court chemin, et la
// charrette et le faucon peints entre leurs deux derniers tics.

#include "ro/render/tick_blend.h"

#include <cmath>
#include <cstddef>
#include <cstdint>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/companion_cart.h"
#include "ro/render/companion_falcon.h"

using Catch::Approx;
using ro::render::BlendDegrees;
using ro::render::BlendScalar;
using ro::render::kWholeTickWeight;
using ro::render::TickBlendWeight;

namespace {

constexpr std::uint32_t kTickMs = 16;
constexpr std::uint32_t kHalfTickMs = 8;
constexpr float kHalfWeight = 0.5F;
constexpr float kNoWeight = 0.0F;
constexpr float kDegreesPerTurn = 360.0F;
constexpr float kFarUnits = 20.0F;
constexpr std::size_t kFramesPerAction = 4;

// Un sol plat à hauteur zéro, et des actions de quatre images.
const ro::render::FalconGround kFlatGround = [](float /*x*/, float /*z*/) { return 0.0F; };
const ro::render::FalconFrameCount kFalconFrames = [](std::size_t /*action*/) {
  return kFramesPerAction;
};
const ro::render::CartFrameCount kCartFrames = [](std::size_t /*action*/) {
  return kFramesPerAction;
};

// Le même point, à l'arrondi près.
bool SamePoint(const ro::render::FalconPoint& left, const ro::render::FalconPoint& right) {
  return left.x == Approx(right.x) && left.y == Approx(right.y) && left.z == Approx(right.z);
}

ro::render::FalconPoint Midpoint(const ro::render::FalconPoint& left,
                                 const ro::render::FalconPoint& right) {
  return ro::render::FalconPoint{(left.x + right.x) / 2.0F, (left.y + right.y) / 2.0F,
                                 (left.z + right.z) / 2.0F};
}

}  // namespace

TEST_CASE("Entre deux tics : le poids est le reste du pas", "[render][tick_blend]") {
  CHECK(TickBlendWeight(0, kTickMs, true) == kNoWeight);
  CHECK(TickBlendWeight(kHalfTickMs, kTickMs, true) == kHalfWeight);
  CHECK(TickBlendWeight(kTickMs, kTickMs, true) == kWholeTickWeight);
  // Plus d'un pas — un tic en retard — ne déborde jamais au-delà du courant.
  CHECK(TickBlendWeight(3 * kTickMs, kTickMs, true) == kWholeTickWeight);
  // ⚠ SANS INTERPOLATION, le tic courant quoi qu'ait compté l'horloge.
  CHECK(TickBlendWeight(0, kTickMs, false) == kWholeTickWeight);
  CHECK(TickBlendWeight(kHalfTickMs, kTickMs, false) == kWholeTickWeight);
}

TEST_CASE("Entre deux tics : au poids un, l'état courant au bit près", "[render][tick_blend]") {
  // Des valeurs qu'un calcul « a + (b − a) × 1 » arrondirait.
  constexpr float kPrevious = 0.1F;
  constexpr float kCurrent = 1.0e7F + 0.3F;
  CHECK(BlendScalar(kPrevious, kCurrent, kWholeTickWeight) == kCurrent);
  CHECK(BlendDegrees(kPrevious, kCurrent, kWholeTickWeight) == kCurrent);
  CHECK(BlendScalar(kPrevious, kCurrent, kNoWeight) == kPrevious);
}

TEST_CASE("Entre deux tics : un angle passe par zéro degré", "[render][tick_blend]") {
  constexpr float kBefore = 350.0F;
  constexpr float kAfter = 10.0F;
  CHECK(BlendDegrees(kBefore, kAfter, kNoWeight) == Approx(kBefore));
  // À mi-chemin : zéro degré, et non cent quatre-vingts.
  const float half = BlendDegrees(kBefore, kAfter, kHalfWeight);
  CHECK(std::remainder(half, kDegreesPerTurn) == Approx(0.0F).margin(1e-4));
  CHECK(BlendDegrees(kBefore, kAfter, kWholeTickWeight) == kAfter);
  // Dans l'autre sens aussi.
  const float back = BlendDegrees(kAfter, kBefore, kHalfWeight);
  CHECK(std::remainder(back, kDegreesPerTurn) == Approx(0.0F).margin(1e-4));
  // Un angle négatif, comme l'orientation d'un modèle : le plus court chemin
  // de −170° à 170° passe par 180°.
  CHECK(std::remainder(BlendDegrees(-170.0F, 170.0F, kHalfWeight), kDegreesPerTurn) ==
        Approx(-180.0F).margin(1e-4));
}

TEST_CASE("Entre deux tics : la charrette glisse vers son maître", "[render][tick_blend]") {
  const ro::render::FalconMaster master{{0.0F, 0.0F, 0.0F}, 0.0F};
  ro::render::Cart cart = ro::render::BornCart(master);
  // Née : rien à mélanger.
  CHECK(SamePoint(ro::render::CartDrawnPosition(cart, kNoWeight), cart.position));

  const ro::render::FalconMaster gone{{kFarUnits, 0.0F, 0.0F}, 0.0F};
  const ro::render::FalconPoint before = cart.position;
  ro::render::StepCart(cart, gone, kFlatGround, kCartFrames, 1.0F, 0.0F);
  REQUIRE_FALSE(SamePoint(cart.position, before));
  CHECK(SamePoint(cart.previous_position, before));
  CHECK(SamePoint(ro::render::CartDrawnPosition(cart, kNoWeight), before));
  CHECK(
      SamePoint(ro::render::CartDrawnPosition(cart, kHalfWeight), Midpoint(before, cart.position)));
  // ⚠ L'OPTION COUPÉE : la position du tic, exactement celle d'avant.
  const ro::render::FalconPoint whole = ro::render::CartDrawnPosition(cart, kWholeTickWeight);
  CHECK(whole.x == cart.position.x);
  CHECK(whole.y == cart.position.y);
  CHECK(whole.z == cart.position.z);
}

TEST_CASE("Entre deux tics : le faucon glisse vers son maître", "[render][tick_blend]") {
  const ro::render::FalconMaster master{{0.0F, 0.0F, 0.0F}, 0.0F};
  ro::render::Falcon falcon = ro::render::BornFalcon(master);
  CHECK(SamePoint(ro::render::FalconDrawnPosition(falcon, kNoWeight), falcon.position));

  const ro::render::FalconMaster gone{{kFarUnits, 0.0F, kFarUnits}, 0.0F};
  const ro::render::FalconPoint before = falcon.position;
  ro::render::StepFalcon(falcon, gone, kFlatGround, kFalconFrames, 0.0F);
  REQUIRE_FALSE(SamePoint(falcon.position, before));
  CHECK(SamePoint(falcon.previous_position, before));
  CHECK(SamePoint(ro::render::FalconDrawnPosition(falcon, kNoWeight), before));
  CHECK(SamePoint(ro::render::FalconDrawnPosition(falcon, kHalfWeight),
                  Midpoint(before, falcon.position)));
  const ro::render::FalconPoint whole = ro::render::FalconDrawnPosition(falcon, kWholeTickWeight);
  CHECK(whole.x == falcon.position.x);
  CHECK(whole.y == falcon.position.y);
  CHECK(whole.z == falcon.position.z);
}
