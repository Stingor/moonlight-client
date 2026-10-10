// Voir l'en-tête.

#include "ro/sim/companion_skills.h"

#include <algorithm>
#include <array>
#include <utility>

#include "ro/sim/companion_sight.h"
#include "ro/sim/skill_book.h"

namespace ro::sim {
namespace {

// ── CE QUI NE PART JAMAIS DE LUI-MÊME ───────────────────────────────────────
//
// S.B.R.44 (Filir) : une fois touché, le serveur ramène l'intimité de
// l'homoncule à « déteste ».
constexpr std::uint16_t kSkillSbr44 = 8012;
// Bio Explosion (Vanilmirth) : l'homoncule s'autodétruit, et son intimité tombe
// à « déteste ».
constexpr std::uint16_t kSkillBioExplosion = 8016;
// Overed Boost : à sa fin, l'homoncule perd cinquante de satiété.
constexpr std::uint16_t kSkillOveredBoost = 8023;
// Scapegoat : le mercenaire donne ses PV au maître et en meurt, ce qui met fin
// à son contrat.
constexpr std::uint16_t kSkillScapegoat = 8235;
constexpr std::array<std::uint16_t, 4> kNeverAutomatic = {kSkillSbr44, kSkillBioExplosion,
                                                          kSkillOveredBoost, kSkillScapegoat};

// ── LES DEBUFFS ─────────────────────────────────────────────────────────────
//
// Les skills de compagnon qui visent l'ennemi sans dégâts, d'après la base de
// skills pré-renewal du serveur. Ce sont des debuffs ; ils relèvent des
// tactiques par monstre (un champ « Debuff » éventuel) et des règles si/alors.
//
// ⚠ BRANDISH SPEAR N'Y EST PAS : la base le marque sans dégâts, mais ses dégâts
// passent par sa zone, et il blesse bien la cible.
constexpr std::uint16_t kSkillProvoke = 8232;
constexpr std::uint16_t kSkillDecreaseAgi = 8234;
constexpr std::uint16_t kSkillLexDivina = 8236;
constexpr std::uint16_t kSkillSense = 8237;
constexpr std::uint16_t kSkillMindBlaster = 8241;
constexpr std::array<std::uint16_t, 5> kDebuffs = {kSkillProvoke, kSkillDecreaseAgi,
                                                   kSkillLexDivina, kSkillSense, kSkillMindBlaster};

// Le temps écoulé de `since_ms` à `now_ms`, en signé : négatif quand le fait
// est daté après l'instant de la décision.
[[nodiscard]] std::int64_t ElapsedMs(std::uint32_t now_ms, std::uint32_t since_ms) noexcept {
  return static_cast<std::int64_t>(now_ms) - static_cast<std::int64_t>(since_ms);
}

[[nodiscard]] bool Casting(const Actor& self, std::uint32_t now_ms) noexcept {
  return self.cast.has_value() && !self.cast->cancelled &&
         now_ms - self.cast->started_ms < self.cast->duration_ms;
}

[[nodiscard]] const SkillPermission* PermissionOf(const CompanionSkillSettings& settings,
                                                  std::uint16_t skill_id) noexcept {
  const auto found = std::ranges::find_if(
      settings.permissions,
      [skill_id](const SkillPermission& permission) { return permission.skill_id == skill_id; });
  return found != settings.permissions.end() ? &*found : nullptr;
}

// Le niveau qui partira, borné à l'appris ; zéro quand le skill est décoché.
[[nodiscard]] std::uint16_t PermittedLevel(const CompanionSkillSettings& settings,
                                           const CompanionSkillFact& skill) noexcept {
  const SkillPermission* permission = PermissionOf(settings, skill.skill_id);
  if (permission == nullptr) {
    return skill.level;
  }
  if (!permission->allowed) {
    return 0;
  }
  return permission->level == 0 ? skill.level : std::min(permission->level, skill.level);
}

}  // namespace

std::optional<std::uint32_t> LastTraceMs(const Actor& self, std::uint16_t skill_id) noexcept {
  if (skill_id == 0) {
    return std::nullopt;
  }
  std::optional<std::uint32_t> latest;
  const auto note = [&latest](std::uint32_t seen_ms) {
    if (!latest.has_value() || ElapsedMs(seen_ms, *latest) > 0) {
      latest = seen_ms;
    }
  };
  if (self.cast.has_value() && self.cast->skill_id == skill_id) {
    note(self.cast->started_ms);
  }
  if (self.attack.has_value() && self.attack->skill_id == skill_id) {
    note(self.attack->started_ms);
  }
  if (self.skill_use.has_value() && self.skill_use->skill_id == skill_id) {
    note(self.skill_use->started_ms);
  }
  if (self.ground_skill.has_value() && self.ground_skill->skill_id == skill_id) {
    note(self.ground_skill->started_ms);
  }
  return latest;
}

bool NeverCastAutomatically(std::uint16_t skill_id) noexcept {
  return std::ranges::find(kNeverAutomatic, skill_id) != kNeverAutomatic.end();
}

bool IsCompanionDebuff(std::uint16_t skill_id) noexcept {
  return std::ranges::find(kDebuffs, skill_id) != kDebuffs.end();
}

bool IsOffensiveCompanionSkill(const CompanionSkillFact& skill) noexcept {
  return skill.level > 0 && (skill.targeting & kSkillOnEnemy) != 0 &&
         (skill.targeting & kSkillOnGround) == 0 && !IsCompanionDebuff(skill.skill_id) &&
         !NeverCastAutomatically(skill.skill_id);
}

void CompanionSkillCaster::Observe(const Actor& self, std::uint32_t now_ms) {
  std::erase_if(suspensions_, [now_ms](const Suspension& suspension) {
    return ElapsedMs(now_ms, suspension.until_ms) >= 0;
  });
  // UNE TRACE consomme l'attente : le skill est parti.
  const auto traced = [&self](const Pending& pending) {
    const std::optional<std::uint32_t> seen_ms = LastTraceMs(self, pending.skill_id);
    return seen_ms.has_value() && ElapsedMs(*seen_ms, pending.sent_ms) >= 0;
  };
  if (last_emitted_.has_value() && traced(*last_emitted_)) {
    last_emitted_.reset();
  }
  // L'ÉMOTICÔNE « PENSER » est le refus du DERNIER skill émis, s'il l'a
  // précédée de peu : une émoticône plus ancienne que l'envoi ne le vise pas.
  const bool thinking = self.emotion.has_value() && self.emotion->pack == kBaseEmotionPack &&
                        self.emotion->id == kEmotionThink;
  if (thinking && last_emitted_.has_value()) {
    const std::int64_t after_ms = ElapsedMs(self.emotion_since_ms, last_emitted_->sent_ms);
    if (after_ms > 0 && after_ms < static_cast<std::int64_t>(kSkillTraceMs)) {
      Refuse(*last_emitted_, now_ms);
      last_emitted_.reset();
    }
  }
  std::vector<Pending> silent;
  std::erase_if(pending_, [&](const Pending& pending) {
    if (traced(pending)) {
      traced_.push_back(SkillTrace{.skill_id = pending.skill_id,
                                   .target_id = pending.target_id,
                                   .rule_index = pending.rule_index});
      return true;
    }
    if (ElapsedMs(now_ms, pending.sent_ms) >= static_cast<std::int64_t>(kSkillTraceMs)) {
      silent.push_back(pending);
      return true;
    }
    return false;
  });
  for (const Pending& pending : silent) {
    Refuse(pending, now_ms);
  }
}

void CompanionSkillCaster::NoteRefused(std::uint16_t skill_id, std::uint32_t now_ms) {
  // Le skill émis que ce refus vise : celui qui attend sa trace, sinon le
  // dernier émis.
  Pending refused{.skill_id = skill_id};
  if (const auto found = std::ranges::find_if(
          pending_, [skill_id](const Pending& pending) { return pending.skill_id == skill_id; });
      found != pending_.end()) {
    refused = *found;
  } else if (last_emitted_.has_value() && last_emitted_->skill_id == skill_id) {
    refused = *last_emitted_;
  }
  Refuse(refused, now_ms);
}

void CompanionSkillCaster::Refuse(const Pending& refused, std::uint32_t now_ms) {
  const std::uint16_t skill_id = refused.skill_id;
  std::erase_if(pending_,
                [skill_id](const Pending& pending) { return pending.skill_id == skill_id; });
  if (refused.rule_index != kNoRule) {
    if (refused.rule_index != kOrphanRule) {
      refused_rules_.push_back(refused.rule_index);
    }
    return;
  }
  std::erase_if(suspensions_, [skill_id](const Suspension& suspension) {
    return suspension.skill_id == skill_id;
  });
  suspensions_.push_back(
      Suspension{.skill_id = skill_id, .until_ms = now_ms + kSkillRefusalSuspendMs});
}

bool CompanionSkillCaster::Held(const CompanionSkillSettings& settings,
                                const SkillMoment& moment) const {
  if (settings.offensive == OffensiveSkills::kNever || !moment.sp_known ||
      Casting(moment.self, moment.now_ms)) {
    return true;
  }
  return last_sent_ms_.has_value() &&
         ElapsedMs(moment.now_ms, *last_sent_ms_) < static_cast<std::int64_t>(settings.delay_ms);
}

std::uint16_t CompanionSkillCaster::CastableLevel(const CompanionSkillSettings& settings,
                                                  const CompanionSkillFact& skill,
                                                  const SkillMoment& moment) const {
  if (!IsOffensiveCompanionSkill(skill) || !CastableNow(settings, skill, moment)) {
    return 0;
  }
  return PermittedLevel(settings, skill);
}

bool CompanionSkillCaster::CastableNow(const CompanionSkillSettings& settings,
                                       const CompanionSkillFact& skill,
                                       const SkillMoment& moment) const {
  if (skill.level == 0 || skill.cooldown_remaining_ms > 0 ||
      Suspended(skill.skill_id, moment.now_ms)) {
    return false;
  }
  // UN SKILL QUI ATTEND SA TRACE ne repart pas : on attend la trace, ou la fin
  // du délai qui le tient pour refusé.
  if (std::ranges::any_of(pending_, [&skill](const Pending& pending) {
        return pending.skill_id == skill.skill_id;
      })) {
    return false;
  }
  // ⚠ LA SP À GARDER SE COMPARE AU COÛT DU NIVEAU APPRIS, le seul que le livre
  // donne : un skill réglé plus bas, qui coûterait moins, peut être retenu à
  // tort, jamais lancé à tort.
  return moment.sp - skill.sp_cost >= settings.sp_reserve_points;
}

std::optional<SkillPick> CompanionSkillCaster::Choose(const CompanionSkillSettings& settings,
                                                      std::span<const CompanionSkillFact> skills,
                                                      const SkillMoment& moment,
                                                      std::uint16_t forced_level) const {
  if (!moment.stopped || Held(settings, moment)) {
    return std::nullopt;
  }
  for (const CompanionSkillFact& skill : skills) {
    const std::uint16_t level = CastableLevel(settings, skill, moment);
    if (level == 0 ||
        !WithinCells(moment.self_cell, moment.target_cell, std::max(skill.range_cells, 1))) {
      continue;
    }
    return SkillPick{.skill_id = skill.skill_id,
                     .level = forced_level > 0 ? std::min(forced_level, skill.level) : level};
  }
  return std::nullopt;
}

std::optional<SkillPick> CompanionSkillCaster::ChooseOpening(
    const CompanionSkillSettings& settings, std::span<const CompanionSkillFact> skills,
    const SkillMoment& moment, std::uint16_t skill_id) const {
  if (!moment.stopped || Held(settings, moment)) {
    return std::nullopt;
  }
  const auto found = std::ranges::find_if(
      skills, [skill_id](const CompanionSkillFact& skill) { return skill.skill_id == skill_id; });
  if (found == skills.end() || (found->targeting & kSkillOnEnemy) == 0 ||
      NeverCastAutomatically(skill_id) || !CastableNow(settings, *found, moment) ||
      !WithinCells(moment.self_cell, moment.target_cell, std::max(found->range_cells, 1))) {
    return std::nullopt;
  }
  return SkillPick{.skill_id = skill_id, .level = found->level};
}

std::optional<int> CompanionSkillCaster::ReachNow(const CompanionSkillSettings& settings,
                                                  std::span<const CompanionSkillFact> skills,
                                                  const SkillMoment& moment) const {
  if (Held(settings, moment)) {
    return std::nullopt;
  }
  std::optional<int> longest;
  for (const CompanionSkillFact& skill : skills) {
    if (CastableLevel(settings, skill, moment) > 0) {
      longest = std::max(longest.value_or(0), std::max(skill.range_cells, 1));
    }
  }
  return longest;
}

bool CompanionSkillCaster::RuleCastable(const CompanionSkillFact& skill, std::uint32_t delay_ms,
                                        std::int64_t sp_reserve_points,
                                        const SkillMoment& moment) const {
  if (!moment.sp_known || Casting(moment.self, moment.now_ms) ||
      (last_sent_ms_.has_value() &&
       ElapsedMs(moment.now_ms, *last_sent_ms_) < static_cast<std::int64_t>(delay_ms))) {
    return false;
  }
  CompanionSkillSettings reserve;
  reserve.sp_reserve_points = sp_reserve_points;
  return CastableNow(reserve, skill, moment);
}

void CompanionSkillCaster::NoteSent(const SkillPick& pick, std::uint32_t target_id,
                                    std::uint32_t now_ms, int rule_index) {
  last_sent_ms_ = now_ms;
  const Pending sent{.skill_id = pick.skill_id,
                     .sent_ms = now_ms,
                     .target_id = target_id,
                     .rule_index = rule_index};
  last_emitted_ = sent;
  pending_.push_back(sent);
}

std::vector<SkillTrace> CompanionSkillCaster::TakeTraced() { return std::exchange(traced_, {}); }

std::vector<int> CompanionSkillCaster::TakeRefusedRules() {
  return std::exchange(refused_rules_, {});
}

void CompanionSkillCaster::RemapRuleSkills(std::span<const int> old_to_new) noexcept {
  const auto remapped = [old_to_new](int rule_index) {
    if (rule_index < 0) {
      return rule_index;
    }
    return static_cast<std::size_t>(rule_index) < old_to_new.size()
               ? old_to_new[static_cast<std::size_t>(rule_index)]
               : kOrphanRule;
  };
  for (Pending& pending : pending_) {
    pending.rule_index = remapped(pending.rule_index);
  }
  if (last_emitted_.has_value()) {
    last_emitted_->rule_index = remapped(last_emitted_->rule_index);
  }
  for (SkillTrace& trace : traced_) {
    trace.rule_index = remapped(trace.rule_index);
  }
  std::vector<int> refused;
  for (const int rule_index : refused_rules_) {
    if (const int moved = remapped(rule_index); moved >= 0) {
      refused.push_back(moved);
    }
  }
  refused_rules_ = std::move(refused);
}

int CompanionSkillCaster::PendingOfRule(int rule_index) const noexcept {
  return static_cast<int>(std::ranges::count_if(
      pending_, [rule_index](const Pending& pending) { return pending.rule_index == rule_index; }));
}

int CompanionSkillCaster::PendingOfRuleOn(int rule_index, std::uint32_t target_id) const noexcept {
  return static_cast<int>(
      std::ranges::count_if(pending_, [rule_index, target_id](const Pending& pending) {
        return pending.rule_index == rule_index && pending.target_id == target_id;
      }));
}

int CompanionSkillCaster::PendingOn(std::uint32_t target_id) const noexcept {
  return PendingOfRuleOn(kNoRule, target_id);
}

void CompanionSkillCaster::NoteDirectSent(std::uint16_t skill_id, std::uint32_t now_ms) {
  last_sent_ms_ = now_ms;
  last_emitted_ = Pending{.skill_id = skill_id, .sent_ms = now_ms};
}

bool CompanionSkillCaster::Suspended(std::uint16_t skill_id, std::uint32_t now_ms) const noexcept {
  return std::ranges::any_of(suspensions_, [skill_id, now_ms](const Suspension& suspension) {
    return suspension.skill_id == skill_id && ElapsedMs(now_ms, suspension.until_ms) < 0;
  });
}

std::vector<std::pair<std::uint16_t, std::uint32_t>> CompanionSkillCaster::SuspensionsLeft(
    std::uint32_t now_ms) const {
  std::vector<std::pair<std::uint16_t, std::uint32_t>> left;
  for (const Suspension& suspension : suspensions_) {
    if (const std::int64_t remaining_ms = ElapsedMs(suspension.until_ms, now_ms);
        remaining_ms > 0) {
      left.emplace_back(suspension.skill_id, static_cast<std::uint32_t>(remaining_ms));
    }
  }
  return left;
}

void CompanionSkillCaster::Suspend(std::uint16_t skill_id, std::uint32_t until_ms) {
  std::erase_if(suspensions_, [skill_id](const Suspension& suspension) {
    return suspension.skill_id == skill_id;
  });
  suspensions_.push_back(Suspension{.skill_id = skill_id, .until_ms = until_ms});
}

std::optional<int> CompanionSkillCaster::LongestReach(const CompanionSkillSettings& settings,
                                                      std::span<const CompanionSkillFact> skills) {
  std::optional<int> longest;
  for (const CompanionSkillFact& skill : skills) {
    if (IsOffensiveCompanionSkill(skill) && PermittedLevel(settings, skill) > 0) {
      longest = std::max(longest.value_or(0), std::max(skill.range_cells, 1));
    }
  }
  return longest;
}

}  // namespace ro::sim
