// Ce que la carte retient d'un script : les repères, les demandes de navigation,
// et les positions des membres de la guilde.

#include "ro/sim/map_viewpoints.h"

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/guild.h"
#include "ro/sim/navigation_request.h"

using ro::sim::GuildBook;
using ro::sim::GuildMemberCell;
using ro::sim::kEphemeralViewpointLifetimeMs;
using ro::sim::MapViewpoints;
using ro::sim::NavigationRequest;
using ro::sim::NavigationRequests;
using ro::sim::Viewpoint;

namespace {

constexpr std::uint32_t kNpcA = 110'000'001;
constexpr std::uint32_t kNpcB = 110'000'002;
constexpr std::uint32_t kPlacedMs = 5'000;

Viewpoint Mark(std::uint32_t npc_id, std::int32_t point_id, bool permanent, std::int16_t x = 10) {
  return Viewpoint{.npc_id = npc_id,
                   .point_id = point_id,
                   .cell_x = x,
                   .cell_y = 20,
                   .color_argb = ro::sim::OpaqueViewpointColor(0x00FF8000),
                   .permanent = permanent,
                   .placed_ms = kPlacedMs};
}

}  // namespace

TEST_CASE("Repères : la couleur du serveur devient opaque", "[sim][viewpoints]") {
  CHECK(ro::sim::OpaqueViewpointColor(0x00FF8000) == 0xFFFF8000U);
  // Un alpha que le serveur aurait laissé passer est écrasé, pas conservé.
  CHECK(ro::sim::OpaqueViewpointColor(0x12FF8000) == 0xFFFF8000U);
  CHECK(ro::sim::OpaqueViewpointColor(0) == 0xFF000000U);
}

TEST_CASE("Repères : deux PNJ peuvent employer le même numéro de point", "[sim][viewpoints]") {
  MapViewpoints marks;
  marks.Place(Mark(kNpcA, 1, true, 10));
  marks.Place(Mark(kNpcB, 1, true, 30));
  REQUIRE(marks.viewpoints().size() == 2);
  CHECK(marks.Find(kNpcA, 1)->cell_x == 10);
  CHECK(marks.Find(kNpcB, 1)->cell_x == 30);

  // Retirer celui d'un PNJ laisse celui de l'autre.
  CHECK(marks.Remove(kNpcA, 1));
  CHECK(marks.Find(kNpcA, 1) == nullptr);
  CHECK(marks.Find(kNpcB, 1) != nullptr);
  CHECK_FALSE(marks.Remove(kNpcA, 1));
}

TEST_CASE("Repères : reposer la même clé remplace le repère et garde sa place",
          "[sim][viewpoints]") {
  MapViewpoints marks;
  marks.Place(Mark(kNpcA, 1, true, 10));
  marks.Place(Mark(kNpcA, 2, true, 11));
  marks.Place(Mark(kNpcA, 1, false, 99));
  REQUIRE(marks.viewpoints().size() == 2);
  CHECK(marks.viewpoints()[0].point_id == 1);
  CHECK(marks.viewpoints()[0].cell_x == 99);
  CHECK_FALSE(marks.viewpoints()[0].permanent);
}

TEST_CASE("Repères : l'éphémère expire à quinze secondes, le permanent jamais",
          "[sim][viewpoints]") {
  static_assert(kEphemeralViewpointLifetimeMs == 15'000);
  MapViewpoints marks;
  marks.Place(Mark(kNpcA, 1, false));
  marks.Place(Mark(kNpcA, 2, true));

  CHECK(marks.PurgeExpired(kPlacedMs + kEphemeralViewpointLifetimeMs - 1) == 0);
  CHECK(marks.viewpoints().size() == 2);

  CHECK(marks.PurgeExpired(kPlacedMs + kEphemeralViewpointLifetimeMs) == 1);
  REQUIRE(marks.viewpoints().size() == 1);
  CHECK(marks.viewpoints()[0].point_id == 2);

  CHECK(marks.PurgeExpired(kPlacedMs + 10 * kEphemeralViewpointLifetimeMs) == 0);
  CHECK(marks.viewpoints().size() == 1);
}

TEST_CASE("Repères : l'expiration survit au tour de l'horloge", "[sim][viewpoints]") {
  MapViewpoints marks;
  Viewpoint mark = Mark(kNpcA, 1, false);
  mark.placed_ms = 0xFFFFFFFFU - 1000;
  marks.Place(mark);
  // 1000 ms plus tard l'horloge a fait le tour : 5 s écoulées, pas 49 jours.
  CHECK(marks.PurgeExpired(4000) == 0);
  CHECK(marks.PurgeExpired(0xFFFFFFFFU - 1000 + kEphemeralViewpointLifetimeMs) == 1);
}

TEST_CASE("Repères : Clear vide tout", "[sim][viewpoints]") {
  MapViewpoints marks;
  marks.Place(Mark(kNpcA, 1, true));
  marks.Place(Mark(kNpcA, 2, false));
  marks.Clear();
  CHECK(marks.empty());
}

TEST_CASE("Navigation : les demandes se prennent dans l'ordre d'arrivée", "[sim][navigation]") {
  NavigationRequests requests;
  CHECK_FALSE(requests.Take().has_value());

  requests.Push(NavigationRequest{.map = "prontera"});
  requests.Push(NavigationRequest{.map = "geffen"});
  CHECK(requests.pending() == 2);
  CHECK(requests.Take()->map == "prontera");
  CHECK(requests.Take()->map == "geffen");
  CHECK(requests.empty());
}

TEST_CASE("Navigation : au plafond, la plus ancienne demande cède la place", "[sim][navigation]") {
  NavigationRequests requests;
  for (std::size_t index = 0; index < ro::sim::kMaxPendingNavigationRequests + 3; ++index) {
    requests.Push(NavigationRequest{.cell_x = static_cast<std::uint16_t>(index)});
  }
  CHECK(requests.pending() == ro::sim::kMaxPendingNavigationRequests);
  CHECK(requests.Take()->cell_x == 3);
}

TEST_CASE("Guilde : la cellule d'un membre se pose, se remplace, se retire", "[sim][guild]") {
  GuildBook guild;
  constexpr std::uint32_t kMember = 2'000'010;
  CHECK(guild.FindMemberCell(kMember) == nullptr);

  guild.TellMemberCell(kMember, GuildMemberCell{.x = 100, .y = 120});
  REQUIRE(guild.FindMemberCell(kMember) != nullptr);
  CHECK(guild.FindMemberCell(kMember)->x == 100);

  guild.TellMemberCell(kMember, GuildMemberCell{.x = 101, .y = 121});
  CHECK(guild.member_cells().size() == 1);
  CHECK(guild.FindMemberCell(kMember)->y == 121);

  guild.TellMemberCell(kMember, std::nullopt);
  CHECK(guild.FindMemberCell(kMember) == nullptr);
}

TEST_CASE("Guilde : changer de carte ou quitter la guilde oublie les cellules", "[sim][guild]") {
  GuildBook guild;
  guild.TellMemberCell(1, GuildMemberCell{.x = 1, .y = 2});
  guild.TellMemberCell(2, GuildMemberCell{.x = 3, .y = 4});
  guild.ForgetMemberCells();
  CHECK(guild.member_cells().empty());

  guild.TellMemberCell(1, GuildMemberCell{.x = 1, .y = 2});
  guild.Leave();
  CHECK(guild.member_cells().empty());
}
