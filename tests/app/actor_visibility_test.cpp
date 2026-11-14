// Ce que laissent voir d'un acteur ses options de dissimulation.
//
// 🔴 LE JOUEUR CACHÉ N'EST PAS UNE EXCEPTION. Il restait dessiné tel quel, sous
// prétexte que « le jeu montre toujours soi-même » ; le jeu retire le corps à
// tous, lui compris, et ne lui laisse que son ombre.

#include "world/actor_visibility.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/world.h"

using moonclient::Concealment;
using moonclient::ConcealmentOf;
using moonclient::IsHidden;
using moonclient::KeepConcealedSelf;
using ro::sim::Actor;
using ro::sim::ActorKind;
using ro::sim::World;

namespace {

constexpr std::uint32_t kSelf = 150001;
constexpr std::uint32_t kRogue = 150002;

[[nodiscard]] Actor Player(std::uint32_t id, std::uint32_t option) {
  Actor actor;
  actor.id = id;
  actor.kind = ActorKind::kPlayer;
  actor.option = option;
  return actor;
}

[[nodiscard]] World WithSelf() {
  World world;
  world.set_player_id(kSelf);
  return world;
}

}  // namespace

TEST_CASE("Hiding : le joueur caché perd son corps et garde son ombre, les autres perdent tout",
          "[app][visibility]") {
  const World world = WithSelf();
  const Actor self = Player(kSelf, ro::sim::kOptionHide);
  const Actor rogue = Player(kRogue, ro::sim::kOptionHide);
  CHECK(ConcealmentOf(self, world) == Concealment::kShadowOnly);
  CHECK(ConcealmentOf(rogue, world) == Concealment::kNothing);
  // Son corps ne se dessine plus : c'est ce que le jeu disait faux.
  CHECK(IsHidden(self, world));
  CHECK(IsHidden(rogue, world));
}

TEST_CASE("Camouflé ou invisible, le joueur disparaît tout entier, pour lui-même aussi",
          "[app][visibility]") {
  const World world = WithSelf();
  CHECK(ConcealmentOf(Player(kSelf, ro::sim::kOptionCloak), world) == Concealment::kNothing);
  CHECK(ConcealmentOf(Player(kSelf, ro::sim::kOptionInvisible), world) == Concealment::kNothing);
  // Le camouflage l'emporte sur la cachette : l'ombre part avec lui.
  CHECK(ConcealmentOf(Player(kSelf, ro::sim::kOptionHide | ro::sim::kOptionCloak), world) ==
        Concealment::kNothing);
  CHECK(ConcealmentOf(Player(kSelf, 0), world) == Concealment::kVisible);
  CHECK_FALSE(IsHidden(Player(kRogue, 0), world));
}

TEST_CASE("Le joueur dissimulé garde un rang sans corps, et son ombre s'il est seulement caché",
          "[app][visibility]") {
  const ro::render::ComposedActor shadow;
  const glm::vec3 feet(10.0F, 2.0F, -4.0F);
  std::vector<ro::render::DrawnActor> drawn;
  std::vector<std::uint32_t> ids;

  // Le rang n'a aucun corps : rien ne s'y clique. Son ombre, il la porte comme
  // un corps porte la sienne, parmi les corps que l'eau recouvre.
  KeepConcealedSelf(Concealment::kShadowOnly, kSelf, shadow, feet, drawn, ids);
  REQUIRE(drawn.size() == 1);
  CHECK(drawn[0].actor == nullptr);
  CHECK(drawn[0].shadow == &shadow);
  CHECK(drawn[0].ground_position == feet);
  REQUIRE(ids.size() == 1);
  CHECK(ids[0] == kSelf);

  KeepConcealedSelf(Concealment::kNothing, kSelf, shadow, feet, drawn, ids);
  REQUIRE(drawn.size() == 2);
  CHECK(drawn[1].actor == nullptr);
  CHECK(drawn[1].shadow == nullptr);
  CHECK(ids.size() == 2);
}

TEST_CASE("Visibilité : un warp laisse le clic descendre au sol", "[app][visibility]") {
  Actor warp;
  warp.kind = ActorKind::kNonPlayer;
  warp.look.job = ro::sim::kPortalJob;
  CHECK(moonclient::ClickTakerUnderMouse(&warp) == nullptr);

  // Un PNJ ordinaire, lui, prend le clic.
  Actor merchant = warp;
  merchant.look.job = ro::sim::kPortalJob + 1;
  CHECK(moonclient::ClickTakerUnderMouse(&merchant) == &merchant);
  CHECK(moonclient::ClickTakerUnderMouse(nullptr) == nullptr);
}
