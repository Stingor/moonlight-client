#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include "ro/sim/survivors_content.h"
#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_news.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"
#include "ro/sim/survivors_score.h"
#include "ro/sim/survivors_state.h"

#include "survivors_space.h"
#include "survivors_systems.h"

namespace ro::sim::survivors {

namespace {

constexpr std::uint32_t kWholeChance = 100;

[[nodiscard]] std::uint32_t SaturatedSum(std::uint32_t a, std::uint64_t b) noexcept {
  return static_cast<std::uint32_t>(
      std::min<std::uint64_t>(std::uint64_t{a} + b, std::numeric_limits<std::uint32_t>::max()));
}

template <typename Counter>
void CountOne(Counter& counter) noexcept {
  if (counter < std::numeric_limits<Counter>::max()) ++counter;
}

[[nodiscard]] bool Banished(const SurvivorsField& field, std::uint16_t id) {
  return std::find(field.banished.begin(), field.banished.end(), id) != field.banished.end();
}

// Un Id que le départ a retiré des réserves : il n'est jamais offert.
[[nodiscard]] bool Locked(const SurvivorsField& field, std::uint16_t id) {
  return std::binary_search(field.start.locked_ids.begin(), field.start.locked_ids.end(), id);
}

[[nodiscard]] OwnedSkill* OwnedSkillOf(SurvivorsHero& hero, std::uint16_t skill_id) {
  const auto found =
      std::find_if(hero.skills.begin(), hero.skills.end(),
                   [&](const OwnedSkill& owned) { return owned.skill_id == skill_id; });
  return found == hero.skills.end() ? nullptr : &*found;
}

[[nodiscard]] OwnedCard* OwnedCardOf(SurvivorsHero& hero, std::uint16_t card_item_id) {
  const auto found =
      std::find_if(hero.cards.begin(), hero.cards.end(),
                   [&](const OwnedCard& owned) { return owned.card_item_id == card_item_id; });
  return found == hero.cards.end() ? nullptr : &*found;
}

[[nodiscard]] std::size_t SkillSlots(const SurvivorsField& field) noexcept {
  return std::min<std::size_t>(field.rules->caps.skill_slots, kMostSkillSlots);
}

// La majoration d'XP en cours : la statistique, plus celle d'un Battle Manual
// tant qu'il dure, sommées avant une seule troncature.
[[nodiscard]] std::int64_t ExpBonusPct(const SurvivorsField& field) {
  const SurvivorsHero& hero = field.hero;
  std::int64_t bonus_pct = hero.stats.Of(SurvivorsStat::kExpPct);
  if (field.step < hero.exp_boost_until_step) {
    if (const PickupRule* manual = FindPickup(*field.rules, PickupKind::kManual)) {
      bonus_pct += manual->amount;
    }
  }
  return bonus_pct;
}

// De l'expérience gagnée : majorée, puis chaque niveau atteint ouvre un
// choix. ExpToNext ne rend jamais 0 : la boucle finit. Le niveau qui fait
// renaître le héros le dit, et majore ses dégâts dès ce pas.
void GainExp(SurvivorsField& field, std::uint32_t exp) {
  SurvivorsHero& hero = field.hero;
  const bool was_reborn = field.hero_class != nullptr && Reborn(*field.hero_class, hero.level);
  const std::uint16_t level_before = hero.level;
  const auto gained = static_cast<std::uint64_t>(Scaled(exp, ExpBonusPct(field)));
  hero.exp = SaturatedSum(hero.exp, gained);
  field.tally.exp_total = SaturatedSum(field.tally.exp_total, gained);
  while (hero.level < std::numeric_limits<std::uint16_t>::max()) {
    const std::uint32_t needed = TeamExpToNext(field);
    if (hero.exp < needed) break;
    hero.exp -= needed;
    ++hero.level;
    CountOne(field.pending_level_ups);
    CountOne(field.news.level_ups);
  }
  if (!was_reborn && field.hero_class != nullptr && Reborn(*field.hero_class, hero.level)) {
    RecomputeStats(field);
  }
  for (std::size_t index = 0; index < HeroCount(field); ++index) {
    const ClassRule* hero_class = ClassAt(field, index);
    if (hero_class != nullptr && !Reborn(*hero_class, level_before) &&
        Reborn(*hero_class, hero.level)) {
      field.news.reborn_by_hero[index] = true;
    }
  }
  // Le niveau de l'équipe vaut pour tous, et fait renaître les autres classes.
  if (!field.allies.empty() && hero.level != level_before) {
    SyncTeam(field);
    ForEachHero(field, [&field] { RecomputeStats(field); });
  }
}

// Vrai si le héros tient déjà un AUTRE skill du groupe exclusif de `rule`.
[[nodiscard]] bool GroupTaken(const SurvivorsRules& rules, const SurvivorsHero& hero,
                              const SurvivorsSkillRule& rule) {
  if (rule.exclusive_group == 0) return false;
  return std::ranges::any_of(hero.skills, [&](const OwnedSkill& owned) {
    const SurvivorsSkillRule* held = FindSkill(rules, owned.skill_id);
    return held != nullptr && held->skill_id != rule.skill_id &&
           held->exclusive_group == rule.exclusive_group;
  });
}

// Une offre tirée : son poids, dans l'ordre des réserves de la classe.
struct OfferCandidate {
  SurvivorsOffer offer;
  std::uint32_t weight = 0;
};

// Les offres possibles, skills puis cartes, dans l'ordre des réserves : un
// possédé sous son niveau maximal, un nouveau tant qu'un emplacement est libre.
// Un Id banni ou verrouillé n'y entre pas, ni un skill dont le héros possède
// l'évolué, qu'il en soit le premier skill ou le second d'une union, ni un
// nouveau d'un groupe exclusif dont il tient déjà un skill.
[[nodiscard]] std::vector<OfferCandidate> OfferCandidates(SurvivorsField& field) {
  const SurvivorsRules& rules = *field.rules;
  const ClassRule& hero_class = *field.hero_class;
  SurvivorsHero& hero = field.hero;
  std::vector<OfferCandidate> candidates;
  const auto spent = [&](std::uint16_t skill_id) {
    return std::ranges::any_of(rules.evolutions, [&](const EvolutionRule& evolution) {
      return (evolution.skill_id == skill_id || evolution.partner_skill_id == skill_id) &&
             OwnedSkillOf(hero, evolution.evolved_skill_id) != nullptr;
    });
  };
  for (const std::uint16_t skill_id : hero_class.skill_pool) {
    const SurvivorsSkillRule* rule = FindSkill(rules, skill_id);
    if (rule == nullptr || Banished(field, skill_id) || Locked(field, skill_id)) continue;
    if (spent(skill_id)) continue;
    if (const OwnedSkill* owned = OwnedSkillOf(hero, skill_id); owned != nullptr) {
      if (std::size_t{owned->level} < rule->levels.size()) {
        candidates.push_back(
            {{OfferKind::kSkillLevel, skill_id, static_cast<std::uint8_t>(owned->level + 1U)},
             rules.tuning.offer_weight_owned});
      }
    } else if (hero.skills.size() < SkillSlots(field) && !rule->levels.empty() &&
               !GroupTaken(rules, hero, *rule)) {
      candidates.push_back({{OfferKind::kNewSkill, skill_id, 1}, rules.tuning.offer_weight_fresh});
    }
  }
  const auto offer_card = [&](std::uint16_t card_item_id) {
    const CardRule* rule = FindCard(rules, card_item_id);
    if (rule == nullptr || Banished(field, card_item_id) || Locked(field, card_item_id)) return;
    if (const OwnedCard* owned = OwnedCardOf(hero, card_item_id); owned != nullptr) {
      if (std::size_t{owned->level} < rule->per_level.size()) {
        candidates.push_back(
            {{OfferKind::kCardLevel, card_item_id, static_cast<std::uint8_t>(owned->level + 1U)},
             rules.tuning.offer_weight_owned});
      }
    } else if (hero.cards.size() < rules.caps.card_slots && !rule->per_level.empty()) {
      candidates.push_back(
          {{OfferKind::kNewCard, card_item_id, 1}, rules.tuning.offer_weight_fresh});
    }
  };
  for (const std::uint16_t card_item_id : hero_class.card_pool) offer_card(card_item_id);
  // Puis les cartes que les classes des coéquipiers prêtent, après sa réserve,
  // chacune une fois.
  std::vector<std::uint16_t> lent;
  for (std::size_t index = 0; index < HeroCount(field); ++index) {
    const ClassRule* lender = ClassAt(field, index);
    if (lender == nullptr) continue;
    for (const std::uint16_t card_item_id : lender->lent_cards) {
      if (std::ranges::find(hero_class.card_pool, card_item_id) != hero_class.card_pool.end() ||
          std::ranges::find(lent, card_item_id) != lent.end()) {
        continue;
      }
      lent.push_back(card_item_id);
      offer_card(card_item_id);
    }
  }
  return candidates;
}

// Ce qu'un héros a déjà reçu : la somme des niveaux de ses skills.
[[nodiscard]] std::uint32_t SkillLevelsOf(const SurvivorsHero& hero) noexcept {
  std::uint32_t levels = 0;
  for (const OwnedSkill& owned : hero.skills) levels += owned.level;
  return levels;
}

// LES OFFRES D'UNE ÉQUIPE : celles de chaque héros, dans l'ordre de leur index.
// Une carte vaut pour tous : celle qu'un héros tient n'est plus offerte
// neuve à un autre, ni deux fois. Le poids d'une offre croît avec le retard de
// son héros sur le mieux servi, pour que personne ne reste oublié.
[[nodiscard]] std::vector<OfferCandidate> TeamOfferCandidates(SurvivorsField& field) {
  std::uint32_t most_levels = 0;
  for (std::size_t index = 0; index < HeroCount(field); ++index) {
    most_levels = std::max(most_levels, SkillLevelsOf(HeroAt(field, index)));
  }
  std::vector<OfferCandidate> team;
  const auto card_taken = [&](std::uint16_t card_item_id) {
    for (std::size_t index = 0; index < HeroCount(field); ++index) {
      for (const OwnedCard& owned : HeroAt(field, index).cards) {
        if (owned.card_item_id == card_item_id) return true;
      }
    }
    return std::ranges::any_of(team, [&](const OfferCandidate& candidate) {
      return candidate.offer.kind == OfferKind::kNewCard && candidate.offer.id == card_item_id;
    });
  };
  ForEachHero(field, [&] {
    const std::uint32_t behind = most_levels - SkillLevelsOf(field.hero);
    const std::uint16_t mount_card_id =
        field.hero_class != nullptr ? field.hero_class->mount_card_item_id : std::uint16_t{0};
    for (OfferCandidate& candidate : OfferCandidates(field)) {
      // Une carte neuve ne va qu'à un héros, sauf sa monture, qui est personnelle.
      if (candidate.offer.kind == OfferKind::kNewCard && candidate.offer.id != mount_card_id &&
          card_taken(candidate.offer.id)) {
        continue;
      }
      candidate.offer.hero_index = field.stage;
      candidate.weight += candidate.weight * behind;
      team.push_back(candidate);
    }
  });
  return team;
}

// Applique une offre choisie. Faux, sans rien changer, pour une offre qui ne
// s'applique plus à l'état du héros.
[[nodiscard]] bool Grant(SurvivorsField& field, const SurvivorsOffer& offer) {
  const SurvivorsRules& rules = *field.rules;
  SurvivorsHero& hero = field.hero;
  switch (offer.kind) {
    case OfferKind::kNewSkill: {
      const SurvivorsSkillRule* rule = FindSkill(rules, offer.id);
      if (rule == nullptr || OwnedSkillOf(hero, offer.id) != nullptr ||
          hero.skills.size() >= SkillSlots(field) || GroupTaken(rules, hero, *rule)) {
        return false;
      }
      // Un skill acquis tire dès ce pas ; une orbite part au début de sa phase.
      hero.skills.push_back(OwnedSkill{
          .skill_id = offer.id, .level = 1, .ready_step = field.step, .phase_step = field.step});
      return true;
    }
    case OfferKind::kSkillLevel: {
      const SurvivorsSkillRule* rule = FindSkill(rules, offer.id);
      OwnedSkill* owned = OwnedSkillOf(hero, offer.id);
      if (rule == nullptr || owned == nullptr || offer.next_level != owned->level + 1 ||
          std::size_t{offer.next_level} > rule->levels.size()) {
        return false;
      }
      owned->level = offer.next_level;
      // Une orbite montée de niveau repart au début de sa phase active.
      if (rule->archetype == SkillArchetype::kOrbit) owned->phase_step = field.step;
      return true;
    }
    case OfferKind::kNewCard: {
      const CardRule* rule = FindCard(rules, offer.id);
      if (rule == nullptr || OwnedCardOf(hero, offer.id) != nullptr ||
          hero.cards.size() >= rules.caps.card_slots) {
        return false;
      }
      hero.cards.push_back(OwnedCard{.card_item_id = offer.id, .level = 1});
      return true;
    }
    case OfferKind::kCardLevel: {
      const CardRule* rule = FindCard(rules, offer.id);
      OwnedCard* owned = OwnedCardOf(hero, offer.id);
      if (rule == nullptr || owned == nullptr || offer.next_level != owned->level + 1 ||
          std::size_t{offer.next_level} > rule->per_level.size()) {
        return false;
      }
      owned->level = offer.next_level;
      return true;
    }
    case OfferKind::kHeal:
      // Le soin va à tous les héros debout.
      ForEachStanding(field, [&] {
        field.hero.hp = static_cast<std::int32_t>(std::min<std::int64_t>(
            std::int64_t{field.hero.hp} + std::max(rules.tuning.heal_offer_hp, 0),
            field.hero.max_hp));
      });
      return true;
    case OfferKind::kZeny:
      hero.zeny = SaturatedSum(hero.zeny, rules.tuning.zeny_offer);
      return true;
  }
  return false;
}

// Une gemme de `exp` en `at` ; rien pour une XP nulle. Au plafond, l'XP
// rejoint la plus ancienne gemme.
void DropGem(SurvivorsField& field, FieldPoint at, std::uint32_t exp) {
  if (exp == 0) return;
  if (field.gems.size() < field.rules->caps.most_gems) {
    field.gems.push_back(
        SurvivorsGem{.position = at, .previous_position = at, .exp = exp, .pull_units = 0});
  } else if (!field.gems.empty()) {
    field.gems.front().exp = SaturatedSum(field.gems.front().exp, exp);
  }
}

// La flaque qu'un monstre laisse en tombant en `at` : armée dès ce pas, elle
// blesse pendant `puddle_ms`, au plus une fois par `puddle_period_ms`. Rien
// sans dégâts, rayon ou durée, ni au plafond des menaces au sol.
void LeavePuddle(SurvivorsField& field, const FoeBehaviour& behaviour, FieldPoint at) {
  if (behaviour.puddle_damage <= 0 || behaviour.puddle_radius_units <= 0 ||
      behaviour.puddle_ms == 0 || field.hazards.size() >= field.rules->caps.most_hazards) {
    return;
  }
  const std::uint64_t until = std::uint64_t{field.step} + StepsOf(behaviour.puddle_ms);
  field.hazards.push_back(
      SurvivorsHazard{.kind = HazardKind::kPuddle,
                      .center = at,
                      .radius_units = behaviour.puddle_radius_units,
                      .damage = behaviour.puddle_damage,
                      .armed_step = field.step,
                      .until_step = static_cast<std::uint32_t>(std::min<std::uint64_t>(
                          until, std::numeric_limits<std::uint32_t>::max())),
                      .period_steps = StepsOf(behaviour.puddle_period_ms),
                      .next_hurt_step = field.step});
}

// Un choix soldé : le suivant s'ouvre, ou la partie reprend.
void Settle(SurvivorsField& field) {
  if (field.pending_level_ups > 0) --field.pending_level_ups;
  if (field.pending_level_ups > 0) {
    BuildOffers(field);
    return;
  }
  field.offers.clear();
  field.phase = SurvivorsPhase::kRunning;
}

}  // namespace

void RecomputeStats(SurvivorsField& field) {
  if (field.rules == nullptr || field.hero_class == nullptr) return;
  SurvivorsHero& hero = field.hero;
  // Les statistiques : le bonus du loadout, plus chaque carte à son niveau,
  // cumulés en saturant dans l'int32.
  SurvivorsStats stats = field.start.loadout.bonus;
  // Les cartes valent pour toute l'équipe, sauf la monture de celui qui la tient.
  for (std::size_t index = 0; index < HeroCount(field); ++index) {
    const ClassRule* holder_class = ClassAt(field, index);
    for (const OwnedCard& owned : HeroAt(field, index).cards) {
      const CardRule* card = FindCard(*field.rules, owned.card_item_id);
      if (card == nullptr || owned.level == 0 ||
          std::size_t{owned.level} > card->per_level.size()) {
        continue;
      }
      if (index != field.stage && holder_class != nullptr &&
          owned.card_item_id == holder_class->mount_card_item_id) {
        continue;
      }
      std::int32_t& value = stats.values[static_cast<std::size_t>(card->stat)];
      value = SaturatedAdd(value, card->per_level[owned.level - 1U]);
    }
  }
  // La renaissance, une fois atteinte.
  if (Reborn(*field.hero_class, hero.level)) {
    std::int32_t& damage = stats.values[static_cast<std::size_t>(SurvivorsStat::kDamagePct)];
    damage = SaturatedAdd(damage, field.hero_class->reborn_damage_pct);
  }
  // Puis chaque posture en cours, à son niveau.
  for (const OwnedSkill& owned : hero.skills) {
    if (field.step >= owned.stance_until_step) continue;
    const SurvivorsSkillRule* skill = FindSkill(*field.rules, owned.skill_id);
    if (skill == nullptr || skill->archetype != SkillArchetype::kStance ||
        skill->stance_stat == SurvivorsStat::kMaxHp) {
      continue;
    }
    const SkillLevel* level = LevelOf(*skill, owned);
    if (level == nullptr) continue;
    std::int32_t& value = stats.values[static_cast<std::size_t>(skill->stance_stat)];
    value = SaturatedAdd(value, level->stance_bonus);
  }
  hero.stats = stats;
  // Chaque relève qu'une carte ajoute s'inscrit une fois dans ses vies.
  const std::int32_t card_lives =
      std::clamp(stats.Of(SurvivorsStat::kExtraLives), 0, kMostCardLives);
  if (card_lives > hero.card_lives_granted) {
    const std::int32_t added = card_lives - hero.card_lives_granted;
    field.lives_left = static_cast<std::uint8_t>(
        std::min<std::int32_t>(field.lives_left + added, std::numeric_limits<std::uint8_t>::max()));
    hero.card_lives_granted = card_lives;
  }

  // Un gain de PV max s'ajoute aux PV courants ; une perte les plafonne. Un
  // héros garde au moins un PV max.
  const ClassRule& hero_class = *field.hero_class;
  const std::int32_t max_hp =
      std::max(SaturatedAdd(hero_class.max_hp, stats.Of(SurvivorsStat::kMaxHp)), 1);
  if (!hero.down && max_hp > hero.max_hp) {
    hero.hp = SaturatedAdd(hero.hp, std::int64_t{max_hp} - hero.max_hp);
  }
  hero.max_hp = max_hp;
  hero.hp = std::min(hero.hp, hero.max_hp);

  hero.units_per_step =
      ScaledUnits(UnitsPerStep(hero_class.walk_ms_per_cell), stats.Of(SurvivorsStat::kWalkPct));
  hero.magnet_units = ScaledUnits(hero_class.magnet_units, stats.Of(SurvivorsStat::kMagnetPct));
}

void BuryFoes(SurvivorsField& field) { BuryFallen(field); }

void BuryFallen(SurvivorsField& field) {
  if (field.rules == nullptr) return;
  const SurvivorsRules& rules = *field.rules;
  for (const SurvivorsFoe& foe : field.foes) {
    if (foe.hp > 0) continue;
    const FoeRule* rule = FoeRuleOf(*field.rules, foe);
    const FoeRank rank = rule != nullptr ? rule->rank : FoeRank::kCommon;
    if (field.news.falls.size() < kMostNewsFalls) {
      field.news.falls.push_back(SurvivorsFall{.foe_serial = foe.serial,
                                               .foe_id = foe.foe_id,
                                               .at = foe.position,
                                               .rank = rank,
                                               .hero_index = foe.last_hurt_hero});
    } else {
      ++field.news.dropped;
    }
    CountOne(field.tally.kills);
    CountInRecap(field.recap.kills_by_foe, foe.foe_id, 1);
    if (rule == nullptr) continue;
    field.tally.kill_points = SaturatedSum(field.tally.kill_points, KillPoints(*rule, foe.elite));
    if (rank != FoeRank::kCommon) CountOne(field.tally.boss_kills);
    if (foe.elite) CountOne(field.tally.elite_kills);
    // La chute du boss final est la victoire : la partie s'arrête à ce pas, et
    // sa carte va droit au héros, qui n'aurait plus le temps de la ramasser.
    const bool final_fall =
        field.phase == SurvivorsPhase::kRunning && IsFinalBoss(field, foe.serial);
    if (final_fall) {
      field.phase = SurvivorsPhase::kOver;
      field.tally.outcome = SurvivorsOutcome::kVictory;
    }
    // Sa gemme (à `exp_pct` % pour une élite), puis celle de l'XP qu'il avait
    // volée, rendue d'un bloc.
    std::uint32_t exp = rule->exp;
    if (foe.elite) {
      exp = static_cast<std::uint32_t>(
          std::min<std::int64_t>(Scaled(exp, std::int64_t{rules.elites.exp_pct} - kPercent),
                                 std::numeric_limits<std::uint32_t>::max()));
    }
    DropGem(field, foe.position, exp);
    DropGem(field, foe.position, foe.carried_exp);
    LeavePuddle(field, rule->behaviour, foe.position);
    DropCard(field, foe, *rule, final_fall ? CardLanding::kHand : CardLanding::kGround);
    // Une élite ou un mini-boss lâche un coffre, même au plafond des objets ;
    // un objet de coffre nul n'en fait lâcher aucun.
    if ((foe.elite || rank == FoeRank::kMiniBoss) && rules.chests.item_id != 0) {
      (void)DropPickup(field, PickupKind::kChest, rules.chests.item_id, foe.position);
    }
  }
  std::erase_if(field.foes, [](const SurvivorsFoe& foe) { return foe.hp <= 0; });
}

void GatherGems(SurvivorsField& field) {
  if (field.rules == nullptr) return;
  const SurvivorsTuning& tuning = field.rules->tuning;
  const FieldPoint hero = field.hero.position;
  const std::int64_t magnet = field.hero.magnet_units;
  const std::int64_t gather = tuning.hero_radius_units;
  // Une gemme saisie garde une traction non nulle : c'est sa marque.
  const std::int32_t pull_start = std::max(tuning.gem_pull_start_units, 1);
  // Retrait stable : les gemmes gardent leur ordre de naissance.
  std::size_t kept = 0;
  for (std::size_t index = 0; index < field.gems.size(); ++index) {
    SurvivorsGem gem = field.gems[index];
    // UNE GEMME SAISIE NE SUIT QUE CELUI QUI L'A SAISIE, et se lâche s'il tombe.
    if (gem.pull_units > 0 && gem.pull_hero != field.stage && HeroAt(field, gem.pull_hero).down) {
      gem.pull_units = 0;
    }
    const bool followed_elsewhere = gem.pull_units > 0 && gem.pull_hero != field.stage;
    if (!followed_elsewhere) gem.previous_position = gem.position;
    if (gem.pull_units == 0 && DistanceSquared(gem.position, hero) <= magnet * magnet) {
      gem.pull_units = pull_start;
      gem.pull_hero = static_cast<std::uint8_t>(field.stage);
    }
    if (gem.pull_units > 0 && !followed_elsewhere) {
      const std::uint32_t distance =
          IntegerSqrt(static_cast<std::uint64_t>(DistanceSquared(gem.position, hero)));
      const auto stride =
          static_cast<std::int32_t>(std::min<std::int64_t>(gem.pull_units, std::int64_t{distance}));
      gem.position = gem.position + Toward(gem.position, hero, stride);
      gem.pull_units = static_cast<std::int32_t>(
          std::min<std::int64_t>(std::int64_t{gem.pull_units} + tuning.gem_pull_accel_units,
                                 std::numeric_limits<std::int32_t>::max()));
      gem.pull_units = std::max(gem.pull_units, 1);
    }
    if (DistanceSquared(gem.position, hero) < gather * gather) {
      CountOne(field.tally.gems_gathered);
      CountOne(field.news.gems_gathered);
      GainExp(field, gem.exp);
      continue;
    }
    field.gems[kept++] = gem;
  }
  field.gems.resize(kept);
}

void BuildOffers(SurvivorsField& field) {
  if (field.rules == nullptr || field.hero_class == nullptr) return;
  field.vote_serial = static_cast<std::uint8_t>(field.vote_serial + 1U);
  std::vector<OfferCandidate> candidates =
      field.allies.empty() ? OfferCandidates(field) : TeamOfferCandidates(field);
  // Le tirage de la chance a lieu dans tous les cas : le flux des offres ne
  // dépend pas du nombre de candidats.
  // Le tirage est celui de l'équipe : la meilleure chance de ses héros.
  std::int64_t best_luck = 0;
  for (std::size_t index = 0; index < HeroCount(field); ++index) {
    best_luck =
        std::max<std::int64_t>(best_luck, HeroAt(field, index).stats.Of(SurvivorsStat::kLuckPct));
  }
  const auto luck_pct = static_cast<std::uint32_t>(std::clamp<std::int64_t>(
      field.allies.empty() ? field.hero.stats.Of(SurvivorsStat::kLuckPct) : best_luck, 0,
      kWholeChance));
  const std::uint32_t lucky = field.offer_random.Chance(luck_pct, kWholeChance) ? 1U : 0U;
  const std::uint32_t wanted =
      std::min<std::uint32_t>(std::uint32_t{field.rules->caps.offers} + lucky, kMostOffers);

  field.offers.clear();
  std::vector<std::uint32_t> weights;
  weights.reserve(candidates.size());
  for (const OfferCandidate& candidate : candidates) weights.push_back(candidate.weight);
  // Sans remise : une offre tirée prend un poids nul.
  while (field.offers.size() < wanted) {
    const std::size_t picked = field.offer_random.PickWeighted(weights);
    if (picked >= weights.size()) break;
    field.offers.push_back(candidates[picked].offer);
    weights[picked] = 0;
  }
  // Rien à offrir : un soin et des zenys, que le joueur prend toujours.
  if (field.offers.empty()) {
    field.offers.push_back(SurvivorsOffer{.kind = OfferKind::kHeal, .id = 0, .next_level = 0});
    field.offers.push_back(SurvivorsOffer{.kind = OfferKind::kZeny, .id = 0, .next_level = 0});
  }
}

bool BallotAccepted(const SurvivorsField& field, SurvivorsAnswer ballot) {
  if (field.phase != SurvivorsPhase::kChoosing) return false;
  if (ballot.kind == AnswerKind::kRaise) {
    return field.leaves_left > 0 && std::size_t{ballot.offer_index} < HeroCount(field) &&
           HeroAt(field, ballot.offer_index).down;
  }
  if (std::size_t{ballot.offer_index} >= field.offers.size()) return false;
  const SurvivorsOffer& offer = field.offers[ballot.offer_index];
  switch (ballot.kind) {
    case AnswerKind::kPick:
      return true;
    case AnswerKind::kReroll:
      return field.rerolls_left > 0;
    case AnswerKind::kSkip:
      return field.skips_left > 0;
    case AnswerKind::kBanish:
      return field.banishes_left > 0 && offer.kind != OfferKind::kHeal &&
             offer.kind != OfferKind::kZeny;
    case AnswerKind::kRaise:
      break;
  }
  return false;
}

void AdvanceVote(SurvivorsField& field, std::span<const SurvivorsTeamInput> inputs) {
  if (field.vote_ticks_left == 0) field.vote_ticks_left = kVoteTicks;
  const std::size_t heroes = HeroCount(field);
  std::size_t voters = 0;
  for (std::size_t voter = 0; voter < heroes; ++voter) {
    if (!field.voter_gone[voter]) ++voters;
  }
  for (std::size_t voter = 0; voter < std::min(heroes, inputs.size()); ++voter) {
    if (!field.voter_gone[voter] && inputs[voter].ballot.has_value() &&
        BallotAccepted(field, *inputs[voter].ballot)) {
      field.ballots[voter] = inputs[voter].ballot;
    }
  }
  --field.vote_ticks_left;
  // Les options votées, dans l'ordre de leur premier bulletin, et leurs voix.
  std::array<SurvivorsAnswer, kMostHeroes> options{};
  std::array<std::uint32_t, kMostHeroes> votes{};
  std::size_t option_count = 0;
  std::size_t cast = 0;
  std::uint32_t best = 0;
  for (std::size_t voter = 0; voter < heroes; ++voter) {
    if (!field.ballots[voter].has_value()) continue;
    ++cast;
    std::size_t option = 0;
    while (option < option_count && !(options[option] == *field.ballots[voter])) ++option;
    if (option == option_count) options[option_count++] = *field.ballots[voter];
    best = std::max(best, ++votes[option]);
  }
  const bool majority = std::size_t{best} * 2 > voters;
  if (!majority && cast < voters && field.vote_ticks_left > 0) return;
  // L'option retenue : la plus votée, ou le tirage parmi les ex æquo ; sans
  // aucun bulletin, le tirage parmi les offres.
  SurvivorsAnswer chosen{};
  if (cast == 0) {
    chosen.kind = AnswerKind::kPick;
    chosen.offer_index = static_cast<std::uint8_t>(
        field.offer_random.Below(static_cast<std::uint32_t>(field.offers.size())));
  } else {
    std::uint32_t tied = 0;
    for (std::size_t option = 0; option < option_count; ++option) {
      if (votes[option] == best) ++tied;
    }
    std::uint32_t draw = tied > 1 ? field.offer_random.Below(tied) : 0;
    for (std::size_t option = 0; option < option_count; ++option) {
      if (votes[option] != best) continue;
      if (draw == 0) {
        chosen = options[option];
        break;
      }
      --draw;
    }
  }
  field.ballots = {};
  field.vote_ticks_left = 0;
  // Une offre qui ne s'applique plus laisse passer le niveau, sans charge.
  if (!ApplyAnswer(field, chosen)) Settle(field);
}

bool ApplyAnswer(SurvivorsField& field, SurvivorsAnswer answer) {
  if (field.rules == nullptr || field.phase != SurvivorsPhase::kChoosing) return false;
  if (answer.kind == AnswerKind::kRaise) {
    if (!BallotAccepted(field, answer)) return false;
    --field.leaves_left;
    PutOnStage(field, answer.offer_index);
    RaiseHero(field);
    PutOnStage(field, 0);
    Settle(field);
    return true;
  }
  // Toute réponse, même sans offre à désigner, porte un index pris dans les
  // offres : un choix n'a qu'un encodage, et un journal porteur d'un index
  // absurde ne se rejoue pas.
  if (std::size_t{answer.offer_index} >= field.offers.size()) return false;
  switch (answer.kind) {
    case AnswerKind::kPick: {
      // L'offre va à son héros ; ses cartes valent pour tous.
      const SurvivorsOffer offer = field.offers[answer.offer_index];
      PutOnStage(field, offer.hero_index);
      const bool granted = Grant(field, offer);
      PutOnStage(field, 0);
      if (!granted) return false;
      if (field.allies.empty()) {
        RecomputeStats(field);
      } else {
        SyncTeam(field);
        ForEachHero(field, [&field] { RecomputeStats(field); });
      }
      Settle(field);
      return true;
    }
    case AnswerKind::kSkip:
      if (field.skips_left == 0) return false;
      --field.skips_left;
      Settle(field);
      return true;
    case AnswerKind::kReroll:
      if (field.rerolls_left == 0) return false;
      --field.rerolls_left;
      BuildOffers(field);
      return true;
    case AnswerKind::kBanish: {
      if (field.banishes_left == 0) return false;
      const SurvivorsOffer& offer = field.offers[answer.offer_index];
      if (offer.kind == OfferKind::kHeal || offer.kind == OfferKind::kZeny) return false;
      --field.banishes_left;
      if (!Banished(field, offer.id)) field.banished.push_back(offer.id);
      BuildOffers(field);
      return true;
    }
    case AnswerKind::kRaise:  // jugée plus haut
      break;
  }
  return false;
}

}  // namespace ro::sim::survivors
