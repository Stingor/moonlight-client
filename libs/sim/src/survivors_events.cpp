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
#include "ro/sim/walk_field.h"

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
  }
  return event.count;
}

// L'élite due : son espèce par les poids de la vague en vigueur, puis sa place
// comme une naissance ordinaire. Vrai quand elle est soldée (née, ou sans
// espèce à tirer) ; faux sans place, et elle réessaie au pas suivant.
[[nodiscard]] bool SpawnElite(SurvivorsField& field) {
  const SurvivorsRules& rules = *field.rules;
  if (rules.waves.empty()) return true;
  const WaveRule& wave = rules.waves[std::min(field.wave_index, rules.waves.size() - 1)];
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
      NearestPassable(field.arena, CellOf(wanted.x), CellOf(wanted.y), kEventSearchRing);
  if (!cell) return false;
  (void)HatchFoe(field, rule, CellCenter(cell->x, cell->y), options);
  return true;
}

void SpawnEvents(SurvivorsField& field) {
  if (field.rules == nullptr) return;
  const SurvivorsRules& rules = *field.rules;
  const std::uint64_t elapsed_ms = std::uint64_t{field.step} * kSurvivorsStepMs;
  while (field.next_event < rules.events.size() &&
         rules.events[field.next_event].at_ms <= elapsed_ms) {
    const EventRule& event = rules.events[field.next_event];
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
