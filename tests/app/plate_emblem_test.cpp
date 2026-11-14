// L'IMAGE DE TÊTE D'UNE ÉTIQUETTE : l'emblème de guilde, ou celui du clan.
//
// 🔴 CE BANC GARDE LA RÈGLE DU JOUEUR : une guilde fait toujours taire le clan,
// même sans emblème. Un clan qui percerait sous une guilde sans emblème se
// verrait sur l'étiquette sans que rien ne le signale.

#include "guild/plate_emblem.h"

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/image.h"
#include "ro/render/clan_emblem.h"
#include "ro/render/entity_names.h"
#include "ro/sim/actor.h"
#include "ro/sim/guild.h"
#include "ro/sim/status_effects.h"
#include "ro/sim/world.h"

using moonclient::ChoosePlateEmblem;
using moonclient::ClanIdOf;
using moonclient::EmblemPlates;
using moonclient::NamePlateEmblem;
using moonclient::PlateEmblemChoice;
using moonclient::PlateEmblemKind;
using ro::render::ClanEmblems;
using ro::render::EntityNamesLook;
using ro::sim::Actor;
using ro::sim::ActorKind;
using ro::sim::GuildBook;
using ro::sim::World;

namespace {

constexpr std::int32_t kGuild = 77;
constexpr std::int32_t kGuildVersion = 3;
constexpr std::int32_t kClan = 3;
constexpr std::int32_t kNoGuild = 0;
constexpr std::int32_t kNoClan = 0;
constexpr std::uint32_t kMe = 150'000;
constexpr std::uint32_t kOther = 150'001;
constexpr std::int32_t kEmblemSide = 24;

[[nodiscard]] ro::formats::Image Square() {
  ro::formats::Image image;
  image.width = kEmblemSide;
  image.height = kEmblemSide;
  image.pixels.assign(static_cast<std::size_t>(kEmblemSide * kEmblemSide) * 4U, 255);
  return image;
}

[[nodiscard]] Actor Player(std::uint32_t id, std::uint32_t guild_id, std::int32_t clan_id) {
  Actor actor;
  actor.id = id;
  actor.kind = ActorKind::kPlayer;
  actor.guild_id = guild_id;
  if (clan_id != kNoClan) {
    actor.status_effects.insert(ro::sim::kEfstClanInfo);
    actor.status_second_values.insert_or_assign(ro::sim::kEfstClanInfo, clan_id);
  }
  return actor;
}

[[nodiscard]] bool Same(PlateEmblemChoice got, PlateEmblemKind kind, std::int32_t owner_id) {
  return got.kind == kind && got.owner_id == owner_id;
}

}  // namespace

TEST_CASE("Image de tête : la guilde passe devant le clan", "[app][embleme]") {
  const EntityNamesLook look;
  CHECK(Same(ChoosePlateEmblem(kGuild, kClan, look), PlateEmblemKind::kGuild, kGuild));
  CHECK(Same(ChoosePlateEmblem(kNoGuild, kClan, look), PlateEmblemKind::kClan, kClan));
  CHECK(Same(ChoosePlateEmblem(kNoGuild, kNoClan, look), PlateEmblemKind::kNone, 0));
}

TEST_CASE("Image de tête : une guilde fait taire le clan, sa case décochée ou non",
          "[app][embleme]") {
  EntityNamesLook look;
  look.guild_emblem = false;
  CHECK(Same(ChoosePlateEmblem(kGuild, kClan, look), PlateEmblemKind::kNone, 0));
  // Sans guilde, la case de guilde ne compte pas.
  CHECK(Same(ChoosePlateEmblem(kNoGuild, kClan, look), PlateEmblemKind::kClan, kClan));
}

TEST_CASE("Image de tête : la case du clan ne commande que le clan", "[app][embleme]") {
  EntityNamesLook look;
  look.clan_emblem = false;
  CHECK(Same(ChoosePlateEmblem(kNoGuild, kClan, look), PlateEmblemKind::kNone, 0));
  CHECK(Same(ChoosePlateEmblem(kGuild, kClan, look), PlateEmblemKind::kGuild, kGuild));
}

TEST_CASE("Image de tête : le clan se lit sur l'état de clan", "[app][embleme]") {
  CHECK(ClanIdOf(Player(kOther, 0, kClan)) == kClan);
  CHECK(ClanIdOf(Player(kOther, 0, kNoClan)) == kNoClan);
  // ⚠ UNE VALEUR SANS ÉTAT NE COMPTE PAS : l'état levé emporte le clan.
  Actor lifted = Player(kOther, 0, kClan);
  lifted.status_effects.erase(ro::sim::kEfstClanInfo);
  CHECK(ClanIdOf(lifted) == kNoClan);
}

TEST_CASE("Image de tête : guilde avec emblème, guilde sans, clan, rien", "[app][embleme]") {
  World world;
  world.set_player_id(kMe);
  constexpr std::uint32_t kBareGuildMember = 150'002;
  constexpr std::uint32_t kClanMember = 150'003;
  constexpr std::uint32_t kLoner = 150'004;
  constexpr std::int32_t kBareGuild = 78;
  world.Spawn(Player(kOther, static_cast<std::uint32_t>(kGuild), kClan));
  world.Spawn(Player(kBareGuildMember, static_cast<std::uint32_t>(kBareGuild), kClan));
  world.Spawn(Player(kClanMember, 0, kClan));
  world.Spawn(Player(kLoner, 0, kNoClan));
  world.Spawn(Player(kMe, 0, kClan));
  GuildBook book;
  book.TellEmblemVersion(kGuild, kGuildVersion);
  EmblemPlates plates;
  plates.Store(kGuild, kGuildVersion, Square());
  ClanEmblems clans;
  clans.by_clan.emplace(kClan, Square());
  const EntityNamesLook look;

  const auto plate_of = [&](std::uint32_t id) {
    return NamePlateEmblem(*world.Find(id), world, book, &plates, clans, look);
  };
  CHECK(plate_of(kOther) == plates.Find(kGuild, kGuildVersion));
  // 🔴 SA GUILDE N'A PAS D'EMBLÈME, ET LE CLAN NE LE REMPLACE PAS.
  CHECK(plate_of(kBareGuildMember) == nullptr);
  CHECK(plate_of(kClanMember) == clans.Of(kClan));
  CHECK(plate_of(kLoner) == nullptr);
  // MOI, HORS GUILDE AU CARNET : mon clan.
  CHECK(plate_of(kMe) == clans.Of(kClan));
  // Un clan dont l'installation n'a pas l'image : rien.
  ClanEmblems none;
  CHECK(NamePlateEmblem(*world.Find(kClanMember), world, book, &plates, none, look) == nullptr);
}

TEST_CASE("Image de tête : le fichier d'un clan porte son numéro sur deux chiffres",
          "[app][embleme]") {
  CHECK(ro::render::ClanEmblemPath(kClan) ==
        "data\\texture\\유저인터페이스\\clan_system\\clan_emblem03.bmp");
}
