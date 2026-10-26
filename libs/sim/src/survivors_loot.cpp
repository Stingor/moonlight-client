#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <vector>

#include "ro/core/pcg32.h"
#include "ro/sim/survivors_content.h"
#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_ground.h"
#include "ro/sim/survivors_news.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"
#include "ro/sim/survivors_state.h"
#include "ro/sim/survivors_terrain.h"

#include "survivors_space.h"
#include "survivors_systems.h"
#include "survivors_volley.h"

namespace ro::sim::survivors {

// LE BUTIN : les plantes, posées au départ sur une carte, à l'éveil de leur
// morceau sur un sol généré, les objets au sol que le héros
// ramasse à son contact, et les coffres, qui font monter ce qu'il possède et
// évoluer un skill au plus haut niveau dont il porte la carte. Rien n'y est
// choisi par le joueur : un coffre s'applique seul, et le journal n'en sait
// rien.

namespace {

template <typename Counter>
void CountOne(Counter& counter) noexcept {
  if (counter < std::numeric_limits<Counter>::max()) ++counter;
}

[[nodiscard]] std::uint32_t SaturatedSum(std::uint32_t a, std::int64_t b) noexcept {
  return static_cast<std::uint32_t>(
      std::clamp<std::int64_t>(std::int64_t{a} + b, 0, std::numeric_limits<std::uint32_t>::max()));
}

// Le pas de fin d'un effet de `ms` pris à ce pas : il vaut dès le pas suivant,
// pendant ses pas entiers, et ne raccourcit jamais un effet en cours.
[[nodiscard]] std::uint32_t LastsUntil(std::uint32_t current_until, std::uint32_t step,
                                       std::uint32_t ms) noexcept {
  const std::uint64_t until = std::uint64_t{step} + 1 + StepsOf(ms);
  return std::max(current_until, static_cast<std::uint32_t>(std::min<std::uint64_t>(
                                     until, std::numeric_limits<std::uint32_t>::max())));
}

[[nodiscard]] bool Banished(const SurvivorsField& field, std::uint16_t id) {
  return std::find(field.banished.begin(), field.banished.end(), id) != field.banished.end();
}

[[nodiscard]] bool OwnsSkill(const SurvivorsHero& hero, std::uint16_t skill_id) {
  return std::any_of(hero.skills.begin(), hero.skills.end(),
                     [&](const OwnedSkill& owned) { return owned.skill_id == skill_id; });
}

[[nodiscard]] bool OwnsCard(const SurvivorsHero& hero, std::uint16_t card_item_id) {
  return std::any_of(hero.cards.begin(), hero.cards.end(),
                     [&](const OwnedCard& owned) { return owned.card_item_id == card_item_id; });
}

// ── Les coffres ────────────────────────────────────────────────────────────

// L'évolution que le skill de l'emplacement `slot` peut prendre : skill au
// plus haut niveau, carte possédée à tout niveau, évolué pas encore possédé.
// nullptr sinon.
[[nodiscard]] const EvolutionRule* DueEvolution(const SurvivorsField& field, std::size_t slot) {
  const SurvivorsRules& rules = *field.rules;
  const OwnedSkill& owned = field.hero.skills[slot];
  const SurvivorsSkillRule* rule = FindSkill(rules, owned.skill_id);
  const EvolutionRule* evolution = FindEvolutionOf(rules, owned.skill_id);
  if (rule == nullptr || evolution == nullptr || rule->levels.empty() ||
      std::size_t{owned.level} != rule->levels.size()) {
    return nullptr;
  }
  if (!OwnsCard(field.hero, evolution->card_item_id) ||
      OwnsSkill(field.hero, evolution->evolved_skill_id) ||
      FindSkill(rules, evolution->evolved_skill_id) == nullptr) {
    return nullptr;
  }
  return evolution;
}

// L'ÉVOLUTION : l'emplacement garde son index et prend l'évolué au niveau 1,
// prêt à tirer dès ce pas, une orbite au début de sa phase.
void Evolve(SurvivorsField& field, std::size_t slot, const EvolutionRule& evolution) {
  OwnedSkill& owned = field.hero.skills[slot];
  const std::uint16_t from_skill_id = owned.skill_id;
  owned = OwnedSkill{.skill_id = evolution.evolved_skill_id,
                     .level = 1,
                     .ready_step = field.step,
                     .phase_step = field.step};
  CountOne(field.hero.evolutions);
  CountOne(field.tally.evolutions);
  if (field.news.evolutions.size() < kMostNewsEvolutions) {
    field.news.evolutions.push_back(SurvivorsEvolutionNews{.slot = static_cast<std::uint8_t>(slot),
                                                           .from_skill_id = from_skill_id,
                                                           .to_skill_id = owned.skill_id});
  } else {
    ++field.news.dropped;
  }
}

// Une montée possible par un coffre : un skill (par son emplacement) ou une
// carte (par son index) possédés, sous leur plus haut niveau.
struct Growth {
  bool card = false;
  std::size_t index = 0;
};

// Les montées possibles, skills par emplacement puis cartes par index ; un Id
// banni ne monte plus.
void GrowthCandidates(const SurvivorsField& field, std::vector<Growth>& candidates) {
  const SurvivorsRules& rules = *field.rules;
  const SurvivorsHero& hero = field.hero;
  candidates.clear();
  for (std::size_t slot = 0; slot < hero.skills.size(); ++slot) {
    const OwnedSkill& owned = hero.skills[slot];
    const SurvivorsSkillRule* rule = FindSkill(rules, owned.skill_id);
    if (rule == nullptr || Banished(field, owned.skill_id)) continue;
    if (std::size_t{owned.level} < rule->levels.size()) candidates.push_back({false, slot});
  }
  for (std::size_t index = 0; index < hero.cards.size(); ++index) {
    const OwnedCard& owned = hero.cards[index];
    const CardRule* rule = FindCard(rules, owned.card_item_id);
    if (rule == nullptr || Banished(field, owned.card_item_id)) continue;
    if (std::size_t{owned.level} < rule->per_level.size()) candidates.push_back({true, index});
  }
}

// Un gain de coffre qui n'est pas une évolution : une montée tirée par le flux
// du butin parmi les candidats, au poids `offer_weight_owned` chacun ; sans
// candidat (ou d'un poids nul), des zenys, sans tirage.
[[nodiscard]] SurvivorsChestGain Grow(SurvivorsField& field, std::vector<Growth>& candidates,
                                      std::vector<std::uint32_t>& weights) {
  const SurvivorsRules& rules = *field.rules;
  SurvivorsHero& hero = field.hero;
  GrowthCandidates(field, candidates);
  std::size_t picked = candidates.size();
  if (!candidates.empty()) {
    weights.assign(candidates.size(), rules.tuning.offer_weight_owned);
    picked = field.loot_random.PickWeighted(weights);
  }
  if (picked >= candidates.size()) {
    hero.zeny = SaturatedSum(hero.zeny, rules.tuning.zeny_offer);
    return SurvivorsChestGain{.kind = ChestGainKind::kZeny, .id = 0, .level = 0};
  }
  const Growth growth = candidates[picked];
  if (growth.card) {
    OwnedCard& owned = hero.cards[growth.index];
    ++owned.level;
    return SurvivorsChestGain{
        .kind = ChestGainKind::kCardLevel, .id = owned.card_item_id, .level = owned.level};
  }
  OwnedSkill& owned = hero.skills[growth.index];
  ++owned.level;
  // Une orbite montée de niveau repart au début de sa phase active.
  const SurvivorsSkillRule* rule = FindSkill(rules, owned.skill_id);
  if (rule != nullptr && rule->archetype == SkillArchetype::kOrbit) owned.phase_step = field.step;
  return SurvivorsChestGain{
      .kind = ChestGainKind::kSkillLevel, .id = owned.skill_id, .level = owned.level};
}

// LE COFFRE : un Below(10000) du flux du butin fixe le nombre de gains —
// `quintuple` sous `quintuple_per_10000 × (100 + chance) / 100`, `triple` sous
// `triple_per_10000 × (100 + chance) / 100`, `single` sinon. Les évolutions
// dues passent en tête, par emplacement croissant, une fois atteint
// `evolution_from_ms` ; chaque gain restant est une montée (Grow).
void OpenChest(SurvivorsField& field, FieldPoint at) {
  const SurvivorsRules& rules = *field.rules;
  const ChestRule& chest = rules.chests;
  const std::uint32_t draw = field.loot_random.Below(kChestDrawBound);
  const std::int64_t luck_pct = field.hero.stats.Of(SurvivorsStat::kLuckPct);
  std::size_t gains = chest.single;
  if (draw < Scaled(chest.quintuple_per_10000, luck_pct)) {
    gains = chest.quintuple;
  } else if (draw < Scaled(chest.triple_per_10000, luck_pct)) {
    gains = chest.triple;
  }
  gains = std::min(gains, kMostChestGains);

  SurvivorsChestNews news{.at = at};
  const auto push = [&](const SurvivorsChestGain& gain) { news.gains[news.gain_count++] = gain; };
  if (std::uint64_t{field.step} * kSurvivorsStepMs >= chest.evolution_from_ms) {
    for (std::size_t slot = 0; slot < field.hero.skills.size() && news.gain_count < gains; ++slot) {
      const EvolutionRule* evolution = DueEvolution(field, slot);
      if (evolution == nullptr) continue;
      Evolve(field, slot, *evolution);
      push({.kind = ChestGainKind::kEvolution, .id = evolution->evolved_skill_id, .level = 1});
    }
  }
  std::vector<Growth> candidates;
  std::vector<std::uint32_t> weights;
  while (news.gain_count < gains) push(Grow(field, candidates, weights));

  RecomputeStats(field);
  CountOne(field.tally.chests_opened);
  if (field.news.chests.size() < kMostNewsChests) {
    field.news.chests.push_back(news);
  } else {
    ++field.news.dropped;
  }
}

// ── Les objets ─────────────────────────────────────────────────────────────

// HOLY WATER : tout monstre commun tangible (élites comprises) à moins de
// `reach_units` du héros tombe, compté comme un tué, avec sa gemme et son
// coffre ; aucun boss.
void Purify(SurvivorsField& field, std::int32_t reach_units) {
  const std::int64_t reach = reach_units;
  const FieldPoint hero = field.hero.position;
  for (SurvivorsFoe& foe : field.foes) {
    if (!Tangible(field, foe) || IsBoss(*field.rules, foe)) continue;
    if (DistanceSquared(foe.position, hero) < reach * reach) foe.hp = 0;
  }
  BuryFallen(field);
}

// BUBBLE GUM : chaque gemme que l'aimant n'a pas encore saisie l'est.
void SeizeGems(SurvivorsField& field) {
  const std::int32_t pull_start = std::max(field.rules->tuning.gem_pull_start_units, 1);
  for (SurvivorsGem& gem : field.gems) {
    if (gem.pull_units == 0) gem.pull_units = pull_start;
  }
}

// L'effet d'un objet ramassé : un coffre s'ouvre ; les autres suivent leur
// règle, et sont sans effet sans elle.
void Apply(SurvivorsField& field, const SurvivorsPickup& pickup) {
  if (pickup.kind == PickupKind::kChest) {
    OpenChest(field, pickup.position);
    return;
  }
  const PickupRule* rule = FindPickup(*field.rules, pickup.kind);
  if (rule == nullptr) return;
  SurvivorsHero& hero = field.hero;
  switch (pickup.kind) {
    case PickupKind::kMeat:
      hero.hp = static_cast<std::int32_t>(
          std::min<std::int64_t>(hero.max_hp, std::int64_t{hero.hp} + std::max(rule->amount, 0)));
      break;
    case PickupKind::kGold:
      hero.zeny = SaturatedSum(hero.zeny, std::max(rule->amount, 0));
      break;
    case PickupKind::kManual:
      hero.exp_boost_until_step =
          LastsUntil(hero.exp_boost_until_step, field.step, rule->duration_ms);
      break;
    case PickupKind::kHaste:
      hero.haste_until_step = LastsUntil(hero.haste_until_step, field.step, rule->duration_ms);
      break;
    case PickupKind::kHolyWater:
      Purify(field, rule->reach_units);
      break;
    case PickupKind::kMagnetAll:
      SeizeGems(field);
      break;
    case PickupKind::kChest:  // ouvert plus haut
    case PickupKind::kCard:   // ramassée par GatherPickups, sans effet
      break;
  }
}

// La carte ramassée : au récapitulatif, et une nouvelle sous son plafond.
void TakeCard(SurvivorsField& field, const SurvivorsPickup& card) {
  field.recap.cards_found.push_back(card.item_id);
  if (field.news.cards_found.size() < kMostNewsCards) {
    field.news.cards_found.push_back(
        SurvivorsCardNews{.card_item_id = card.item_id, .at = card.position});
  } else {
    ++field.news.dropped;
  }
}

// ── Les plantes ────────────────────────────────────────────────────────────

// Vrai si les règles posent des plantes : une espèce, un bloc et une densité.
[[nodiscard]] bool PlantsPosed(const SurvivorsRules& rules) noexcept {
  return !rules.plants.empty() && rules.tuning.plant_block_cells > 0 &&
         rules.tuning.plant_cells_per_plant != 0;
}

[[nodiscard]] std::vector<std::uint32_t> PlantWeights(const SurvivorsRules& rules) {
  std::vector<std::uint32_t> weights;
  weights.reserve(rules.plants.size());
  for (const PlantRule& plant : rules.plants) weights.push_back(plant.weight);
  return weights;
}

// LA PLANTE D'UN BLOC, parmi ses cases praticables `open` : un Chance du
// nombre de ces cases sur `plant_cells_per_plant`, même pour un bloc vide
// (le décor d'un bloc ne décale pas les suivants) ; si oui, un Below pour la
// case et un PickWeighted pour l'espèce.
void PlantBlock(SurvivorsField& field, core::Pcg32& random, std::span<const std::uint32_t> weights,
                std::span<const FieldPoint> open) {
  const SurvivorsRules& rules = *field.rules;
  const auto open_count = static_cast<std::uint32_t>(open.size());
  if (!random.Chance(open_count, rules.tuning.plant_cells_per_plant) || open.empty()) return;
  const FieldPoint at = open[random.Below(open_count)];
  const std::size_t pick = random.PickWeighted(weights);
  if (pick >= rules.plants.size()) return;
  field.plants.push_back(SurvivorsPlant{
      .serial = field.next_serial++, .foe_id = rules.plants[pick].foe_id, .position = at});
}

// L'éveil d'un morceau : ses blocs, du sud au nord puis d'ouest en est,
// comptés depuis son coin sud-ouest et rognés à son bord.
void WakeChunk(SurvivorsField& field, std::span<const std::uint32_t> weights, std::int32_t chunk_x,
               std::int32_t chunk_y) {
  const TerrainChunk* chunk = field.ground.ChunkAt(chunk_x, chunk_y);
  if (chunk == nullptr) return;
  // Une copie : le pointeur du sol ne vaut que jusqu'à sa prochaine question.
  const TerrainChunk woken = *chunk;
  const std::int32_t block = field.rules->tuning.plant_block_cells;
  const std::int32_t west = chunk_x * kTerrainChunkCells;
  const std::int32_t south = chunk_y * kTerrainChunkCells;
  core::Pcg32 random(TerrainPlantSeed(field.start.seed, chunk_x, chunk_y), kFieldStream);
  std::vector<FieldPoint> open;
  for (std::int32_t block_y = 0; block_y < kTerrainChunkCells; block_y += block) {
    for (std::int32_t block_x = 0; block_x < kTerrainChunkCells; block_x += block) {
      open.clear();
      const std::int32_t last_y = std::min(block_y + block, kTerrainChunkCells);
      const std::int32_t last_x = std::min(block_x + block, kTerrainChunkCells);
      for (std::int32_t y = block_y; y < last_y; ++y) {
        for (std::int32_t x = block_x; x < last_x; ++x) {
          if (woken.Passable(x, y)) open.push_back(CellCenter(west + x, south + y));
        }
      }
      PlantBlock(field, random, weights, open);
    }
  }
}

}  // namespace

void PlantField(SurvivorsField& field) {
  if (field.rules == nullptr || !field.ground.bounded()) return;
  const SurvivorsRules& rules = *field.rules;
  if (!PlantsPosed(rules)) return;
  const std::int32_t block = rules.tuning.plant_block_cells;
  const std::vector<std::uint32_t> weights = PlantWeights(rules);

  const SurvivorsGround& ground = field.ground;
  std::vector<FieldPoint> open;
  for (std::int32_t block_y = 0; block_y < ground.height(); block_y += block) {
    for (std::int32_t block_x = 0; block_x < ground.width(); block_x += block) {
      open.clear();
      const std::int32_t last_y = std::min(block_y + block, ground.height());
      const std::int32_t last_x = std::min(block_x + block, ground.width());
      for (std::int32_t y = block_y; y < last_y; ++y) {
        for (std::int32_t x = block_x; x < last_x; ++x) {
          if (ground.Passable(x, y)) open.push_back(CellCenter(x, y));
        }
      }
      PlantBlock(field, field.field_random, weights, open);
    }
  }
}

void WakePlantChunks(SurvivorsField& field) {
  if (field.rules == nullptr || field.ground.bounded()) return;
  const SurvivorsRules& rules = *field.rules;
  const std::int32_t wake = rules.tuning.plant_wake_cells;
  if (!PlantsPosed(rules) || wake <= 0) return;
  const std::int32_t hero_x = CellOf(field.hero.position.x);
  const std::int32_t hero_y = CellOf(field.hero.position.y);
  constexpr std::int32_t kLastChunk = kTerrainWorldChunks - 1;
  const std::int32_t west = std::clamp(TerrainChunkOf(hero_x - wake), 0, kLastChunk);
  const std::int32_t east = std::clamp(TerrainChunkOf(hero_x + wake), 0, kLastChunk);
  const std::int32_t south = std::clamp(TerrainChunkOf(hero_y - wake), 0, kLastChunk);
  const std::int32_t north = std::clamp(TerrainChunkOf(hero_y + wake), 0, kLastChunk);
  std::vector<std::uint32_t> weights;
  for (std::int32_t chunk_y = south; chunk_y <= north; ++chunk_y) {
    for (std::int32_t chunk_x = west; chunk_x <= east; ++chunk_x) {
      const std::uint32_t key = PlantChunkKey(chunk_x, chunk_y);
      std::vector<std::uint32_t>& woken = field.woken_chunks;
      const auto at = std::lower_bound(woken.begin(), woken.end(), key);
      if (at != woken.end() && *at == key) continue;
      woken.insert(at, key);
      if (weights.empty()) weights = PlantWeights(rules);
      WakeChunk(field, weights, chunk_x, chunk_y);
    }
  }
}

void ForgetFarPlants(SurvivorsField& field) {
  if (field.rules == nullptr || field.ground.bounded()) return;
  const std::int64_t forget = field.rules->tuning.plant_forget_units;
  if (forget <= 0) return;
  const FieldPoint hero = field.hero.position;
  std::erase_if(field.plants, [&](const SurvivorsPlant& plant) {
    const std::int64_t dx = std::int64_t{plant.position.x} - hero.x;
    const std::int64_t dy = std::int64_t{plant.position.y} - hero.y;
    return std::max(dx < 0 ? -dx : dx, dy < 0 ? -dy : dy) > forget;
  });
}

void DropCard(SurvivorsField& field, const SurvivorsFoe& foe, const FoeRule& rule) {
  if (rule.card_item_id == 0) return;
  bool dropped = false;
  if (rule.rank != FoeRank::kCommon &&
      std::find(field.card_guarantees_spent.begin(), field.card_guarantees_spent.end(),
                rule.foe_id) == field.card_guarantees_spent.end()) {
    field.card_guarantees_spent.push_back(rule.foe_id);
    dropped = true;
  } else {
    dropped = field.card_random.Chance(1, field.rules->card_drops.common_odds);
  }
  if (!dropped) return;
  field.pickups.push_back(SurvivorsPickup{.serial = field.next_card_serial,
                                          .kind = PickupKind::kCard,
                                          .item_id = rule.card_item_id,
                                          .position = foe.position});
  if (field.next_card_serial < std::numeric_limits<std::uint32_t>::max()) {
    ++field.next_card_serial;
  }
  if (field.news.cards_dropped.size() < kMostNewsCards) {
    field.news.cards_dropped.push_back(
        SurvivorsCardNews{.card_item_id = rule.card_item_id, .at = foe.position});
  } else {
    ++field.news.dropped;
  }
}

void GatherPickups(SurvivorsField& field) {
  if (field.rules == nullptr) return;
  const SurvivorsTuning& tuning = field.rules->tuning;
  const FieldPoint hero = field.hero.position;
  BreakPlantsWithin(field, hero, hero, tuning.hero_radius_units);
  if (field.pickups.empty()) return;
  // Retrait stable d'abord, effets ensuite, dans l'ordre de chute : un effet
  // qui fait tomber un coffre (Holy Water) le pose pour le pas suivant.
  const std::int64_t reach = std::int64_t{tuning.hero_radius_units} + tuning.pickup_radius_units;
  std::vector<SurvivorsPickup> taken;
  std::size_t kept = 0;
  for (std::size_t index = 0; index < field.pickups.size(); ++index) {
    const SurvivorsPickup pickup = field.pickups[index];
    if (DistanceSquared(pickup.position, hero) < reach * reach) {
      taken.push_back(pickup);
      continue;
    }
    field.pickups[kept++] = pickup;
  }
  field.pickups.resize(kept);
  for (const SurvivorsPickup& pickup : taken) {
    if (pickup.kind == PickupKind::kCard) {
      TakeCard(field, pickup);
      continue;
    }
    if (field.news.pickups_taken.size() < kMostNewsPickups) {
      field.news.pickups_taken.push_back(SurvivorsPickupNews{
          .kind = pickup.kind, .item_id = pickup.item_id, .at = pickup.position});
    } else {
      ++field.news.dropped;
    }
    Apply(field, pickup);
  }
}

}  // namespace ro::sim::survivors
