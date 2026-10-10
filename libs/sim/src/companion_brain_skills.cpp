// LES SKILLS AUTOMATIQUES DU CERVEAU D'UN COMPAGNON EN COMBAT : sur sa cible,
// avec ce que sa tactique en dit, et sur les monstres harcelables. Voir
// `ro/sim/companion_ai.h`.

#include <algorithm>
#include <vector>

#include "ro/sim/companion_ai.h"
#include "ro/sim/companion_tactics.h"
#include "ro/sim/movement.h"
#include "ro/sim/world.h"

#include "companion_brain_detail.h"

namespace ro::sim {

std::optional<CompanionOrder> CompanionBrain::TrySkill(const Situation& s, const Actor& target,
                                                       CellPosition target_cell,
                                                       bool in_attack_range) {
  const OffensiveSkills mode = s.settings.skills.offensive;
  // UNE CIBLE À ÉVITER se combat à distance : « en combat » n'attend pas le
  // contact pour elle.
  const bool avoided = !target_direct_ && AvoidThresholdOf(s, target).has_value();
  if (mode == OffensiveSkills::kNever ||
      (mode == OffensiveSkills::kInCombat && !in_attack_range && !avoided) ||
      !SkillsAllowedOn(s, target)) {
    return std::nullopt;
  }
  const std::optional<TunedSkills> tuned = TunedForTarget(s);
  if (!tuned.has_value()) {
    return std::nullopt;
  }
  const SkillMoment moment = MomentOf(s, target_cell);
  std::optional<SkillPick> pick;
  // LE SKILL D'OUVERTURE, une fois au premier contact de chaque cible ; tant
  // qu'il ne peut pas partir, les autres skills restent permis. Il ne compte
  // comme fait qu'une fois parti (sa trace).
  const bool opening = !target_direct_ && target_tactic_.has_value() &&
                       target_tactic_->opening_skill_id != 0 && !opened_.contains(target_id_);
  if (opening) {
    pick = skills_.ChooseOpening(tuned->settings, s.facts.skills, moment,
                                 target_tactic_->opening_skill_id);
    if (pick.has_value()) {
      opening_wanted_[target_id_] = pick->skill_id;
    }
  }
  if (!pick.has_value()) {
    pick = skills_.Choose(tuned->settings, s.facts.skills, moment, tuned->forced_level);
  }
  if (!pick.has_value()) {
    return std::nullopt;
  }
  skills_.NoteSent(*pick, target_id_, s.now_ms);
  // UN SKILL COMPTE COMME UNE ATTAQUE pour la borne des cibles qui ne reçoivent
  // aucun coup : en « skills seuls », c'est la seule chose qui part.
  if (!unanswered_since_ms_.has_value()) {
    unanswered_since_ms_ = s.now_ms;
  }
  return Emit(s,
              CompanionOrder{.kind = CompanionOrder::Kind::kSkill,
                             .target_id = target_id_,
                             .skill_id = pick->skill_id,
                             .skill_level = pick->level},
              target_direct_ ? CompanionAiReason::kDirectAttack : target_reason_);
}

std::optional<CompanionBrain::TunedSkills> CompanionBrain::TunedFor(const Situation& s,
                                                                    const CompanionTactic* tactic,
                                                                    std::uint32_t target_id) const {
  TunedSkills tuned{.settings = s.settings.skills};
  if (tactic == nullptr) {
    return tuned;
  }
  if (tactic->sp_reserve_points != kTacticSpReserveFromBase) {
    tuned.settings.sp_reserve_points = tactic->sp_reserve_points;
  }
  // L'USAGE : les skills partis sur cette cible, et ceux qui attendent leur
  // trace. Un refus n'en consomme aucun.
  const auto found = skill_uses_.find(target_id);
  const int uses = (found != skill_uses_.end() ? found->second : 0) + skills_.PendingOn(target_id);
  switch (tactic->skill_use) {
    case TacticSkillUse::kNever:
      return std::nullopt;
    case TacticSkillUse::kTimes:
      if (uses >= tactic->skill_use_count) {
        return std::nullopt;
      }
      break;
    case TacticSkillUse::kOnceAtLevel:
      if (uses >= 1) {
        return std::nullopt;
      }
      tuned.forced_level = static_cast<std::uint16_t>(tactic->skill_use_count);
      break;
    case TacticSkillUse::kAlways:
      break;
  }
  return tuned;
}

std::optional<CompanionBrain::TunedSkills> CompanionBrain::TunedForTarget(
    const Situation& s) const {
  // Un ordre direct n'est jamais soumis aux tactiques.
  const CompanionTactic* tactic =
      !target_direct_ && target_tactic_.has_value() ? &*target_tactic_ : nullptr;
  return TunedFor(s, tactic, target_id_);
}

void CompanionBrain::CountTracedSkills(std::uint32_t now_ms) {
  for (const SkillTrace& trace : skills_.TakeTraced()) {
    // LE SKILL D'UNE RÈGLE compte pour elle seule : son délai et son quota.
    if (trace.rule_index != kNoRule) {
      // Une règle d'une liste qui a changé ne compte plus rien.
      if (trace.rule_index >= 0 &&
          static_cast<std::size_t>(trace.rule_index) < rule_states_.size()) {
        CountRuleUse(static_cast<std::size_t>(trace.rule_index), trace.target_id, now_ms);
      }
      continue;
    }
    ++skill_uses_[trace.target_id];
    if (const auto wanted = opening_wanted_.find(trace.target_id);
        wanted != opening_wanted_.end() && wanted->second == trace.skill_id) {
      opened_.insert(trace.target_id);
      opening_wanted_.erase(wanted);
    }
  }
}

std::optional<CompanionOrder> CompanionBrain::TryHarass(const Situation& s) {
  if (target_direct_ || s.settings.skills.offensive == OffensiveSkills::kNever ||
      ArrivalHolding(s)) {
    return std::nullopt;
  }
  struct Candidate {
    const Actor* actor = nullptr;
    const CompanionTactic* tactic = nullptr;
    int distance_cells = 0;
  };
  std::vector<Candidate> candidates;
  for (const auto& [actor_id, actor] : s.world.actors()) {
    if (actor_id == target_id_ || actor.kind != ActorKind::kMonster ||
        harassed_.contains(actor_id) || !CanBeTargeted(actor) ||
        SummonSideOf(s.world, actor, s.side) == SummonSide::kOwnSide) {
      continue;
    }
    const CompanionTactic* tactic = TacticOf(s, actor);
    const CellPosition cell = StandingCell(actor);
    if (tactic == nullptr || !tactic->harass ||
        !WithinCells(s.self_cell, cell, kCompanionSightCells) || SparedFor(s, actor, tactic)) {
      continue;
    }
    candidates.push_back(Candidate{
        .actor = &actor, .tactic = tactic, .distance_cells = WholeCellDistance(s.self_cell, cell)});
  }
  std::ranges::sort(candidates, [](const Candidate& a, const Candidate& b) {
    return a.distance_cells != b.distance_cells ? a.distance_cells < b.distance_cells
                                                : a.actor->id < b.actor->id;
  });
  for (const Candidate& candidate : candidates) {
    // SON USAGE À LUI : « jamais », « N fois », « une fois au niveau L ».
    const std::optional<TunedSkills> tuned = TunedFor(s, candidate.tactic, candidate.actor->id);
    if (!tuned.has_value()) {
      continue;
    }
    const std::optional<SkillPick> pick = skills_.Choose(
        tuned->settings, s.facts.skills,
        MomentOf(s, companion_detail::Plain(StandingCell(*candidate.actor))), tuned->forced_level);
    if (!pick.has_value()) {
      continue;
    }
    harassed_.insert(candidate.actor->id);
    skills_.NoteSent(*pick, candidate.actor->id, s.now_ms);
    return Emit(s,
                CompanionOrder{.kind = CompanionOrder::Kind::kSkill,
                               .target_id = candidate.actor->id,
                               .skill_id = pick->skill_id,
                               .skill_level = pick->level},
                CompanionAiReason::kHarass);
  }
  return std::nullopt;
}

bool CompanionBrain::SkillsAllowedOn(const Situation& s, const Actor& target) const {
  // ⚠ LE DÉLAI D'ARRIVÉE BRIDE AUSSI LES SKILLS, automatiques comme d'ordre
  // direct : tout ce qui part du cerveau attend la fin du délai que le serveur
  // pose après chaque entrée du maître sur une carte.
  if (ArrivalHolding(s)) {
    return false;
  }
  // PAS DE SKILL sur le monstre d'un autre joueur, même attaqué sur ordre.
  const CompanionTactic* tactic =
      !target_direct_ && target_tactic_.has_value() ? &*target_tactic_ : nullptr;
  return !(target.kind == ActorKind::kMonster &&
           SparedFor(s, target, tactic, /*engaged=*/target.id == target_id_));
}

SkillMoment CompanionBrain::MomentOf(const Situation& s, CellPosition target_cell) const {
  return SkillMoment{.self = s.self,
                     .self_cell = s.self_cell,
                     .target_cell = target_cell,
                     .stopped = !IsWalking(s.self, s.now_ms) && MoveSettled(s.now_ms),
                     .sp = s.facts.sp,
                     .sp_known = s.facts.sp_known,
                     .now_ms = s.now_ms};
}

}  // namespace ro::sim
