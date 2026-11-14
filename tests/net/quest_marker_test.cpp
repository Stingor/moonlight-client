// LA MARQUE DE QUÊTE D'UN PNJ : 0x0446 note, par PNJ, la case, l'effet et la
// couleur, à part des acteurs, jusqu'à l'entrée sur une autre carte.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/world_update.h"
#include "ro/proto/packet_lengths.h"
#include "ro/proto/quest_marker_packets.h"
#include "ro/sim/actor.h"
#include "ro/sim/quest_markers.h"
#include "ro/sim/world.h"

#include "support/net_frames.h"

using ro::net::ApplyFrame;
using ro::net::FrameOutcome;
using ro::proto::QuestMarkerNotice;
using ro::sim::QuestMarker;
using ro::test_support::FrameOf;

namespace {

// Le PNJ d'une capture réelle à Payon, marqué de l'effet 0 en jaune.
constexpr std::uint32_t kPayonNpc = 3'011'809;
constexpr std::int16_t kPayonNpcX = 154;
constexpr std::int16_t kPayonNpcY = 101;
constexpr std::uint16_t kQuestEffect = 0;
constexpr std::uint16_t kJobEffect = 2;
constexpr std::uint16_t kYellow = 1;
constexpr std::uint16_t kGreen = 2;
constexpr std::uint32_t kNowMs = 5000;

[[nodiscard]] QuestMarkerNotice Notice(std::uint16_t effect, std::uint16_t color) {
  QuestMarkerNotice notice;
  notice.npc_id = kPayonNpc;
  notice.x = kPayonNpcX;
  notice.y = kPayonNpcY;
  notice.effect = effect;
  notice.color = color;
  return notice;
}

}  // namespace

TEST_CASE("Marque de quête : la trame a la longueur que le serveur déclare", "[net][quest]") {
  const std::optional<std::int16_t> declared = ro::proto::PacketLength(QuestMarkerNotice::kOpcode);
  REQUIRE(declared.has_value());
  CHECK(static_cast<std::size_t>(*declared) == QuestMarkerNotice::kWireSize);
  CHECK(Encode(Notice(kQuestEffect, kYellow)).size() == QuestMarkerNotice::kWireSize);
}

TEST_CASE("Marque de quête : les octets se relisent champ par champ", "[net][quest]") {
  // Le PNJ, l'effet et la couleur de la capture ; la case est quelconque.
  // 0446, PNJ 3011809, case (154, 101), effet 0, couleur 1 — petit-boutiste.
  const std::vector<std::uint8_t> captured = {0x46, 0x04, 0xE1, 0xF4, 0x2D, 0x00, 0x9A,
                                              0x00, 0x65, 0x00, 0x00, 0x00, 0x01, 0x00};
  QuestMarkerNotice read;
  REQUIRE(ro::proto::Decode(captured, read));
  CHECK(read.npc_id == kPayonNpc);
  CHECK(read.x == kPayonNpcX);
  CHECK(read.y == kPayonNpcY);
  CHECK(read.effect == kQuestEffect);
  CHECK(read.color == kYellow);
  // Une trame tronquée ne se lit pas.
  const std::vector<std::uint8_t> short_frame(captured.begin(), captured.end() - 2);
  CHECK_FALSE(ro::proto::Decode(short_frame, read));
}

TEST_CASE("Marque de quête : reçue avant le PNJ, elle se garde et l'attend", "[net][quest]") {
  // 🔴 AUCUN ACTEUR : le serveur envoie toutes les marques de la carte à
  // l'entrée, PNJ en vue ou non. Les refuser faute d'acteur les perdrait pour
  // de bon — il ne les redit pas.
  ro::sim::World world;
  CHECK(ApplyFrame(FrameOf(Encode(Notice(kQuestEffect, kYellow))), world, kNowMs) ==
        FrameOutcome::kApplied);
  const QuestMarker* shown = world.quest_markers().ShownFor(kPayonNpc);
  REQUIRE(shown != nullptr);
  CHECK(shown->cell_x == kPayonNpcX);
  CHECK(shown->cell_y == kPayonNpcY);
  CHECK(shown->effect == kQuestEffect);
  CHECK(shown->color == kYellow);
}

TEST_CASE("Marque de quête : une seconde annonce remplace la première", "[net][quest]") {
  ro::sim::World world;
  REQUIRE(ApplyFrame(FrameOf(Encode(Notice(kQuestEffect, kYellow))), world, kNowMs) ==
          FrameOutcome::kApplied);
  REQUIRE(ApplyFrame(FrameOf(Encode(Notice(kJobEffect, kGreen))), world, kNowMs) ==
          FrameOutcome::kApplied);
  CHECK(world.quest_markers().all().size() == 1);
  const QuestMarker* shown = world.quest_markers().ShownFor(kPayonNpc);
  REQUIRE(shown != nullptr);
  CHECK(shown->effect == kJobEffect);
  CHECK(shown->color == kGreen);
}

TEST_CASE("Marque de quête : 9999 la retire sans effacer l'entrée, ni sa case", "[net][quest]") {
  ro::sim::World world;
  REQUIRE(ApplyFrame(FrameOf(Encode(Notice(kQuestEffect, kYellow))), world, kNowMs) ==
          FrameOutcome::kApplied);
  // Le serveur retire avec la couleur nulle, mais c'est l'EFFET qui décide : la
  // couleur restée ne la ferait pas revenir.
  CHECK(ApplyFrame(FrameOf(Encode(Notice(ro::sim::kQuestMarkerRemovedEffect, kYellow))), world,
                   kNowMs) == FrameOutcome::kApplied);
  CHECK(world.quest_markers().ShownFor(kPayonNpc) == nullptr);
  REQUIRE(world.quest_markers().all().count(kPayonNpc) == 1);
  const QuestMarker& removed = world.quest_markers().all().at(kPayonNpc);
  CHECK_FALSE(removed.shown());
  // Plus de case : la minimap ne la situe plus.
  CHECK_FALSE(removed.placed());
  CHECK(removed.cell_x == ro::sim::kQuestMarkerNoCell);
  CHECK(removed.cell_y == ro::sim::kQuestMarkerNoCell);
}

TEST_CASE("Marque de quête : un PNJ qui sort de vue la retrouve en revenant", "[net][quest]") {
  ro::sim::World world;
  ro::sim::Actor npc;
  npc.id = kPayonNpc;
  npc.kind = ro::sim::ActorKind::kNonPlayer;
  npc.position = ro::sim::CellPosition{154, 101, 0};
  world.Spawn(npc);
  REQUIRE(ApplyFrame(FrameOf(Encode(Notice(kQuestEffect, kYellow))), world, kNowMs) ==
          FrameOutcome::kApplied);
  world.Remove(kPayonNpc);
  REQUIRE(world.Find(kPayonNpc) == nullptr);
  world.Spawn(npc);
  CHECK(world.quest_markers().ShownFor(kPayonNpc) != nullptr);
}

TEST_CASE("Marque de quête : l'entrée sur une carte les vide toutes", "[net][quest]") {
  ro::sim::World world;
  REQUIRE(ApplyFrame(FrameOf(Encode(Notice(kQuestEffect, kYellow))), world, kNowMs) ==
          FrameOutcome::kApplied);
  world.EnterMap("prontera", 150, 150);
  CHECK(world.quest_markers().all().empty());
  CHECK(world.quest_markers().ShownFor(kPayonNpc) == nullptr);
}
