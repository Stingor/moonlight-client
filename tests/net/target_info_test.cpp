// Ce que le serveur dit de l'entité désignée, décodé.
//
// 🔴 CE BANC PORTE TROIS CHOSES QU'AUCUNE SESSION NE MONTRERAIT VITE : que le
// masque `known` laisse un champ NON POSÉ plutôt qu'à zéro, que la queue est
// bornée par ce que la trame contient ET par le plafond du serveur, et qu'une
// réponse qui nomme une autre entité ne s'applique pas. Les trois se ressemblent
// à l'écran : des nombres plausibles.

#include "ro/net/target_info.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/proto/map_packets.h"
#include "ro/sim/actor.h"
#include "ro/sim/world.h"

#include "support/wire_bytes.h"

using ro::net::ApplyTargetInfoFrame;
using ro::net::ApplyTargetStatusFrame;
using ro::net::Frame;
using ro::net::FrameOutcome;
using ro::sim::Actor;
using ro::sim::TargetKind;
using ro::sim::World;
using ro::test_support::PutU16;
using ro::test_support::PutU32;

namespace {

constexpr std::uint32_t kWolf = 2000123;
constexpr std::uint32_t kStranger = 2000999;

// L'instant de l'appelant, choisi loin de zéro pour qu'une échéance oubliée ne
// se confonde pas avec « sans échéance ».
constexpr std::uint32_t kNow = 500000;

// `ZC 0x0F2A`, bloc fixe de 34 octets.
[[nodiscard]] std::vector<std::uint8_t> InfoFrame(std::uint32_t gid, std::uint8_t known) {
  std::vector<std::uint8_t> bytes;
  PutU16(bytes, ro::proto::ZcMoonlightTargetInfo::kOpcode);
  PutU16(bytes, static_cast<std::uint16_t>(ro::proto::ZcMoonlightTargetInfo::kWireSize));
  PutU32(bytes, gid);
  bytes.push_back(0);      // status : ok
  bytes.push_back(known);  // known
  bytes.push_back(static_cast<std::uint8_t>(TargetKind::kMonster));
  PutU16(bytes, 42);    // level
  PutU32(bytes, 1200);  // hp
  PutU32(bytes, 5000);  // maxhp
  PutU32(bytes, 30);    // sp
  PutU32(bytes, 80);    // maxsp
  bytes.push_back(3);   // race
  bytes.push_back(2);   // element
  bytes.push_back(4);   // element_lv
  bytes.push_back(1);   // size
  bytes.push_back(1);   // boss
  return bytes;
}

// `ZC 0x0F2D` : bloc fixe de 10 octets, puis `announced` entrées de 10.
[[nodiscard]] std::vector<std::uint8_t> StatusFrame(std::uint32_t gid, std::uint8_t announced,
                                                    std::size_t written) {
  std::vector<std::uint8_t> bytes;
  PutU16(bytes, ro::proto::ZcMoonlightStatusList::kOpcode);
  PutU16(bytes, 0);  // longueur : le décodeur ne s'en sert pas
  PutU32(bytes, gid);
  bytes.push_back(0);  // status : ok
  bytes.push_back(announced);
  for (std::size_t rank = 0; rank < written; ++rank) {
    PutU16(bytes, static_cast<std::uint16_t>(100 + rank));
    PutU32(bytes, static_cast<std::uint32_t>(1000 * (rank + 1)));  // reste
    PutU32(bytes, 60000);                                          // total
  }
  // La longueur annoncée se corrige une fois la taille connue.
  const auto size = static_cast<std::uint16_t>(bytes.size());
  bytes[2] = static_cast<std::uint8_t>(size & 0xFF);
  bytes[3] = static_cast<std::uint8_t>(size >> 8);
  return bytes;
}

// Une entrée comme le serveur l'écrit : le RESTANT dans le champ du total aussi.
[[nodiscard]] std::vector<std::uint8_t> ServerStatusFrame(std::uint32_t gid, std::uint16_t effect,
                                                          std::uint32_t remaining_ms) {
  std::vector<std::uint8_t> bytes;
  PutU16(bytes, ro::proto::ZcMoonlightStatusList::kOpcode);
  PutU16(bytes, 0);
  PutU32(bytes, gid);
  bytes.push_back(0);  // status : ok
  bytes.push_back(1);  // une entrée
  PutU16(bytes, effect);
  PutU32(bytes, remaining_ms);
  PutU32(bytes, remaining_ms);
  const auto size = static_cast<std::uint16_t>(bytes.size());
  bytes[2] = static_cast<std::uint8_t>(size & 0xFF);
  bytes[3] = static_cast<std::uint8_t>(size >> 8);
  return bytes;
}

[[nodiscard]] Frame Framed(const std::vector<std::uint8_t>& bytes) {
  Frame frame;
  frame.opcode = static_cast<std::uint16_t>(bytes[0] | (bytes[1] << 8));
  frame.bytes = bytes;
  return frame;
}

[[nodiscard]] World Designating(std::uint32_t id) {
  World world;
  Actor actor;
  actor.id = id;
  world.Spawn(std::move(actor));
  REQUIRE(world.Select(id));
  return world;
}

}  // namespace

TEST_CASE("Cible : tout renseigné se pose en entier", "[net][cible]") {
  World world = Designating(kWolf);
  const auto bytes = InfoFrame(kWolf, 1 | 2 | 4 | 8);
  REQUIRE(ApplyTargetInfoFrame(Framed(bytes), world) == FrameOutcome::kApplied);

  const auto& told = world.target_detail();
  CHECK(told.id == kWolf);
  CHECK(told.reachable);
  CHECK(told.kind == TargetKind::kMonster);
  REQUIRE(told.level.has_value());
  CHECK(*told.level == 42);
  REQUIRE(told.health.has_value());
  CHECK(*told.health == 1200);
  CHECK(*told.max_health == 5000);
  REQUIRE(told.magic.has_value());
  CHECK(*told.magic == 30);
  REQUIRE(told.nature.has_value());
  CHECK(told.nature->race == 3);
  CHECK(told.nature->element_level == 4);
  // ⚠ LA CLASSE SE RECOPIE, elle ne se réduit pas à « est-ce un boss » : la
  // trame porte 1, et 1 est bien le boss — mais 2 serait un gardien.
  CHECK(told.nature->monster_class == ro::sim::kMonsterClassBoss);
}

TEST_CASE("Cible : un bit à zéro laisse le champ VIDE, pas à zéro", "[net][cible]") {
  // 🔴 LA PORTE PVP PASSE PAR LÀ. Sur un adversaire, le serveur envoie son
  // espèce et tait vie et magie — les octets valent zéro dans la trame. Les
  // poser quand même afficherait « 0 / 0 » : une mort qui n'a pas eu lieu.
  World world = Designating(kWolf);
  const auto bytes = InfoFrame(kWolf, 8);  // la nature seule
  REQUIRE(ApplyTargetInfoFrame(Framed(bytes), world) == FrameOutcome::kApplied);

  const auto& told = world.target_detail();
  CHECK_FALSE(told.health.has_value());
  CHECK_FALSE(told.max_health.has_value());
  CHECK_FALSE(told.magic.has_value());
  CHECK_FALSE(told.level.has_value());
  // Ce qui EST renseigné se pose, lui.
  REQUIRE(told.nature.has_value());
  CHECK(told.nature->race == 3);
}

TEST_CASE("Cible : le bit seize dit des PV en pourcentage", "[net][cible]") {
  World world = Designating(kWolf);
  REQUIRE(ApplyTargetInfoFrame(Framed(InfoFrame(kWolf, ro::net::kTargetKnowsHealth |
                                                           ro::net::kTargetKnowsHealthPercent)),
                               world) == FrameOutcome::kApplied);
  const auto& told = world.target_detail();
  REQUIRE(told.health.has_value());
  CHECK(told.health_in_percent);

  // Sans le bit, les mêmes octets sont des PV exacts.
  REQUIRE(ApplyTargetInfoFrame(Framed(InfoFrame(kWolf, ro::net::kTargetKnowsHealth)), world) ==
          FrameOutcome::kApplied);
  CHECK_FALSE(world.target_detail().health_in_percent);
}

TEST_CASE("Cible : une réponse pour une autre entité ne s'applique pas", "[net][cible]") {
  // Elle n'est pas illisible pour autant : elle est arrivée après un changement
  // de désignation, et la jeter est la bonne conduite.
  World world = Designating(kWolf);
  const auto bytes = InfoFrame(kStranger, 1 | 2 | 4 | 8);
  CHECK(ApplyTargetInfoFrame(Framed(bytes), world) == FrameOutcome::kApplied);
  CHECK(world.target_detail().id == 0);
  CHECK_FALSE(world.target_detail().health.has_value());
}

TEST_CASE("États d'une cible : le reste devient une échéance", "[net][cible]") {
  World world = Designating(kWolf);
  const auto bytes = StatusFrame(kWolf, 3, 3);
  REQUIRE(ApplyTargetStatusFrame(Framed(bytes), world, kNow) == FrameOutcome::kApplied);

  const auto& told = world.target_detail();
  CHECK(told.statuses_told);
  REQUIRE(told.statuses.size() == 3);
  CHECK(told.statuses[0].effect == 100);
  CHECK(told.statuses[0].expires_ms == kNow + 1000);
  CHECK(told.statuses[0].total_ms == 60000);
  CHECK(told.statuses[2].expires_ms == kNow + 3000);
}

TEST_CASE("États d'une cible : la durée de référence survit aux réponses suivantes",
          "[net][cible]") {
  // Le serveur écrit le restant dans le champ du total, et la cible se redemande
  // sans cesse. Sans mémoire, le total suivrait le restant et le voile de la
  // part écoulée resterait vide.
  constexpr std::uint16_t kBlessing = 10;
  constexpr std::uint32_t kFirstLeftMs = 10000;
  constexpr std::uint32_t kElapsedMs = 4000;
  World world = Designating(kWolf);
  REQUIRE(ApplyTargetStatusFrame(Framed(ServerStatusFrame(kWolf, kBlessing, kFirstLeftMs)), world,
                                 kNow) == FrameOutcome::kApplied);
  REQUIRE(
      ApplyTargetStatusFrame(Framed(ServerStatusFrame(kWolf, kBlessing, kFirstLeftMs - kElapsedMs)),
                             world, kNow + kElapsedMs) == FrameOutcome::kApplied);

  const auto& told = world.target_detail();
  REQUIRE(told.statuses.size() == 1);
  CHECK(told.statuses[0].total_ms == kFirstLeftMs);
  CHECK(told.statuses[0].expires_ms == kNow + kFirstLeftMs);
}

TEST_CASE("États d'une cible : un compte qui ment ne fait pas lire au-delà", "[net][cible]") {
  // 🔴 LE COMPTE EST CRU, MAIS PAS SUR PAROLE. Une trame tronquée en annonce
  // vingt et n'en porte que deux : on s'arrête sur ce que le tampon contient.
  World world = Designating(kWolf);
  const auto bytes = StatusFrame(kWolf, 20, 2);
  REQUIRE(ApplyTargetStatusFrame(Framed(bytes), world, kNow) == FrameOutcome::kApplied);
  CHECK(world.target_detail().statuses.size() == 2);
}

TEST_CASE("États d'une cible : le plafond du serveur borne aussi", "[net][cible]") {
  // ⚠ ET LE PLAFOND EST LE SECOND GARDE, indépendant du premier : une trame qui
  // porte VRAIMENT plus que `kStatusListMostEntries` entrées ne dépasse pas.
  World world = Designating(kWolf);
  const std::size_t beyond = ro::net::kStatusListMostEntries + 5;
  const auto bytes = StatusFrame(kWolf, 255, beyond);
  REQUIRE(ApplyTargetStatusFrame(Framed(bytes), world, kNow) == FrameOutcome::kApplied);
  CHECK(world.target_detail().statuses.size() == ro::net::kStatusListMostEntries);
}

TEST_CASE("États d'une cible : un refus se compte, et ne pose aucune liste", "[net][cible]") {
  World world = Designating(kWolf);
  CHECK_FALSE(world.target_detail().told);
  auto refused = StatusFrame(kWolf, 0, 0);
  refused[8] = 1;  // status : hors de vue, ou refusée
  REQUIRE(ApplyTargetStatusFrame(Framed(refused), world, kNow) == FrameOutcome::kApplied);
  CHECK_FALSE(world.target_detail().statuses_told);
  CHECK(world.target_detail().statuses_refusals == 1U);

  // Une réponse de l'état, elle, dit qu'elle a répondu.
  REQUIRE(ApplyTargetInfoFrame(Framed(InfoFrame(kWolf, 1)), world) == FrameOutcome::kApplied);
  CHECK(world.target_detail().told);
  CHECK(world.target_detail().statuses_refusals == 1U);
}

TEST_CASE("États d'une cible : vide n'est pas inconnu", "[net][cible]") {
  World world = Designating(kWolf);

  // Rien demandé encore : on ne sait pas.
  CHECK_FALSE(world.target_detail().statuses_told);

  // Une liste VIDE est une réponse : « aucun état ».
  const auto empty = StatusFrame(kWolf, 0, 0);
  REQUIRE(ApplyTargetStatusFrame(Framed(empty), world, kNow) == FrameOutcome::kApplied);
  CHECK(world.target_detail().statuses_told);
  CHECK(world.target_detail().statuses.empty());
}

TEST_CASE("Cible : les états survivent à une réponse d'état de cible", "[net][cible]") {
  // ⚠ LES DEUX PAQUETS REMPLISSENT LE MÊME DOSSIER, et aucun n'efface l'autre :
  // sans cela la rangée d'états clignoterait à chaque redemande de la vie.
  World world = Designating(kWolf);
  const auto statuses = StatusFrame(kWolf, 2, 2);
  REQUIRE(ApplyTargetStatusFrame(Framed(statuses), world, kNow) == FrameOutcome::kApplied);
  REQUIRE(world.target_detail().statuses.size() == 2);

  const auto info = InfoFrame(kWolf, 1);
  REQUIRE(ApplyTargetInfoFrame(Framed(info), world) == FrameOutcome::kApplied);
  CHECK(world.target_detail().statuses.size() == 2);
  CHECK(world.target_detail().health.has_value());
}

TEST_CASE("Cible : changer de désignation vide le dossier", "[net][cible]") {
  // 🔴 SANS CELA, LES POINTS DE VIE DE L'ENTITÉ D'AVANT RESTERAIENT À L'ÉCRAN
  // sous le nom de la nouvelle, le temps qu'une réponse arrive — et les deux
  // nombres sont également plausibles.
  World world = Designating(kWolf);
  const auto bytes = InfoFrame(kWolf, 1 | 2 | 4 | 8);
  REQUIRE(ApplyTargetInfoFrame(Framed(bytes), world) == FrameOutcome::kApplied);
  REQUIRE(world.target_detail().health.has_value());

  Actor other;
  other.id = kStranger;
  world.Spawn(std::move(other));
  REQUIRE(world.Select(kStranger));

  CHECK(world.target_detail().id == 0);
  CHECK_FALSE(world.target_detail().health.has_value());
  CHECK_FALSE(world.target_detail().statuses_told);
}
