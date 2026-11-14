// LES CLÉS DU CODEX : chaque entrée des règles livrées en a une, et deux
// entrées d'une même catégorie ne la partagent jamais.

#include "survivors/survivors_codex_keys.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/survivors_content.h"
#include "ro/sim/survivors_rules.h"

#include "survivors/survivors_gems.h"

using moonclient::CodexCategory;
using moonclient::CodexCategoryKey;
using moonclient::CodexCategoryOf;
using moonclient::CodexEventKey;
using moonclient::CodexFoeKey;
using moonclient::CodexGemKey;
using moonclient::CodexItemKey;
using moonclient::CodexPlantKey;
using moonclient::CodexSkillKey;
using moonclient::kCodexCategoryCount;

namespace {

[[nodiscard]] const ro::sim::SurvivorsRules& Rules() { return ro::sim::ShippedSurvivorsRules(); }

// Vrai quand chaque clé est non vide et unique.
[[nodiscard]] bool AllDistinct(std::vector<std::string> keys) {
  if (std::ranges::any_of(keys, [](const std::string& key) { return key.empty(); })) return false;
  std::ranges::sort(keys);
  return std::ranges::adjacent_find(keys) == keys.end();
}

}  // namespace

TEST_CASE("Clés du Codex : une par entrée, aucune répétée dans sa catégorie",
          "[app][survivors][codex]") {
  std::vector<std::string> skills;
  for (const auto& skill : Rules().skills) skills.push_back(CodexSkillKey(skill));
  CHECK(AllDistinct(skills));

  std::vector<std::string> foes;
  for (const auto& foe : Rules().foes) foes.push_back(CodexFoeKey(foe.foe_id));
  CHECK(AllDistinct(foes));

  std::vector<std::string> plants;
  for (const auto& plant : Rules().plants) plants.push_back(CodexPlantKey(plant.foe_id));
  CHECK(AllDistinct(plants));

  std::vector<std::string> items;
  for (const auto& pickup : Rules().pickups) items.emplace_back(CodexItemKey(pickup.kind));
  for (const auto& colour : moonclient::kGemColours) {
    items.emplace_back(CodexGemKey(colour.item_id));
  }
  items.emplace_back(CodexItemKey(ro::sim::PickupKind::kCard));
  CHECK(AllDistinct(items));

  std::vector<std::string> events;
  for (const auto& event : Rules().events) events.push_back(CodexEventKey(event));
  events.emplace_back(moonclient::kCodexElitesKey);
  events.emplace_back(moonclient::kCodexNightKey);
  CHECK(AllDistinct(events));
}

TEST_CASE("Clés du Codex : la forme des clés", "[app][survivors][codex]") {
  CHECK(CodexFoeKey(1002) == "1002");
  CHECK(CodexPlantKey(1078) == "1078");
  CHECK(std::string(CodexItemKey(ro::sim::PickupKind::kHolyWater)) == "holy_water");
  CHECK(std::string(CodexGemKey(moonclient::kEmperiumItem)) == "emperium_gem");
  CHECK(std::string(CodexGemKey(1)).empty());
  const ro::sim::EventRule swarm{.kind = ro::sim::EventKind::kSwarm, .foe_id = 1011};
  CHECK(CodexEventKey(swarm) == "swarm_1011");
  for (std::size_t index = 0; index < kCodexCategoryCount; ++index) {
    const auto category = static_cast<CodexCategory>(index);
    CHECK(CodexCategoryOf(CodexCategoryKey(category)) == category);
  }
  CHECK_FALSE(CodexCategoryOf("relics").has_value());
}

TEST_CASE("Clés du Codex : chaque sorte d'objet qui a une règle a sa clé",
          "[app][survivors][codex]") {
  std::vector<std::string> keys;
  for (const auto& pickup : Rules().pickups) {
    REQUIRE(ro::sim::FindPickup(Rules(), pickup.kind) != nullptr);
    keys.emplace_back(CodexItemKey(pickup.kind));
    CHECK(keys.back() != CodexItemKey(ro::sim::PickupKind::kCard));
  }
  CHECK(AllDistinct(keys));
  CHECK(keys.size() == Rules().pickups.size());
}
