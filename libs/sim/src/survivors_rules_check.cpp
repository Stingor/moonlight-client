#include "survivors_rules_check.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <utility>

#include "ro/core/error.h"
#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_rules.h"

namespace ro::sim {

namespace {

using survivors::CellsOutOfBounds;
using survivors::Contains;
using survivors::kMostMeasureUnits;
using survivors::kMostSpeedUnitsPerSecond;
using survivors::MeasureOutOfBounds;
using survivors::PercentOutOfBounds;
using survivors::Refuse;
using survivors::RepeatedId;

// ── Le contrôle du catalogue ────────────────────────────────────────────────

template <typename Rule, typename IdOf>
[[nodiscard]] core::Result<void> CheckUnique(std::span<const Rule> table, const char* what,
                                             IdOf id_of) {
  if (const std::optional<std::uint16_t> repeated = RepeatedId(table, id_of)) {
    return Refuse(std::string(what) + " " + std::to_string(*repeated) + " en double");
  }
  return {};
}

[[nodiscard]] core::Result<void> CheckUniqueIds(const SurvivorsRules& rules) {
  if (auto checked =
          CheckUnique(rules.classes, "classe", [](const ClassRule& rule) { return rule.class_id; });
      !checked) {
    return checked;
  }
  if (auto checked = CheckUnique(rules.skills, "skill",
                                 [](const SurvivorsSkillRule& rule) { return rule.skill_id; });
      !checked) {
    return checked;
  }
  if (auto checked =
          CheckUnique(rules.cards, "carte", [](const CardRule& rule) { return rule.card_item_id; });
      !checked) {
    return checked;
  }
  if (auto checked =
          CheckUnique(rules.foes, "monstre", [](const FoeRule& rule) { return rule.foe_id; });
      !checked) {
    return checked;
  }
  return CheckUnique(rules.arenas, "arène", [](const ArenaRule& rule) { return rule.arena_id; });
}

// Une liste d'Id d'une classe nomme chaque Id au plus une fois : une réserve
// où il reviendrait l'offrirait deux fois dans un même tirage sans remise.
[[nodiscard]] core::Result<void> CheckListUnique(std::span<const std::uint16_t> ids,
                                                 const std::string& named, const char* what) {
  if (const std::optional<std::uint16_t> repeated =
          RepeatedId(ids, [](std::uint16_t listed_id) { return listed_id; })) {
    return Refuse(named + "Id " + std::to_string(*repeated) + " en double dans " + what);
  }
  return {};
}

// Les réserves et les skills de départ de chaque classe.
[[nodiscard]] core::Result<void> CheckClasses(const SurvivorsRules& rules) {
  for (const ClassRule& hero_class : rules.classes) {
    const std::string named = "classe " + std::to_string(hero_class.class_id) + " : ";
    if (hero_class.max_hp <= 0)
      return Refuse(named + "PV max " + std::to_string(hero_class.max_hp));
    // Une marche si lente qu'elle ne fait pas une sous-unité par pas fige le héros.
    if (UnitsPerStep(hero_class.walk_ms_per_cell) <= 0) {
      return Refuse(named + "marche immobile à " + std::to_string(hero_class.walk_ms_per_cell) +
                    " ms par case");
    }
    if (MeasureOutOfBounds(hero_class.magnet_units)) {
      return Refuse(named + "aimant de " + std::to_string(hero_class.magnet_units) +
                    " sous-unités");
    }
    if (PercentOutOfBounds(hero_class.damage_pct_per_level) ||
        PercentOutOfBounds(hero_class.damage_pct_cap)) {
      return Refuse(named + "majoration de dégâts hors de [0, " + std::to_string(kMostStatBonus) +
                    "]");
    }
    if (auto checked = CheckListUnique(hero_class.skill_pool, named, "la réserve de skills");
        !checked) {
      return checked;
    }
    if (auto checked = CheckListUnique(hero_class.card_pool, named, "la réserve de cartes");
        !checked) {
      return checked;
    }
    if (auto checked = CheckListUnique(hero_class.starter_skills, named, "les skills de départ");
        !checked) {
      return checked;
    }
    for (const std::uint16_t skill_id : hero_class.skill_pool) {
      if (FindSkill(rules, skill_id) == nullptr) {
        return Refuse(named + "skill de réserve inconnu " + std::to_string(skill_id));
      }
      // Un bannissement ne nomme qu'un Id : il ne doit désigner qu'une chose.
      if (Contains(hero_class.card_pool, skill_id)) {
        return Refuse(named + "Id " + std::to_string(skill_id) +
                      " à la fois skill et carte de réserve");
      }
    }
    for (const std::uint16_t card_item_id : hero_class.card_pool) {
      if (FindCard(rules, card_item_id) == nullptr) {
        return Refuse(named + "carte de réserve inconnue " + std::to_string(card_item_id));
      }
    }
    for (const std::uint16_t skill_id : hero_class.starter_skills) {
      if (FindSkill(rules, skill_id) == nullptr) {
        return Refuse(named + "skill de départ inconnu " + std::to_string(skill_id));
      }
      if (!Contains(hero_class.skill_pool, skill_id)) {
        return Refuse(named + "skill de départ " + std::to_string(skill_id) +
                      " hors de la réserve");
      }
    }
  }
  return {};
}

// Les niveaux de chaque skill : ce dont FireSkills divise ou compte.
[[nodiscard]] core::Result<void> CheckSkills(const SurvivorsRules& rules) {
  for (const SurvivorsSkillRule& skill : rules.skills) {
    const std::string named = "skill " + std::to_string(skill.skill_id) + " : ";
    if (skill.levels.empty() || skill.levels.size() > kMostSkillLevels) {
      return Refuse(named + std::to_string(skill.levels.size()) + " niveaux, attendu de 1 à " +
                    std::to_string(kMostSkillLevels));
    }
    for (std::size_t index = 0; index < skill.levels.size(); ++index) {
      const SkillLevel& level = skill.levels[index];
      const std::string at = named + "niveau " + std::to_string(index + 1) + " ";
      if (level.amount == 0 || level.amount > kMostVolley) {
        return Refuse(at + "de quantité " + std::to_string(level.amount) + ", attendu de 1 à " +
                      std::to_string(kMostVolley));
      }
      if (MeasureOutOfBounds(level.reach_units) || MeasureOutOfBounds(level.hit_radius_units) ||
          level.speed_units_per_s < 0 || level.speed_units_per_s > kMostSpeedUnitsPerSecond) {
        return Refuse(at + "de portée, de rayon ou de vitesse au-delà de la plus grande arène");
      }
      if (skill.archetype == SkillArchetype::kOrbit &&
          (level.period_ms == 0 || level.life_ms == 0)) {
        return Refuse(at + "d'orbite sans période ou sans phase active");
      }
      // Une frappe ou un tir sans portée ne trouve jamais de monstre.
      if (survivors::ArchetypeAims(skill.archetype) && level.reach_units == 0) {
        return Refuse(at + "sans portée");
      }
      if (skill.archetype == SkillArchetype::kHoming &&
          (ShotUnitsPerStep(level) == 0 || level.life_ms == 0)) {
        return Refuse(at + "de tir à moins d'une sous-unité par pas ou sans durée de vie");
      }
      // Un tir retient chaque monstre touché : son perçage ne dépasse pas sa
      // mémoire.
      if (std::size_t{level.pierce} + 1 > kMostShotHits) {
        return Refuse(at + "perçage " + std::to_string(level.pierce) + ", au plus " +
                      std::to_string(kMostShotHits - 1));
      }
    }
  }
  return {};
}

[[nodiscard]] core::Result<void> CheckCards(const SurvivorsRules& rules) {
  for (const CardRule& card : rules.cards) {
    if (card.per_level.empty() || card.per_level.size() > kMostCardLevels) {
      return Refuse("carte " + std::to_string(card.card_item_id) + " : " +
                    std::to_string(card.per_level.size()) + " niveaux, attendu de 1 à " +
                    std::to_string(kMostCardLevels));
    }
    for (const std::int32_t value : card.per_level) {
      if (value < -kMostStatBonus || value > kMostStatBonus) {
        return Refuse("carte " + std::to_string(card.card_item_id) + " : valeur " +
                      std::to_string(value) + " au-delà de ±" + std::to_string(kMostStatBonus));
      }
    }
  }
  return {};
}

[[nodiscard]] core::Result<void> CheckFoes(const SurvivorsRules& rules) {
  for (const FoeRule& foe : rules.foes) {
    const std::string named = "monstre " + std::to_string(foe.foe_id) + " : ";
    if (foe.hp <= 0) return Refuse(named + "PV " + std::to_string(foe.hp));
    if (foe.touch_damage != 0 && foe.touch_interval_ms == 0) {
      return Refuse(named + "dégâts de contact sans intervalle");
    }
  }
  return {};
}

// Les vagues, leurs tirages pondérés, et les boss.
[[nodiscard]] core::Result<void> CheckWaves(const SurvivorsRules& rules) {
  if (rules.waves.empty() || rules.waves.front().from_ms != 0) {
    return Refuse("aucune vague en vigueur dès 0 ms");
  }
  for (std::size_t index = 0; index < rules.waves.size(); ++index) {
    const WaveRule& wave = rules.waves[index];
    const std::string named = "vague de " + std::to_string(wave.from_ms) + " ms : ";
    if (index > 0 && wave.from_ms < rules.waves[index - 1].from_ms) {
      return Refuse(named + "vagues non triées");
    }
    // Sans intervalle, un lot naîtrait à chaque pas.
    if (wave.batch > 0 && wave.interval_ms == 0) {
      return Refuse(named + "lot de " + std::to_string(wave.batch) + " sans intervalle");
    }
    if (wave.weights.size() != wave.foe_ids.size()) {
      return Refuse(named + std::to_string(wave.weights.size()) + " poids pour " +
                    std::to_string(wave.foe_ids.size()) + " monstres");
    }
    // La somme se tient en 64 bits : PickWeighted la tire en 32.
    std::uint64_t total_weight = 0;
    for (const std::uint32_t weight : wave.weights) total_weight += weight;
    if (total_weight == 0 || total_weight > std::numeric_limits<std::uint32_t>::max()) {
      return Refuse(named + "somme des poids " + std::to_string(total_weight));
    }
    for (const std::uint16_t foe_id : wave.foe_ids) {
      if (FindFoe(rules, foe_id) == nullptr) {
        return Refuse(named + "monstre inconnu " + std::to_string(foe_id));
      }
    }
  }
  for (std::size_t index = 0; index < rules.bosses.size(); ++index) {
    const BossRule& boss = rules.bosses[index];
    if (index > 0 && boss.at_ms < rules.bosses[index - 1].at_ms) {
      return Refuse("boss non triés à " + std::to_string(boss.at_ms) + " ms");
    }
    if (FindFoe(rules, boss.foe_id) == nullptr) {
      return Refuse("boss inconnu " + std::to_string(boss.foe_id));
    }
  }
  return {};
}

// La courbe d'XP et la table des éléments.
[[nodiscard]] core::Result<void> CheckCurve(const SurvivorsRules& rules) {
  if (rules.exp_curve.empty() || rules.exp_curve.front().from_level != 1) {
    return Refuse("courbe d'XP qui ne commence pas au niveau 1");
  }
  for (std::size_t index = 0; index < rules.exp_curve.size(); ++index) {
    const ExpSegment& segment = rules.exp_curve[index];
    if (index > 0 && segment.from_level < rules.exp_curve[index - 1].from_level) {
      return Refuse("courbe d'XP non triée au niveau " + std::to_string(segment.from_level));
    }
    if (segment.base == 0) {
      return Refuse("courbe d'XP à base nulle au niveau " + std::to_string(segment.from_level));
    }
  }
  if (rules.elements.size() != kSurvivorsElementCount) {
    return Refuse(std::to_string(rules.elements.size()) + " lignes d'éléments, attendu " +
                  std::to_string(kSurvivorsElementCount));
  }
  return {};
}

// Les réglages : chaque mesure dans les bornes de la plus grande arène, et ce
// sans quoi rien ne naît ni ne se ramasse.
[[nodiscard]] core::Result<void> CheckTuning(const SurvivorsRules& rules) {
  const SurvivorsTuning& tuning = rules.tuning;
  // Sans rayon, une gemme tirée jusque sur le héros n'est jamais ramassée.
  if (tuning.hero_radius_units <= 0) return Refuse("réglages : rayon du héros nul");
  const std::array<std::pair<const char*, std::int32_t>, 10> measures{{
      {"rayon du héros", tuning.hero_radius_units},
      {"rayon d'un petit monstre", tuning.foe_radius_units[0]},
      {"rayon d'un monstre moyen", tuning.foe_radius_units[1]},
      {"rayon d'un grand monstre", tuning.foe_radius_units[2]},
      {"portée du contact", tuning.touch_reach_units},
      {"poursuite directe", tuning.direct_chase_units},
      {"rappel", tuning.recall_units},
      {"poussée de séparation", tuning.separation_max_push_units},
      {"traction d'une gemme", tuning.gem_pull_start_units},
      {"accélération d'une gemme", tuning.gem_pull_accel_units},
  }};
  for (const auto& [name, units] : measures) {
    if (MeasureOutOfBounds(units)) {
      return Refuse(std::string("réglages : ") + name + " de " + std::to_string(units) +
                    " sous-unités, attendu de 0 à " + std::to_string(kMostMeasureUnits));
    }
  }
  if (CellsOutOfBounds(tuning.flow_radius_cells) || CellsOutOfBounds(tuning.spawn_search_ring)) {
    return Refuse("réglages : champ de poursuite ou anneau de recherche hors de [0, " +
                  std::to_string(kMostArenaSideCells) + "] cases");
  }
  if (tuning.spawn_tries_per_foe == 0) return Refuse("réglages : aucun essai d'apparition");
  if (PercentOutOfBounds(tuning.foe_hp_growth_pct_per_minute)) {
    return Refuse("réglages : croissance des PV de " +
                  std::to_string(tuning.foe_hp_growth_pct_per_minute) + " % par minute");
  }
  return {};
}

[[nodiscard]] core::Result<void> CheckFrameAndCaps(const SurvivorsRules& rules) {
  const SpawnFrame& frame = rules.spawn_frame;
  if (frame.half_width_units <= 0 || frame.north_units < 0 || frame.south_units < 0 ||
      frame.margin_units < 0 ||
      std::int64_t{frame.north_units} + std::int64_t{frame.south_units} <= 0) {
    return Refuse("cadre d'apparition nul ou négatif");
  }
  // Le périmètre et la marge restent loin des 32 bits.
  if (MeasureOutOfBounds(frame.half_width_units) || MeasureOutOfBounds(frame.north_units) ||
      MeasureOutOfBounds(frame.south_units) || MeasureOutOfBounds(frame.margin_units)) {
    return Refuse("cadre d'apparition au-delà de " + std::to_string(kMostArenaSideCells) +
                  " cases");
  }
  const SurvivorsCaps& caps = rules.caps;
  if (caps.most_foes == 0 || caps.most_shots == 0 || caps.most_strikes == 0 ||
      caps.most_gems == 0 || caps.skill_slots == 0 || caps.card_slots == 0) {
    return Refuse("plafond nul");
  }
  if (caps.skill_slots > kMostSkillSlots) {
    return Refuse(std::to_string(caps.skill_slots) + " emplacements de skill, au plus " +
                  std::to_string(kMostSkillSlots));
  }
  if (caps.offers == 0 || caps.offers > kMostOffers) {
    return Refuse(std::to_string(caps.offers) + " offres, attendu de 1 à " +
                  std::to_string(kMostOffers));
  }
  return {};
}

}  // namespace

core::Result<void> CheckSurvivorsRules(const SurvivorsRules& rules) {
  if (rules.ruleset_version == 0) return Refuse("version de règles nulle");
  if (rules.run_ms == 0) return Refuse("durée de partie nulle");
  // La partie classée et le défi ont une arène où se jouer.
  if (FirstRankedArena(rules) == nullptr) return Refuse("aucune arène classée");
  // Chaque contrôle suppose les précédents : un Id se cherche dans une table
  // déjà sans doublon. Le contenu d'une arène complète passe après le noyau.
  using RulesCheck = core::Result<void> (*)(const SurvivorsRules&);
  constexpr std::array<RulesCheck, 18> kChecks{CheckUniqueIds,
                                               CheckClasses,
                                               CheckSkills,
                                               CheckCards,
                                               CheckFoes,
                                               CheckWaves,
                                               CheckCurve,
                                               CheckTuning,
                                               CheckFrameAndCaps,
                                               survivors::CheckSkillContent,
                                               survivors::CheckFoeBehaviours,
                                               survivors::CheckBossScripts,
                                               survivors::CheckEventsAndElites,
                                               survivors::CheckEvolutions,
                                               survivors::CheckLoot,
                                               survivors::CheckContentTuning,
                                               survivors::CheckMetaRules,
                                               survivors::CheckTerrains};
  for (const RulesCheck check : kChecks) {
    if (core::Result<void> checked = check(rules); !checked) return checked;
  }
  return {};
}

}  // namespace ro::sim
