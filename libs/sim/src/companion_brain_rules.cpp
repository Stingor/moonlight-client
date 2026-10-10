// LES RÈGLES SI/ALORS DU CERVEAU D'UN COMPAGNON : leurs conditions, leur
// lecture de haut en bas, et ce que fait la règle qui décide. Voir
// `ro/sim/companion_rules.h` et `ro/sim/companion_ai.h`.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <utility>
#include <vector>

#include "ro/sim/companion_ai.h"
#include "ro/sim/companion_rules.h"
#include "ro/sim/monster_sheet.h"
#include "ro/sim/movement.h"
#include "ro/sim/skill_book.h"
#include "ro/sim/walk_map.h"
#include "ro/sim/world.h"

#include "companion_brain_detail.h"

namespace ro::sim {
namespace {

using companion_detail::AwayFrom;
using companion_detail::Casting;
using companion_detail::CellAt;
using companion_detail::HealthPercent;
using companion_detail::MsOf;
using companion_detail::PercentOf;
using companion_detail::Plain;
using companion_detail::SignedElapsedMs;

using companion_detail::kOwnerSideCells;

// LE RAYON PRUDENT d'une zone que les tables du client ne déclarent pas : la
// meilleure case ne compte alors que ce qu'un rayon d'une case couvre.
constexpr int kRuleAreaFallbackCells = 1;
// Le poids d'un monstre sans tactique.
constexpr float kPlainWeight = 1.0F;

// UN ALLIÉ QUI PEUT RECEVOIR UN SKILL : vivant, PV non nuls, ni caché.
[[nodiscard]] bool AliveAlly(const Actor& actor) noexcept {
  return actor.posture != Posture::kDead && actor.health != 0 && CanBeTargeted(actor);
}

}  // namespace

void CompanionBrain::SyncRules(const CompanionAiSettings& settings, std::uint32_t now_ms) {
  if (rules_seen_ != settings.rules) {
    // 🔴 CHAQUE ÉTAT SUIT SA RÈGLE : une règle inchangée garde son délai, son
    // quota et sa suspension à son nouveau rang. Une règle neuve ou modifiée
    // part à neuf, une copie aussi — la première jumelle garde l'état, et la
    // même place l'emporte sur une autre.
    const std::vector<CompanionRule>& rules = settings.rules;
    std::vector<int> old_to_new(rules_seen_.size(), kOrphanRule);
    std::vector<RuleState> states(rules.size());
    const auto pair = [&](std::size_t old_index, std::size_t new_index) {
      old_to_new[old_index] = static_cast<int>(new_index);
      states[new_index] = rule_states_[old_index];
    };
    std::vector<bool> paired(rules.size(), false);
    for (std::size_t index = 0; index < rules.size() && index < rules_seen_.size(); ++index) {
      if (rules[index] == rules_seen_[index] && index < rule_states_.size()) {
        pair(index, index);
        paired[index] = true;
      }
    }
    for (std::size_t new_index = 0; new_index < rules.size(); ++new_index) {
      for (std::size_t old_index = 0;
           !paired[new_index] && old_index < rules_seen_.size() && old_index < rule_states_.size();
           ++old_index) {
        if (old_to_new[old_index] == kOrphanRule && rules_seen_[old_index] == rules[new_index]) {
          pair(old_index, new_index);
          paired[new_index] = true;
        }
      }
    }
    if (deciding_rule_.has_value()) {
      const int moved = deciding_rule_->index < old_to_new.size()
                            ? old_to_new[deciding_rule_->index]
                            : kOrphanRule;
      if (moved >= 0) {
        deciding_rule_->index = static_cast<std::size_t>(moved);
      } else {
        deciding_rule_.reset();
      }
    }
    skills_.RemapRuleSkills(old_to_new);
    rules_seen_ = rules;
    rule_states_ = std::move(states);
  }
  // UN REFUS DU SERVEUR SUSPEND LA RÈGLE qui l'a causé, le temps qu'un skill
  // refusé l'est, au lieu de la rejouer à chaque tick.
  for (const int index : skills_.TakeRefusedRules()) {
    if (index >= 0 && static_cast<std::size_t>(index) < rule_states_.size()) {
      rule_states_[static_cast<std::size_t>(index)].suspended_until_ms =
          now_ms + kSkillRefusalSuspendMs;
    }
  }
}

int CompanionBrain::QuotaUses(std::size_t index) const {
  const RuleState& state = rule_states_[index];
  const int traced = state.quota_target_id == target_id_ ? state.quota_uses : 0;
  return traced + skills_.PendingOfRuleOn(static_cast<int>(index), target_id_);
}

void CompanionBrain::CountRuleUse(std::size_t index, std::uint32_t combat_target_id,
                                  std::uint32_t now_ms) {
  RuleState& state = rule_states_[index];
  state.last_acted_ms = now_ms;
  if (state.quota_target_id != combat_target_id) {
    state.quota_target_id = combat_target_id;
    state.quota_uses = 0;
  }
  ++state.quota_uses;
}

std::optional<CompanionBrain::RuleDecision> CompanionBrain::TryRules(const Situation& s) {
  for (std::size_t index = 0; index < s.settings.rules.size() && index < rule_states_.size();
       ++index) {
    const CompanionRule& rule = s.settings.rules[index];
    const RuleState& state = rule_states_[index];
    if (!ValidRule(rule) ||
        (state.suspended_until_ms.has_value() &&
         SignedElapsedMs(s.now_ms, *state.suspended_until_ms) < 0) ||
        (rule.own_delay_ms > 0 && state.last_acted_ms.has_value() &&
         s.now_ms - *state.last_acted_ms < rule.own_delay_ms) ||
        !RuleHolds(s, rule)) {
      continue;
    }
    // VRAIE MAIS INFAISABLE : la suivante a sa chance. « Ne rien faire » et
    // « revenir au maître » décident aussi sans ordre : ils retiennent
    // l'offensive.
    std::optional<RuleDecision> decision = RuleAct(s, index, rule);
    if (!decision.has_value() ||
        (rule.action != RuleAction::kDoNothing && rule.action != RuleAction::kReturnToOwner &&
         !decision->order.has_value())) {
      continue;
    }
    deciding_rule_ = DecidingRule{.index = index, .at_ms = s.now_ms};
    return decision;
  }
  return std::nullopt;
}

std::optional<CompanionBrain::RuleDecision> CompanionBrain::RuleAct(const Situation& s,
                                                                    std::size_t index,
                                                                    const CompanionRule& rule) {
  if (rule.action == RuleAction::kDoNothing) {
    return RuleDecision{};
  }
  // LE QUOTA SE COMPTE PAR CIBLE DE COMBAT, quelle que soit la cible de
  // l'action : un autre combat repart de zéro.
  if (rule.per_target_most > 0 && QuotaUses(index) >= rule.per_target_most) {
    return std::nullopt;
  }
  const bool stopped = !IsWalking(s.self, s.now_ms) && MoveSettled(s.now_ms);
  switch (rule.action) {
    case RuleAction::kReturnToOwner: {
      // 🔴 TANT QU'ELLE EST VRAIE, LA RÈGLE GARDE LA MAIN, même sans ordre à
      // donner : retour en cours, déjà à côté du maître, ou arrêté après un
      // retour qu'il n'a pas démenti en bougeant. Rendue à la base, elle
      // laisserait l'initiative relancer la poursuite au tic suivant, et le
      // compagnon ferait la navette entre le monstre et le maître.
      const std::uint32_t combat_target_id = target_id_;
      EndFight(s, false);
      if (WholeCellDistance(s.self_cell, s.owner_cell) <= kOwnerSideCells ||
          (stopped && followed_owner_cell_ == s.owner_cell)) {
        return RuleDecision{};
      }
      std::optional<CompanionOrder> order =
          FollowOwner(s, kOwnerSideCells, CompanionAiReason::kRule);
      if (order.has_value()) {
        CountRuleUse(index, combat_target_id, s.now_ms);
      }
      return RuleDecision{.order = order};
    }
    case RuleAction::kAttack: {
      const int range_cells = std::max(s.facts.attack_range_cells, 1);
      const Actor* aimed = AimedActor(s, rule.aim, range_cells);
      if (aimed == nullptr || IsAbandoned(aimed->id, s.now_ms) ||
          !WithinCells(s.self_cell, StandingCell(*aimed), range_cells) || !AttackReady(s)) {
        return std::nullopt;
      }
      // LES GARDES DU COMBAT valent pour une attaque de règle : des attaques
      // restées sans réponse lâchent la cible, et elle n'est plus choisie seule.
      if (mode_ == Mode::kFight && target_id_ == aimed->id && AttacksUnanswered(s)) {
        abandoned_target_id_ = aimed->id;
        abandoned_at_ms_ = s.now_ms;
        EndFight(s, true);
        return std::nullopt;
      }
      if (mode_ != Mode::kFight || target_id_ != aimed->id) {
        const CompanionTactic* tactic =
            aimed->kind == ActorKind::kMonster ? TacticOf(s, *aimed) : nullptr;
        TargetPick pick{.actor_id = aimed->id,
                        .reason = CompanionAiReason::kRule,
                        .distance_cells = WholeCellDistance(s.self_cell, StandingCell(*aimed))};
        if (tactic != nullptr) {
          pick.rank = tactic->rank;
          pick.tactic = *tactic;
        }
        StartFight(pick, /*direct=*/false, /*from_hold=*/false, s.now_ms);
      }
      CountRuleUse(index, aimed->id, s.now_ms);
      return RuleDecision{
          .order =
              Emit(s, CompanionOrder{.kind = CompanionOrder::Kind::kAttack, .target_id = aimed->id},
                   CompanionAiReason::kRule)};
    }
    case RuleAction::kStepAway: {
      const Actor* aimed = AimedActor(s, rule.aim, kCompanionSightCells);
      if (aimed == nullptr) {
        return std::nullopt;
      }
      const CellPosition aimed_cell = Plain(StandingCell(*aimed));
      const int distance_cells = WholeCellDistance(s.self_cell, aimed_cell);
      if (distance_cells >= rule.step_cells) {
        return std::nullopt;
      }
      const CellPosition away = AwayFrom(s.self_cell, aimed_cell, rule.step_cells - distance_cells);
      // CONTRE UN MUR, ou arrêté après avoir déjà visé cette case : le pas ne
      // mène nulle part.
      if (away == s.self_cell || !WalkableCell(s, away) || (stopped && moved_to_cell_ == away)) {
        return std::nullopt;
      }
      std::optional<CompanionOrder> order = MoveTo(s, away, CompanionAiReason::kRule);
      if (order.has_value()) {
        CountRuleUse(index, target_id_, s.now_ms);
      }
      return RuleDecision{.order = order};
    }
    case RuleAction::kCastSkill:
      return RuleCast(s, index, rule);
    case RuleAction::kDoNothing:
      break;
  }
  return std::nullopt;
}

std::optional<CompanionBrain::RuleDecision> CompanionBrain::RuleCast(const Situation& s,
                                                                     std::size_t index,
                                                                     const CompanionRule& rule) {
  const auto found = std::ranges::find_if(s.facts.skills, [&rule](const CompanionSkillFact& skill) {
    return skill.skill_id == rule.skill_id;
  });
  // POSSÉDÉ, ET DU GENRE QUE LA CIBLE D'ACTION DEMANDE : un skill de soi ne se
  // vise pas ailleurs, un skill au sol vise une case.
  if (found == s.facts.skills.end() || found->level == 0 ||
      (found->targeting & RuleAimTargeting(rule.aim)) == 0 || ArrivalHolding(s)) {
    return std::nullopt;
  }
  const CompanionSkillFact& skill = *found;
  const int range_cells = std::max(skill.range_cells, 1);
  std::uint32_t target_id = 0;
  const Actor* aimed = nullptr;
  CellPosition cell = s.self_cell;
  const bool on_self = rule.aim == RuleAim::kSelf;
  if (on_self) {
    target_id = s.self.id;
  } else if (!AimsAtCell(rule.aim)) {
    aimed = AimedActor(s, rule.aim, range_cells);
    if (aimed == nullptr) {
      return std::nullopt;
    }
    target_id = aimed->id;
    cell = Plain(StandingCell(*aimed));
  } else if (rule.aim == RuleAim::kCellUnderOwner) {
    cell = s.owner_cell;
  } else if (rule.aim == RuleAim::kCellUnderTarget) {
    aimed = AimedActor(s, RuleAim::kTarget, range_cells);
    if (aimed == nullptr) {
      return std::nullopt;
    }
    cell = Plain(StandingCell(*aimed));
  } else {
    const int radius_cells =
        skill.area_radius_cells > 0 ? skill.area_radius_cells : kRuleAreaFallbackCells;
    const std::optional<CellPosition> best = BestCell(s, range_cells, radius_cells);
    if (!best.has_value()) {
      return std::nullopt;
    }
    cell = *best;
  }
  const SkillMoment moment = MomentOf(s, cell);
  // ⚠ LE SERVEUR MESURE LA PORTÉE SUR LA POSITION COURANTE : arrêté, et à portée
  // d'où il est. Une case se lance sur du marchable.
  if (!on_self && (!moment.stopped || !WithinCells(s.self_cell, cell, range_cells))) {
    return std::nullopt;
  }
  if (AimsAtCell(rule.aim) && !WalkableCell(s, cell)) {
    return std::nullopt;
  }
  // LE DÉLAI SE COMPTE À LA TRACE : un skill qui attend la sienne retient sa
  // règle.
  const int rule_index = static_cast<int>(index);
  if (skills_.PendingOfRule(rule_index) > 0) {
    return std::nullopt;
  }
  // LA SP À GARDER est celle de la tactique du monstre visé quand elle en fixe
  // une, sinon celle du niveau 1. ⚠ UN SKILL DE RÈGLE N'ENTAME PAS LES QUOTAS
  // DE LA TACTIQUE (« N fois », ouverture) : ils règlent les skills
  // automatiques, et la règle a les siens.
  std::int64_t reserve = s.settings.skills.sp_reserve_points;
  if (aimed != nullptr && aimed->kind == ActorKind::kMonster) {
    if (const CompanionTactic* tactic = TacticOf(s, *aimed);
        tactic != nullptr && tactic->sp_reserve_points != kTacticSpReserveFromBase) {
      reserve = tactic->sp_reserve_points;
    }
  }
  if (rule.ignores_sp_reserve) {
    reserve = 0;
  }
  if (!skills_.RuleCastable(skill, s.settings.skills.delay_ms, reserve, moment)) {
    return std::nullopt;
  }
  const SkillPick pick{.skill_id = skill.skill_id,
                       .level = rule.skill_level == 0
                                    ? skill.level
                                    : std::min<std::uint16_t>(rule.skill_level, skill.level)};
  // L'ATTENTE SE RANGE SOUS LA CIBLE DE COMBAT : c'est elle que le quota compte.
  skills_.NoteSent(pick, target_id_, s.now_ms, rule_index);
  CompanionOrder order{
      .kind = CompanionOrder::Kind::kSkill, .skill_id = pick.skill_id, .skill_level = pick.level};
  if (AimsAtCell(rule.aim)) {
    order.cell = cell;
  } else {
    order.target_id = target_id;
  }
  return RuleDecision{.order = Emit(s, order, CompanionAiReason::kRule)};
}

const Actor* CompanionBrain::AimedActor(const Situation& s, RuleAim aim, int range_cells) const {
  switch (aim) {
    case RuleAim::kTarget: {
      const Actor* target = target_id_ != 0 ? s.world.Find(target_id_) : nullptr;
      return target != nullptr && IsRuleFoe(s, *target) ? target : nullptr;
    }
    case RuleAim::kSelf:
      return &s.self;
    case RuleAim::kOwner:
      return AliveAlly(s.owner) ? &s.owner : nullptr;
    case RuleAim::kOwnerTarget: {
      if (!s.owner.attack.has_value() || !IsAttacking(s.owner, s.owner.attack->target, s.now_ms)) {
        return nullptr;
      }
      const Actor* aimed = s.world.Find(s.owner.attack->target);
      return aimed != nullptr && IsRuleFoe(s, *aimed) ? aimed : nullptr;
    }
    case RuleAim::kOwnerAttacker: {
      const Actor* nearest = nullptr;
      int nearest_cells = 0;
      for (const auto& [actor_id, actor] : s.world.actors()) {
        if (!IsRuleFoe(s, actor) || !IsAttacking(actor, s.owner.id, s.now_ms)) {
          continue;
        }
        const int cells = WholeCellDistance(s.self_cell, StandingCell(actor));
        if (nearest == nullptr || cells < nearest_cells ||
            (cells == nearest_cells && actor_id < nearest->id)) {
          nearest = &actor;
          nearest_cells = cells;
        }
      }
      return nearest;
    }
    case RuleAim::kFriend: {
      // LE PLUS BLESSÉ DES AMIS À PORTÉE, vivants et PV connus seulement : un
      // mort n'est pas « le plus blessé ».
      const Actor* hurt = nullptr;
      std::int64_t hurt_percent = 0;
      for (const Actor* friend_actor : s.friends) {
        const std::optional<std::int64_t> percent = HealthPercent(*friend_actor);
        if (!percent.has_value() || !AliveAlly(*friend_actor) ||
            !WithinCells(s.self_cell, StandingCell(*friend_actor), range_cells)) {
          continue;
        }
        if (hurt == nullptr || *percent < hurt_percent ||
            (*percent == hurt_percent && friend_actor->id < hurt->id)) {
          hurt = friend_actor;
          hurt_percent = *percent;
        }
      }
      return hurt;
    }
    case RuleAim::kCellUnderTarget:
    case RuleAim::kCellUnderOwner:
    case RuleAim::kBestCell:
      break;
  }
  return nullptr;
}

std::optional<CellPosition> CompanionBrain::BestCell(const Situation& s, int range_cells,
                                                     int radius_cells) const {
  struct Enemy {
    CellPosition cell;
    float weight = 0.0F;
  };
  std::vector<Enemy> enemies;
  for (const auto& [actor_id, actor] : s.world.actors()) {
    if (IsRuleTarget(s, actor)) {
      enemies.push_back(Enemy{.cell = Plain(StandingCell(actor)), .weight = WeightOf(s, actor)});
    }
  }
  const auto covers = [radius_cells](CellPosition centre, CellPosition cell) {
    return std::abs(static_cast<int>(centre.x) - static_cast<int>(cell.x)) <= radius_cells &&
           std::abs(static_cast<int>(centre.y) - static_cast<int>(cell.y)) <= radius_cells;
  };
  // ⚠ LA LIGNE DE VUE N'EST PAS VÉRIFIÉE : la carte du client ne dit que le
  // marchable, et un précipice qui arrête le pas n'arrête pas un sort. Une case
  // derrière un mur peut donc être refusée par le serveur ; la règle se
  // suspend alors comme pour tout refus.
  std::optional<CellPosition> best;
  float best_weight = 0.0F;
  int best_squared = 0;
  for (const Enemy& around : enemies) {
    for (int dy = -radius_cells; dy <= radius_cells; ++dy) {
      for (int dx = -radius_cells; dx <= radius_cells; ++dx) {
        const CellPosition cell =
            CellAt(static_cast<int>(around.cell.x) + dx, static_cast<int>(around.cell.y) + dy);
        if (!WithinCells(s.self_cell, cell, range_cells) || !WalkableCell(s, cell)) {
          continue;
        }
        float weight = 0.0F;
        for (const Enemy& enemy : enemies) {
          if (covers(cell, enemy.cell)) {
            weight += enemy.weight;
          }
        }
        const int sx = static_cast<int>(cell.x) - static_cast<int>(s.self_cell.x);
        const int sy = static_cast<int>(cell.y) - static_cast<int>(s.self_cell.y);
        const int squared = (sx * sx) + (sy * sy);
        // LE PLUS DE POIDS, puis la plus proche, puis la plus petite (y, x) :
        // l'ordre de parcours du monde ne décide jamais.
        const bool better = !best.has_value() || weight > best_weight ||
                            (weight == best_weight &&
                             (squared < best_squared ||
                              (squared == best_squared &&
                               (cell.y < best->y || (cell.y == best->y && cell.x < best->x)))));
        if (better) {
          best = cell;
          best_weight = weight;
          best_squared = squared;
        }
      }
    }
  }
  return best;
}

bool CompanionBrain::IsRuleTarget(const Situation& s, const Actor& actor) const {
  if (!IsRuleEnemy(s, actor) || SummonSideOf(s.world, actor, s.side) == SummonSide::kOthers) {
    return false;
  }
  const CompanionTactic* tactic = TacticOf(s, actor);
  return (tactic == nullptr || tactic->rank != TacticRank::kIgnore) &&
         !SparedFor(s, actor, tactic, /*engaged=*/actor.id == target_id_);
}

bool CompanionBrain::WalkableCell(const Situation& s, CellPosition cell) {
  const WalkMap* map = s.facts.walk_map;
  return map == nullptr || map->Passable(cell.x, cell.y);
}

bool CompanionBrain::IsRuleEnemy(const Situation& s, const Actor& actor) const {
  return actor.kind == ActorKind::kMonster && CanBeTargeted(actor) &&
         SummonSideOf(s.world, actor, s.side) != SummonSide::kOwnSide &&
         WithinCells(s.self_cell, StandingCell(actor), kCompanionSightCells);
}

float CompanionBrain::WeightOf(const Situation& s, const Actor& monster) const {
  const CompanionTactic* tactic = TacticOf(s, monster);
  return tactic != nullptr ? tactic->weight : kPlainWeight;
}

float CompanionBrain::AttackersWeight(const Situation& s, std::uint32_t victim_id) const {
  float weight = 0.0F;
  for (const auto& [actor_id, actor] : s.world.actors()) {
    if (IsRuleEnemy(s, actor) && IsAttacking(actor, victim_id, s.now_ms)) {
      weight += WeightOf(s, actor);
    }
  }
  return weight;
}

}  // namespace ro::sim
