// LES EFFETS D'OFFICE D'UNE CRÉATURE : posés quand elle paraît, retirés quand
// elle se cache, reposés quand elle reparaît.

#include "world/monster_auras.h"

#include <algorithm>
#include <cstdint>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/world.h"

using moonclient::DressMonsters;
using ro::sim::Actor;
using ro::sim::ActorKind;
using ro::sim::World;
using ro::vfs::MonsterSizeEffects;

namespace {

constexpr std::uint32_t kPoring = 1002;
constexpr std::uint32_t kMonsterId = 110001;
constexpr std::uint32_t kGreen = 890;
constexpr std::uint32_t kAura = 892;

constexpr std::string_view kTable = R"(
version: 1
monsters:
  1002: {size: 1.2, effects: [890, 892]}
)";

[[nodiscard]] MonsterSizeEffects Table() {
  auto table = MonsterSizeEffects::Parse(kTable);
  REQUIRE(table.has_value());
  return *table;
}

[[nodiscard]] Actor Monster(std::uint32_t option) {
  Actor actor;
  actor.id = kMonsterId;
  actor.kind = ActorKind::kMonster;
  actor.look.job = kPoring;
  actor.option = option;
  return actor;
}

[[nodiscard]] bool Wears(const World& world, std::uint32_t effect) {
  const Actor* actor = world.Find(kMonsterId);
  REQUIRE(actor != nullptr);
  return std::ranges::any_of(actor->effects,
                             [effect](const auto& played) { return played.id == effect; });
}

}  // namespace

TEST_CASE("Effets d'office : une créature qui paraît les reçoit tous", "[app][monster]") {
  const MonsterSizeEffects table = Table();
  World world;
  world.Spawn(Monster(0));
  DressMonsters(world, &table, 100);
  CHECK(Wears(world, kGreen));
  CHECK(Wears(world, kAura));
  // Une seconde passe ne les repose pas.
  const std::size_t worn = world.Find(kMonsterId)->effects.size();
  DressMonsters(world, &table, 200);
  CHECK(world.Find(kMonsterId)->effects.size() == worn);
}

TEST_CASE("Effets d'office : cachée elle les perd, reparue elle les reprend", "[app][monster]") {
  const MonsterSizeEffects table = Table();
  World world;
  world.Spawn(Monster(0));
  DressMonsters(world, &table, 100);
  world.Spawn(Monster(ro::sim::kOptionCloak));
  DressMonsters(world, &table, 200);
  CHECK_FALSE(Wears(world, kGreen));
  CHECK_FALSE(Wears(world, kAura));
  world.Spawn(Monster(0));
  DressMonsters(world, &table, 300);
  CHECK(Wears(world, kGreen));
}

TEST_CASE("Effets d'office : une réapparition dans la même salve est une apparition neuve",
          "[app][monster]") {
  // 🔴 RETIRÉE PUIS RÉANNONCÉE ENTRE DEUX PASSES, la créature arrive la liste
  // vide : c'est la liste qui le dit, pas une mémoire par identifiant.
  const MonsterSizeEffects table = Table();
  World world;
  world.Spawn(Monster(0));
  DressMonsters(world, &table, 100);
  REQUIRE(world.Remove(kMonsterId));
  world.Spawn(Monster(0));
  DressMonsters(world, &table, 200);
  CHECK(Wears(world, kAura));
}

TEST_CASE("Effets d'office : le plafond de la liste ne les chasse pas", "[app][monster]") {
  // 🔴 LE PLAFOND EST À NOUS, pas au client, qui les garde tant que l'acteur
  // vit. Un combat chargé chassait les auras, qui repartaient de zéro.
  const MonsterSizeEffects table = Table();
  World world;
  world.Spawn(Monster(0));
  DressMonsters(world, &table, 100);
  constexpr std::uint32_t kFirstHit = 5000;
  for (std::uint32_t hit = 0; hit < 2 * ro::sim::kEffectsKeptPerActor; ++hit) {
    REQUIRE(world.RecordEffect(kMonsterId, kFirstHit + hit, 200 + hit));
  }
  CHECK(world.Find(kMonsterId)->effects.size() == ro::sim::kEffectsKeptPerActor);
  CHECK(Wears(world, kGreen));
  CHECK(Wears(world, kAura));
  // Le plus récent des coups est là, le plus ancien est parti.
  CHECK(Wears(world, kFirstHit + 2 * ro::sim::kEffectsKeptPerActor - 1));
  CHECK_FALSE(Wears(world, kFirstHit));
}

TEST_CASE("Effets d'office : une classe absente, ou un joueur, n'en reçoit pas", "[app][monster]") {
  const MonsterSizeEffects table = Table();
  World world;
  Actor other = Monster(0);
  other.look.job = kPoring + 1;
  world.Spawn(other);
  Actor player = Monster(0);
  player.id = kMonsterId + 1;
  player.kind = ActorKind::kPlayer;
  world.Spawn(player);
  DressMonsters(world, &table, 100);
  CHECK(world.Find(kMonsterId)->effects.empty());
  CHECK(world.Find(kMonsterId + 1)->effects.empty());
  // Et sans table, rien.
  world.Spawn(Monster(0));
  DressMonsters(world, nullptr, 100);
  CHECK(world.Find(kMonsterId)->effects.empty());
}
