// CE QUE LA CARTE AUTORISE, ET LES ONZE BITS QUI LE DISENT.
//
// 🔴 CE FICHIER EXISTE POUR UNE SEULE RAISON : UN DÉCALAGE D'UN BIT NE SE VOIT
// NULLE PART AILLEURS. Le serveur compose son mot de drapeaux à la main, onze
// décalages écrits en clair dans `clif_map_property`, et sa propre structure
// nommée — `packet_maptypeproperty2` — n'est employée nulle part : aucun
// générateur ne peut vérifier l'ordre à notre place. Les recopier d'un rang
// échangerait « le curseur d'attaque paraît » contre « l'emblème de guilde
// paraît », et le jeu marcherait quand même — de travers, sur un seul écran, et
// seulement en zone PVP.
//
// D'où l'épreuve tenue ici : CHAQUE bit est levé SEUL, et l'on vérifie qu'il
// lève exactement un champ et pas un autre. Un décalage fait tomber deux cas à
// la fois, celui du bit déplacé et celui de son voisin.

#include "ro/sim/map_mode.h"

#include <array>
#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/world_update.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/world.h"

#include "support/net_frames.h"

using ro::net::ApplyFrame;
using ro::net::FrameOutcome;
using ro::sim::MapKind;
using ro::sim::MapMode;
using ro::sim::World;
using ro::test_support::FrameOf;

namespace {

// La trame telle que le serveur l'écrit : le type, puis le mot de drapeaux en
// quatre octets, du plus faible au plus fort.
[[nodiscard]] ro::proto::PacketMaptypeproperty2 Property(std::int16_t kind, std::uint32_t flags) {
  ro::proto::PacketMaptypeproperty2 packet;
  packet.type = kind;
  for (std::size_t index = 0; index < packet.flag.size(); ++index) {
    packet.flag[index] = static_cast<std::uint8_t>(flags >> (index * 8U) & 0xFFU);
  }
  return packet;
}

// Le mode qu'un monde retient après avoir reçu ce type et ces drapeaux.
[[nodiscard]] MapMode ModeAfter(std::int16_t kind, std::uint32_t flags) {
  World world;
  REQUIRE(ApplyFrame(FrameOf(Encode(Property(kind, flags))), world, 0) == FrameOutcome::kApplied);
  return world.map_mode();
}

// Le rang d'un bit, levé seul.
[[nodiscard]] constexpr std::uint32_t Only(std::uint32_t rank) { return 1U << rank; }

// Ce que `enum map_property` appelle une zone PVP libre.
constexpr std::int16_t kFreePvpZone = 1;
constexpr std::int16_t kNothing = 0;

}  // namespace

TEST_CASE("Propriété de carte : chaque bit lève son champ, et lui seul", "[net][pvp]") {
  // ⚠ L'ORDRE DE CETTE TABLE EST CELUI DU SERVEUR. Il se relit dans
  // `clif_map_property`, src/map/clif.cpp, une ligne par décalage.
  struct Case {
    std::uint32_t rank;
    bool MapMode::* champ;
  };
  constexpr std::array<Case, 11> kCases = {{
      {0, &MapMode::attackable_beyond_party},
      {1, &MapMode::attackable_beyond_guild},
      {2, &MapMode::guild_emblems_shown},
      {3, &MapMode::simple_effects_forced},
      {4, &MapMode::attackable_without_shift},
      {5, &MapMode::pvp_counter_shown},
      {6, &MapMode::party_changes_refused},
      {7, &MapMode::battlefield},
      {8, &MapMode::costumes_hidden},
      {9, &MapMode::cart_allowed},
      {10, &MapMode::sun_moon_star_miracle_allowed},
  }};

  for (const Case& entry : kCases) {
    const MapMode mode = ModeAfter(kNothing, Only(entry.rank));
    CHECK(mode.*(entry.champ));
    // Et AUCUN autre : c'est cette moitié-là qui attrape un décalage.
    for (const Case& other : kCases) {
      if (other.rank != entry.rank) {
        CHECK_FALSE(mode.*(other.champ));
      }
    }
  }
}

TEST_CASE("Propriété de carte : le type nomme l'espèce de zone", "[net][pvp]") {
  CHECK(ModeAfter(kNothing, 0).kind == MapKind::kNothing);
  CHECK(ModeAfter(kFreePvpZone, 0).kind == MapKind::kFreePvpZone);
  // La zone de siège, celle que le serveur envoie sous `mapdata_flag_gvg`.
  CHECK(ModeAfter(3, 0).kind == MapKind::kAgitZone);
}

TEST_CASE("Propriété de carte : un type inconnu se lit carte ordinaire", "[net][pvp]") {
  // ⚠ ET LES DRAPEAUX RESTENT LUS : ils ne dépendent pas du type. Jeter la
  // trame entière sur un type qu'on ne connaît pas éteindrait le ciblage sur
  // une carte où le serveur vient pourtant de l'ouvrir.
  const MapMode mode = ModeAfter(99, Only(0));
  CHECK(mode.kind == MapKind::kNothing);
  CHECK(mode.attackable_beyond_party);
}

TEST_CASE("Propriété de carte : ce que ce serveur envoie vraiment sur une carte pvp",
          "[net][pvp]") {
  // 🔴 LE CAS RÉEL, et il ne lève QUE trois bits. Sur `pvp_n_3-5`, qui porte le
  // mapflag `pvp` sans être ni champ de bataille ni zone GvG, `clif_map_property`
  // lève PARTY (mapflag pvp), DISABLE_LOCKON (mapdata_flag_vs) et COUNT_PK
  // (mapflag pvp) — et rien d'autre. GUILD reste à zéro.
  //
  // ⚠ CE QUI VEUT DIRE QUE LA GUILDE N'EST PAS PROTÉGÉE ICI, et que la trame ne
  // le dit pas : cette carte porte aussi `pvp_noguild`, si bien que le serveur
  // laisse frapper ses propres compagnons de guilde. Aucun bit ne le transporte.
  // Voir `MapMode` pour ce que cette déclaration ne dit pas.
  const MapMode mode = ModeAfter(kFreePvpZone, Only(0) | Only(4) | Only(5));

  CHECK(mode.kind == MapKind::kFreePvpZone);
  CHECK(mode.attackable_beyond_party);
  CHECK(mode.attackable_without_shift);
  CHECK(mode.pvp_counter_shown);
  CHECK_FALSE(mode.attackable_beyond_guild);
  CHECK_FALSE(mode.battlefield);
  CHECK(ro::sim::IsVersusMap(mode));
}

TEST_CASE("Propriété de carte : une carte ordinaire n'ouvre aucun ciblage", "[net][pvp]") {
  // Ce que le serveur envoie en QUITTANT une zone de combat : type zéro, et les
  // deux permissions levées parce qu'il les écrit à l'envers des autres.
  const MapMode mode = ModeAfter(kNothing, Only(9) | Only(10));

  CHECK_FALSE(ro::sim::IsVersusMap(mode));
  CHECK(mode.cart_allowed);
  CHECK(mode.sun_moon_star_miracle_allowed);
}

TEST_CASE("Propriété de carte : le changement de carte éteint le mode", "[net][pvp]") {
  // 🔴 LE SERVEUR NE REDIT LE MODE QU'APRÈS L'ENTRÉE, et il ne dit jamais
  // « éteins ce que tu avais » : sans cette remise à neuf, on viserait les
  // passants de la carte suivante le temps d'un aller-retour.
  World world;
  REQUIRE(ApplyFrame(FrameOf(Encode(Property(kFreePvpZone, Only(0) | Only(5)))), world, 0) ==
          FrameOutcome::kApplied);
  REQUIRE(ro::sim::IsVersusMap(world.map_mode()));

  world.EnterMap("prontera", 150, 150);

  CHECK_FALSE(ro::sim::IsVersusMap(world.map_mode()));
  CHECK_FALSE(world.map_mode().pvp_counter_shown);
  CHECK(world.map_mode().kind == MapKind::kNothing);
}

TEST_CASE("Type de carte : le champ de bataille laisse le groupe voir ses caches", "[net][pvp]") {
  // `ZC_NOTIFY_MAPPROPERTY2 0x01D6` dit le TYPE de la carte, une autre
  // énumération que la propriété ; le serveur ne l'envoie que sur un champ de
  // bataille. Le client l'éteint à chaque carte neuve.
  World world;
  ro::proto::ZcNotifyMapproperty2 packet;
  packet.type = static_cast<std::int16_t>(ro::sim::kMapTypeBattlefield);
  REQUIRE(ApplyFrame(FrameOf(Encode(packet)), world, 0) == FrameOutcome::kApplied);
  CHECK(world.map_type() == ro::sim::kMapTypeBattlefield);
  CHECK(ro::sim::PartySeesConcealedEffects(world.map_type()));

  world.EnterMap("prontera", 150, 150);

  CHECK(world.map_type() == 0);
  CHECK_FALSE(ro::sim::PartySeesConcealedEffects(world.map_type()));
}
