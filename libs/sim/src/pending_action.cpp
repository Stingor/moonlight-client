#include "ro/sim/pending_action.h"

#include <algorithm>
#include <cmath>

#include "ro/sim/long_walk.h"
#include "ro/sim/movement.h"
#include "ro/sim/world.h"

namespace ro::sim {
namespace {

// La case entière sous une position fractionnaire.
//
// Le client fait `floor(largeur / 2 + monde / 5)`, où l'origine du monde tombe au CENTRE de la
// carte : une case fractionnaire y vaut donc `floor(f + 0,5)`, c'est-à-dire l'arrondi. C'est le
// même calcul que `StoppedCell`.
std::int32_t CellOf(float fractional) { return static_cast<std::int32_t>(std::lround(fractional)); }

CellPosition Clamped(std::int32_t x, std::int32_t y) {
  CellPosition cell;
  cell.x = static_cast<std::uint16_t>(std::max(0, x));
  cell.y = static_cast<std::uint16_t>(std::max(0, y));
  return cell;
}

float DistanceCells(FractionalCell from, FractionalCell to) {
  const float dx = from.x - to.x;
  const float dy = from.y - to.y;
  return std::sqrt(dx * dx + dy * dy);
}

bool SameCell(const std::optional<CellPosition>& kept, CellPosition cell) {
  return kept.has_value() && kept->x == cell.x && kept->y == cell.y;
}

// Le numérateur et le dénominateur du rapport de vitesses que le client
// compare : le quadruple, puis une division par trois.
inline constexpr std::uint32_t kSpeedRatioNumerator = 4;
inline constexpr std::uint32_t kSpeedRatioDenominator = 3;

// LA CIBLE AU-DELÀ D'UNE MARCHE : on s'en approche par étapes, chacune une
// marche que le serveur accepte, la suivante demandée à la fin de la
// précédente. Rien quand la case d'approche est à portée d'une marche :
// l'approche ordinaire reprend la main.
std::optional<PendingMove> ApproachByLegs(const World& world, WalkPlanner& planner,
                                          const WalkMap& walk, PendingAction& action,
                                          ApproachWalkClock& clock, CellPosition standing,
                                          CellPosition approach, std::uint32_t now_ms) {
  const Actor& self = *world.player();
  if (action.leg_to.has_value()) {
    const bool taken = LegTaken(self, *action.leg_to, action.leg_sent_ms);
    // Pas encore partie : le serveur la fait partir, ou l'aura refusée au
    // terme du délai. Partie : elle court encore.
    if ((!taken && now_ms - action.leg_sent_ms < kLegConfirmMs) ||
        (taken && LegStillRunning(self, planner.PathFor(self), now_ms))) {
      return PendingMove{PendingStep::kWait, *action.leg_to};
    }
  }
  const CellPosition toward = WalkStopToward(walk, standing, approach, LegSteps(world));
  if (toward.x == approach.x && toward.y == approach.y) {
    action.leg_to.reset();
    return std::nullopt;
  }
  if (!AdmitWalkRequest(world, clock, now_ms)) {
    return PendingMove{PendingStep::kWait, toward};
  }
  action.leg_to = toward;
  action.leg_sent_ms = now_ms;
  return PendingMove{PendingStep::kApproach, toward};
}

}  // namespace

std::uint32_t ChaseRepeatMs(std::uint16_t self_ms_per_cell,
                            std::uint16_t target_ms_per_cell) noexcept {
  if (self_ms_per_cell == 0 || target_ms_per_cell == 0) {
    return kChaseRepeatSlowMs;
  }
  const std::uint32_t scaled =
      static_cast<std::uint32_t>(self_ms_per_cell) * kSpeedRatioNumerator / kSpeedRatioDenominator;
  return scaled >= target_ms_per_cell ? kChaseRepeatQuickMs : kChaseRepeatSlowMs;
}

CellPosition ApproachCell(FractionalCell self, FractionalCell target, std::uint16_t range_cells,
                          const WalkMap& walk) {
  const float dx = self.x - target.x;
  const float dy = self.y - target.y;
  const float span = DistanceCells(self, target);
  // SUR LA CASE DE LA CIBLE, IL N'Y A PAS DE DIRECTION : le client y divise par
  // zéro et lit une case indéterminée. Rester où l'on est est la seule réponse
  // sensée, et l'on y est de toute façon à portée de tout.
  if (span <= 0.0F) {
    return Clamped(CellOf(self.x), CellOf(self.y));
  }
  const float reach = static_cast<float>(range_cells) / span;
  const std::int32_t wanted_x = CellOf(target.x + dx * reach);
  const std::int32_t wanted_y = CellOf(target.y + dy * reach);
  if (walk.Passable(wanted_x, wanted_y)) {
    return Clamped(wanted_x, wanted_y);
  }
  std::int32_t tried_x = wanted_x;
  std::int32_t tried_y = wanted_y;
  for (const CellStep& step : kApproachFallbacks) {
    tried_x = wanted_x + step.dx;
    tried_y = wanted_y + step.dy;
    if (walk.Passable(tried_x, tried_y)) {
      break;
    }
  }
  return Clamped(tried_x, tried_y);
}

bool WithinSkillRange(FractionalCell self, FractionalCell target,
                      std::uint16_t range_cells) noexcept {
  return static_cast<float>(range_cells) + kRangeToleranceCells >= DistanceCells(self, target);
}

PendingMove AdvancePendingAction(const World& world, WalkPlanner& planner, const WalkMap& walk,
                                 PendingAction& action, ApproachWalkClock& clock,
                                 std::uint32_t now_ms) {
  const Actor* player = world.player();
  const bool aims_cell = action.ground_cell.has_value();
  const Actor* target = aims_cell ? nullptr : world.Find(action.target_id);
  // UNE UNITÉ AU SOL SE VISE AUSSI — un bloc d'Ice Wall se frappe. Elle ne
  // bouge ni ne meurt : sa case suffit, et son retrait par le serveur fait
  // renoncer comme la perte d'un acteur.
  const auto unit = world.skill_units().find(action.target_id);
  const bool aims_unit = !aims_cell && target == nullptr && unit != world.skill_units().end();
  if (player == nullptr || (target == nullptr && !aims_unit && !aims_cell)) {
    return {PendingStep::kGiveUp, {}};
  }
  // Le client abandonne une ATTAQUE sur une cible morte (l'état trois de la
  // cible) ; un lanceur mort n'a rien à lancer non plus.
  //
  // 🔴 UNE COMPÉTENCE, ELLE, VISE UN MORT : le cas de la compétence ciblée ne
  // lit pas l'état de sa cible. Resurrection et la Feuille d'Yggdrasil n'ont
  // pas d'autre cible, et c'est le serveur qui refuse ce qui ne se lance pas
  // sur un cadavre.
  const bool attacks = action.skill_id == 0;
  if ((attacks && target != nullptr && target->posture == Posture::kDead) ||
      player->posture == Posture::kDead) {
    return {PendingStep::kGiveUp, {}};
  }

  // ⚠ LA POSITION INTERPOLÉE, ET NON LA CASE ANNONCÉE. Le client lit les
  // coordonnées du MONDE, c'est-à-dire là où le sprite se trouve à cet
  // instant. Prendre l'arrivée du déplacement dirait « je suis arrivé » dès que
  // le serveur confirme la marche, donc avant d'avoir bougé.
  const FractionalCell here = InterpolatedCell(*player, planner.PathFor(*player), now_ms);
  FractionalCell there{};
  if (aims_cell) {
    there = {static_cast<float>(action.ground_cell->x), static_cast<float>(action.ground_cell->y)};
  } else if (aims_unit) {
    there = {static_cast<float>(unit->second.cell.x), static_cast<float>(unit->second.cell.y)};
  } else {
    there = InterpolatedCell(*target, planner.PathFor(*target), now_ms);
  }
  const CellPosition standing = Clamped(CellOf(here.x), CellOf(here.y));

  // DEUX CRITÈRES DE TIR, et le second n'est pas une commodité : arrivé sur la
  // case qu'on visait, on lance MÊME hors de portée. Sans lui, une cible qu'on
  // ne peut pas approcher davantage — un mur entre les deux — ferait piétiner
  // sans fin.
  if (SameCell(action.walking_to, standing) || WithinSkillRange(here, there, action.range_cells)) {
    return {PendingStep::kRelease, standing};
  }

  const CellPosition toward = ApproachCell(here, there, action.range_cells, walk);
  if (const std::optional<PendingMove> leg =
          ApproachByLegs(world, planner, walk, action, clock, standing, toward, now_ms)) {
    return *leg;
  }
  // ⚠ LA COMPÉTENCE N'A AUCUNE CADENCE : le cas de la compétence ciblée ne
  // compare aucune horloge — seul le
  // changement de case visée relance la marche. La cadence est celle de
  // l'ATTAQUE, au cas 1 du même tic, et la prêter à la compétence retarderait
  // chaque poursuite. Elle vaut donc zéro ici, et la garde ne mord pas.
  if (SameCell(action.walking_to, toward)) {
    return {PendingStep::kWait, toward};
  }
  // ⚠ ET LA CADENCE NE COMPTE QU'À PARTIR DE LA DEUXIÈME MARCHE. Le premier
  // pas d'une poursuite part tout de suite : chez le client, la case gardée
  // vaut moins un à l'armement, si bien que rien ne peut retenir cette
  // première demande. `walking_to` porte exactement cette question.
  if (action.walking_to.has_value() && now_ms - action.approached_ms < action.chase_repeat_ms) {
    return {PendingStep::kWait, toward};
  }
  action.walking_to = toward;
  action.approached_ms = now_ms;
  // Trop tôt après la précédente : la marche tombe, et la case reste retenue.
  if (!AdmitWalkRequest(world, clock, now_ms)) {
    return {PendingStep::kWait, toward};
  }
  return {PendingStep::kApproach, toward};
}

bool AdmitWalkRequest(const World& world, ApproachWalkClock& clock, std::uint32_t now_ms) {
  // Une carte nouvelle, un personnage nouveau : son horloge repart.
  if (clock.map_entry != world.map_entries()) {
    clock = ApproachWalkClock{.sent_ms = std::nullopt, .map_entry = world.map_entries()};
  }
  if (clock.sent_ms.has_value() && now_ms - *clock.sent_ms <= kApproachWalkFloorMs) {
    return false;
  }
  clock.sent_ms = now_ms;
  return true;
}
}  // namespace ro::sim
