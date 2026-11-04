#include "game/survivors_objective.h"

#include <algorithm>
#include <cstddef>

#include "ro/sim/survivors_content.h"

namespace moonclient {
namespace {

// Un évolué né d'un évolué se remonte ; au-delà de cette profondeur, une
// chaîne serait une boucle des règles, et le chemin s'arrête.
constexpr std::size_t kMostObjectiveDepth = 4;

void AddOnce(std::vector<std::uint16_t>& ids, std::uint16_t id) {
  if (id != 0 && std::ranges::find(ids, id) == ids.end()) ids.push_back(id);
}

void Walk(const ro::sim::SurvivorsRules& rules, std::uint16_t skill_id, std::size_t depth,
          SurvivorsObjectivePath& path) {
  if (depth > kMostObjectiveDepth || ro::sim::FindSkill(rules, skill_id) == nullptr) return;
  if (std::ranges::find(path.skill_ids, skill_id) != path.skill_ids.end()) return;
  path.skill_ids.push_back(skill_id);
  for (const ro::sim::EvolutionRule& evolution : rules.evolutions) {
    if (evolution.evolved_skill_id != skill_id) continue;
    Walk(rules, evolution.skill_id, depth + 1, path);
    if (evolution.partner_skill_id != 0) Walk(rules, evolution.partner_skill_id, depth + 1, path);
    AddOnce(path.card_item_ids, evolution.card_item_id);
  }
}

}  // namespace

SurvivorsObjectivePath SurvivorsObjectivePathOf(const ro::sim::SurvivorsRules& rules,
                                                std::uint16_t objective_skill_id) {
  SurvivorsObjectivePath path;
  if (objective_skill_id != 0) Walk(rules, objective_skill_id, 0, path);
  return path;
}

bool OfferServesObjective(const SurvivorsObjectivePath& path,
                          const ro::sim::SurvivorsOffer& offer) {
  switch (offer.kind) {
    case ro::sim::OfferKind::kNewSkill:
    case ro::sim::OfferKind::kSkillLevel:
      return std::ranges::find(path.skill_ids, offer.id) != path.skill_ids.end();
    case ro::sim::OfferKind::kNewCard:
    case ro::sim::OfferKind::kCardLevel:
      return std::ranges::find(path.card_item_ids, offer.id) != path.card_item_ids.end();
    case ro::sim::OfferKind::kHeal:
    case ro::sim::OfferKind::kZeny:
      return false;
  }
  return false;
}

}  // namespace moonclient
