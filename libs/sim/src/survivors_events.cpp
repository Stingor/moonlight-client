#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "ro/sim/packed_position.h"
#include "ro/sim/survivors_content.h"
#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"
#include "ro/sim/survivors_state.h"
#include "ro/sim/survivors_terrain.h"

#include "survivors_space.h"
#include "survivors_systems.h"

namespace ro::sim::survivors {

// LES NAISSANCES GROUPÉES : un événement fait naître, une fois, à son heure,
// un essaim, un raid, un anneau ou un groupe autour du héros, sous le plafond
// des monstres ; une élite naît à intervalle, hors plafond comme un boss.
// Seuls l'essaim et le raid tirent (un Below(4) par placement, le côté du
// cadre) ; l'élite tire son espèce et sa place comme une naissance ordinaire.
//
// Un événement que le plafond arrête reprend son SEUL reste aux pas suivants,
// comme un lot de vague : il attend qu'une place se libère, sans tirer tant
// que la horde est au plafond, puis place ses membres restants autour du héros
// tel qu'il est alors, l'essaim et le raid sur un côté tiré à nouveau : un
// essaim que le plafond arrête tire autant de Below(4) que de placements, de
// un à `count`. Les événements suivants attendent qu'il soit soldé. Un membre
// dont la case est murée est sauté : il compte comme placé.
//
// LA RUPTURE DES CUVES ne tire rien : chaque membre est la cuve la plus proche
// du héros qui n'a pas cédé, à portée, lue dans le terrain généré ; à distance
// égale, la plus au sud, puis la plus à l'ouest.

namespace {

// Les quatre côtés du cadre, dans l'ordre de son périmètre : nord, est, sud,
// ouest. Un essaim ou un raid tire l'un d'eux.
enum class FrameSide : std::uint8_t { kNorth, kEast, kSouth, kWest };
inline constexpr std::uint32_t kFrameSides = 4;
// Deux monstres voisins d'un essaim, d'un raid ou d'un groupe sont à une case.
inline constexpr std::int32_t kFileSpacingUnits = Cells(1);
// Un point d'événement se ramène à la case praticable la plus proche à un
// anneau au plus ; une case murée plus loin est sautée.
inline constexpr std::int32_t kEventSearchRing = 1;
inline constexpr std::uint8_t kQuarterTurn = kHeadingSteps / 4;

// Vrai quand la horde a atteint le plafond des monstres.
[[nodiscard]] bool AtFoeCap(const SurvivorsField& field) noexcept {
  return field.foes.size() >= field.rules->caps.most_foes;
}

// Le milieu du côté `side` du cadre centré sur `hero`, et le cap qui longe ce
// côté (le sens du périmètre), et celui qui s'en éloigne vers l'extérieur.
struct SideLine {
  FieldPoint middle;
  std::uint8_t along = 0;
  std::uint8_t outward = 0;
};

[[nodiscard]] SideLine LineOf(const SpawnFrame& frame, FieldPoint hero, FrameSide side) noexcept {
  // Les caps : 0 au nord, 16 à l'ouest, 32 au sud, 48 à l'est.
  switch (side) {
    case FrameSide::kNorth:
      return {hero + FieldPoint{0, frame.north_units}, 3 * kQuarterTurn, 0};
    case FrameSide::kEast:
      return {hero + FieldPoint{frame.half_width_units, 0}, 2 * kQuarterTurn, 3 * kQuarterTurn};
    case FrameSide::kSouth:
      return {hero - FieldPoint{0, frame.south_units}, kQuarterTurn, 2 * kQuarterTurn};
    case FrameSide::kWest:
      return {hero - FieldPoint{frame.half_width_units, 0}, 0, kQuarterTurn};
  }
  return {};
}

// Le côté du cadre que regarde le héros, par quart de tour autour de son cap :
// c'est là que naît un groupe, sans tirage.
[[nodiscard]] FrameSide FacedSide(std::uint8_t facing) noexcept {
  // Un quart de tour centré sur le nord (caps 56 à 7) donne le nord, puis
  // l'ouest, le sud, l'est dans le sens trigonométrique.
  const std::uint32_t quarter =
      (std::uint32_t{facing} + kQuarterTurn / 2) % kHeadingSteps / kQuarterTurn;
  constexpr std::array<FrameSide, kFrameSides> kByQuarter = {FrameSide::kNorth, FrameSide::kWest,
                                                             FrameSide::kSouth, FrameSide::kEast};
  return kByQuarter[quarter];
}

// Le décalage du k-ième de `count` monstres d'une file centrée, en sous-unités
// : ( 2k − (count − 1) ) × espacement / 2.
[[nodiscard]] std::int32_t CenteredOffset(std::size_t member, std::size_t count) noexcept {
  const std::int64_t twice =
      2 * static_cast<std::int64_t>(member) - static_cast<std::int64_t>(count) + 1;
  return static_cast<std::int32_t>(twice * kFileSpacingUnits / 2);
}

// L'ESSAIM et le RAID : un côté tiré du cadre ; l'essaim y naît en file
// centrée sur le milieu du côté, le long du côté, le raid en colonne qui part
// du milieu vers l'extérieur. Tous filent au cap qui va du milieu au héros, et
// quittent la partie au loin. Chaque fonction de placement part du membre
// `first`, s'arrête au plafond et rend le membre suivant à placer.
[[nodiscard]] std::size_t SpawnDrift(SurvivorsField& field, const EventRule& event,
                                     const FoeRule& rule, std::size_t first) {
  const FieldPoint hero = field.hero.position;
  const auto side = static_cast<FrameSide>(field.spawn_random.Below(kFrameSides));
  const SideLine line = LineOf(field.rules->spawn_frame, hero, side);
  const HatchOptions options{.drift_heading = HeadingOf(hero - line.middle, line.outward)};
  for (std::size_t member = first; member < event.count; ++member) {
    if (AtFoeCap(field)) return member;
    const FieldPoint offset =
        event.kind == EventKind::kSwarm
            ? TurnVector(line.along, CenteredOffset(member, event.count))
            : TurnVector(line.outward, static_cast<std::int32_t>(member) * kFileSpacingUnits);
    (void)HatchNear(field, rule, line.middle + offset, options);
  }
  return event.count;
}

// L'ANNEAU : `count` monstres autour du héros, le k-ième au cap `k × 64 /
// count`, à `radius_units` ; une case murée sans voisine praticable est sautée.
[[nodiscard]] std::size_t SpawnRing(SurvivorsField& field, const EventRule& event,
                                    const FoeRule& rule, std::size_t first) {
  const FieldPoint hero = field.hero.position;
  for (std::size_t member = first; member < event.count; ++member) {
    if (AtFoeCap(field)) return member;
    const auto heading =
        static_cast<std::uint8_t>(member * kHeadingSteps / event.count % kHeadingSteps);
    (void)HatchNear(field, rule, hero + TurnVector(heading, event.radius_units), {});
  }
  return event.count;
}

// LE GROUPE : des naissances ordinaires, serrées en cercle d'une case autour
// du milieu du côté du cadre que regarde le héros.
[[nodiscard]] std::size_t SpawnGroup(SurvivorsField& field, const EventRule& event,
                                     const FoeRule& rule, std::size_t first) {
  const FieldPoint hero = field.hero.position;
  const SideLine line = LineOf(field.rules->spawn_frame, hero, FacedSide(field.hero.facing));
  for (std::size_t member = first; member < event.count; ++member) {
    if (AtFoeCap(field)) return member;
    const auto heading =
        static_cast<std::uint8_t>(member * kHeadingSteps / event.count % kHeadingSteps);
    (void)HatchNear(field, rule, line.middle + TurnVector(heading, kFileSpacingUnits), {});
  }
  return event.count;
}

// L'ACIDE D'UNE CUVE ROMPUE : une flaque large de deux cases, six secondes.
constexpr FoeBehaviour kTankAcid{.puddle_damage = 4,
                                 .puddle_radius_units = CellTenths(20),
                                 .puddle_ms = 6000,
                                 .puddle_period_ms = 500};

// Une cuve du terrain : son coin sud-ouest et son milieu, en demi-cases.
struct Tank {
  CellPosition corner;
  std::int64_t centre_x2 = 0;
  std::int64_t centre_y2 = 0;
};

// La cuve la plus proche du héros qui n'a pas cédé, dont le milieu est à
// moins de `radius_units` ; rien sur une carte, ou sans cuve à portée.
[[nodiscard]] std::optional<Tank> NearestTank(const SurvivorsField& field, const EventRule& event) {
  const TerrainRule* terrain = field.ground.rule();
  if (field.ground.bounded() || terrain == nullptr) return std::nullopt;
  const TerrainPieceRule* tank_rule = nullptr;
  for (const TerrainPieceRule& piece : terrain->pieces) {
    if (piece.piece_id == event.piece_id) tank_rule = &piece;
  }
  if (tank_rule == nullptr) return std::nullopt;
  const std::int64_t hero_x2 = 2 * std::int64_t{CellOf(field.hero.position.x)} + 1;
  const std::int64_t hero_y2 = 2 * std::int64_t{CellOf(field.hero.position.y)} + 1;
  const std::int32_t reach = event.radius_units / Cells(1);
  const std::int64_t reach2 = std::int64_t{2} * reach;
  const std::int32_t hero_x = CellOf(field.hero.position.x);
  const std::int32_t hero_y = CellOf(field.hero.position.y);
  std::optional<Tank> best;
  std::int64_t best_distance = 0;
  for (std::int32_t chunk_y = TerrainChunkOf(hero_y - reach);
       chunk_y <= TerrainChunkOf(hero_y + reach); ++chunk_y) {
    for (std::int32_t chunk_x = TerrainChunkOf(hero_x - reach);
         chunk_x <= TerrainChunkOf(hero_x + reach); ++chunk_x) {
      const TerrainChunk chunk =
          GenerateTerrainChunk(*terrain, field.ground.run_seed(), chunk_x, chunk_y);
      for (const PlacedPiece& placed : chunk.pieces) {
        if (placed.piece_id != event.piece_id || placed.x < 0 || placed.y < 0) continue;
        const CellPosition corner{.x = static_cast<std::uint16_t>(placed.x),
                                  .y = static_cast<std::uint16_t>(placed.y)};
        const bool burst = std::ranges::any_of(field.burst_tanks, [&](const CellPosition& done) {
          return done.x == corner.x && done.y == corner.y;
        });
        if (burst) continue;
        const FootprintSize size = FootprintSizeOf(*tank_rule, placed.quarter);
        const Tank tank{.corner = corner,
                        .centre_x2 = 2 * std::int64_t{placed.x} + size.width,
                        .centre_y2 = 2 * std::int64_t{placed.y} + size.height};
        const std::int64_t dx = tank.centre_x2 - hero_x2;
        const std::int64_t dy = tank.centre_y2 - hero_y2;
        const std::int64_t distance = dx * dx + dy * dy;
        if (distance > reach2 * reach2) continue;
        const bool nearer = !best || distance < best_distance ||
                            (distance == best_distance &&
                             (tank.corner.y < best->corner.y ||
                              (tank.corner.y == best->corner.y && tank.corner.x < best->corner.x)));
        if (nearer) {
          best = tank;
          best_distance = distance;
        }
      }
    }
  }
  return best;
}

// LA RUPTURE DES CUVES : une cuve par membre ; le monstre naît à son pied, au
// plus près de son milieu, dans l'acide.
[[nodiscard]] std::size_t SpawnTanks(SurvivorsField& field, const EventRule& event,
                                     const FoeRule& rule, std::size_t first) {
  for (std::size_t member = first; member < event.count; ++member) {
    if (AtFoeCap(field)) return member;
    const std::optional<Tank> tank = NearestTank(field, event);
    if (!tank) return event.count;
    field.burst_tanks.push_back(tank->corner);
    const FieldPoint centre{.x = static_cast<std::int32_t>(tank->centre_x2 * Cells(1) / 2),
                            .y = static_cast<std::int32_t>(tank->centre_y2 * Cells(1) / 2)};
    LeavePuddle(field, kTankAcid, centre);
    (void)HatchNear(field, rule, centre, {});
  }
  return event.count;
}

// Les membres de `event` à partir de `first` ; rend le membre suivant à
// placer, `count` quand l'événement est soldé. Au plafond, rien ne se tire.
[[nodiscard]] std::size_t SpawnEvent(SurvivorsField& field, const EventRule& event,
                                     std::size_t first) {
  const FoeRule* rule = FindFoe(*field.rules, event.foe_id);
  if (rule == nullptr || first >= event.count) return event.count;
  if (AtFoeCap(field)) return first;
  switch (event.kind) {
    case EventKind::kSwarm:
    case EventKind::kRaid:
      return SpawnDrift(field, event, *rule, first);
    case EventKind::kRing:
      return SpawnRing(field, event, *rule, first);
    case EventKind::kGroup:
      return SpawnGroup(field, event, *rule, first);
    case EventKind::kTanks:
      return SpawnTanks(field, event, *rule, first);
  }
  return event.count;
}

// L'élite due : son espèce par les poids de la vague en vigueur, puis sa place
// comme une naissance ordinaire. Vrai quand elle est soldée (née, ou sans
// espèce à tirer) ; faux sans place, et elle réessaie au pas suivant.
[[nodiscard]] bool SpawnElite(SurvivorsField& field) {
  const SurvivorsRules& rules = *field.rules;
  const std::span<const WaveRule> waves = field.content.waves;
  if (waves.empty()) return true;
  const WaveRule& wave = waves[std::min(field.wave_index, waves.size() - 1)];
  if (wave.weights.size() != wave.foe_ids.size()) return true;
  const std::size_t pick = field.spawn_random.PickWeighted(wave.weights);
  if (pick >= wave.foe_ids.size()) return true;
  const FoeRule* rule = FindFoe(rules, wave.foe_ids[pick]);
  if (rule == nullptr) return true;
  const std::optional<FieldPoint> at = DrawSpawnPoint(field);
  if (!at) return false;
  (void)HatchFoe(field, *rule, *at, HatchOptions{.elite = true});
  return true;
}

}  // namespace

bool HatchNear(SurvivorsField& field, const FoeRule& rule, FieldPoint wanted,
               const HatchOptions& options) {
  if (field.foes.size() >= field.rules->caps.most_foes) return false;
  const std::optional<CellPosition> cell =
      NearestPassable(field.ground, CellOf(wanted.x), CellOf(wanted.y), kEventSearchRing);
  if (!cell) return false;
  (void)HatchFoe(field, rule, CellCenter(cell->x, cell->y), options);
  return true;
}

void SpawnEvents(SurvivorsField& field) {
  if (field.rules == nullptr) return;
  const SurvivorsRules& rules = *field.rules;
  const std::uint64_t elapsed_ms = std::uint64_t{field.step} * kSurvivorsStepMs;
  const std::span<const EventRule> events = field.content.events;
  while (field.next_event < events.size() && events[field.next_event].at_ms <= elapsed_ms) {
    const EventRule& event = events[field.next_event];
    const std::size_t next_member = SpawnEvent(field, event, field.event_members_placed);
    if (next_member < event.count) {
      // next_member < count <= 255 : le compte tient dans l'octet.
      field.event_members_placed = static_cast<std::uint8_t>(next_member);
      break;
    }
    field.event_members_placed = 0;
    ++field.next_event;
  }
  const EliteRule& elites = rules.elites;
  if (elites.interval_ms == 0 || field.next_elite_ms > elites.last_ms ||
      field.next_elite_ms > elapsed_ms) {
    return;
  }
  if (SpawnElite(field)) field.next_elite_ms += elites.interval_ms;
}

}  // namespace ro::sim::survivors
