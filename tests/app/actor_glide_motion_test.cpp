// LE BOND DE BODY RELOCATION GLISSE EN MARCHANT : le Monk montre sa marche,
// tourné vers sa destination, figée sur sa septième image — ou sur la dernière
// si son action en a moins. La traînée recopie cette pose.

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/sprite_frame.h"
#include "ro/sim/actor.h"
#include "ro/sim/movement.h"
#include "ro/sim/walk_path.h"
#include "ro/sim/world.h"

namespace {

constexpr std::uint32_t kMonk = 51;
constexpr std::uint32_t kSnapMs = 8'000;
// Quinze millisecondes par case, comme le bond.
constexpr std::uint16_t kGlideMsPerCell = 15;
// En plein glissement : deux cases sur quatre.
constexpr std::uint32_t kMidGlideMs = kSnapMs + 30;
constexpr std::uint8_t kNorth = 0;
constexpr std::uint8_t kSouthWest = 3;

}  // namespace

TEST_CASE("Body Relocation : la marche du glissement tient sa septième image",
          "[actor_glide_motion]") {
  CHECK(ro::render::GlideHeldWalkFrame(10) == 7);
  CHECK(ro::render::GlideHeldWalkFrame(8) == 7);
  // Une marche courte tient sa dernière image.
  CHECK(ro::render::GlideHeldWalkFrame(4) == 3);
  CHECK(ro::render::GlideHeldWalkFrame(0) == 0);
}

TEST_CASE("Body Relocation : le Monk glisse tourné vers où il va", "[actor_glide_motion]") {
  ro::sim::World world;
  ro::sim::Actor monk;
  monk.id = kMonk;
  monk.kind = ro::sim::ActorKind::kPlayer;
  monk.position = ro::sim::CellPosition{20, 20, kSouthWest};
  world.Spawn(monk);
  REQUIRE(world.StartGliding(kMonk, ro::sim::CellPosition{20, 24, 0}, kGlideMsPerCell, kSnapMs));
  ro::sim::WalkPath north;
  north.directions = {kNorth, kNorth, kNorth, kNorth};
  const ro::sim::Actor& gliding = *world.Find(kMonk);
  CHECK(ro::sim::IsWalking(gliding, north, kMidGlideMs));
  // ⚠ L'ANCIEN REGARD NE TIENT PAS : le glissement tourne comme une marche.
  CHECK(ro::sim::FacingAt(gliding, north, kMidGlideMs) == kNorth);
}
