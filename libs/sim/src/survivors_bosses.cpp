#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>

#include "ro/sim/survivors_content.h"
#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_news.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"
#include "ro/sim/survivors_state.h"

#include "survivors_space.h"
#include "survivors_systems.h"

namespace ro::sim::survivors {

// LES BOSS SCRIPTÉS : chaque boss en vie suit son script, coup par coup. Un
// coup est actif tant que to_hp_pct < PV restants (en %) <= from_hp_pct, et
// part dès que son pas prêt est venu, puis attend sa période. Un coup qui ne
// peut pas partir (plafond des monstres, aucun point de chute, corps déjà
// tenu) garde son pas prêt et réessaie au pas suivant. Seul le séisme tire de
// l'aléa : deux Between du flux du combat par cercle hors du premier.

namespace {

// L'escorte d'un boss, et ses invoqués quand le coup ne donne pas de rayon,
// naissent en cercle à deux cases de lui.
constexpr std::int32_t kBodyguardRingUnits = Cells(2);
constexpr std::uint8_t kHalfTurn = kHeadingSteps / 2;

// `step` + `steps`, saturé dans l'uint32.
[[nodiscard]] std::uint32_t Later(std::uint32_t step, std::uint64_t steps) noexcept {
  return static_cast<std::uint32_t>(std::min<std::uint64_t>(
      std::uint64_t{step} + steps, std::numeric_limits<std::uint32_t>::max()));
}

// LA PHASE d'un coup, en produits croisés exacts : aucun pourcentage arrondi
// ne fait basculer un boss d'une phase à l'autre avant son seuil.
[[nodiscard]] bool InPhase(const BossMoveRule& move, const SurvivorsFoe& foe) noexcept {
  const std::int64_t hp_scaled = std::int64_t{std::max(foe.hp, 0)} * kWholeHpPct;
  const std::int64_t max_hp = std::max(foe.max_hp, 1);
  return std::int64_t{move.to_hp_pct} * max_hp < hp_scaled &&
         hp_scaled <= std::int64_t{move.from_hp_pct} * max_hp;
}

// Un coup qui mène le corps du boss : un seul à la fois.
[[nodiscard]] constexpr bool HoldsBody(BossMoveKind kind) noexcept {
  return kind == BossMoveKind::kLeap || kind == BossMoveKind::kCharge ||
         kind == BossMoveKind::kFlee;
}

void PublishMove(SurvivorsNews& news, std::uint32_t serial, BossMoveKind kind) {
  if (news.boss_moves.size() < kMostNewsBossMoves) {
    news.boss_moves.push_back(SurvivorsBossMoveNews{.serial = serial, .kind = kind});
  } else {
    ++news.dropped;
  }
}

// Les pas d'une charge ou d'une fuite : ceux de `effect_ms`, au moins un.
[[nodiscard]] std::uint32_t EffectSteps(const BossMoveRule& move) noexcept {
  return std::max<std::uint32_t>(StepsOf(move.effect_ms), 1);
}

// Le pas d'une charge : sa portée répartie sur ses pas, tronquée.
[[nodiscard]] std::int32_t ChargePerStep(const BossMoveRule& move) noexcept {
  return static_cast<std::int32_t>(std::int64_t{std::max(move.reach_units, 0)} / EffectSteps(move));
}

// `count` monstres `rule` en cercle autour de `center`, le k-ième au cap
// `k × 64 / count`, à `radius_units`, sous le plafond des monstres ; une case
// murée sans voisine praticable est sautée.
void HatchCircle(SurvivorsField& field, const FoeRule& rule, FieldPoint center, std::size_t count,
                 std::int32_t radius_units) {
  for (std::size_t member = 0; member < count; ++member) {
    const auto heading = static_cast<std::uint8_t>(member * kHeadingSteps / count % kHeadingSteps);
    (void)HatchNear(field, rule, center + TurnVector(heading, radius_units));
  }
}

// L'ESCORTE, née une fois à l'arrivée du boss, sous le plafond : tronquée à la
// place qui reste, jamais rattrapée.
void Escort(SurvivorsField& field, const BossRule& boss, FieldPoint at) {
  const FoeRule* rule = FindFoe(*field.rules, boss.escort_foe_id);
  if (rule == nullptr || boss.escort_count == 0) return;
  HatchCircle(field, *rule, at, boss.escort_count, kBodyguardRingUnits);
}

// L'INVOCATION : `count` monstres en cercle autour du boss, à `radius_units`
// (deux cases sans rayon), tronqués à la place qui reste. Faux, sans
// naissance, au plafond : le coup réessaie au pas suivant.
[[nodiscard]] bool Summon(SurvivorsField& field, const BossMoveRule& move, FieldPoint at) {
  const FoeRule* rule = FindFoe(*field.rules, move.foe_id);
  if (rule == nullptr) return true;
  if (field.foes.size() >= field.rules->caps.most_foes) return false;
  HatchCircle(field, *rule, at, move.count,
              move.radius_units > 0 ? move.radius_units : kBodyguardRingUnits);
  return true;
}

// LE SÉISME : `count` cercles annoncés, le premier sur le héros, chacun des
// autres décalé de deux Between dans [−reach, reach] (x puis y) ; chacun
// blesse une fois le héros qui le chevauche au pas où il frappe. Les tirages
// ont lieu même au plafond des menaces, qui écarte le cercle et son annonce.
void Quake(SurvivorsField& field, const SurvivorsFoe& foe, const BossMoveRule& move) {
  const std::uint32_t strike_step = Later(field.step, StepsOf(move.telegraph_ms));
  const FieldPoint hero = field.hero.position;
  const std::int32_t reach = std::max(move.reach_units, 0);
  for (std::size_t circle = 0; circle < move.count; ++circle) {
    FieldPoint center = hero;
    if (circle > 0) {
      const std::int32_t dx = field.combat_random.Between(-reach, reach);
      const std::int32_t dy = field.combat_random.Between(-reach, reach);
      center = hero + FieldPoint{dx, dy};
    }
    if (field.hazards.size() >= field.rules->caps.most_hazards) continue;
    field.hazards.push_back(SurvivorsHazard{.kind = HazardKind::kQuake,
                                            .center = center,
                                            .radius_units = move.radius_units,
                                            .damage = move.damage,
                                            .armed_step = strike_step,
                                            .until_step = Later(strike_step, 1),
                                            .period_steps = 0,
                                            .next_hurt_step = 0});
    PublishWarning(field.news, SurvivorsWarning{.kind = WarningKind::kQuake,
                                                .source_serial = foe.serial,
                                                .from = center,
                                                .to = center,
                                                .radius_units = move.radius_units,
                                                .strike_step = strike_step});
  }
}

// LE HURLEMENT : chaque monstre vivant de l'espèce visée marche majoré de
// `boost_pct` pendant `effect_ms` ; un hurlement plus récent remplace le
// précédent.
void Howl(SurvivorsField& field, const BossMoveRule& move) {
  const std::uint32_t until = Later(field.step, StepsOf(move.effect_ms));
  for (SurvivorsFoe& foe : field.foes) {
    if (foe.foe_id != move.foe_id) continue;
    foe.boost_until_step = until;
    foe.boost_pct = move.boost_pct;
  }
}

// ── Les coups qui mènent le corps ──────────────────────────────────────────

// Un pas de charge, au cap fixé à l'annonce ; un mur l'arrête à la fin de ce
// pas.
void Dash(SurvivorsField& field, SurvivorsFoe& foe, const BossMoveRule& move) {
  const FieldPoint from = foe.position;
  const FieldPoint delta = TurnVector(foe.act_heading, ChargePerStep(move));
  foe.position = FoeSlide(field.arena, TraitsOf(*field.rules, foe), from, delta);
  foe.facing = HeadingOf(delta, foe.facing);
  if (SlideBlocked(from, delta, foe.position)) foe.act_until_step = Later(field.step, 1);
}

// Un pas de fuite : à sa marche, droit à l'opposé du héros (à l'opposé de son
// cap s'ils se confondent).
void Retreat(SurvivorsField& field, SurvivorsFoe& foe) {
  const std::int32_t pace = PaceOf(field, foe);
  FieldPoint away = Toward(field.hero.position, foe.position, pace);
  if (away == FieldPoint{}) {
    away = TurnVector(static_cast<std::uint8_t>((foe.facing + kHalfTurn) % kHeadingSteps), pace);
  }
  foe.position = FoeSlide(field.arena, TraitsOf(*field.rules, foe), foe.position, away);
  foe.facing = HeadingOf(away, foe.facing);
}

// LE COUP JOUÉ, à son pas : un bond se pose et tient ce pas ; une charge et
// une fuite durent leurs pas, le premier compris.
void Play(SurvivorsField& field, SurvivorsFoe& foe, const BossMoveRule& move) {
  switch (move.kind) {
    case BossMoveKind::kLeap:
      foe.position = foe.act_target;
      foe.act_until_step = Later(field.step, 1);
      return;
    case BossMoveKind::kCharge:
      foe.act_until_step = Later(field.step, EffectSteps(move));
      Dash(field, foe, move);
      return;
    case BossMoveKind::kFlee:
      foe.act_until_step = Later(field.step, EffectSteps(move));
      Retreat(field, foe);
      return;
    case BossMoveKind::kSummon:
    case BossMoveKind::kQuake:
    case BossMoveKind::kHowl:
      break;
  }
  foe.act_until_step = field.step;
}

// Le corps du boss rendu à sa marche ordinaire.
void Release(SurvivorsFoe& foe, SurvivorsBossTrack& track) {
  foe.scripted = false;
  foe.telegraph_until_step = 0;
  track.held_move = kNoBossMove;
}

// LE COUP QUI TIENT LE CORPS, à ce pas : annoncé, le boss attend immobile
// son pas puis le joue ; joué, il file jusqu'à la fin de ses pas, et le corps
// est alors rendu, à ce pas même.
void Carry(SurvivorsField& field, SurvivorsFoe& foe, SurvivorsBossTrack& track,
           const BossMoveRule& move) {
  if (foe.telegraph_until_step != 0) {
    if (field.step < foe.telegraph_until_step) return;
    foe.telegraph_until_step = 0;
    Play(field, foe, move);
    return;
  }
  if (foe.act_until_step <= field.step) {
    Release(foe, track);
    return;
  }
  if (move.kind == BossMoveKind::kCharge) Dash(field, foe, move);
  if (move.kind == BossMoveKind::kFlee) Retreat(field, foe);
}

// Un coup qui prend le corps : annoncé jusqu'à `strike_step`, ou joué tout de
// suite si ce pas est venu.
void Hold(SurvivorsField& field, SurvivorsFoe& foe, SurvivorsBossTrack& track, std::size_t index,
          const BossMoveRule& move, std::uint32_t strike_step) {
  foe.scripted = true;
  track.held_move = static_cast<std::uint8_t>(index);
  if (strike_step > field.step) {
    foe.telegraph_until_step = strike_step;
    return;
  }
  Play(field, foe, move);
}

// LE BOND : annoncé vers son point de chute, à `reach_units` vers le héros,
// jamais sur un mur. Faux sans point de chute.
[[nodiscard]] bool StartLeap(SurvivorsField& field, SurvivorsFoe& foe, SurvivorsBossTrack& track,
                             std::size_t index, const BossMoveRule& move) {
  const std::optional<FieldPoint> landing =
      LeapLanding(field.arena, foe.position, field.hero.position, foe.facing, move.reach_units);
  if (!landing) return false;
  const std::uint32_t strike_step = Later(field.step, StepsOf(move.telegraph_ms));
  foe.act_target = *landing;
  foe.facing = HeadingOf(*landing - foe.position, foe.facing);
  PublishWarning(field.news, SurvivorsWarning{.kind = WarningKind::kLeap,
                                              .source_serial = foe.serial,
                                              .from = foe.position,
                                              .to = *landing,
                                              .radius_units = FoeRadius(*field.rules, foe),
                                              .strike_step = strike_step});
  Hold(field, foe, track, index, move, strike_step);
  return true;
}

// LA CHARGE : annoncée droit vers le héros, sur `reach_units` en `effect_ms`.
// Faux pour une charge qui ne bouge pas d'une sous-unité par pas.
[[nodiscard]] bool StartCharge(SurvivorsField& field, SurvivorsFoe& foe, SurvivorsBossTrack& track,
                               std::size_t index, const BossMoveRule& move) {
  const std::int32_t per_step = ChargePerStep(move);
  if (per_step <= 0) return false;
  const std::uint32_t strike_step = Later(field.step, StepsOf(move.telegraph_ms));
  const std::int64_t length = std::int64_t{per_step} * EffectSteps(move);
  foe.act_heading = HeadingOf(field.hero.position - foe.position, foe.facing);
  foe.act_target =
      foe.position + TurnVector(foe.act_heading, static_cast<std::int32_t>(std::min<std::int64_t>(
                                                     length, Cells(kMostArenaSideCells))));
  foe.facing = foe.act_heading;
  PublishWarning(field.news, SurvivorsWarning{.kind = WarningKind::kCharge,
                                              .source_serial = foe.serial,
                                              .from = foe.position,
                                              .to = foe.act_target,
                                              .radius_units = FoeRadius(*field.rules, foe),
                                              .strike_step = strike_step});
  Hold(field, foe, track, index, move, strike_step);
  return true;
}

// Le coup `move` (index `index` du script) qui part à ce pas ; faux quand il
// doit attendre. Une invocation fait naître des monstres : `foe` n'est plus
// valide après elle.
[[nodiscard]] bool Fire(SurvivorsField& field, SurvivorsFoe& foe, SurvivorsBossTrack& track,
                        std::size_t index, const BossMoveRule& move) {
  switch (move.kind) {
    case BossMoveKind::kSummon:
      return Summon(field, move, foe.position);
    case BossMoveKind::kLeap:
      return StartLeap(field, foe, track, index, move);
    case BossMoveKind::kCharge:
      return StartCharge(field, foe, track, index, move);
    case BossMoveKind::kQuake:
      Quake(field, foe, move);
      return true;
    case BossMoveKind::kHowl:
      Howl(field, move);
      return true;
    case BossMoveKind::kFlee:
      Hold(field, foe, track, index, move, field.step);
      return true;
  }
  return false;
}

// LE PAS D'UN BOSS : son escorte à l'arrivée, le coup qui tient son corps,
// puis chaque coup de son script, dans l'ordre du script. La fuite ne part
// que sur un boss touché depuis le passage précédent. Faux pour un boss parti
// (mort, ou règle disparue) : son suivi s'efface.
[[nodiscard]] bool RunBoss(SurvivorsField& field, SurvivorsBossTrack& track) {
  const SurvivorsRules& rules = *field.rules;
  if (track.rule_index >= rules.bosses.size()) return false;
  const BossRule& boss = rules.bosses[track.rule_index];
  SurvivorsFoe* foe = FoeBySerial(field, track.serial);
  if (foe == nullptr) return false;
  foe->previous_position = foe->position;
  const bool hurt = foe->hp < track.seen_hp;
  track.seen_hp = foe->hp;
  if (!track.escorted) {
    track.escorted = true;
    Escort(field, boss, foe->position);
    // Une naissance peut déplacer les monstres en mémoire : le boss se
    // retrouve par son serial.
    foe = FoeBySerial(field, track.serial);
  }
  if (track.held_move < boss.moves.size()) {
    Carry(field, *foe, track, boss.moves[track.held_move]);
  }
  const std::size_t move_count = std::min(boss.moves.size(), kMostBossMoves);
  for (std::size_t index = 0; index < move_count; ++index) {
    const BossMoveRule& move = boss.moves[index];
    if (track.ready_step[index] > field.step || !InPhase(move, *foe)) continue;
    if (HoldsBody(move.kind) && track.held_move != kNoBossMove) continue;
    if (move.kind == BossMoveKind::kFlee && !hurt) continue;
    if (!Fire(field, *foe, track, index, move)) continue;
    track.ready_step[index] = Later(field.step, StepsOf(move.period_ms));
    PublishMove(field.news, track.serial, move.kind);
    foe = FoeBySerial(field, track.serial);
  }
  return true;
}

}  // namespace

void TrackBoss(SurvivorsField& field, std::uint32_t serial, std::size_t rule_index) {
  const SurvivorsRules& rules = *field.rules;
  const SurvivorsFoe* foe = FoeBySerial(field, serial);
  if (foe == nullptr || rule_index >= rules.bosses.size() ||
      rule_index > std::numeric_limits<std::uint16_t>::max()) {
    return;
  }
  SurvivorsBossTrack track{.serial = serial,
                           .rule_index = static_cast<std::uint16_t>(rule_index),
                           .ready_step = {},
                           .seen_hp = foe->hp,
                           .held_move = kNoBossMove,
                           .escorted = false};
  const auto& moves = rules.bosses[rule_index].moves;
  for (std::size_t index = 0; index < std::min(moves.size(), kMostBossMoves); ++index) {
    track.ready_step[index] = Later(field.step, StepsOf(moves[index].first_ms));
  }
  field.boss_tracks.push_back(track);
}

bool IsFinalBoss(const SurvivorsField& field, std::uint32_t serial) {
  const auto& bosses = field.rules->bosses;
  for (const SurvivorsBossTrack& track : field.boss_tracks) {
    if (track.serial == serial) {
      return track.rule_index < bosses.size() && bosses[track.rule_index].final;
    }
  }
  return false;
}

void RunBossScripts(SurvivorsField& field) {
  if (field.rules == nullptr) return;
  // Retrait stable : les suivis gardent l'ordre de naissance des boss.
  std::size_t kept = 0;
  for (std::size_t index = 0; index < field.boss_tracks.size(); ++index) {
    SurvivorsBossTrack track = field.boss_tracks[index];
    if (RunBoss(field, track)) field.boss_tracks[kept++] = track;
  }
  field.boss_tracks.resize(kept);
}

}  // namespace ro::sim::survivors
