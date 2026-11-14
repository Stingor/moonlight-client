// LES EMBLÈMES DES ÉTIQUETTES : ce qui se demande, ce qui se garde.
//
// 🔴 CE BANC GARDE DEUX FAUTES MUETTES. Une image qui changerait d'adresse
// ressortirait avec la texture d'une autre ; une demande qui ne partirait pas
// laisserait l'étiquette sans emblème, sans erreur ni journal.

#include "guild/emblem_plates.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/image.h"
#include "ro/sim/actor.h"
#include "ro/sim/guild.h"
#include "ro/sim/world.h"

using moonclient::EmblemOnPlate;
using moonclient::EmblemPlates;
using ro::sim::Actor;
using ro::sim::ActorKind;
using ro::sim::GuildBelonging;
using ro::sim::GuildBook;
using ro::sim::World;

namespace {

constexpr std::int32_t kGuild = 77;
constexpr std::int32_t kOwnGuild = 42;
constexpr std::uint32_t kMe = 150'000;
constexpr std::uint32_t kOther = 150'001;

[[nodiscard]] ro::formats::Image Square() {
  ro::formats::Image image;
  image.width = 24;
  image.height = 24;
  image.pixels.assign(24U * 24U * 4U, 255);
  return image;
}

[[nodiscard]] Actor Player(std::uint32_t id, std::uint32_t guild_id) {
  Actor actor;
  actor.id = id;
  actor.kind = ActorKind::kPlayer;
  actor.guild_id = guild_id;
  return actor;
}

}  // namespace

TEST_CASE("Emblèmes des étiquettes : une absence se demande une fois", "[app][embleme]") {
  EmblemPlates plates;
  CHECK(plates.Demand(kGuild, 3) == nullptr);
  CHECK(plates.Demand(kGuild, 3) == nullptr);
  const std::vector<EmblemPlates::Key> wanted = plates.TakeWanted();
  REQUIRE(wanted.size() == 1);
  CHECK(wanted.front() == EmblemPlates::Key{kGuild, 3});
  // ⚠ SE PREND ET S'EFFACE : la file ne se relit pas deux fois.
  CHECK(plates.TakeWanted().empty());
}

TEST_CASE("Emblèmes des étiquettes : une version nulle ne demande rien", "[app][embleme]") {
  EmblemPlates plates;
  CHECK(plates.Demand(kGuild, 0) == nullptr);
  CHECK(plates.Demand(0, 3) == nullptr);
  CHECK(plates.TakeWanted().empty());
}

TEST_CASE("Emblèmes des étiquettes : rangée, l'image ne bouge plus", "[app][embleme]") {
  EmblemPlates plates;
  plates.Store(kGuild, 3, Square());
  const ro::formats::Image* first = plates.Demand(kGuild, 3);
  REQUIRE(first != nullptr);
  // Mille autres rangements, et une seconde image sous la même clé : aucune ne
  // déplace la première.
  for (std::int32_t guild = 1; guild <= 1000; ++guild) {
    plates.Store(guild + 1000, 1, Square());
  }
  ro::formats::Image other = Square();
  other.width = 12;
  plates.Store(kGuild, 3, std::move(other));
  CHECK(plates.Demand(kGuild, 3) == first);
  CHECK(first->width == 24);
  CHECK(plates.TakeWanted().empty());
}

TEST_CASE("Emblèmes des étiquettes : une autre version est une autre image", "[app][embleme]") {
  EmblemPlates plates;
  plates.Store(kGuild, 3, Square());
  CHECK(plates.Demand(kGuild, 4) == nullptr);
  REQUIRE(plates.TakeWanted().size() == 1);
}

TEST_CASE("Emblèmes des étiquettes : un emblème illisible ne se redemande plus", "[app][embleme]") {
  EmblemPlates plates;
  plates.Reject(kGuild, 3);
  CHECK(plates.Settled(kGuild, 3));
  CHECK(plates.Demand(kGuild, 3) == nullptr);
  CHECK(plates.TakeWanted().empty());
}

TEST_CASE("Emblème d'une étiquette : la guilde de l'apparition, la mienne au carnet",
          "[app][embleme]") {
  World world;
  world.set_player_id(kMe);
  // ⚠ MON ACTEUR PORTE LA GUILDE D'UN AUTRE : seul le carnet doit compter.
  world.Spawn(Player(kMe, static_cast<std::uint32_t>(kGuild)));
  world.Spawn(Player(kOther, static_cast<std::uint32_t>(kGuild)));
  GuildBook book;
  book.TellEmblemVersion(kGuild, 3);
  EmblemPlates plates;
  plates.Store(kGuild, 3, Square());

  CHECK(EmblemOnPlate(*world.Find(kOther), world, book, plates) == plates.Find(kGuild, 3));
  // HORS GUILDE, JE N'EN AI PAS, quoi que mon acteur porte.
  CHECK(EmblemOnPlate(*world.Find(kMe), world, book, plates) == nullptr);
  CHECK(plates.TakeWanted().empty());

  // ⚠ LE SERVEUR NE M'ANNONCE JAMAIS À MOI-MÊME : ma guilde vient du carnet.
  book.TellBelonging(GuildBelonging{.guild_id = kOwnGuild, .emblem_version = 5, .name = "Lune"});
  CHECK(EmblemOnPlate(*world.Find(kMe), world, book, plates) == nullptr);
  const std::vector<EmblemPlates::Key> wanted = plates.TakeWanted();
  REQUIRE(wanted.size() == 1);
  CHECK(wanted.front() == EmblemPlates::Key{kOwnGuild, 5});
}

TEST_CASE("Emblème d'une étiquette : sans version connue, rien ne se demande", "[app][embleme]") {
  World world;
  world.set_player_id(kMe);
  world.Spawn(Player(kOther, static_cast<std::uint32_t>(kGuild)));
  GuildBook book;
  EmblemPlates plates;
  CHECK(EmblemOnPlate(*world.Find(kOther), world, book, plates) == nullptr);
  CHECK(plates.TakeWanted().empty());
}
