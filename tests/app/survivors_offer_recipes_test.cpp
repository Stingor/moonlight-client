// LES RECETTES D'UNE OFFRE DU CHOIX DE NIVEAU : les combos et les évolutions
// qu'un sort ou une carte concerne, ce qu'elle complète, et ce que le Codex en a
// vu.

#include "game/survivors_offer_recipes.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/survivors_content.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_state.h"

#include "survivors/survivors_codex_keys.h"

namespace {

using moonclient::CodexCategory;
using moonclient::SurvivorsCodexSeen;
using moonclient::SurvivorsOfferRecipe;
using ro::sim::OfferKind;
using ro::sim::SurvivorsOffer;

constexpr std::uint16_t kNapalmBeatId = 11;
constexpr std::uint16_t kSoulStrikeId = 13;
constexpr std::uint16_t kNapalmVulcanId = 400;
constexpr std::uint16_t kBashId = 5;
constexpr std::uint16_t kHeadCrushId = 398;
constexpr std::uint16_t kGolemCardId = 4072;

[[nodiscard]] std::optional<SurvivorsOfferRecipe> Giving(
    const std::vector<SurvivorsOfferRecipe>& recipes, std::uint16_t evolved_skill_id) {
  const auto found =
      std::ranges::find(recipes, evolved_skill_id, &SurvivorsOfferRecipe::evolved_skill_id);
  if (found == recipes.end()) return std::nullopt;
  return *found;
}

void See(const ro::sim::SurvivorsRules& rules, SurvivorsCodexSeen& seen, std::uint16_t skill_id) {
  const ro::sim::SurvivorsSkillRule* skill = ro::sim::FindSkill(rules, skill_id);
  REQUIRE(skill != nullptr);
  seen.Add(CodexCategory::kSkills, moonclient::CodexSkillKey(*skill));
}

}  // namespace

TEST_CASE("Recettes survivors : un sort offert dit son évolution et sa carte", "[app][survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  const ro::sim::SurvivorsHero hero;
  const SurvivorsCodexSeen seen;
  const std::vector<SurvivorsOfferRecipe> recipes = moonclient::SurvivorsOfferRecipesOf(
      rules, hero, SurvivorsOffer{.kind = OfferKind::kNewSkill, .id = kBashId, .next_level = 1},
      seen, {});
  const std::optional<SurvivorsOfferRecipe> head_crush = Giving(recipes, kHeadCrushId);
  REQUIRE(head_crush.has_value());
  CHECK(head_crush->partner_id == kGolemCardId);
  CHECK_FALSE(head_crush->partner_is_skill);
  CHECK_FALSE(head_crush->union_of_two);
  CHECK_FALSE(head_crush->completes);
  // Rien n'est vu au Codex : la carte et l'évolué ne sont que des silhouettes.
  CHECK_FALSE(head_crush->partner_seen);
  CHECK_FALSE(head_crush->evolved_seen);
}

TEST_CASE("Recettes survivors : une carte offerte complète le sort que le héros tient",
          "[app][survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  ro::sim::SurvivorsHero hero;
  hero.skills.push_back(ro::sim::OwnedSkill{.skill_id = kBashId, .level = 3});
  SurvivorsCodexSeen seen;
  See(rules, seen, kBashId);
  const std::vector<std::uint16_t> found_cards = {kGolemCardId};
  const std::vector<SurvivorsOfferRecipe> recipes = moonclient::SurvivorsOfferRecipesOf(
      rules, hero, SurvivorsOffer{.kind = OfferKind::kNewCard, .id = kGolemCardId, .next_level = 1},
      seen, found_cards);
  const std::optional<SurvivorsOfferRecipe> head_crush = Giving(recipes, kHeadCrushId);
  REQUIRE(head_crush.has_value());
  CHECK(head_crush->partner_id == kBashId);
  CHECK(head_crush->partner_is_skill);
  CHECK(head_crush->completes);
  CHECK(head_crush->partner_seen);
  // L'évolué, jamais vu, reste une silhouette.
  CHECK_FALSE(head_crush->evolved_seen);

  // Le héros qui tient déjà l'évolué n'a plus rien à compléter.
  hero.skills.push_back(ro::sim::OwnedSkill{.skill_id = kHeadCrushId, .level = 1});
  CHECK_FALSE(
      Giving(moonclient::SurvivorsOfferRecipesOf(
                 rules, hero,
                 SurvivorsOffer{.kind = OfferKind::kCardLevel, .id = kGolemCardId, .next_level = 2},
                 seen, found_cards),
             kHeadCrushId)
          .has_value());
}

TEST_CASE("Recettes survivors : un combo se lit depuis ses deux sorts", "[app][survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  ro::sim::SurvivorsHero hero;
  hero.skills.push_back(ro::sim::OwnedSkill{.skill_id = kSoulStrikeId, .level = 2});
  SurvivorsCodexSeen seen;
  See(rules, seen, kSoulStrikeId);
  See(rules, seen, kNapalmVulcanId);
  for (const std::uint16_t offered : {kNapalmBeatId, kSoulStrikeId}) {
    const std::uint16_t other = offered == kNapalmBeatId ? kSoulStrikeId : kNapalmBeatId;
    const std::optional<SurvivorsOfferRecipe> vulcan =
        Giving(moonclient::SurvivorsOfferRecipesOf(
                   rules, hero,
                   SurvivorsOffer{.kind = OfferKind::kSkillLevel, .id = offered, .next_level = 3},
                   seen, {}),
               kNapalmVulcanId);
    REQUIRE(vulcan.has_value());
    CHECK(vulcan->union_of_two);
    CHECK(vulcan->partner_is_skill);
    CHECK(vulcan->partner_id == other);
    CHECK(vulcan->evolved_seen);
    // Le héros tient Soul Strike : Napalm Beat complète le combo, l'inverse non.
    CHECK(vulcan->completes == (offered == kNapalmBeatId));
    CHECK(vulcan->partner_seen == (other == kSoulStrikeId));
  }
}

TEST_CASE("Recettes survivors : un soin ni des zenys n'ont aucune recette", "[app][survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  const ro::sim::SurvivorsHero hero;
  const SurvivorsCodexSeen seen;
  CHECK(moonclient::SurvivorsOfferRecipesOf(rules, hero, SurvivorsOffer{.kind = OfferKind::kHeal},
                                            seen, {})
            .empty());
  CHECK(moonclient::SurvivorsOfferRecipesOf(rules, hero, SurvivorsOffer{.kind = OfferKind::kZeny},
                                            seen, {})
            .empty());
}
