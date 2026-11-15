#include "game/survivors_offer_recipes.h"

#include <algorithm>

#include "ro/sim/survivors_content.h"

#include "survivors/survivors_codex_keys.h"

namespace moonclient {
namespace {

[[nodiscard]] bool SkillSeen(const ro::sim::SurvivorsRules& rules, const SurvivorsCodexSeen& seen,
                             std::uint16_t skill_id) {
  const ro::sim::SurvivorsSkillRule* skill = ro::sim::FindSkill(rules, skill_id);
  return skill != nullptr && seen.Has(CodexCategory::kSkills, CodexSkillKey(*skill));
}

[[nodiscard]] bool HoldsSkill(const ro::sim::SurvivorsHero& hero, std::uint16_t skill_id) {
  return std::ranges::any_of(hero.skills, [skill_id](const ro::sim::OwnedSkill& owned) {
    return owned.skill_id == skill_id;
  });
}

[[nodiscard]] bool HoldsCard(const ro::sim::SurvivorsHero& hero, std::uint16_t card_item_id) {
  return std::ranges::any_of(hero.cards, [card_item_id](const ro::sim::OwnedCard& owned) {
    return owned.card_item_id == card_item_id;
  });
}

}  // namespace

std::vector<SurvivorsOfferRecipe> SurvivorsOfferRecipesOf(
    const ro::sim::SurvivorsRules& rules, const ro::sim::SurvivorsHero& hero,
    const ro::sim::SurvivorsOffer& offer, const SurvivorsCodexSeen& seen,
    std::span<const std::uint16_t> found_cards) {
  std::vector<SurvivorsOfferRecipe> recipes;
  const bool skill_offer =
      offer.kind == ro::sim::OfferKind::kNewSkill || offer.kind == ro::sim::OfferKind::kSkillLevel;
  const bool card_offer =
      offer.kind == ro::sim::OfferKind::kNewCard || offer.kind == ro::sim::OfferKind::kCardLevel;
  if (!skill_offer && !card_offer) return recipes;
  for (const ro::sim::EvolutionRule& evolution : rules.evolutions) {
    if (HoldsSkill(hero, evolution.evolved_skill_id)) continue;
    const bool union_of_two = evolution.partner_skill_id != 0;
    SurvivorsOfferRecipe recipe{.evolved_skill_id = evolution.evolved_skill_id,
                                .union_of_two = union_of_two};
    if (skill_offer && evolution.skill_id == offer.id) {
      recipe.partner_id = union_of_two ? evolution.partner_skill_id : evolution.card_item_id;
      recipe.partner_is_skill = union_of_two;
    } else if (skill_offer && union_of_two && evolution.partner_skill_id == offer.id) {
      recipe.partner_id = evolution.skill_id;
      recipe.partner_is_skill = true;
    } else if (card_offer && !union_of_two && evolution.card_item_id == offer.id) {
      recipe.partner_id = evolution.skill_id;
      recipe.partner_is_skill = true;
    } else {
      continue;
    }
    if (recipe.partner_is_skill) {
      recipe.completes = HoldsSkill(hero, recipe.partner_id);
      recipe.partner_seen = SkillSeen(rules, seen, recipe.partner_id);
    } else {
      recipe.completes = HoldsCard(hero, recipe.partner_id);
      recipe.partner_seen = std::ranges::find(found_cards, recipe.partner_id) != found_cards.end();
    }
    recipe.evolved_seen = SkillSeen(rules, seen, recipe.evolved_skill_id);
    recipes.push_back(recipe);
  }
  return recipes;
}

}  // namespace moonclient
