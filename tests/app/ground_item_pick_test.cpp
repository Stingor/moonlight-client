// UN OBJET AU SOL ET UN AUTRE ACTEUR SE DISPUTENT LE PIXEL : le recul de chacun
// (`PickBiasOf`) départage, et le monstre vivant passe devant le butin.

#include <cstddef>
#include <optional>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/actor_pick.h"
#include "ro/render/camera.h"
#include "ro/render/sprite_renderer.h"
#include "ro/sim/actor.h"

#include "world/actor_visibility.h"

namespace {

constexpr float kViewportWidth = 800.0F;
constexpr float kViewportHeight = 600.0F;
constexpr float kAspectRatio = kViewportWidth / kViewportHeight;
// L'écart en profondeur de deux acteurs posés sur des cases voisines.
constexpr float kNeighbourDepth = 5.0F;
constexpr std::size_t kItemRank = 0;
constexpr std::size_t kOtherRank = 1;

const glm::vec3 kPile{50.0F, 0.0F, 50.0F};

ro::render::Camera LookingAtPile() {
  ro::render::Camera camera;
  camera.look_at = kPile;
  camera.distance = 100.0F;
  camera.pitch_degrees = 45.0F;
  camera.fov_degrees = 45.0F;
  return camera;
}

ro::sim::Actor Someone(ro::sim::ActorKind kind) {
  ro::sim::Actor actor;
  actor.kind = kind;
  return actor;
}

ro::render::DrawnActor PointAt(const glm::vec3& ground, float pick_bias) {
  ro::render::DrawnActor drawn{nullptr, nullptr, ground};
  drawn.point_pick = true;
  drawn.pick_bias = pick_bias;
  return drawn;
}

// LE RANG DÉSIGNÉ au centre de l'écran, entre le tas et un autre acteur posé
// sur la case voisine — devant le tas si `other_in_front`, derrière sinon.
std::optional<std::size_t> PickedOverPile(const ro::sim::Actor& other, bool other_in_front) {
  const ro::render::Camera camera = LookingAtPile();
  const glm::vec3 toward_eye = glm::normalize(camera.Position() - kPile);
  const glm::vec3 other_ground =
      kPile + toward_eye * (other_in_front ? kNeighbourDepth : -kNeighbourDepth);
  const std::vector<ro::render::DrawnActor> actors = {
      PointAt(kPile, moonclient::PickBiasOf(Someone(ro::sim::ActorKind::kItem))),
      PointAt(other_ground, moonclient::PickBiasOf(other))};
  return ro::render::PickActor(camera, kAspectRatio, actors, kViewportWidth / 2.0F,
                               kViewportHeight / 2.0F, kViewportWidth, kViewportHeight);
}

}  // namespace

TEST_CASE("Butin : un monstre qui y marche prend le curseur, même derrière le tas", "[app][pick]") {
  const ro::sim::Actor monster = Someone(ro::sim::ActorKind::kMonster);
  CHECK(PickedOverPile(monster, true) == kOtherRank);
  CHECK(PickedOverPile(monster, false) == kOtherRank);
}

TEST_CASE("Butin : il passe devant le joueur, son familier et le monstre qui agonise",
          "[app][pick]") {
  ro::sim::Actor pet = Someone(ro::sim::ActorKind::kMonster);
  pet.familiar = ro::sim::Familiar::kPet;
  ro::sim::Actor dying = Someone(ro::sim::ActorKind::kMonster);
  dying.posture = ro::sim::Posture::kDead;
  for (const ro::sim::Actor& other : {Someone(ro::sim::ActorKind::kPlayer), pet, dying}) {
    CHECK(PickedOverPile(other, true) == kItemRank);
  }
}

TEST_CASE("Butin : un PNJ planté dessus garde le clic", "[app][pick]") {
  CHECK(PickedOverPile(Someone(ro::sim::ActorKind::kNonPlayer), false) == kOtherRank);
}
