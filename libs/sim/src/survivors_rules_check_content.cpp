#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <utility>

#include "ro/core/error.h"
#include "ro/sim/survivors_content.h"
#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_terrain.h"

#include "survivors_rules_check.h"
#include "survivors_space.h"

namespace ro::sim::survivors {

// LE CONTRÔLE DU CONTENU D'UNE ARÈNE COMPLÈTE. Chaque refus ferme une division
// par zéro, une boucle sans fin ou un débordement que la simulation ferait
// sinon : elle lit ces règles sans les revérifier.

namespace {

// Une vitesse en sous-unités par seconde qui ne fait pas une sous-unité par
// pas : ce qu'elle porte ne bouge jamais.
[[nodiscard]] bool Stalls(std::int32_t units_per_s) noexcept {
  return units_per_s / static_cast<std::int32_t>(kStepsPerSecond) <= 0;
}

[[nodiscard]] bool SpeedOutOfBounds(std::int64_t units_per_s) noexcept {
  return units_per_s < 0 || units_per_s > kMostSpeedUnitsPerSecond;
}

// Une liste de mesures nommées, chacune dans [0, kMostMeasureUnits].
template <std::size_t kCount>
[[nodiscard]] core::Result<void> CheckMeasures(
    const std::string& named,
    const std::array<std::pair<const char*, std::int64_t>, kCount>& measures) {
  for (const auto& [name, units] : measures) {
    if (MeasureOutOfBounds(units)) {
      return Refuse(named + name + " de " + std::to_string(units) +
                    " sous-unités, attendu de 0 à " + std::to_string(kMostMeasureUnits));
    }
  }
  return {};
}

// Une liste de majorations nommées, chacune dans [0, kMostStatBonus].
template <std::size_t kCount>
[[nodiscard]] core::Result<void> CheckPercents(
    const std::string& named,
    const std::array<std::pair<const char*, std::int64_t>, kCount>& percents) {
  for (const auto& [name, pct] : percents) {
    if (PercentOutOfBounds(pct)) {
      return Refuse(named + name + " de " + std::to_string(pct) + " %, attendu de 0 à " +
                    std::to_string(kMostStatBonus));
    }
  }
  return {};
}

[[nodiscard]] bool HasArea(SkillArchetype archetype) noexcept {
  return archetype == SkillArchetype::kClusterBlast || archetype == SkillArchetype::kNova ||
         archetype == SkillArchetype::kRing;
}

// Un mur et un cercle de piliers sont des zones : sans vie, elles meurent nées.
[[nodiscard]] bool IsZone(SkillArchetype archetype) noexcept {
  return archetype == SkillArchetype::kWall || archetype == SkillArchetype::kRing;
}

[[nodiscard]] core::Result<void> CheckLevelContent(const SurvivorsSkillRule& skill,
                                                   const SkillLevel& level, const std::string& at) {
  if (auto checked = CheckMeasures<4>(at, {{{"aire", level.area_radius_units},
                                            {"longueur", level.length_units},
                                            {"recul", level.knockback_units},
                                            {"portée de rebond", level.bounce_reach_units}}});
      !checked) {
    return checked;
  }
  if (auto checked =
          CheckPercents<2>(at, {{{"éclat", level.splash_pct}, {"rebond", level.bounce_pct}}});
      !checked) {
    return checked;
  }
  if (HasArea(skill.archetype) && level.area_radius_units == 0) return Refuse(at + "sans aire");
  // Le plus dense compte les voisins dans l'aire : une aire nulle n'en compte
  // aucun, et la visée retomberait sur l'ordre de naissance.
  if (skill.target == TargetRule::kDensest && level.area_radius_units == 0) {
    return Refuse(at + "visant le plus dense sans aire");
  }
  if (skill.archetype == SkillArchetype::kWall && level.length_units == 0) {
    return Refuse(at + "de mur sans longueur");
  }
  if (IsZone(skill.archetype) && level.life_ms == 0) return Refuse(at + "de zone sans vie");
  if (skill.archetype == SkillArchetype::kLinear && Stalls(level.speed_units_per_s)) {
    return Refuse(at + "de projectile droit à moins d'une sous-unité par pas");
  }
  // Un rebond cherche un monstre non touché à sa portée, et retient chacun.
  if (level.bounces > 0 && level.bounce_reach_units == 0) {
    return Refuse(at + "de rebond sans portée");
  }
  if (std::size_t{level.bounces} + 1 > kMostShotHits) {
    return Refuse(at + std::to_string(level.bounces) + " rebonds, au plus " +
                  std::to_string(kMostShotHits - 1));
  }
  return {};
}

[[nodiscard]] core::Result<void> CheckBossMove(const SurvivorsRules& rules,
                                               const BossMoveRule& move, const std::string& at) {
  // Un coup qui invoque ou presse nomme son espèce ; les autres n'en nomment
  // aucune, ou une connue.
  if ((NamesFoe(move.kind) || move.foe_id != 0) && FindFoe(rules, move.foe_id) == nullptr) {
    return Refuse(at + "espèce inconnue " + std::to_string(move.foe_id));
  }
  if (move.from_hp_pct <= move.to_hp_pct || move.from_hp_pct > kWholeHpPct) {
    return Refuse(at + "seuils de PV de " + std::to_string(move.from_hp_pct) + " à " +
                  std::to_string(move.to_hp_pct) + " %");
  }
  if (move.period_ms == 0) return Refuse(at + "sans période");
  // Comptées en pas, comme la simulation les joue.
  if (StepsOf(move.telegraph_ms) >= StepsOf(move.period_ms)) {
    return Refuse(at + "annonce aussi longue que la période");
  }
  if (auto checked =
          CheckMeasures<2>(at, {{{"portée", move.reach_units}, {"rayon", move.radius_units}}});
      !checked) {
    return checked;
  }
  return CheckPercents<1>(at, {{{"majoration", move.boost_pct}}});
}

}  // namespace

core::Result<void> CheckSkillContent(const SurvivorsRules& rules) {
  for (const SurvivorsSkillRule& skill : rules.skills) {
    const std::string named = "skill " + std::to_string(skill.skill_id) + " : ";
    // Le plus dense se cherche autour d'un point visé : celui d'une frappe ou
    // d'un éclat.
    if (skill.target == TargetRule::kDensest && skill.archetype != SkillArchetype::kTargetStrike &&
        skill.archetype != SkillArchetype::kClusterBlast) {
      return Refuse(named + "ciblage du plus dense hors d'une frappe ou d'un éclat");
    }
    // Seule une frappe tire sa cible au hasard : aucun tirage n'est prévu pour
    // un éclat.
    if (skill.target == TargetRule::kRandom && skill.archetype == SkillArchetype::kClusterBlast) {
      return Refuse(named + "ciblage au hasard d'un éclat");
    }
    for (std::size_t index = 0; index < skill.levels.size(); ++index) {
      const std::string at = named + "niveau " + std::to_string(index + 1) + " ";
      if (auto checked = CheckLevelContent(skill, skill.levels[index], at); !checked) {
        return checked;
      }
    }
  }
  return {};
}

core::Result<void> CheckFoeBehaviours(const SurvivorsRules& rules) {
  for (const FoeRule& foe : rules.foes) {
    const FoeBehaviour& behaviour = foe.behaviour;
    const std::string named = "monstre " + std::to_string(foe.foe_id) + " : ";
    if (behaviour.pack == 0) return Refuse(named + "meute vide");
    if (HasActs(behaviour) && behaviour.period_ms == 0) {
      return Refuse(named + "comportement à acte sans période");
    }
    // L'annonce précède l'acte dans la même période, comptées en pas comme la
    // simulation les joue.
    const std::uint32_t period_steps = StepsOf(behaviour.period_ms);
    const std::uint32_t telegraph_steps = StepsOf(behaviour.telegraph_ms);
    if (behaviour.period_ms != 0 && telegraph_steps >= period_steps) {
      return Refuse(named + "annonce aussi longue que la période");
    }
    // Une cachette rend le monstre tangible avant la période suivante : sinon
    // elle se renoue sans fin, et rien ne peut plus le viser ni le tuer.
    if (ActOf(behaviour) == FoeAct::kHide &&
        std::uint64_t{telegraph_steps} + StepsOf(behaviour.active_ms) >= period_steps) {
      return Refuse(named + "cachette qui couvre toute sa période");
    }
    if (SpeedOutOfBounds(behaviour.dash_units_per_s) ||
        SpeedOutOfBounds(behaviour.shot_units_per_s)) {
      return Refuse(named + "vitesse au-delà de la plus grande arène");
    }
    if (behaviour.move == FoeMove::kRanged && Stalls(behaviour.shot_units_per_s)) {
      return Refuse(named + "tir à moins d'une sous-unité par pas");
    }
    if (behaviour.move == FoeMove::kCharger && Stalls(behaviour.dash_units_per_s)) {
      return Refuse(named + "charge à moins d'une sous-unité par pas");
    }
    if (behaviour.puddle_damage != 0 && behaviour.puddle_period_ms == 0) {
      return Refuse(named + "flaque qui blesse sans période");
    }
    if (auto checked =
            CheckMeasures<3>(named, {{{"portée", behaviour.reach_units},
                                      {"rayon de tir", behaviour.shot_radius_units},
                                      {"rayon de flaque", behaviour.puddle_radius_units}}});
        !checked) {
      return checked;
    }
  }
  return {};
}

core::Result<void> CheckBossScripts(const SurvivorsRules& rules) {
  const auto finals = std::count_if(rules.bosses.begin(), rules.bosses.end(),
                                    [](const BossRule& boss) { return boss.final; });
  if (finals > 1) return Refuse(std::to_string(finals) + " boss finals, au plus un");
  for (std::size_t index = 0; index < rules.bosses.size(); ++index) {
    const BossRule& boss = rules.bosses[index];
    const std::string named =
        "boss " + std::to_string(boss.foe_id) + " à " + std::to_string(boss.at_ms) + " ms : ";
    // Sa chute clôt la partie : aucun boss ne vient après lui.
    if (boss.final && index + 1 != rules.bosses.size()) {
      return Refuse(named + "boss final qui n'est pas le dernier");
    }
    if (boss.escort_count > 0 && FindFoe(rules, boss.escort_foe_id) == nullptr) {
      return Refuse(named + "escorte inconnue " + std::to_string(boss.escort_foe_id));
    }
    if (boss.moves.size() > kMostBossMoves) {
      return Refuse(named + std::to_string(boss.moves.size()) + " coups, au plus " +
                    std::to_string(kMostBossMoves));
    }
    // Un script mène le corps du boss par les mêmes champs qu'un acte de
    // comportement : les deux ne se partagent pas un monstre.
    const FoeRule* rule = FindFoe(rules, boss.foe_id);
    if (!boss.moves.empty() && rule != nullptr && HasActs(rule->behaviour)) {
      return Refuse(named + "script sur une espèce à acte de comportement");
    }
    for (std::size_t move = 0; move < boss.moves.size(); ++move) {
      const std::string at = named + "coup " + std::to_string(move + 1) + " ";
      if (auto checked = CheckBossMove(rules, boss.moves[move], at); !checked) return checked;
    }
  }
  return {};
}

core::Result<void> CheckEventsAndElites(const SurvivorsRules& rules) {
  for (std::size_t index = 0; index < rules.events.size(); ++index) {
    const EventRule& event = rules.events[index];
    const std::string named = "événement à " + std::to_string(event.at_ms) + " ms : ";
    if (index > 0 && event.at_ms < rules.events[index - 1].at_ms) {
      return Refuse(named + "événements non triés");
    }
    if (FindFoe(rules, event.foe_id) == nullptr) {
      return Refuse(named + "monstre inconnu " + std::to_string(event.foe_id));
    }
    if (event.count == 0) return Refuse(named + "aucun monstre");
    if (event.kind == EventKind::kRing && event.radius_units == 0) {
      return Refuse(named + "anneau sans rayon");
    }
    if (auto checked = CheckMeasures<1>(named, {{{"rayon", event.radius_units}}}); !checked) {
      return checked;
    }
  }
  const EliteRule& elites = rules.elites;
  if (elites.interval_ms != 0 && elites.hp_pct == 0) {
    return Refuse("élites sans PV");
  }
  return CheckPercents<3>(
      "élites : ",
      {{{"PV", elites.hp_pct}, {"contact", elites.touch_pct}, {"XP", elites.exp_pct}}});
}

core::Result<void> CheckEvolutions(const SurvivorsRules& rules) {
  // Un skill n'a qu'une évolution, et un évolué ne vient que d'un skill.
  if (const std::optional<std::uint16_t> repeated =
          RepeatedId(rules.evolutions, [](const EvolutionRule& rule) { return rule.skill_id; })) {
    return Refuse("skill " + std::to_string(*repeated) + " à deux évolutions");
  }
  if (const std::optional<std::uint16_t> repeated = RepeatedId(
          rules.evolutions, [](const EvolutionRule& rule) { return rule.evolved_skill_id; })) {
    return Refuse("évolué " + std::to_string(*repeated) + " en double");
  }
  for (const EvolutionRule& evolution : rules.evolutions) {
    const std::string named = "évolution de " + std::to_string(evolution.skill_id) + " : ";
    if (FindSkill(rules, evolution.skill_id) == nullptr ||
        FindCard(rules, evolution.card_item_id) == nullptr ||
        FindSkill(rules, evolution.evolved_skill_id) == nullptr) {
      return Refuse(named + "skill, carte ou évolué inconnu");
    }
    // Un évolué ne s'offre pas : seul un coffre le donne.
    for (const ClassRule& hero_class : rules.classes) {
      if (Contains(hero_class.skill_pool, evolution.evolved_skill_id) ||
          Contains(hero_class.starter_skills, evolution.evolved_skill_id)) {
        return Refuse(named + "évolué " + std::to_string(evolution.evolved_skill_id) +
                      " dans la réserve de la classe " + std::to_string(hero_class.class_id));
      }
    }
  }
  return {};
}

core::Result<void> CheckLoot(const SurvivorsRules& rules) {
  for (std::size_t later = 1; later < rules.pickups.size(); ++later) {
    for (std::size_t earlier = 0; earlier < later; ++earlier) {
      if (rules.pickups[earlier].kind == rules.pickups[later].kind) {
        return Refuse("objet de sorte " +
                      std::to_string(static_cast<int>(rules.pickups[later].kind)) + " en double");
      }
    }
  }
  for (const PickupRule& pickup : rules.pickups) {
    const std::string named = "objet " + std::to_string(pickup.item_id) + " : ";
    if (auto checked = CheckMeasures<1>(named, {{{"portée", pickup.reach_units}}}); !checked) {
      return checked;
    }
  }
  // La somme se tient en 64 bits : PickWeighted la tire en 32.
  std::uint64_t total_weight = 0;
  for (const PlantRule& plant : rules.plants) {
    if (FindPickup(rules, plant.pickup) == nullptr) {
      return Refuse("plante " + std::to_string(plant.foe_id) + " sans règle d'objet");
    }
    total_weight += plant.weight;
  }
  if (!rules.plants.empty() &&
      (total_weight == 0 || total_weight > std::numeric_limits<std::uint32_t>::max())) {
    return Refuse("plantes : somme des poids " + std::to_string(total_weight));
  }
  const ChestRule& chests = rules.chests;
  if (chests.item_id == 0 && rules.elites.interval_ms != 0) {
    return Refuse("élites sans objet de coffre");
  }
  // La nouvelle d'un coffre porte tous ses gains.
  if (chests.single == 0 || chests.single > chests.triple || chests.triple > chests.quintuple ||
      chests.quintuple > kMostChestGains) {
    return Refuse("coffre de " + std::to_string(chests.single) + ", " +
                  std::to_string(chests.triple) + " ou " + std::to_string(chests.quintuple) +
                  " gains, attendu croissant de 1 à " + std::to_string(kMostChestGains));
  }
  return {};
}

core::Result<void> CheckContentTuning(const SurvivorsRules& rules) {
  const SurvivorsTuning& tuning = rules.tuning;
  if (auto checked = CheckPercents<2>(
          "réglages : ", {{{"croissance des PV des boss", tuning.boss_hp_growth_pct_per_minute},
                           {"échelle de marche des boss", tuning.boss_walk_scale_pct}}});
      !checked) {
    return checked;
  }
  if (auto checked =
          CheckMeasures<2>("réglages : ", {{{"recul par pas", tuning.knock_units_per_step},
                                            {"ramassage d'un objet", tuning.pickup_radius_units}}});
      !checked) {
    return checked;
  }
  if (CellsOutOfBounds(tuning.plant_block_cells)) {
    return Refuse("réglages : bloc de plantes hors de [0, " + std::to_string(kMostArenaSideCells) +
                  "] cases");
  }
  // Une plante se tire par bloc, avec une chance divisée par la densité.
  if (!rules.plants.empty() &&
      (tuning.plant_block_cells == 0 || tuning.plant_cells_per_plant == 0)) {
    return Refuse("réglages : plantes sans bloc ou sans densité");
  }
  if (CellsOutOfBounds(tuning.plant_wake_cells)) {
    return Refuse("réglages : éveil des plantes hors de [0, " +
                  std::to_string(kMostArenaSideCells) + "] cases");
  }
  if (auto checked =
          CheckMeasures<1>("réglages : ", {{{"oubli des plantes", tuning.plant_forget_units}}});
      !checked) {
    return checked;
  }
  // Une plante naît au plus à un morceau au-delà du carré d'éveil : l'oubli
  // qui l'atteindrait dès sa naissance la ferait naître pour rien.
  if (tuning.plant_wake_cells > 0 && tuning.plant_forget_units > 0 &&
      tuning.plant_forget_units < Cells(tuning.plant_wake_cells + kTerrainChunkCells)) {
    return Refuse("réglages : oubli des plantes en deçà de l'éveil plus un morceau");
  }
  // Un recul qui ne bouge pas d'une sous-unité par pas ne finit jamais.
  if (tuning.knock_units_per_step == 0) {
    for (const SurvivorsSkillRule& skill : rules.skills) {
      for (const SkillLevel& level : skill.levels) {
        if (level.knockback_units > 0) {
          return Refuse("réglages : recul du skill " + std::to_string(skill.skill_id) +
                        " sans vitesse de recul");
        }
      }
    }
  }
  // Le pas de fin d'une partie prolongée se compte encore en 32 bits de ms.
  if (std::uint64_t{rules.run_ms} + tuning.final_overtime_ms >
      std::numeric_limits<std::uint32_t>::max()) {
    return Refuse("réglages : dépassement de " + std::to_string(tuning.final_overtime_ms) +
                  " ms au-delà de 32 bits");
  }
  const SurvivorsCaps& caps = rules.caps;
  if (caps.most_blasts == 0 || caps.most_zones == 0 || caps.most_foe_shots == 0 ||
      caps.most_hazards == 0 || caps.most_pickups == 0) {
    return Refuse("plafond du contenu nul");
  }
  return {};
}

core::Result<void> CheckMetaRules(const SurvivorsRules& rules) {
  for (const FoeRule& foe : rules.foes) {
    if (foe.card_item_id != 0 && FindCard(rules, foe.card_item_id) == nullptr) {
      return Refuse("monstre " + std::to_string(foe.foe_id) + " : carte inconnue " +
                    std::to_string(foe.card_item_id));
    }
  }
  if (rules.card_drops.common_odds == 0) return Refuse("cote de chute des cartes nulle");
  const std::span<const CursedWaterRank> cursed = rules.cursed_water;
  if (!cursed.empty()) {
    if (cursed.size() != std::size_t{kMostCursedWater} + 1) {
      return Refuse("Cursed Water de " + std::to_string(cursed.size()) + " rangs, attendu " +
                    std::to_string(kMostCursedWater + 1));
    }
    if (cursed.front().foe_hp_pct != 0 || cursed.front().touch_pct != 0) {
      return Refuse("Cursed Water : le rang 0 majore les monstres");
    }
    for (std::size_t rank = 0; rank < cursed.size(); ++rank) {
      const std::string named = "Cursed Water " + std::to_string(rank) + " : ";
      if (auto checked = CheckPercents<2>(
              named, {{{"PV", cursed[rank].foe_hp_pct}, {"contact", cursed[rank].touch_pct}}});
          !checked) {
        return checked;
      }
    }
  }
  const ReviveRule& revive = rules.revive;
  if (revive.hp_pct <= 0 || revive.hp_pct > kPercent) {
    return Refuse("relève à " + std::to_string(revive.hp_pct) + " % des PV");
  }
  return CheckMeasures<1>("relève : ", {{{"onde", revive.push_units}}});
}

}  // namespace ro::sim::survivors
