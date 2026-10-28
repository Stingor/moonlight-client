#include "survivors/arena_effect_plan.h"

#include <algorithm>

#include "ro/render/effect_visibility.h"
#include "ro/sim/level_up_effect.h"

namespace moonclient {
namespace {

// L'apparence que le héros montre : celle de l'évolution dès la première.
[[nodiscard]] std::uint16_t HeroJobOf(const ro::sim::SurvivorsField& field) {
  if (field.hero_class == nullptr) return 0;
  const ro::sim::ClassRule& hero_class = *field.hero_class;
  return field.hero.evolutions > 0 && hero_class.evolved_job_id != 0 ? hero_class.evolved_job_id
                                                                     : hero_class.job_id;
}

// Ajoute un lancer si le budget lui fait place.
void Admit(ArenaEffectBudget& budget, const ArenaLaunch& launch, std::vector<ArenaLaunch>& out) {
  if (budget.Admit()) out.push_back(launch);
}

// L'effet qu'un lancer de skill fait naître, d'après sa sorte.
void PlanCast(const ro::sim::SurvivorsCast& cast, const SkillLook& look, ArenaEffectBudget& budget,
              std::vector<ArenaLaunch>& out) {
  switch (cast.kind) {
    case ro::sim::CastKind::kStrike:
    case ro::sim::CastKind::kBlast:
      Admit(budget,
            ArenaLaunch{
                .effect_id = look.effect_id, .kind = ArenaLaunchKind::kOnGround, .at = cast.to},
            out);
      return;
    case ro::sim::CastKind::kShot:
    case ro::sim::CastKind::kNova:
      Admit(budget,
            ArenaLaunch{.effect_id = look.effect_id,
                        .kind = look.bearer == SkillBearer::kHero ? ArenaLaunchKind::kOnHero
                                                                  : ArenaLaunchKind::kOnGround,
                        .at = look.bearer == SkillBearer::kHero ? cast.from : cast.to,
                        .target = cast.to},
            out);
      return;
    // Orbites et zones se lisent sur l'état : voir `PlanArenaOrbs`.
    case ro::sim::CastKind::kOrbitOn:
    case ro::sim::CastKind::kOrbitOff:
    case ro::sim::CastKind::kWallOn:
    case ro::sim::CastKind::kRingOn:
    case ro::sim::CastKind::kZoneOff:
      return;
  }
}

}  // namespace

void ArenaEffectBudget::BeginFrame(std::size_t living) noexcept {
  living_ = living;
  launched_ = 0;
  skipped_ = 0;
}

void ArenaEffectLives::Add(std::uint32_t ends_ms) { ends_ms_.push_back(ends_ms); }

void ArenaEffectLives::Expire(std::uint32_t now_ms) {
  if (now_ms < last_now_ms_) ends_ms_.clear();
  last_now_ms_ = now_ms;
  std::erase_if(ends_ms_, [now_ms](std::uint32_t ends_ms) { return ends_ms <= now_ms; });
}

void ArenaEffectLives::Clear() noexcept {
  ends_ms_.clear();
  last_now_ms_ = 0;
}

bool ArenaEffectBudget::Admit() noexcept {
  if (launched_ >= kMostArenaLaunchesPerFrame || living_ >= kMostArenaLivingEffects) {
    ++skipped_;
    ++skipped_total_;
    return false;
  }
  ++launched_;
  ++living_;
  return true;
}

ArenaSkillLooks::ArenaSkillLooks(const ro::sim::SurvivorsRules& rules) {
  looks_.reserve(rules.skills.size());
  for (const ro::sim::SurvivorsSkillRule& skill : rules.skills) {
    looks_.push_back(SkillLookOf(skill));
  }
}

const SkillLook* ArenaSkillLooks::Find(std::uint16_t skill_id) const noexcept {
  const auto found = std::ranges::find(looks_, skill_id, &SkillLook::skill_id);
  return found != looks_.end() ? &*found : nullptr;
}

void PlanArenaLaunches(const ro::sim::SurvivorsNews& news, const ro::sim::SurvivorsField& field,
                       const ArenaSkillLooks& looks, ArenaEffectSwitches switches,
                       ArenaEffectBudget& budget, std::vector<ArenaLaunch>& out) {
  out.clear();
  if (!switches.shown) return;
  if (!switches.reduced) {
    if (news.level_ups > 0) {
      Admit(budget,
            ArenaLaunch{.effect_id = ro::sim::BaseLevelUpEffectId(HeroJobOf(field)),
                        .kind = ArenaLaunchKind::kOnHero,
                        .at = field.hero.position},
            out);
    }
    if (!news.evolutions.empty()) {
      Admit(budget,
            ArenaLaunch{.effect_id = kJobChangeEffect,
                        .kind = ArenaLaunchKind::kOnHero,
                        .at = field.hero.position},
            out);
    }
    // UNE CARTE PRISE BRILLE SUR LE HÉROS, une fois par image.
    if (!news.cards_found.empty()) {
      Admit(budget,
            ArenaLaunch{.effect_id = kCardPickupEffect,
                        .kind = ArenaLaunchKind::kOnHero,
                        .at = field.hero.position},
            out);
    }
    for (const ro::sim::SurvivorsCast& cast : news.casts) {
      if (const SkillLook* look = looks.Find(cast.skill_id)) PlanCast(cast, *look, budget, out);
    }
  }
  for (const ro::sim::SurvivorsHit& hit : news.hits) {
    const SkillLook* look = looks.Find(hit.skill_id);
    if (look == nullptr) continue;
    Admit(budget,
          ArenaLaunch{.effect_id = switches.reduced ? kPlainHitEffect : look->hit_effect_id,
                      .kind = ArenaLaunchKind::kOnGround,
                      .at = hit.at},
          out);
  }
}

void PlanArenaOrbs(std::span<const ro::sim::SurvivorsOrb> orbs, const ArenaSkillLooks& looks,
                   ArenaEffectSwitches switches, std::vector<ArenaOrbLook>& out) {
  out.clear();
  if (!switches.shown || switches.reduced) return;
  for (const ro::sim::SurvivorsOrb& orb : orbs) {
    if (const SkillLook* look = looks.Find(orb.skill_id)) {
      out.push_back(ArenaOrbLook{.effect_id = look->effect_id, .at = orb.position});
    }
  }
}

bool ArenaZoneGroundShown(std::uint32_t effect_id, ArenaEffectSwitches switches) {
  return (switches.shown && !switches.reduced) || ro::render::IsForcedVisibleEffect(effect_id);
}

void PlanArenaZoneGrounds(std::span<const ro::sim::SurvivorsZone> zones,
                          const ArenaSkillLooks& looks, ArenaEffectSwitches switches,
                          std::vector<ArenaZoneGround>& out) {
  out.clear();
  for (const ro::sim::SurvivorsZone& zone : zones) {
    const SkillLook* look = looks.Find(zone.skill_id);
    if (look == nullptr || look->ground_effect_id == 0) continue;
    if (!ArenaZoneGroundShown(look->ground_effect_id, switches)) continue;
    out.push_back(ArenaZoneGround{.actor_id = kFirstZoneGroundActorId + zone.serial,
                                  .effect_id = look->ground_effect_id,
                                  .at = zone.from});
  }
}

}  // namespace moonclient
