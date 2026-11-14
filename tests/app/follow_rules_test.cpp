// Le suivi d'un joueur : qui se suit, ce qui l'arrête, la cadence de ses pas,
// la perte de vue qui pardonne un retour, le demi-tour assis ou en salon, la
// case franchissable la plus proche, et la commande `/follow`.

#include "world/follow_rules.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/actor.h"
#include "ro/sim/walk_map.h"
#include "ro/sim/world.h"

using moonclient::AdvanceFollow;
using moonclient::BeginFollow;
using moonclient::CanFollow;
using moonclient::EndFollow;
using moonclient::FollowDecision;
using moonclient::FollowedPlayer;
using moonclient::FollowEnd;
using moonclient::FollowState;
using moonclient::FollowStep;
using moonclient::FollowTick;
using moonclient::kFollowLostMs;
using moonclient::kFollowStepMs;
using moonclient::PlayerInViewNamed;
using moonclient::ReadFollowCommand;
using ro::sim::Actor;
using ro::sim::ActorKind;
using ro::sim::WalkMap;
using ro::sim::World;

namespace {

constexpr std::uint32_t kSelf = 150001;
constexpr std::uint32_t kRogue = 150002;
constexpr std::uint32_t kTwin = 150003;
constexpr std::uint32_t kWolf = 2000123;
constexpr std::uint32_t kStart = 50000;
constexpr std::uint16_t kRogueX = 3;
constexpr std::uint16_t kRogueY = 2;

[[nodiscard]] Actor Spawned(std::uint32_t id, ActorKind kind, std::string name) {
  Actor actor;
  actor.id = id;
  actor.kind = kind;
  actor.name = std::move(name);
  return actor;
}

[[nodiscard]] Actor Rogue() {
  Actor rogue = Spawned(kRogue, ActorKind::kPlayer, "Rogue");
  rogue.position.x = kRogueX;
  rogue.position.y = kRogueY;
  return rogue;
}

[[nodiscard]] World Peopled() {
  World world;
  world.Spawn(Spawned(kSelf, ActorKind::kPlayer, "Moi"));
  world.set_player_id(kSelf);
  world.Spawn(Rogue());
  world.Spawn(Spawned(kWolf, ActorKind::kMonster, "Wolf"));
  return world;
}

[[nodiscard]] FollowState Following() {
  FollowState state;
  BeginFollow(state, Rogue());
  return state;
}

[[nodiscard]] FollowTick At(std::uint32_t now_ms) { return FollowTick{.now_ms = now_ms}; }

}  // namespace

TEST_CASE("Suivi : seul un autre joueur se suit", "[app][suivi]") {
  CHECK(CanFollow(Rogue(), kSelf));
  CHECK_FALSE(CanFollow(Spawned(kSelf, ActorKind::kPlayer, "Moi"), kSelf));
  CHECK_FALSE(CanFollow(Spawned(kWolf, ActorKind::kMonster, "Wolf"), kSelf));
  CHECK_FALSE(CanFollow(Spawned(kTwin, ActorKind::kNonPlayer, "Kafra"), kSelf));

  const World world = Peopled();
  CHECK(FollowedPlayer(world, kRogue) != nullptr);
  // Un monstre de cet identifiant n'est pas le joueur suivi.
  CHECK(FollowedPlayer(world, kWolf) == nullptr);
  CHECK(FollowedPlayer(world, 0) == nullptr);
}

TEST_CASE("Suivi : le premier pas part aussitôt, vers la case du joueur", "[app][suivi]") {
  const World world = Peopled();
  FollowState state = Following();
  const FollowDecision first = AdvanceFollow(state, world, nullptr, At(kStart));
  CHECK(first.step == FollowStep::kWalk);
  CHECK(first.cell.x == kRogueX);
  CHECK(first.cell.y == kRogueY);
}

TEST_CASE("Suivi : un pas par seconde, strictement au-delà", "[app][suivi]") {
  const World world = Peopled();
  FollowState state = Following();
  REQUIRE(AdvanceFollow(state, world, nullptr, At(kStart)).step == FollowStep::kWalk);
  CHECK(AdvanceFollow(state, world, nullptr, At(kStart + kFollowStepMs)).step ==
        FollowStep::kNothing);
  CHECK(AdvanceFollow(state, world, nullptr, At(kStart + kFollowStepMs + 1)).step ==
        FollowStep::kWalk);
}

TEST_CASE("Suivi : le bouton gauche l'arrête, appuyé ou tenu", "[app][suivi]") {
  const World world = Peopled();
  FollowState state = Following();
  state.left_pressed = true;
  const FollowDecision pressed = AdvanceFollow(state, world, nullptr, At(kStart));
  CHECK(pressed.step == FollowStep::kStop);
  CHECK(pressed.end == FollowEnd::kLetGo);
  // L'appui est consommé ; l'arrêt, lui, revient à l'appelant.
  CHECK_FALSE(state.left_pressed);
  CHECK(state.active());

  FollowState held = Following();
  CHECK(AdvanceFollow(held, world, nullptr, FollowTick{.left_held = true, .now_ms = kStart}).step ==
        FollowStep::kStop);
}

TEST_CASE("Suivi : sans suivi, rien, et l'appui gauche ne survit pas", "[app][suivi]") {
  const World world = Peopled();
  FollowState state;
  state.left_pressed = true;
  CHECK(AdvanceFollow(state, world, nullptr, At(kStart)).step == FollowStep::kNothing);
  CHECK_FALSE(state.left_pressed);
  FollowState ended = Following();
  EndFollow(ended);
  CHECK_FALSE(ended.active());
  CHECK(ended.followed_name.empty());
}

TEST_CASE("Suivi : hors de vue plus de dix secondes, il s'arrête", "[app][suivi]") {
  World world = Peopled();
  FollowState state = Following();
  REQUIRE(world.Remove(kRogue));
  CHECK(AdvanceFollow(state, world, nullptr, At(kStart)).step == FollowStep::kNothing);
  CHECK(AdvanceFollow(state, world, nullptr, At(kStart + kFollowLostMs)).step ==
        FollowStep::kNothing);
  const FollowDecision lost = AdvanceFollow(state, world, nullptr, At(kStart + kFollowLostMs + 1));
  CHECK(lost.step == FollowStep::kStop);
  CHECK(lost.end == FollowEnd::kOutOfSight);
}

TEST_CASE("Suivi : un retour avant le délai remet l'horloge de perte à zéro", "[app][suivi]") {
  World world = Peopled();
  FollowState state = Following();
  REQUIRE(world.Remove(kRogue));
  CHECK(AdvanceFollow(state, world, nullptr, At(kStart)).step == FollowStep::kNothing);
  // Il reparaît de l'autre côté du portail…
  world.Spawn(Rogue());
  CHECK(AdvanceFollow(state, world, nullptr, At(kStart + kFollowLostMs)).step == FollowStep::kWalk);
  // … repart, et dix nouvelles secondes lui sont rendues.
  REQUIRE(world.Remove(kRogue));
  CHECK(AdvanceFollow(state, world, nullptr, At(kStart + kFollowLostMs + 1)).step ==
        FollowStep::kNothing);
  CHECK(AdvanceFollow(state, world, nullptr, At(kStart + (2 * kFollowLostMs) + 1)).step ==
        FollowStep::kNothing);
  CHECK(AdvanceFollow(state, world, nullptr, At(kStart + (2 * kFollowLostMs) + 2)).step ==
        FollowStep::kStop);
}

TEST_CASE("Suivi : assis ou en salon, le personnage se tourne seulement", "[app][suivi]") {
  World world = Peopled();
  REQUIRE(world.SetPosture(kSelf, ro::sim::Posture::kSitting, kStart));
  FollowState state = Following();
  const FollowDecision sitting = AdvanceFollow(state, world, nullptr, At(kStart));
  CHECK(sitting.step == FollowStep::kTurn);
  CHECK(sitting.cell.x == kRogueX);

  const World standing = Peopled();
  FollowState in_room = Following();
  CHECK(
      AdvanceFollow(in_room, standing, nullptr, FollowTick{.in_chat_room = true, .now_ms = kStart})
          .step == FollowStep::kTurn);
}

TEST_CASE("Suivi : une case bloquée se remplace par la plus proche qui se franchit",
          "[app][suivi]") {
  const World world = Peopled();
  // Le joueur suivi se tient sur le seul mur, en (3, 2) ; sa voisine de
  // gauche se franchit. La première ligne est le nord.
  constexpr std::array<std::string_view, 4> kRows{
      "######",
      "##.#..",
      "######",
      "######",
  };
  const WalkMap walk = WalkMap::FromPicture(kRows);
  REQUIRE_FALSE(walk.Passable(kRogueX, kRogueY));
  FollowState state = Following();
  const FollowDecision step = AdvanceFollow(state, world, &walk, At(kStart));
  CHECK(step.step == FollowStep::kWalk);
  CHECK(walk.Passable(step.cell.x, step.cell.y));

  // Rien qui se franchisse alentour : aucun pas.
  constexpr std::array<std::string_view, 4> kWalls{
      "######",
      "######",
      "######",
      "######",
  };
  const WalkMap walls = WalkMap::FromPicture(kWalls);
  FollowState walled = Following();
  CHECK(AdvanceFollow(walled, world, &walls, At(kStart)).step == FollowStep::kNothing);
}

TEST_CASE("Suivi : /follow nomme le joueur, ou arrête", "[app][suivi][commande]") {
  CHECK(ReadFollowCommand("/follow Rogue") == std::optional<std::string>("Rogue"));
  CHECK(ReadFollowCommand("/FOLLOW  Le Rogue ") == std::optional<std::string>("Le Rogue"));
  CHECK(ReadFollowCommand("/follow") == std::optional<std::string>(""));
  CHECK(ReadFollowCommand("/follow   ") == std::optional<std::string>(""));
  CHECK_FALSE(ReadFollowCommand("/followme").has_value());
  CHECK_FALSE(ReadFollowCommand("bonjour").has_value());
}

TEST_CASE("Suivi : le nom se compare exactement, soi excepté", "[app][suivi][commande]") {
  World world = Peopled();
  const Actor* found = PlayerInViewNamed(world, "Rogue");
  REQUIRE(found != nullptr);
  CHECK(found->id == kRogue);
  CHECK(PlayerInViewNamed(world, "rogue") == nullptr);
  CHECK(PlayerInViewNamed(world, "Moi") == nullptr);
  CHECK(PlayerInViewNamed(world, "Wolf") == nullptr);
  // Deux homonymes : toujours le même, le plus petit identifiant.
  world.Spawn(Spawned(kTwin, ActorKind::kPlayer, "Rogue"));
  CHECK(PlayerInViewNamed(world, "Rogue")->id == kRogue);
}
