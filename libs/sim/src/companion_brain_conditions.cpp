// LES CONDITIONS DES RÈGLES SI/ALORS du cerveau d'un compagnon : ce qu'elles
// lisent du monde et de l'IA, et l'inconnu tenu pour faux. Voir
// `ro/sim/companion_rules.h`.

#include <algorithm>
#include <cstdint>
#include <map>
#include <optional>

#include "ro/sim/companion_ai.h"
#include "ro/sim/companion_rules.h"
#include "ro/sim/monster_sheet.h"
#include "ro/sim/movement.h"
#include "ro/sim/world.h"

#include "companion_brain_detail.h"

namespace ro::sim {

using companion_detail::Casting;
using companion_detail::HealthPercent;
using companion_detail::Measured;
using companion_detail::MsOf;
using companion_detail::PercentOf;
using companion_detail::Plain;
using companion_detail::SignedElapsedMs;
using companion_detail::Stated;

void CompanionBrain::NoteRuleFacts(const Situation& s) {
  const auto attacked = [&s](std::uint32_t victim_id) {
    return std::ranges::any_of(s.world.actors(), [&s, victim_id](const auto& entry) {
      return !s.side.Includes(entry.first) && IsAttacking(entry.second, victim_id, s.now_ms);
    });
  };
  const auto track = [&s](bool now_attacked, std::optional<std::uint32_t>& since_ms) {
    if (!now_attacked) {
      since_ms.reset();
    } else if (!since_ms.has_value()) {
      since_ms = s.now_ms;
    }
  };
  track(attacked(s.owner.id), owner_attacked_since_ms_);
  track(attacked(s.self.id), self_attacked_since_ms_);
  // TOUTES LES TRACES : incantation, coup, emploi d'un skill de soutien ou de
  // soi, case visée.
  for (const std::uint16_t skill_id :
       {s.self.cast.has_value() ? s.self.cast->skill_id : std::uint16_t{0},
        s.self.attack.has_value() ? s.self.attack->skill_id : std::uint16_t{0},
        s.self.skill_use.has_value() ? s.self.skill_use->skill_id : std::uint16_t{0},
        s.self.ground_skill.has_value() ? s.self.ground_skill->skill_id : std::uint16_t{0}}) {
    if (const std::optional<std::uint32_t> seen_ms = LastTraceMs(s.self, skill_id);
        seen_ms.has_value()) {
      skill_seen_ms_[skill_id] = *seen_ms;
    }
  }
}

std::optional<bool> CompanionBrain::CompanionWears(const Situation& s, StatusEffectId status) {
  // 🔴 LA LISTE COMPLÈTE DU SERVEUR DÉCIDE LES DEUX FORMES : il distingue une
  // liste vide (rien n'est porté) d'une réponse inconnue, si bien qu'une liste
  // valide et fraîche dit aussi ce qui n'est PAS porté. Une pose ou un retrait
  // reçus après elle la complètent.
  const std::optional<std::uint32_t>& list_ms = s.facts.statuses_ms;
  if (list_ms.has_value() && SignedElapsedMs(s.now_ms, *list_ms) >= 0 &&
      SignedElapsedMs(s.now_ms, *list_ms) < kCompanionStatusListFreshMs) {
    if (const auto since = s.self.status_since_ms.find(status);
        since != s.self.status_since_ms.end() && SignedElapsedMs(since->second, *list_ms) > 0) {
      return s.self.status_effects.contains(status);
    }
    return std::ranges::find(s.facts.statuses, status) != s.facts.statuses.end();
  }
  // SANS LISTE, OU PÉRIMÉE : un statut vu est porté, un statut absent est
  // inconnu — le client ne voit pas tous ceux du compagnon.
  return s.self.status_effects.contains(status) ? std::optional(true) : std::nullopt;
}

bool CompanionBrain::RuleHolds(const Situation& s, const CompanionRule& rule) {
  return std::ranges::all_of(rule.conditions, [this, &s](const RuleCondition& condition) {
    return ConditionHolds(s, condition).value_or(false);
  });
}

std::optional<bool> CompanionBrain::ConditionHolds(const Situation& s,
                                                   const RuleCondition& condition) {
  const bool owner = condition.subject == RuleSubject::kOwner;
  const Actor* target = nullptr;
  if (target_id_ != 0) {
    target = s.world.Find(target_id_);
    if (target != nullptr && !CanBeTargeted(*target)) {
      target = nullptr;
    }
  }
  if (condition.subject == RuleSubject::kTarget &&
      condition.predicate != RulePredicate::kHpPercent) {
    return target != nullptr ? TargetFact(s, condition, *target) : std::nullopt;
  }
  switch (condition.predicate) {
    case RulePredicate::kHpPercent: {
      std::optional<std::int64_t> percent;
      if (condition.subject == RuleSubject::kTarget) {
        percent = target != nullptr ? HealthPercent(*target) : std::nullopt;
      } else if (owner) {
        percent = HealthPercent(s.owner);
      } else if (s.facts.max_hp > 0) {
        percent = PercentOf(s.facts.hp, s.facts.max_hp);
      }
      return Measured(percent, condition.comparison, condition.number);
    }
    case RulePredicate::kSpPercent:
      return Measured(s.facts.sp_known && s.facts.max_sp > 0
                          ? std::optional<std::int64_t>(PercentOf(s.facts.sp, s.facts.max_sp))
                          : std::nullopt,
                      condition.comparison, condition.number);
    case RulePredicate::kSpPoints:
      return Measured(s.facts.sp_known ? std::optional<std::int64_t>(s.facts.sp) : std::nullopt,
                      condition.comparison, condition.number);
    case RulePredicate::kAttackedFor: {
      const std::optional<std::uint32_t>& since =
          owner ? owner_attacked_since_ms_ : self_attacked_since_ms_;
      return since.has_value() && SignedElapsedMs(s.now_ms, *since) >= MsOf(condition.seconds);
    }
    case RulePredicate::kAttackersWeight:
      return AttackersWeight(s, owner ? s.owner.id : s.self.id) >=
             static_cast<float>(condition.number);
    case RulePredicate::kHasStatus: {
      if (owner) {
        const bool worn = std::ranges::any_of(
            s.world.player_statuses(),
            [&condition](const ActiveStatus& status) { return status.effect == condition.id; });
        return Stated(worn, condition.comparison);
      }
      return Stated(CompanionWears(s, condition.id), condition.comparison);
    }
    case RulePredicate::kSitting:
      return Stated(s.owner.posture == Posture::kSitting, condition.comparison);
    case RulePredicate::kActivity: {
      CompanionActivity activity = CompanionActivity::kResting;
      if (mode_ == Mode::kFight && target != nullptr) {
        activity =
            WithinCells(s.self_cell, StandingCell(*target), std::max(s.facts.attack_range_cells, 1))
                ? CompanionActivity::kFighting
                : CompanionActivity::kChasing;
      }
      return Stated(static_cast<int>(activity) == condition.choice, condition.comparison);
    }
    case RulePredicate::kCastWithin: {
      const auto recent = [&s, &condition](const std::map<std::uint16_t, std::uint32_t>& times) {
        const auto found = times.find(condition.id);
        return found != times.end() &&
               SignedElapsedMs(s.now_ms, found->second) < MsOf(condition.seconds);
      };
      if (recent(skill_seen_ms_)) {
        return Stated(true, condition.comparison);
      }
      // ⚠ SANS TRACE RÉCENTE, « N'A PAS LANCÉ » N'EST VRAI QUE SI LE CERVEAU SAIT
      // N'AVOIR RIEN ENVOYÉ depuis : un envoi encore sans trace est inconnu.
      if (recent(skill_sent_ms_)) {
        return std::nullopt;
      }
      return Stated(false, condition.comparison);
    }
    case RulePredicate::kAppearedWithin: {
      std::optional<std::uint32_t> latest = arrived_ms_;
      if (s.self.appeared_ms.has_value() &&
          (!latest.has_value() || SignedElapsedMs(*s.self.appeared_ms, *latest) > 0)) {
        latest = s.self.appeared_ms;
      }
      if (!latest.has_value()) {
        return std::nullopt;
      }
      return SignedElapsedMs(s.now_ms, *latest) < MsOf(condition.seconds);
    }
    case RulePredicate::kFartherThan:
      return WholeCellDistance(s.self_cell, s.owner_cell) > condition.cells;
    case RulePredicate::kEnemiesNear: {
      std::optional<CellPosition> centre;
      switch (static_cast<RuleAround>(condition.choice)) {
        case RuleAround::kCompanion:
          centre = s.self_cell;
          break;
        case RuleAround::kOwner:
          centre = s.owner_cell;
          break;
        case RuleAround::kTarget:
          if (target != nullptr) {
            centre = Plain(StandingCell(*target));
          }
          break;
      }
      if (!centre.has_value()) {
        return std::nullopt;
      }
      float weight = 0.0F;
      for (const auto& [actor_id, actor] : s.world.actors()) {
        if (IsRuleEnemy(s, actor) && WithinCells(*centre, StandingCell(actor), condition.cells)) {
          weight += WeightOf(s, actor);
        }
      }
      return weight >= static_cast<float>(condition.number);
    }
    default:
      break;
  }
  return std::nullopt;
}

std::optional<bool> CompanionBrain::TargetFact(const Situation& s, const RuleCondition& condition,
                                               const Actor& target) {
  const bool monster = target.kind == ActorKind::kMonster;
  switch (condition.predicate) {
    case RulePredicate::kRace:
    case RulePredicate::kElement:
    case RulePredicate::kSize:
    case RulePredicate::kMonsterRank: {
      if (!monster || s.facts.monster_sheets == nullptr) {
        return std::nullopt;
      }
      const MonsterSheet* sheet = s.facts.monster_sheets->Find(target.look.job);
      if (sheet == nullptr) {
        // LA FICHE ABSENTE SE DEMANDE ; en attendant, la condition est fausse.
        if (!s.facts.monster_sheets->IsUnknown(target.look.job)) {
          wanted_monster_sheet_ = target.look.job;
        }
        return std::nullopt;
      }
      int value = sheet->boss_rank;
      if (condition.predicate == RulePredicate::kRace) {
        value = sheet->race;
      } else if (condition.predicate == RulePredicate::kElement) {
        value = sheet->element;
      } else if (condition.predicate == RulePredicate::kSize) {
        value = sheet->size;
      }
      return Stated(value == condition.choice, condition.comparison);
    }
    case RulePredicate::kMonsterJob:
      return Stated(monster && target.look.job == condition.id, condition.comparison);
    case RulePredicate::kCasting:
      return Stated(Casting(target, s.now_ms), condition.comparison);
    case RulePredicate::kAttacks: {
      bool attacks = false;
      switch (static_cast<RuleWhom>(condition.choice)) {
        case RuleWhom::kOwner:
          attacks = IsAttacking(target, s.owner.id, s.now_ms);
          break;
        case RuleWhom::kCompanion:
          attacks = IsAttacking(target, s.self.id, s.now_ms);
          break;
        case RuleWhom::kFriend:
          attacks = std::ranges::any_of(s.friends, [&target, &s](const Actor* friend_actor) {
            return IsAttacking(target, friend_actor->id, s.now_ms);
          });
          break;
      }
      return Stated(attacks, condition.comparison);
    }
    default:
      break;
  }
  return std::nullopt;
}

}  // namespace ro::sim
