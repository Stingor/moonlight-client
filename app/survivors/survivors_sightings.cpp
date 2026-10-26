#include "survivors/survivors_sightings.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "ro/sim/survivors_content.h"
#include "ro/sim/survivors_state.h"

#include "survivors/arena_night.h"
#include "survivors/survivors_gems.h"

namespace moonclient {
namespace {

[[nodiscard]] std::vector<SurvivorsCodexMark>& ListOf(SurvivorsCodexSeen& seen,
                                                      CodexCategory category) {
  return seen.lists[static_cast<std::size_t>(category)];
}

void NoteSkill(SurvivorsCodexSeen& seen, const ro::sim::SurvivorsRules& rules,
               std::uint16_t skill_id) {
  if (const ro::sim::SurvivorsSkillRule* skill = ro::sim::FindSkill(rules, skill_id)) {
    seen.Add(CodexCategory::kSkills, CodexSkillKey(*skill));
  }
}

[[nodiscard]] bool OffersSkill(ro::sim::OfferKind kind) noexcept {
  return kind == ro::sim::OfferKind::kNewSkill || kind == ro::sim::OfferKind::kSkillLevel;
}

void NoteSkills(SurvivorsCodexSeen& seen, const ro::sim::SurvivorsField& field,
                const ro::sim::SurvivorsNews& news) {
  const ro::sim::SurvivorsRules& rules = *field.rules;
  for (const ro::sim::OwnedSkill& owned : field.hero.skills) NoteSkill(seen, rules, owned.skill_id);
  for (const ro::sim::SurvivorsOffer& offer : field.offers) {
    if (OffersSkill(offer.kind)) NoteSkill(seen, rules, offer.id);
  }
  for (const ro::sim::SurvivorsEvolutionNews& evolution : news.evolutions) {
    NoteSkill(seen, rules, evolution.to_skill_id);
  }
}

void NoteFoes(SurvivorsCodexSeen& seen, const ro::sim::SurvivorsField& field,
              const ro::sim::SurvivorsNews& news) {
  for (const ro::sim::SurvivorsFoe& foe : field.foes) {
    seen.Add(CodexCategory::kFoes, CodexFoeKey(foe.foe_id));
    if (foe.elite) seen.Add(CodexCategory::kEvents, kCodexElitesKey);
  }
  for (const ro::sim::SurvivorsFall& fall : news.falls) {
    seen.Add(CodexCategory::kFoes, CodexFoeKey(fall.foe_id));
  }
}

void NoteGround(SurvivorsCodexSeen& seen, const ro::sim::SurvivorsField& field,
                const ro::sim::SurvivorsNews& news) {
  const ro::sim::SpawnFrame& frame = field.rules->spawn_frame;
  const ro::sim::FieldPoint hero = field.hero.position;
  for (const ro::sim::SurvivorsPlant& plant : field.plants) {
    if (InHeroFrame(frame, hero, plant.position)) {
      seen.Add(CodexCategory::kPlants, CodexPlantKey(plant.foe_id));
    }
  }
  for (const ro::sim::SurvivorsPickup& pickup : field.pickups) {
    if (InHeroFrame(frame, hero, pickup.position)) {
      seen.Add(CodexCategory::kItems, CodexItemKey(pickup.kind));
    }
  }
  for (const ro::sim::SurvivorsGem& gem : field.gems) {
    if (InHeroFrame(frame, hero, gem.position)) {
      seen.Add(CodexCategory::kItems, CodexGemKey(GemItemOf(gem.exp)));
    }
  }
  for (const ro::sim::SurvivorsPickupNews& taken : news.pickups_taken) {
    seen.Add(CodexCategory::kItems, CodexItemKey(taken.kind));
  }
  if (!news.cards_found.empty()) {
    seen.Add(CodexCategory::kItems, CodexItemKey(ro::sim::PickupKind::kCard));
  }
}

void NoteEvents(SurvivorsCodexSeen& seen, const ro::sim::SurvivorsField& field,
                std::uint64_t presentation_ms) {
  const ro::sim::SurvivorsRules& rules = *field.rules;
  const std::size_t passed = std::min(field.next_event, rules.events.size());
  for (std::size_t index = 0; index < passed; ++index) {
    seen.Add(CodexCategory::kEvents, CodexEventKey(rules.events[index]));
  }
  if (presentation_ms >= kArenaNightFallsMs) seen.Add(CodexCategory::kEvents, kCodexNightKey);
}

}  // namespace

bool SurvivorsCodexSeen::Has(CodexCategory category, std::string_view key) const {
  return std::ranges::any_of(
      lists[static_cast<std::size_t>(category)],
      [key](const SurvivorsCodexMark& mark) { return !mark.foreign && mark.text == key; });
}

bool SurvivorsCodexSeen::Add(CodexCategory category, std::string_view key) {
  if (key.empty() || Has(category, key)) return false;
  ListOf(*this, category).push_back(SurvivorsCodexMark{.text = std::string(key)});
  return true;
}

bool Merge(SurvivorsCodexSeen& into, const SurvivorsCodexSeen& from) {
  bool changed = false;
  for (std::size_t index = 0; index < kCodexCategoryCount; ++index) {
    const auto category = static_cast<CodexCategory>(index);
    for (const SurvivorsCodexMark& mark : from.lists[index]) {
      if (!mark.foreign && into.Add(category, mark.text)) changed = true;
    }
  }
  return changed;
}

// Le cadre entier, bornes comprises : une borne haute de ce que montre l'image.
bool InHeroFrame(const ro::sim::SpawnFrame& frame, ro::sim::FieldPoint hero,
                 ro::sim::FieldPoint at) noexcept {
  const std::int64_t across = std::int64_t{at.x} - hero.x;
  const std::int64_t north = std::int64_t{at.y} - hero.y;
  return across >= -std::int64_t{frame.half_width_units} &&
         across <= std::int64_t{frame.half_width_units} &&
         north >= -std::int64_t{frame.south_units} && north <= std::int64_t{frame.north_units};
}

void NoteSightings(SurvivorsCodexSeen& seen, const ro::sim::SurvivorsField& field,
                   const ro::sim::SurvivorsNews& news, std::uint64_t presentation_ms) {
  if (field.rules == nullptr) return;
  NoteSkills(seen, field, news);
  NoteFoes(seen, field, news);
  NoteGround(seen, field, news);
  NoteEvents(seen, field, presentation_ms);
}

}  // namespace moonclient
