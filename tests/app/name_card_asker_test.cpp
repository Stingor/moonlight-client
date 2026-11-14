// « QUI EST-CE ? » : QUAND LA CARTE DE NOM SE RÉCLAME.
//
// 🔴 CE BANC GARDE UNE PERTE QUE LE JEU NE SIGNALE PAS. Un changement de carte
// emporte la carte de nom de chaque acteur, et le serveur ne la repousse pas
// pour un joueur : si la demande ne repart pas au survol suivant, le guildien
// revient sans groupe, sans guilde, sans titre — et rien ne dit pourquoi.

#include "world/name_card_asker.h"

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/actor.h"
#include "ro/sim/world.h"

using moonclient::NameCardAsker;
using ro::sim::Actor;
using ro::sim::ActorKind;
using ro::sim::NameCard;
using ro::sim::World;

namespace {

constexpr std::uint32_t kMe = 150'000;
constexpr std::uint32_t kGuildmate = 150'042;
constexpr std::uint32_t kBeast = 110'000;
constexpr std::uint32_t kGuildId = 7;

[[nodiscard]] Actor Someone(std::uint32_t id, ActorKind kind) {
  Actor actor;
  actor.id = id;
  actor.kind = kind;
  actor.name = "G";
  actor.guild_id = kind == ActorKind::kPlayer ? kGuildId : 0;
  return actor;
}

// Une carte peuplée du joueur, d'un guildien et d'un monstre dont la carte
// est déjà là — le serveur pousse celle d'un monstre.
[[nodiscard]] World Populated() {
  World world;
  world.set_player_id(kMe);
  world.Spawn(Someone(kMe, ActorKind::kPlayer));
  world.Spawn(Someone(kGuildmate, ActorKind::kPlayer));
  world.Spawn(Someone(kBeast, ActorKind::kMonster));
  static_cast<void>(world.TellActorName(kBeast, "Poring", NameCard{}));
  return world;
}

}  // namespace

TEST_CASE("Carte de nom : un survol ininterrompu ne demande qu'une fois", "[app][noms]") {
  const World world = Populated();
  NameCardAsker asker;
  const Actor* guildmate = world.Find(kGuildmate);
  REQUIRE(guildmate != nullptr);
  CHECK(asker.AskedFor(guildmate) == kGuildmate);
  CHECK(asker.AskedFor(guildmate) == 0);
  CHECK(asker.AskedFor(guildmate) == 0);
}

TEST_CASE("Carte de nom : après un changement de carte, le guildien se redemande", "[app][noms]") {
  World world = Populated();
  NameCardAsker asker;
  REQUIRE(asker.AskedFor(world.Find(kGuildmate)) == kGuildmate);
  REQUIRE(world.TellActorName(kGuildmate, "G", NameCard{.banner = "Moonlight", .badge = "Membre"}));
  CHECK(asker.AskedFor(world.Find(kGuildmate)) == 0);
  // On survole un monstre, dont la carte est déjà là : rien ne part.
  CHECK(asker.AskedFor(world.Find(kBeast)) == 0);

  // 🔴 LE CHANGEMENT DE CARTE, puis le même guildien, sous le même identifiant,
  // SANS image vide entre les deux : il arrive avant qu'on ait bougé la souris.
  world.EnterMap("prontera", 150, 150);
  world.Spawn(Someone(kGuildmate, ActorKind::kPlayer));
  const Actor* back = world.Find(kGuildmate);
  REQUIRE(back != nullptr);
  REQUIRE_FALSE(back->card.has_value());
  CHECK(back->guild_id == kGuildId);
  CHECK(asker.AskedFor(back) == kGuildmate);
}

TEST_CASE("Carte de nom : l'arrivée sur une carte oublie la demande en cours", "[app][noms]") {
  // ⚠ LE CAS OÙ LA SOURIS N'A PAS BOUGÉ : le guildien était survolé au moment
  // du départ, et il l'est encore à l'arrivée. Seul l'oubli explicite rouvre
  // la question.
  World world = Populated();
  NameCardAsker asker;
  REQUIRE(asker.AskedFor(world.Find(kGuildmate)) == kGuildmate);
  world.EnterMap("prontera", 150, 150);
  world.Spawn(Someone(kGuildmate, ActorKind::kPlayer));
  asker.Forget();
  CHECK(asker.AskedFor(world.Find(kGuildmate)) == kGuildmate);
}

TEST_CASE("Carte de nom : une réponse perdue se redemande au survol suivant", "[app][noms]") {
  // Le serveur jette sans répondre une demande hors de portée : la question
  // doit pouvoir se reposer dès que la souris revient.
  const World world = Populated();
  NameCardAsker asker;
  REQUIRE(asker.AskedFor(world.Find(kGuildmate)) == kGuildmate);
  CHECK(asker.AskedFor(nullptr) == 0);
  CHECK(asker.AskedFor(world.Find(kGuildmate)) == kGuildmate);
}

TEST_CASE("Carte de nom : ni un butin ni une unité de compétence ne se demandent", "[app][noms]") {
  World world = Populated();
  world.Spawn(Someone(200'001, ActorKind::kItem));
  world.Spawn(Someone(200'002, ActorKind::kSkillUnit));
  NameCardAsker asker;
  CHECK(asker.AskedFor(world.Find(200'001)) == 0);
  CHECK(asker.AskedFor(world.Find(200'002)) == 0);
}
