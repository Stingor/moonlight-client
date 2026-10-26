#pragma once

// LES AIDES QUE PLUSIEURS SYSTÈMES DU MODE SURVIVORS PARTAGENT. Une seule
// définition par règle : le rappel et l'apparition lisent le même cadre et le
// même champ, le héros et la horde glissent le long des mêmes murs, les frappes
// et les tirs retrouvent un monstre et son niveau de la même façon, chaque
// majoration en pour cent sature de la même façon, et la horde comme les
// comportements lisent le même acte, la même cachette et la même marche.
//
// En-tête PRIVÉ de `libs/sim/src`.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <optional>

#include "ro/sim/packed_position.h"
#include "ro/sim/survivors_content.h"
#include "ro/sim/survivors_flow.h"
#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_ground.h"
#include "ro/sim/survivors_news.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"
#include "ro/sim/survivors_state.h"

namespace ro::sim::survivors {

// ── Les pourcentages ───────────────────────────────────────────────────────

inline constexpr std::int64_t kPercent = 100;
// Une défense : au-delà, `valeur × (100 + bonus)` sortirait des 64 bits pour
// une valeur de 32 bits. Les statistiques validées en restent très loin.
inline constexpr std::int64_t kMostScaledBonusPct = std::int64_t{1} << 30;

// `value`, ramené dans [0, 0xFFFFFFFF], majoré de `bonus_pct` pour cent et
// tronqué ; jamais négatif (un bonus sous −100 % donne 0).
[[nodiscard]] inline std::int64_t Scaled(std::int64_t value, std::int64_t bonus_pct) noexcept {
  const std::int64_t base =
      std::clamp<std::int64_t>(value, 0, std::numeric_limits<std::uint32_t>::max());
  const std::int64_t factor = kPercent + std::clamp(bonus_pct, -kPercent, kMostScaledBonusPct);
  return base * factor / kPercent;
}

// Ramené dans [0, INT32_MAX].
[[nodiscard]] inline std::int32_t ClampedUnits(std::int64_t units) noexcept {
  return static_cast<std::int32_t>(
      std::clamp<std::int64_t>(units, 0, std::numeric_limits<std::int32_t>::max()));
}

// Une mesure majorée de `bonus_pct` pour cent, dans [0, INT32_MAX].
[[nodiscard]] inline std::int32_t ScaledUnits(std::int64_t value, std::int64_t bonus_pct) noexcept {
  return ClampedUnits(Scaled(value, bonus_pct));
}

// `total` + `amount`, saturé dans l'int32 : un cumul ne déborde jamais.
[[nodiscard]] inline std::int32_t SaturatedAdd(std::int32_t total, std::int64_t amount) noexcept {
  return static_cast<std::int32_t>(std::clamp<std::int64_t>(
      std::int64_t{total} + amount, std::numeric_limits<std::int32_t>::min(),
      std::numeric_limits<std::int32_t>::max()));
}

// ── Les niveaux ────────────────────────────────────────────────────────────

// Le niveau en vigueur d'un skill possédé ; nullptr hors de sa table.
[[nodiscard]] inline const SkillLevel* LevelOf(const SurvivorsSkillRule& rule,
                                               const OwnedSkill& owned) noexcept {
  if (owned.level == 0 || std::size_t{owned.level} > rule.levels.size()) return nullptr;
  return &rule.levels[owned.level - 1U];
}

// ── L'espace ───────────────────────────────────────────────────────────────

// `units` + `delta`, ou rien hors de l'int32 : aucune arène ne s'étend si loin.
[[nodiscard]] inline std::optional<std::int32_t> Shifted(std::int32_t units,
                                                         std::int32_t delta) noexcept {
  const std::int64_t sum = std::int64_t{units} + delta;
  if (sum < std::numeric_limits<std::int32_t>::min() ||
      sum > std::numeric_limits<std::int32_t>::max()) {
    return std::nullopt;
  }
  return static_cast<std::int32_t>(sum);
}

// `from` avancé de `delta`, axe par axe (x puis y) : un axe dont la case
// d'arrivée n'est pas praticable est refusé, et l'autre glisse le long du mur.
// Un axe est pris en entier ou pas du tout.
[[nodiscard]] inline FieldPoint Slide(const SurvivorsGround& ground, FieldPoint from,
                                      FieldPoint delta) noexcept {
  FieldPoint at = from;
  if (delta.x != 0) {
    const std::optional<std::int32_t> x = Shifted(at.x, delta.x);
    if (x && ground.Passable(CellOf(*x), CellOf(at.y))) at.x = *x;
  }
  if (delta.y != 0) {
    const std::optional<std::int32_t> y = Shifted(at.y, delta.y);
    if (y && ground.Passable(CellOf(at.x), CellOf(*y))) at.y = *y;
  }
  return at;
}

// Vrai si Slide a refusé un axe que `delta` demandait.
[[nodiscard]] inline bool SlideBlocked(FieldPoint from, FieldPoint delta, FieldPoint at) noexcept {
  return (delta.x != 0 && at.x == from.x) || (delta.y != 0 && at.y == from.y);
}

// La case praticable du sol la plus proche de (x, y), cherchée en anneaux
// carrés jusqu'à `max_ring` : sur un anneau, la plus proche à vol d'oiseau, et
// à égalité la première dans l'ordre de balayage (du sud au nord, d'ouest en
// est). La règle est celle du NearestPassable d'une carte, case pour case.
[[nodiscard]] inline std::optional<CellPosition> NearestPassable(const SurvivorsGround& ground,
                                                                 std::int32_t x, std::int32_t y,
                                                                 std::int32_t max_ring) {
  for (std::int32_t ring = 0; ring <= max_ring; ++ring) {
    std::optional<CellPosition> best;
    std::int64_t best_distance = std::numeric_limits<std::int64_t>::max();
    for (std::int32_t dy = -ring; dy <= ring; ++dy) {
      for (std::int32_t dx = -ring; dx <= ring; ++dx) {
        if (std::max(std::abs(dx), std::abs(dy)) != ring) continue;
        if (!ground.Passable(x + dx, y + dy)) continue;
        const std::int64_t distance = std::int64_t{dx} * dx + std::int64_t{dy} * dy;
        if (distance >= best_distance) continue;
        best_distance = distance;
        best =
            CellPosition{static_cast<std::uint16_t>(x + dx), static_cast<std::uint16_t>(y + dy), 0};
      }
    }
    if (best) return best;
  }
  return std::nullopt;
}

// Strictement dans le cadre d'apparition centré sur `hero` ; son périmètre
// n'en fait pas partie.
[[nodiscard]] inline bool InsideSpawnFrame(const SpawnFrame& frame, FieldPoint hero,
                                           FieldPoint point) noexcept {
  const std::int64_t dx = std::int64_t{point.x} - hero.x;
  const std::int64_t dy = std::int64_t{point.y} - hero.y;
  return dx > -frame.half_width_units && dx < frame.half_width_units && dy > -frame.south_units &&
         dy < frame.north_units;
}

// UNE IMPASSE : une case que le champ de poursuite couvre, mais d'où aucun
// chemin ne mène au héros (une poche fermée, un enclos). Une case hors de la
// fenêtre du champ n'en est pas une : le champ n'en sait rien. L'apparition et
// le rappel n'y posent jamais un monstre, et un monstre qui s'y trouve est
// rappelé.
[[nodiscard]] inline bool Stranded(const HordeFlow& flow, FieldPoint at) noexcept {
  const std::int32_t cell_x = CellOf(at.x);
  const std::int32_t cell_y = CellOf(at.y);
  return flow.Covers(cell_x, cell_y) && !flow.CostFrom(cell_x, cell_y).has_value();
}

// `foes` est rangé par naissance et le serial est le numéro de naissance : les
// serials y croissent strictement, et la recherche est dichotomique. nullptr
// pour un monstre parti.
[[nodiscard]] inline SurvivorsFoe* FoeBySerial(SurvivorsField& field, std::uint32_t serial) {
  auto& foes = field.foes;
  const auto found = std::lower_bound(
      foes.begin(), foes.end(), serial,
      [](const SurvivorsFoe& foe, std::uint32_t wanted) { return foe.serial < wanted; });
  return found != foes.end() && found->serial == serial ? &*found : nullptr;
}

// Le rayon d'un monstre, selon sa taille ; celui d'un petit pour un Id inconnu
// des règles.
[[nodiscard]] inline std::int32_t FoeRadius(const SurvivorsRules& rules,
                                            const SurvivorsFoe& foe) noexcept {
  const FoeRule* rule = FindFoe(rules, foe.foe_id);
  const FoeSize size = rule != nullptr ? rule->size : FoeSize::kSmall;
  return rules.tuning.foe_radius_units[static_cast<std::size_t>(size)];
}

// Le plus grand rayon de monstre : ce qu'une requête de grille ajoute à sa
// propre portée pour ne manquer aucun contact.
[[nodiscard]] inline std::int32_t LargestFoeRadius(const SurvivorsRules& rules) noexcept {
  const auto& radii = rules.tuning.foe_radius_units;
  return *std::max_element(radii.begin(), radii.end());
}

// Un mini-boss ou un boss : il ne subit qu'une part du ralentissement, et ni
// gel ni recul.
[[nodiscard]] inline bool IsBoss(const SurvivorsRules& rules, const SurvivorsFoe& foe) noexcept {
  const FoeRule* rule = FindFoe(rules, foe.foe_id);
  return rule != nullptr && rule->rank != FoeRank::kCommon;
}

// ── Les objets au sol ──────────────────────────────────────────────────────

// Un objet de sorte `kind` posé en `at`, au numéro suivant ; faux, sans rien
// poser, au plafond des objets. Le coffre passe hors plafond : il est la seule
// source d'évolution, et ses chutes restent bornées par les élites et les
// mini-boss du catalogue. Les cartes au sol (DropCard) ne comptent pas au
// plafond : une carte tombée ne prive la partie d'aucun objet.
inline bool DropPickup(SurvivorsField& field, PickupKind kind, std::uint16_t item_id,
                       FieldPoint at) {
  if (kind != PickupKind::kChest) {
    const auto counted = static_cast<std::size_t>(std::count_if(
        field.pickups.begin(), field.pickups.end(),
        [](const SurvivorsPickup& pickup) { return pickup.kind != PickupKind::kCard; }));
    if (counted >= field.rules->caps.most_pickups) return false;
  }
  field.pickups.push_back(SurvivorsPickup{
      .serial = field.next_serial++, .kind = kind, .item_id = item_id, .position = at});
  return true;
}

// ── Les statuts ────────────────────────────────────────────────────────────

[[nodiscard]] inline bool Frozen(const SurvivorsField& field, const SurvivorsFoe& foe) noexcept {
  return field.step < foe.freeze_until_step;
}

// Un monstre qui marche à ce pas : ni immobile par sa règle, ni gelé. Un
// monstre qui ne marche pas compte comme immobile à la séparation et à la
// poussée du héros.
[[nodiscard]] inline bool Walks(const SurvivorsField& field, const SurvivorsFoe& foe) noexcept {
  return foe.units_per_step > 0 && !Frozen(field, foe);
}

// ── Les comportements ──────────────────────────────────────────────────────

// L'acte qu'un monstre répète à intervalle : celui de sa marche (tir, charge,
// bond), sinon la cachette de ses traits, sinon aucun.
enum class FoeAct : std::uint8_t { kNone, kShoot, kCharge, kBlink, kHide };

[[nodiscard]] inline FoeAct ActOf(const FoeBehaviour& behaviour) noexcept {
  switch (behaviour.move) {
    case FoeMove::kRanged:
      return FoeAct::kShoot;
    case FoeMove::kCharger:
      return FoeAct::kCharge;
    case FoeMove::kBlinker:
      return FoeAct::kBlink;
    case FoeMove::kChase:
    case FoeMove::kStill:
    case FoeMove::kFlyer:
    case FoeMove::kThief:
      break;
  }
  return (behaviour.traits & kFoeHides) != 0 ? FoeAct::kHide : FoeAct::kNone;
}

// Les traits d'un monstre ; aucun pour un Id inconnu des règles.
[[nodiscard]] inline std::uint8_t TraitsOf(const SurvivorsRules& rules,
                                           const SurvivorsFoe& foe) noexcept {
  const FoeRule* rule = FindFoe(rules, foe.foe_id);
  return rule != nullptr ? rule->behaviour.traits : std::uint8_t{0};
}

// UN MONSTRE CACHÉ est intangible : ni visé, ni touché, ni touchant. Il l'est
// pendant son acte de cachette, tant que `step < act_until_step`.
[[nodiscard]] inline bool Hidden(const SurvivorsField& field, const SurvivorsFoe& foe) noexcept {
  if (foe.act_until_step <= field.step) return false;
  const FoeRule* rule = FindFoe(*field.rules, foe.foe_id);
  return rule != nullptr && ActOf(rule->behaviour) == FoeAct::kHide;
}

// Un monstre qu'un coup peut viser ou toucher : vivant, et pas caché.
[[nodiscard]] inline bool Tangible(const SurvivorsField& field, const SurvivorsFoe& foe) noexcept {
  return foe.hp > 0 && !Hidden(field, foe);
}

// ── La marche ──────────────────────────────────────────────────────────────

// La marche d'un monstre à ce pas : la sienne, majorée sous le hurlement d'un
// boss, moins le ralentissement d'un glacé, dont un boss ne subit qu'une part.
[[nodiscard]] inline std::int32_t PaceOf(const SurvivorsField& field,
                                         const SurvivorsFoe& foe) noexcept {
  const SurvivorsTuning& tuning = field.rules->tuning;
  const std::int32_t speed = field.step < foe.boost_until_step
                                 ? ScaledUnits(foe.units_per_step, foe.boost_pct)
                                 : foe.units_per_step;
  if (foe.chill_until_step <= field.step) return speed;
  std::int64_t slow_pct = std::clamp<std::int64_t>(tuning.chill_slow_pct, 0, kPercent);
  if (IsBoss(*field.rules, foe)) {
    slow_pct =
        slow_pct * std::clamp<std::int64_t>(tuning.boss_chill_share_pct, 0, kPercent) / kPercent;
  }
  return static_cast<std::int32_t>(std::int64_t{speed} * (kPercent - slow_pct) / kPercent);
}

// Le pas de `from` vers `goal`, de `speed` au plus : Toward dépasserait un but
// plus proche que sa longueur.
[[nodiscard]] inline FieldPoint StepToward(FieldPoint from, FieldPoint goal,
                                           std::int32_t speed) noexcept {
  const auto distance = static_cast<std::int64_t>(
      IntegerSqrt(static_cast<std::uint64_t>(DistanceSquared(from, goal))));
  return Toward(from, goal, static_cast<std::int32_t>(std::min<std::int64_t>(speed, distance)));
}

// Vrai si `at` est sur une case de l'arène, murée ou non.
[[nodiscard]] inline bool InsideArena(const SurvivorsGround& ground, FieldPoint at) noexcept {
  const std::int32_t cell_x = CellOf(at.x);
  const std::int32_t cell_y = CellOf(at.y);
  return cell_x >= 0 && cell_y >= 0 && cell_x < ground.width() && cell_y < ground.height();
}

// Comme Slide, pour un monstre qui traverse les murs : un axe n'est refusé que
// s'il sort de l'arène.
[[nodiscard]] inline FieldPoint Glide(const SurvivorsGround& ground, FieldPoint from,
                                      FieldPoint delta) noexcept {
  FieldPoint at = from;
  if (delta.x != 0) {
    const std::optional<std::int32_t> x = Shifted(at.x, delta.x);
    if (x && InsideArena(ground, FieldPoint{*x, at.y})) at.x = *x;
  }
  if (delta.y != 0) {
    const std::optional<std::int32_t> y = Shifted(at.y, delta.y);
    if (y && InsideArena(ground, FieldPoint{at.x, *y})) at.y = *y;
  }
  return at;
}

// Le point de chute d'un bond de `from` vers `goal` : à `reach` au cap qui y
// mène (`fallback` pour deux points confondus), ou une case plus près tant
// qu'il tombe sur un mur ou hors de l'arène ; rien si aucun ne convient. Un
// bond ne se pose jamais sur un mur.
[[nodiscard]] inline std::optional<FieldPoint> LeapLanding(const SurvivorsGround& ground,
                                                           FieldPoint from, FieldPoint goal,
                                                           std::uint8_t fallback,
                                                           std::int32_t reach) noexcept {
  const std::uint8_t heading = HeadingOf(goal - from, fallback);
  for (std::int32_t length = reach; length > 0; length -= kSubunitsPerCell) {
    const FieldPoint landing = from + TurnVector(heading, length);
    if (InsideArena(ground, landing) && ground.Passable(CellOf(landing.x), CellOf(landing.y))) {
      return landing;
    }
  }
  return std::nullopt;
}

// Une annonce publiée, ou comptée dans `dropped` au plafond des annonces.
inline void PublishWarning(SurvivorsNews& news, const SurvivorsWarning& warning) {
  if (news.warnings.size() < kMostNewsWarnings) {
    news.warnings.push_back(warning);
  } else {
    ++news.dropped;
  }
}

// Le pas d'un monstre aux traits `traits` : contre les murs, ou au travers
// s'il les traverse.
[[nodiscard]] inline FieldPoint FoeSlide(const SurvivorsGround& ground, std::uint8_t traits,
                                         FieldPoint from, FieldPoint delta) noexcept {
  return (traits & kFoePassesWalls) != 0 ? Glide(ground, from, delta) : Slide(ground, from, delta);
}

}  // namespace ro::sim::survivors
