// LE JOUEUR NAÎT DE LA LISTE DES PERSONNAGES, ses états compris : le serveur de
// zone ne lui redit jamais son option en entrant, et sa monture ne se lit que là.

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/session.h"
#include "ro/proto/common_packets.h"
#include "ro/render/shown_job.h"
#include "ro/sim/actor.h"
#include "ro/sim/world.h"

#include "game/connected_session.h"
#include "world/actor_view.h"

namespace {

constexpr std::uint32_t kAccountId = 2'000'001;
constexpr std::int16_t kKnight = 7;
constexpr std::uint32_t kPecoKnight = 13;
constexpr std::uint16_t kPoisoned = 0x1;

ro::proto::CharacterInfo Knight(std::uint32_t option) {
  ro::proto::CharacterInfo knight;
  knight.job = kKnight;
  knight.name = "Galaad";
  knight.effectstate = static_cast<std::int32_t>(option);
  return knight;
}

ro::sim::World EnteredWorld(const ro::proto::CharacterInfo& chosen) {
  ro::net::LoginPhase::Account account;
  account.account_id = kAccountId;
  ro::sim::World world;
  moonclient::SpawnPlayer(moonclient::ComposeMapEntry(chosen, account, "gonryun", {}), world);
  return world;
}

}  // namespace

TEST_CASE("Entrée : le chevalier reconnecté sur son Peco entre monté", "[app][spawn_player]") {
  const ro::sim::World world = EnteredWorld(Knight(ro::render::kOptionPeco));
  const ro::sim::Actor* self = world.player();
  REQUIRE(self != nullptr);
  CHECK(self->option == ro::render::kOptionPeco);
  CHECK(moonclient::ShownJobOf(*self) == kPecoKnight);
  // Le métier exercé reste celui de la liste.
  CHECK(self->look.job == static_cast<std::uint16_t>(kKnight));
  // L'option arrive comme un changement : son bit s'allume en front montant.
  CHECK(self->option_risen == ro::render::kOptionPeco);
  CHECK(self->option_rise_count == 1);
}

TEST_CASE("Tablée : le chevalier monté s'assoit sur son Peco, comme il entre en jeu",
          "[app][spawn_player]") {
  // 🔴 SA RECETTE DE COULEURS EST RANGÉE SOUS LE CORPS MONTÉ : assis à pied, il
  // en prenait une autre.
  const ro::proto::CharacterInfo chosen = Knight(ro::render::kOptionPeco);
  const ro::sim::Actor seated = moonclient::SeatedActorOf(chosen, /*account_sex=*/1);
  CHECK(seated.option == ro::render::kOptionPeco);
  CHECK(moonclient::ShownJobOf(seated) == kPecoKnight);
  // Le même corps qu'à l'entrée en jeu.
  const ro::sim::World world = EnteredWorld(chosen);
  REQUIRE(world.player() != nullptr);
  CHECK(moonclient::ShownJobOf(seated) == moonclient::ShownJobOf(*world.player()));
  CHECK(moonclient::ShownJobOf(moonclient::SeatedActorOf(Knight(0), 1)) ==
        static_cast<std::uint32_t>(kKnight));
}

TEST_CASE("Entrée : le chevalier à pied entre à pied", "[app][spawn_player]") {
  const ro::sim::World world = EnteredWorld(Knight(0));
  const ro::sim::Actor* self = world.player();
  REQUIRE(self != nullptr);
  CHECK(self->option == 0);
  CHECK(moonclient::ShownJobOf(*self) == static_cast<std::uint32_t>(kKnight));
  CHECK(self->option_rise_count == 0);
}

TEST_CASE("Entrée : la santé et le corps de la liste passent au joueur", "[app][spawn_player]") {
  ro::proto::CharacterInfo knight = Knight(0);
  knight.healthstate = kPoisoned;
  const ro::sim::World world = EnteredWorld(knight);
  const ro::sim::Actor* self = world.player();
  REQUIRE(self != nullptr);
  CHECK(self->health_state == kPoisoned);
  CHECK(self->body_state == 0);
}
