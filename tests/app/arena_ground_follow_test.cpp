// LE RECENTRAGE DU SOL GÉNÉRÉ : la fenêtre suit le héros par pas entiers d'un
// morceau, avec une hystérésis d'un morceau — un aller-retour sur la frontière
// ne recentre rien —, et ce que devient un point du monde dessiné se lit sur le
// sol lui-même. Les fenêtres sont ici des nombres : la décision et la file se
// prouvent sans GPU ni sol.

#include "survivors/arena_ground_follow.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/error.h"
#include "ro/formats/gnd.h"
#include "ro/render/terrain_mesh.h"
#include "ro/sim/survivors_terrain.h"

using moonclient::GroundBuildThread;
using moonclient::GroundFollow;
using moonclient::GroundOffsetFromCentre;
using moonclient::GroundPreparedFate;
using moonclient::GroundPreparedFateOf;
using moonclient::GroundRecentreForced;
using moonclient::GroundWindowAhead;
using moonclient::GroundWindowAround;
using moonclient::GroundWindowFollowing;
using moonclient::GroundWindowOrigin;
using moonclient::GroundWorldShift;
using moonclient::kGroundAheadCells;
using moonclient::kGroundForceCells;
using moonclient::kGroundRecentreCells;
using moonclient::kGroundWindowCells;

namespace {

constexpr float kHalfWindow = static_cast<float>(kGroundWindowCells) / 2.0F;
constexpr auto kStep = static_cast<float>(kGroundRecentreCells);
// Un peu plus, un peu moins qu'une frontière.
constexpr float kNudgeCells = 0.25F;

const GroundWindowOrigin kStart =
    GroundWindowAround(ro::sim::kTerrainStartCell, ro::sim::kTerrainStartCell);

// La case du monde à `east`, `north` cases du centre de la fenêtre `origin`.
glm::vec2 FromCentre(GroundWindowOrigin origin, float east, float north) {
  return glm::vec2(static_cast<float>(origin.x_cells) + kHalfWindow + east,
                   static_cast<float>(origin.y_cells) + kHalfWindow + north);
}

// UNE « FENÊTRE » DE NOMBRES : son coin, et combien de fois on l'a bâtie.
struct CountedWindow {
  GroundWindowOrigin origin;
};

}  // namespace

TEST_CASE("Recentrage : la fenêtre ne bouge qu'au-delà d'un morceau du centre",
          "[app][survivors][ground]") {
  CHECK(GroundWindowFollowing(kStart, FromCentre(kStart, kStep - kNudgeCells, 0.0F)) == kStart);
  CHECK(GroundWindowFollowing(kStart, FromCentre(kStart, 0.0F, -kStep + kNudgeCells)) == kStart);
  const GroundWindowOrigin east =
      GroundWindowFollowing(kStart, FromCentre(kStart, kStep + kNudgeCells, 0.0F));
  CHECK(east.x_cells == kStart.x_cells + kGroundRecentreCells);
  CHECK(east.y_cells == kStart.y_cells);
  const GroundWindowOrigin south_west =
      GroundWindowFollowing(kStart, FromCentre(kStart, -kStep - kNudgeCells, -kStep - kNudgeCells));
  CHECK(south_west.x_cells == kStart.x_cells - kGroundRecentreCells);
  CHECK(south_west.y_cells == kStart.y_cells - kGroundRecentreCells);
}

TEST_CASE("Recentrage : le décalage est un multiple exact d'un morceau et ramène le héros",
          "[app][survivors][ground]") {
  // DES BONDS DE TOUTES LONGUEURS, une téléportation comprise : le coin avance
  // de morceaux entiers, et le héros finit à au plus un morceau du centre.
  for (const float jump : {33.0F, 63.9F, 64.1F, 100.0F, 1000.5F, -47.0F, -300.25F}) {
    const glm::vec2 hero = FromCentre(kStart, jump, -jump / 2.0F);
    const GroundWindowOrigin moved = GroundWindowFollowing(kStart, hero);
    CHECK((moved.x_cells - kStart.x_cells) % kGroundRecentreCells == 0);
    CHECK((moved.y_cells - kStart.y_cells) % kGroundRecentreCells == 0);
    const glm::vec2 offset = GroundOffsetFromCentre(moved, hero);
    CHECK(std::abs(offset.x) <= kStep);
    CHECK(std::abs(offset.y) <= kStep);
    CHECK(GroundWindowFollowing(moved, hero) == moved);
  }
}

TEST_CASE("Recentrage : un aller-retour sur la frontière ne recentre qu'une fois",
          "[app][survivors][ground]") {
  GroundWindowOrigin shown = kStart;
  std::size_t recentres = 0;
  // Le héros piétine de part et d'autre de la frontière est, cent fois.
  constexpr std::size_t kTrips = 100;
  const glm::vec2 border = FromCentre(kStart, kStep, 0.0F);
  for (std::size_t trip = 0; trip < kTrips; ++trip) {
    for (const float side : {-kNudgeCells, kNudgeCells}) {
      const GroundWindowOrigin next = GroundWindowFollowing(shown, border + glm::vec2(side, 0.0F));
      if (next != shown) ++recentres;
      shown = next;
    }
  }
  CHECK(recentres == 1);
  // Revenir d'un morceau en arrière ne suffit pas ; d'un peu plus, si.
  const float back = FromCentre(shown, -kStep + kNudgeCells, 0.0F).x;
  CHECK(GroundWindowFollowing(shown, glm::vec2(back, FromCentre(shown, 0.0F, 0.0F).y)) == shown);
  const float further = FromCentre(shown, -kStep - kNudgeCells, 0.0F).x;
  CHECK(GroundWindowFollowing(shown, glm::vec2(further, FromCentre(shown, 0.0F, 0.0F).y)) ==
        kStart);
}

TEST_CASE("Recentrage : la fenêtre d'avance et la limite où rien n'attend plus",
          "[app][survivors][ground]") {
  const auto ahead = static_cast<float>(kGroundAheadCells);
  CHECK_FALSE(GroundWindowAhead(kStart, FromCentre(kStart, ahead - kNudgeCells, 0.0F)));
  const std::optional<GroundWindowOrigin> next =
      GroundWindowAhead(kStart, FromCentre(kStart, ahead + kNudgeCells, 0.0F));
  REQUIRE(next.has_value());
  CHECK(next->x_cells == kStart.x_cells + kGroundRecentreCells);
  CHECK(next->y_cells == kStart.y_cells);
  // La fenêtre d'avance est celle que le pas suivant demandera.
  CHECK(GroundWindowFollowing(kStart, FromCentre(kStart, kStep + kNudgeCells, 0.0F)) == *next);
  const auto force = static_cast<float>(kGroundForceCells);
  CHECK_FALSE(GroundRecentreForced(kStart, FromCentre(kStart, force - kNudgeCells, 0.0F)));
  CHECK(GroundRecentreForced(kStart, FromCentre(kStart, 0.0F, -force - kNudgeCells)));
}

TEST_CASE("Recentrage : le décalage du monde est celui que le sol donne à une même case",
          "[app][survivors][ground]") {
  // UN SOL PLAT de la taille d'une fenêtre : la même case du monde, comptée
  // depuis deux coins, tombe en deux points dont l'écart est le décalage.
  ro::formats::GndParts parts{.width = moonclient::kGroundWindowCubes,
                              .height = moonclient::kGroundWindowCubes};
  parts.cubes.resize(static_cast<std::size_t>(parts.width) *
                     static_cast<std::size_t>(parts.height));
  auto gnd = ro::formats::GndMap::Assemble(parts);
  REQUIRE(gnd.has_value());
  const GroundWindowOrigin from = kStart;
  const GroundWindowOrigin to{.x_cells = kStart.x_cells + kGroundRecentreCells,
                              .y_cells = kStart.y_cells - kGroundRecentreCells};
  const glm::vec3 shift = GroundWorldShift(from, to);
  const glm::vec2 cell = FromCentre(kStart, 7.5F, -3.25F);
  const auto world_of = [&gnd, &cell](GroundWindowOrigin origin) {
    const std::array<float, 3> point =
        ro::render::CellPositionAt(*gnd, cell.x - static_cast<float>(origin.x_cells),
                                   cell.y - static_cast<float>(origin.y_cells));
    return glm::vec3(point[0], point[1], point[2]);
  };
  const glm::vec3 moved = world_of(to) - world_of(from);
  CHECK(moved.x == shift.x);
  CHECK(moved.y == shift.y);
  CHECK(moved.z == shift.z);
  CHECK(std::fmod(std::abs(shift.x), ro::render::kWorldUnitsPerCell * kStep) == 0.0F);
}

TEST_CASE("Recentrage : la fenêtre préparée se garde, s'abandonne ou se monte",
          "[app][survivors][ground]") {
  const GroundWindowOrigin east{.x_cells = kStart.x_cells + kGroundRecentreCells,
                                .y_cells = kStart.y_cells};
  const auto fate = [&east](glm::vec2 hero, bool uploaded, bool shifts_without_trace) {
    return GroundPreparedFateOf(kStart, east, hero, uploaded, shifts_without_trace);
  };
  const auto ahead = static_cast<float>(kGroundAheadCells);
  // La fenêtre d'avance attend le héros ; s'il repart, elle s'abandonne.
  CHECK(fate(FromCentre(kStart, ahead + 1.0F, 0.0F), true, true) == GroundPreparedFate::kKeep);
  CHECK(fate(FromCentre(kStart, -1.0F, 0.0F), true, true) == GroundPreparedFate::kDrop);
  // L'HYSTÉRÉSIS DE L'AVANCE : revenue en deçà de la moitié du chemin, elle sert
  // encore tant que le héros reste à l'est du centre ; elle cède quand il fait
  // la moitié du chemin vers le nord, où l'avance est une autre fenêtre.
  CHECK(fate(FromCentre(kStart, ahead - 1.0F, 0.0F), true, true) == GroundPreparedFate::kKeep);
  CHECK(fate(FromCentre(kStart, ahead - 1.0F, ahead - 1.0F), true, true) ==
        GroundPreparedFate::kKeep);
  CHECK(fate(FromCentre(kStart, ahead - 1.0F, ahead + 1.0F), true, true) ==
        GroundPreparedFate::kDrop);
  // La frontière passée, elle se monte une fois téléversée…
  const glm::vec2 beyond = FromCentre(kStart, kStep + 1.0F, 0.0F);
  CHECK(fate(beyond, false, true) == GroundPreparedFate::kKeep);
  CHECK(fate(beyond, true, true) == GroundPreparedFate::kMount);
  // … et attend des effets qui laisseraient une trace, jusqu'à la limite.
  CHECK(fate(beyond, true, false) == GroundPreparedFate::kKeep);
  const glm::vec2 edge =
      FromCentre(kStart, static_cast<float>(kGroundForceCells) + kNudgeCells, 0.0F);
  CHECK(fate(edge, true, false) == GroundPreparedFate::kMount);
  // Un pas en diagonale demande une autre fenêtre : celle de l'est s'abandonne.
  CHECK(fate(FromCentre(kStart, kStep + 1.0F, kStep + 1.0F), true, true) ==
        GroundPreparedFate::kDrop);
}

namespace {

// LA SCÈNE DES ESSAIS, réduite à ses décisions et dans l'ordre de
// `ArenaScene::FollowHero` : la fenêtre préparée qui ne sert plus s'abandonne,
// la suivante se demande quand rien ne se prépare, puis la préparée — téléversée
// d'emblée — se garde, s'abandonne ou se monte.
struct FollowedWalk {
  GroundWindowOrigin shown = kStart;
  std::optional<GroundFollow<CountedWindow>::Built> prepared;
  std::size_t mounts = 0;
  std::size_t dropped = 0;
  float most_offset = 0.0F;
  // Le plus grand écart au centre, sur l'un ou l'autre axe et dans les deux
  // sens.
  float most_reach = 0.0F;
  std::optional<float> first_mount_at;

  void Frame(GroundFollow<CountedWindow>& follow, glm::vec2 hero, bool shifts_without_trace,
             float walked) {
    if (prepared.has_value() &&
        GroundPreparedFateOf(shown, prepared->origin, hero, /*uploaded=*/false,
                             shifts_without_trace) == GroundPreparedFate::kDrop) {
      ++dropped;
      prepared.reset();
    }
    if (!prepared.has_value()) {
      prepared = follow.Next(shown, hero, GroundRecentreForced(shown, hero));
    }
    if (prepared.has_value()) {
      switch (GroundPreparedFateOf(shown, prepared->origin, hero, /*uploaded=*/true,
                                   shifts_without_trace)) {
        case GroundPreparedFate::kKeep:
          break;
        case GroundPreparedFate::kDrop:
          ++dropped;
          prepared.reset();
          break;
        case GroundPreparedFate::kMount:
          CHECK(prepared->window.origin == prepared->origin);
          shown = prepared->origin;
          prepared.reset();
          ++mounts;
          if (!first_mount_at.has_value()) first_mount_at = walked;
          break;
      }
    }
    const glm::vec2 offset = GroundOffsetFromCentre(shown, hero);
    most_offset = std::max(most_offset, offset.x);
    most_reach = std::max({most_reach, std::abs(offset.x), std::abs(offset.y)});
  }
};

// Un suivi sur ce fil, qui compte les fenêtres bâties.
GroundFollow<CountedWindow> CountingFollow(std::vector<GroundWindowOrigin>& built) {
  return GroundFollow<CountedWindow>(
      [&built](GroundWindowOrigin origin) -> ro::core::Result<CountedWindow> {
        built.push_back(origin);
        return CountedWindow{.origin = origin};
      },
      GroundBuildThread::kCaller);
}

}  // namespace

TEST_CASE("Recentrage : le suivi monte une fenêtre par morceau marché, et attend les effets",
          "[app][survivors][ground]") {
  std::vector<GroundWindowOrigin> built;
  GroundFollow<CountedWindow> follow(
      [&built](GroundWindowOrigin origin) -> ro::core::Result<CountedWindow> {
        built.push_back(origin);
        return CountedWindow{.origin = origin};
      },
      GroundBuildThread::kCaller);
  // TROIS MILLE CASES VERS L'EST, d'un quart de case par image.
  constexpr float kWalkCells = 3000.0F;
  const glm::vec2 start = FromCentre(kStart, 0.0F, 0.0F);
  FollowedWalk walk;
  for (float walked = 0.0F; walked <= kWalkCells; walked += kNudgeCells) {
    walk.Frame(follow, start + glm::vec2(walked, 0.0F), /*shifts_without_trace=*/true, walked);
  }
  CHECK(walk.mounts == static_cast<std::size_t>(kWalkCells / kStep));
  CHECK(walk.dropped == 0);
  CHECK(walk.shown.y_cells == kStart.y_cells);
  CHECK(walk.most_offset <= kStep + kNudgeCells);
  // CHAQUE FENÊTRE NE SE BÂTIT QU'UNE FOIS : celle d'avance est celle montée,
  // et la dernière attend le héros qui s'est arrêté.
  CHECK(built.size() == walk.mounts + 1);

  // DES EFFETS QUI LAISSERAIENT UNE TRACE : le recentrage attend, jusqu'à la
  // limite où le bord approche.
  GroundFollow<CountedWindow> patient(
      [](GroundWindowOrigin origin) -> ro::core::Result<CountedWindow> {
        return CountedWindow{.origin = origin};
      },
      GroundBuildThread::kCaller);
  FollowedWalk waiting;
  for (float walked = 0.0F; walked <= kHalfWindow && waiting.mounts == 0; walked += kNudgeCells) {
    waiting.Frame(patient, start + glm::vec2(walked, 0.0F), /*shifts_without_trace=*/false, walked);
  }
  REQUIRE(waiting.first_mount_at.has_value());
  CHECK(*waiting.first_mount_at > static_cast<float>(kGroundForceCells));
  CHECK(*waiting.first_mount_at <= static_cast<float>(kGroundForceCells) + kNudgeCells);
}

TEST_CASE("Recentrage : après une avance en diagonale, la fenêtre suit le héros parti à l'est",
          "[app][survivors][ground]") {
  std::vector<GroundWindowOrigin> built;
  GroundFollow<CountedWindow> follow = CountingFollow(built);
  // VERS LE NORD-EST jusqu'à (+20, +20), d'où l'avance au nord-est ; puis
  // plein est à +20 cases au nord, où la fenêtre voulue n'est plus celle-là.
  constexpr float kDiagonalCells = 20.0F;
  constexpr float kEastwardCells = 310.0F;
  const glm::vec2 start = FromCentre(kStart, 0.0F, 0.0F);
  FollowedWalk walk;
  for (float walked = 0.0F; walked <= kDiagonalCells; walked += kNudgeCells) {
    walk.Frame(follow, start + glm::vec2(walked, walked), /*shifts_without_trace=*/true, walked);
  }
  REQUIRE(walk.prepared.has_value());
  CHECK(walk.prepared->origin.y_cells == kStart.y_cells + kGroundRecentreCells);
  for (float walked = 0.0F; walked <= kEastwardCells; walked += kNudgeCells) {
    walk.Frame(follow, start + glm::vec2(kDiagonalCells + walked, kDiagonalCells),
               /*shifts_without_trace=*/true, walked);
  }
  CHECK(walk.shown.y_cells == kStart.y_cells);
  CHECK(walk.mounts == static_cast<std::size_t>((kDiagonalCells + kEastwardCells) / kStep));
  CHECK(walk.most_reach <= kStep + kNudgeCells);
  CHECK(walk.most_reach <= static_cast<float>(kGroundForceCells));
}

TEST_CASE("Recentrage : un bond au-delà de deux morceaux remplace l'avance prête",
          "[app][survivors][ground]") {
  std::vector<GroundWindowOrigin> built;
  GroundFollow<CountedWindow> follow = CountingFollow(built);
  const auto ahead = static_cast<float>(kGroundAheadCells);
  FollowedWalk walk;
  walk.Frame(follow, FromCentre(kStart, ahead + 1.0F, 0.0F), /*shifts_without_trace=*/true, 0.0F);
  REQUIRE(walk.prepared.has_value());
  // LE BOND, une téléportation : la fenêtre voulue est à deux morceaux, et
  // l'avance d'un morceau ne la remplace pas.
  constexpr float kJumpCells = 70.0F;
  const glm::vec2 landed = FromCentre(kStart, kJumpCells, 0.0F);
  walk.Frame(follow, landed, /*shifts_without_trace=*/true, kJumpCells);
  CHECK(walk.shown.x_cells == kStart.x_cells + 2 * kGroundRecentreCells);
  CHECK(std::abs(GroundOffsetFromCentre(walk.shown, landed).x) <= kStep);
  CHECK(walk.dropped == 1);
}

TEST_CASE("Recentrage : piétiner sur la ligne de l'avance ne la rebâtit pas",
          "[app][survivors][ground]") {
  std::vector<GroundWindowOrigin> built;
  GroundFollow<CountedWindow> follow = CountingFollow(built);
  const glm::vec2 line = FromCentre(kStart, static_cast<float>(kGroundAheadCells), 0.0F);
  constexpr std::size_t kTrips = 100;
  FollowedWalk walk;
  for (std::size_t trip = 0; trip < kTrips; ++trip) {
    for (const float side : {kNudgeCells, -kNudgeCells}) {
      walk.Frame(follow, line + glm::vec2(side, 0.0F), /*shifts_without_trace=*/true, 0.0F);
    }
  }
  CHECK(built.size() == 1);
  CHECK(walk.dropped == 0);
  CHECK(walk.mounts == 0);
}

TEST_CASE("Recentrage : sur le fil de travail, la fenêtre arrive et le suivi l'attend au bord",
          "[app][survivors][ground]") {
  GroundFollow<CountedWindow> follow(
      [](GroundWindowOrigin origin) -> ro::core::Result<CountedWindow> {
        return CountedWindow{.origin = origin};
      },
      GroundBuildThread::kWorker);
  // Au-delà de la limite, le suivi attend la fenêtre : elle arrive à coup sûr.
  const glm::vec2 hero =
      FromCentre(kStart, static_cast<float>(kGroundForceCells) + kNudgeCells, 0.0F);
  const auto built = follow.Next(kStart, hero, /*wait=*/true);
  REQUIRE(built.has_value());
  CHECK(built->origin.x_cells == kStart.x_cells + kGroundRecentreCells);
  CHECK(built->window.origin == built->origin);
}

TEST_CASE("Recentrage : une fenêtre refusée ne se redemande pas, la montrée reste",
          "[app][survivors][ground]") {
  std::size_t asked = 0;
  GroundFollow<CountedWindow> follow(
      [&asked](GroundWindowOrigin /*origin*/) -> ro::core::Result<CountedWindow> {
        ++asked;
        return ro::core::Fail(ro::core::ErrorCode::kInvalidArgument, "refusée");
      },
      GroundBuildThread::kCaller);
  const glm::vec2 far = FromCentre(kStart, static_cast<float>(kGroundForceCells) + 1.0F, 0.0F);
  constexpr std::size_t kFrames = 10;
  for (std::size_t frame = 0; frame < kFrames; ++frame) {
    CHECK_FALSE(follow.Next(kStart, far, /*wait=*/true));
  }
  CHECK(asked == 1);
}

TEST_CASE("Recentrage : un départ garde l'arène montée, ou la remonte",
          "[app][survivors][ground]") {
  constexpr std::uint16_t kPlains = 4;
  constexpr std::uint16_t kField = 3;
  constexpr std::uint64_t kSeed = 77;
  const GroundWindowOrigin away{.x_cells = kStart.x_cells + kGroundRecentreCells,
                                .y_cells = kStart.y_cells};
  // UNE CARTE se garde à toute graine, quelle que soit la fenêtre.
  const moonclient::MountedArena map{.arena_id = kField, .generated = false, .run_seed = kSeed};
  CHECK(moonclient::MountedArenaServes(map, kField, kSeed));
  CHECK(moonclient::MountedArenaServes(map, kField, kSeed + 1));
  CHECK_FALSE(moonclient::MountedArenaServes(map, kPlains, kSeed));
  // LE SOL GÉNÉRÉ se garde à la même graine et à la fenêtre du départ seules.
  moonclient::MountedArena ground{.arena_id = kPlains,
                                  .generated = true,
                                  .run_seed = kSeed,
                                  .start_origin = kStart,
                                  .shown_origin = kStart};
  CHECK(moonclient::MountedArenaServes(ground, kPlains, kSeed));
  CHECK_FALSE(moonclient::MountedArenaServes(ground, kPlains, kSeed + 1));
  CHECK_FALSE(moonclient::MountedArenaServes(ground, kField, kSeed));
  ground.shown_origin = away;
  CHECK_FALSE(moonclient::MountedArenaServes(ground, kPlains, kSeed));
}

TEST_CASE("Recentrage : la descente n'est bornée que sur le sol généré",
          "[app][survivors][ground]") {
  CHECK_FALSE(moonclient::GroundDescentReachOf(false).has_value());
  REQUIRE(moonclient::GroundDescentReachOf(true).has_value());
  CHECK(*moonclient::GroundDescentReachOf(true) ==
        static_cast<float>(moonclient::kGroundDescentReachCells));
  // La borne laisse le bord de la fenêtre du départ hors de l'image.
  CHECK(static_cast<float>(moonclient::kGroundDescentReachCells +
                           moonclient::kGroundEdgeMarginCells) <=
        kHalfWindow - static_cast<float>(ro::sim::kTerrainChunkCells) / 2.0F);
}
