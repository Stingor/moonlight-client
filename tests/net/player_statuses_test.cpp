// La durée d'un état du joueur, telle que le fil la donne.
//
// 🔴 CE QUI SE PROUVE ICI NE SE VOIT QU'À L'USURE. Une durée mal lue ne fait
// rien échouer : elle fait disparaître une icône dix secondes après l'avoir
// posée, ou la laisse pour toujours.

#include <cstdint>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/world_update.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/status_effects.h"
#include "ro/sim/world.h"

#include "support/net_frames.h"

using ro::net::ApplyFrame;
using ro::net::FrameOutcome;
using ro::sim::World;
using ro::test_support::FrameOf;

namespace {

constexpr std::uint32_t kPlayer = 2'000'001;
constexpr std::uint32_t kSomeoneElse = 2'000'002;
constexpr ro::sim::StatusEffectId kBlessing = 37;

[[nodiscard]] World WithPlayer() {
  World world;
  world.set_player_id(kPlayer);
  ro::sim::Actor self;
  self.id = kPlayer;
  world.Spawn(std::move(self));
  ro::sim::Actor other;
  other.id = kSomeoneElse;
  world.Spawn(std::move(other));
  return world;
}

// Un état qui commence, avec sa durée. `Total` et `Left` valent la même chose au
// début : c'est ce que le serveur écrit.
[[nodiscard]] std::vector<std::uint8_t> Starts(std::uint32_t actor, ro::sim::StatusEffectId effect,
                                               std::uint32_t duration_ms) {
  ro::proto::PacketStatusChange packet;
  packet.index = static_cast<std::int16_t>(effect);
  packet.AID = actor;
  packet.state = 1;
  packet.Total = duration_ms;
  packet.Left = duration_ms;
  return Encode(packet);
}

// Le même, avec la VALEUR que le serveur annonce — le `val1` du paquet.
[[nodiscard]] std::vector<std::uint8_t> StartsWithValue(std::uint32_t actor,
                                                        ro::sim::StatusEffectId effect,
                                                        std::int32_t value) {
  ro::proto::PacketStatusChange packet;
  packet.index = static_cast<std::int16_t>(effect);
  packet.AID = actor;
  packet.state = 1;
  packet.val1 = value;
  return Encode(packet);
}

[[nodiscard]] std::vector<std::uint8_t> Ends(std::uint32_t actor, ro::sim::StatusEffectId effect) {
  ro::proto::PacketStatusChangeEnd packet;
  packet.index = static_cast<std::int16_t>(effect);
  packet.AID = actor;
  packet.state = 0;
  return Encode(packet);
}

}  // namespace

TEST_CASE("États : la durée se garde, et l'échéance se compte sur NOTRE horloge", "[net][etats]") {
  World world = WithPlayer();
  CHECK(world.player_statuses().empty());

  constexpr std::uint32_t kNow = 10'000;
  constexpr std::uint32_t kDuration = 60'000;
  CHECK(ApplyFrame(FrameOf(Starts(kPlayer, kBlessing, kDuration)), world, kNow) ==
        FrameOutcome::kApplied);

  REQUIRE(world.player_statuses().size() == 1);
  CHECK(world.player_statuses()[0].effect == kBlessing);
  CHECK(world.player_statuses()[0].total_ms == kDuration);
  CHECK(world.player_statuses()[0].expires_ms == kNow + kDuration);

  // Et l'acteur garde sa présence, comme avant : les deux voies coexistent.
  REQUIRE(world.player() != nullptr);
  CHECK(world.player()->status_effects.contains(kBlessing));
}

TEST_CASE("États : « 9999 » n'est pas une durée", "[net][etats]") {
  // 🔴 LE SERVEUR L'ÉCRIT LUI-MÊME : « if (tick <= 0) tick = 9999; this is
  // indeed what official servers do ». Le prendre au mot ferait disparaître
  // tous les buffs permanents au bout de dix secondes.
  World world = WithPlayer();
  CHECK(ApplyFrame(FrameOf(Starts(kPlayer, kBlessing, ro::sim::kUnknownDurationMs)), world, 500) ==
        FrameOutcome::kApplied);

  REQUIRE(world.player_statuses().size() == 1);
  CHECK(world.player_statuses()[0].expires_ms == 0);
  CHECK(world.player_statuses()[0].total_ms == 0);

  // Et le balayage ne l'emporte pas, même très tard.
  world.ExpirePlayerStatuses(10'000'000);
  CHECK(world.player_statuses().size() == 1);
}

TEST_CASE("États : le même état qui revient RENOUVELLE le sien", "[net][etats]") {
  World world = WithPlayer();
  CHECK(ApplyFrame(FrameOf(Starts(kPlayer, kBlessing, 60'000)), world, 1'000) ==
        FrameOutcome::kApplied);
  // ⚠ UN BUFF RELANCÉ SE VOIT À SA DURÉE QUI REPART, pas à une deuxième icône.
  CHECK(ApplyFrame(FrameOf(Starts(kPlayer, kBlessing, 40'000)), world, 30'000) ==
        FrameOutcome::kApplied);

  REQUIRE(world.player_statuses().size() == 1);
  CHECK(world.player_statuses()[0].total_ms == 40'000);
  CHECK(world.player_statuses()[0].expires_ms == 70'000);
}

TEST_CASE("États : ce qui est échu se balaie, et rien d'autre", "[net][etats]") {
  World world = WithPlayer();
  CHECK(ApplyFrame(FrameOf(Starts(kPlayer, kBlessing, 5'000)), world, 1'000) ==
        FrameOutcome::kApplied);
  CHECK(ApplyFrame(FrameOf(Starts(kPlayer, 38, ro::sim::kUnknownDurationMs)), world, 1'000) ==
        FrameOutcome::kApplied);
  REQUIRE(world.player_statuses().size() == 2);

  world.ExpirePlayerStatuses(5'999);
  CHECK(world.player_statuses().size() == 2);

  // ⚠ SANS ÉCHÉANCE N'EST PAS ÉCHU : le permanent survit au balayage qui
  // emporte l'autre.
  world.ExpirePlayerStatuses(6'000);
  REQUIRE(world.player_statuses().size() == 1);
  CHECK(world.player_statuses()[0].effect == 38);
}

TEST_CASE("États : la fin annoncée retire l'entrée", "[net][etats]") {
  World world = WithPlayer();
  CHECK(ApplyFrame(FrameOf(Starts(kPlayer, kBlessing, 60'000)), world, 0) ==
        FrameOutcome::kApplied);
  CHECK(ApplyFrame(FrameOf(Ends(kPlayer, kBlessing)), world, 1'000) == FrameOutcome::kApplied);
  CHECK(world.player_statuses().empty());
  CHECK_FALSE(world.player()->status_effects.contains(kBlessing));
}

TEST_CASE("États : la durée d'un AUTRE acteur ne se garde pas", "[net][etats]") {
  // 🔴 LA DURÉE NE SERT QU'À UNE BARRE, ET LA BARRE EST LA NÔTRE. La tenir pour
  // chaque monstre à l'écran ferait vivre des centaines d'échéances que
  // personne ne lit ; sa PRÉSENCE, elle, décide de ce qu'on dessine autour de
  // lui, et elle se garde comme avant.
  World world = WithPlayer();
  CHECK(ApplyFrame(FrameOf(Starts(kSomeoneElse, kBlessing, 60'000)), world, 0) ==
        FrameOutcome::kApplied);

  CHECK(world.player_statuses().empty());
  REQUIRE(world.Find(kSomeoneElse) != nullptr);
  CHECK(world.Find(kSomeoneElse)->status_effects.contains(kBlessing));
}

TEST_CASE("États : la VALEUR arrive, et repart avec l'état", "[net][etats]") {
  // 🔴 ELLE ÉTAIT SUR LE FIL DEPUIS TOUJOURS ET NOUS LA JETIONS. Six effets se
  // calculent sur elle : Rolling Cutter montre un anneau par tour, la
  // malédiction du Devil assombrit par paliers. Sans elle, ces états-là
  // restaient muets, et rien ne le disait.
  constexpr ro::sim::StatusEffectId kRollingCutter = 339;
  World world = WithPlayer();
  constexpr std::uint32_t kNow = 10'000;
  CHECK(ApplyFrame(FrameOf(StartsWithValue(kPlayer, kRollingCutter, 4)), world, kNow) ==
        FrameOutcome::kApplied);
  REQUIRE(world.player() != nullptr);
  REQUIRE(world.player()->status_values.contains(kRollingCutter));
  CHECK(world.player()->status_values.at(kRollingCutter) == 4);

  // Le tour suivant remplace le précédent : c'est le même état qui change de
  // rang, sans jamais tomber.
  CHECK(ApplyFrame(FrameOf(StartsWithValue(kPlayer, kRollingCutter, 7)), world, kNow) ==
        FrameOutcome::kApplied);
  CHECK(world.player()->status_values.at(kRollingCutter) == 7);

  // ⚠ ET LA VALEUR PART AVEC L'ÉTAT. La garder ferait montrer l'anneau du tour
  // précédent au prochain Rolling Cutter, avant que le serveur ait parlé.
  CHECK(ApplyFrame(FrameOf(Ends(kPlayer, kRollingCutter)), world, kNow) == FrameOutcome::kApplied);
  CHECK_FALSE(world.player()->status_values.contains(kRollingCutter));
  CHECK(world.player()->status_values.empty());
}
