#pragma once

// LES SYSTÈMES D'UNE PARTIE DU MODE SURVIVORS : une fonction libre par système,
// sur le champ entier. SurvivorsRun::Step les appelle dans un ordre fixé, écrit
// à un seul endroit (survivors_run.cpp) ; aucun système n'en appelle un autre,
// sauf les aides de dégâts, de statistiques et d'offres déclarées en fin de
// fichier. Deux systèmes d'un même fichier peuvent partager une aide interne :
// MoveHorde reconstruit la grille pour sa séparation par la même aide de
// survivors_horde.cpp que RebuildGrid. Les aides que plusieurs fichiers
// partagent (pourcentages saturés, niveau d'un skill, glissement contre les
// murs, cadre d'apparition, impasses du champ, monstre par serial, rayons)
// sont dans survivors_space.h ; celles que les archétypes de skill partagent
// (visée, plafonds, statuts) dans survivors_volley.h.
//
// En-tête PRIVÉ de `libs/sim/src`.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"
#include "ro/sim/survivors_state.h"

namespace ro::sim::survivors {

// ── Les systèmes, dans l'ordre du pas ──────────────────────────────────────

// survivors_hero.cpp. Le héros prend le cap reçu, ramené dans [0,
// kHeadingSteps) (rien sur kNoHeading), puis avance de sa marche, axe par axe,
// contre les murs. Sous une Speed Potion, sa marche est majorée de l'objet.
void MoveHero(SurvivorsField& field, SurvivorsInput input);
// survivors_horde.cpp. Le champ de poursuite suit la case du héros.
void RefreshFlow(SurvivorsField& field);
// survivors_spawner.cpp. Vagues (et leurs meutes), plancher et boss.
void SpawnFoes(SurvivorsField& field);
// survivors_events.cpp. Les événements arrivés à leur heure (essaim, raid,
// anneau, groupe), sous le plafond des monstres, puis l'élite due, hors
// plafond.
void SpawnEvents(SurvivorsField& field);
// survivors_bosses.cpp. Les scripts des boss en vie, dans l'ordre de leur
// naissance : l'escorte à l'arrivée, puis chaque coup actif dans sa phase de
// PV et prêt (invocation, bond, charge, séisme, hurlement, fuite), et le coup
// en cours qui mène le corps du boss. Le suivi d'un boss parti s'efface.
void RunBossScripts(SurvivorsField& field);
// survivors_behaviours.cpp. Le pas que mène le comportement d'un monstre :
// essaims et raids qui filent (et quittent la partie au loin), actes annoncés
// puis joués (tir, charge, bond, cachette), volant, voleur, tireur, immobile.
// Pose `previous_position` et `steered` de chaque monstre, sauf d'un boss
// qu'un coup mène (RunBossScripts les a posés).
void SteerFoes(SurvivorsField& field);
// survivors_horde.cpp. Recul (à la place du rappel et de la poursuite), rappel
// (jamais d'un essaim, jamais pour impasse d'un monstre qui traverse les murs),
// poursuite (aucune pour un gelé ni pour un monstre que SteerFoes a mené),
// séparation (sur une grille refaite après la poursuite, sans les monstres
// au-dessus de la horde), poussée du héros ; un monstre gelé y compte comme
// immobile.
void MoveHorde(SurvivorsField& field);
// survivors_horde.cpp. `foe_points` et `foe_grid` reflètent `foes`.
void RebuildGrid(SurvivorsField& field);
// survivors_skills.cpp. Les salves dues, aiguillées par archétype : frappes
// promises, tirs chercheurs ou droits, orbites, éclats, novas, murs, piliers.
// Un niveau que CheckSurvivorsRules aurait refusé (quantité nulle, période ou
// vie d'orbite nulle, vitesse ou vie de tir nulle, aire ou longueur nulle) ne
// tire rien, sans diviser par zéro.
void FireSkills(SurvivorsField& field);
// survivors_skills.cpp. Les frappes arrivées à leur pas, et leurs rebonds.
void LandStrikes(SurvivorsField& field);
// survivors_blasts.cpp. Les éclats et les novas arrivés à leur pas.
void LandBlasts(SurvivorsField& field);
// survivors_shots.cpp. Les projectiles avancent, touchent, expirent.
void AdvanceShots(SurvivorsField& field);
// survivors_zones.cpp. Les murs et les piliers touchent, puis s'éteignent.
void AdvanceZones(SurvivorsField& field);
// survivors_progress.cpp. Les monstres à PV nuls tombent et laissent leur gemme,
// celle de l'XP qu'un voleur portait, leur flaque, et le coffre d'une élite ou
// d'un mini-boss. La chute du boss final pose kOver, kVictory.
void BuryFoes(SurvivorsField& field);
// survivors_threats.cpp. Les tirs des monstres avancent et touchent le héros ;
// les flaques et les séismes armés le blessent.
void AdvanceThreats(SurvivorsField& field);
// survivors_hero.cpp. Le contact des monstres, sauf des gelés et des cachés ;
// PV à 0 : kOver, kFell.
void TouchHero(SurvivorsField& field);
// survivors_progress.cpp. L'aimant, le ramassage, les montées de niveau.
void GatherGems(SurvivorsField& field);
// survivors_loot.cpp. Le héros casse les plantes qu'il touche, puis ramasse
// les objets à son contact et en applique l'effet, coffres compris. Une carte
// ramassée s'inscrit au récapitulatif, sans autre effet.
void GatherPickups(SurvivorsField& field);
// survivors_hero.cpp. Les PV par minute, accumulés pas à pas.
void Regenerate(SurvivorsField& field);

// ── Les aides ──────────────────────────────────────────────────────────────

// survivors_loot.cpp. Au départ : par bloc de `plant_block_cells` de côté,
// lignes du sud au nord puis colonnes d'ouest en est, un Chance du flux décor
// de (cases praticables du bloc) sur `plant_cells_per_plant` ; si oui, un
// Below pour la case, parmi les praticables du bloc dans le même ordre, et un
// PickWeighted pour l'espèce. Rien sans plantes dans les règles, ni sur un sol
// généré, qui n'a pas de bord à paver.
void PlantField(SurvivorsField& field);
// survivors_loot.cpp. Sur un sol généré, au départ puis après chaque marche du
// héros : chaque morceau du monde qui touche le carré de `plant_wake_cells`
// autour de la case du héros et n'est pas encore éveillé s'éveille, par clé
// croissante. Il pose ses plantes comme PlantField pose celles d'une carte,
// par bloc de `plant_block_cells` compté depuis son coin sud-ouest et rogné à
// son bord, mais d'un Pcg32 à lui (TerrainPlantSeed, flux décor) : ses plantes
// ne dépendent ni de l'heure ni des autres morceaux ; seuls leurs numéros de
// naissance suivent l'ordre des éveils. Rien sur un sol borné, sans plantes,
// ou si `plant_wake_cells` est nul.
void WakePlantChunks(SurvivorsField& field);
// survivors_loot.cpp. Sur un sol généré, retire, dans l'ordre de pose, chaque
// plante plus loin du héros que `plant_forget_units` en distance de
// Tchebychev. Son morceau reste éveillé : elle ne repousse jamais. Rien sur un
// sol borné, ou si `plant_forget_units` est nul.
void ForgetFarPlants(SurvivorsField& field);

// Comment naît un monstre, au-delà de son espèce et de sa place.
struct HatchOptions {
  const BossRule* boss = nullptr;           // un boss : sa marche propre
  bool elite = false;                       // majorations de EliteRule
  std::uint8_t drift_heading = kNoHeading;  // un essaim ou un raid file à ce cap
};
// survivors_spawner.cpp. Le monstre `rule` naît en `at` : numéro suivant, PV
// grandis de la minute en cours (au taux des boss pour un mini-boss ou un
// boss), majorés pour une élite, marche à l'échelle du réglage (celle des boss,
// ou la marche propre du BossRule), contact prêt tout de suite, tourné vers le
// héros. Rend son numéro.
std::uint32_t HatchFoe(SurvivorsField& field, const FoeRule& rule, FieldPoint at,
                       const HatchOptions& options = {});
// survivors_events.cpp. Un monstre `rule` né près de `wanted`, au centre de la
// case praticable la plus proche à un anneau au plus, sous le plafond des
// monstres. Faux, sans naissance, au plafond ou sans case praticable à portée.
bool HatchNear(SurvivorsField& field, const FoeRule& rule, FieldPoint wanted,
               const HatchOptions& options = {});
// survivors_bosses.cpp. Le suivi du boss `serial`, né à ce pas de la règle
// `rules.bosses[rule_index]` : chaque coup prêt `first_ms` après sa naissance,
// l'escorte encore due.
void TrackBoss(SurvivorsField& field, std::uint32_t serial, std::size_t rule_index);
// survivors_bosses.cpp. Vrai si `serial` est un boss suivi dont la règle est
// le boss final.
[[nodiscard]] bool IsFinalBoss(const SurvivorsField& field, std::uint32_t serial);
// survivors_spawner.cpp. Le point du périmètre du cadre à l'abscisse
// curviligne `along`, poussé vers l'extérieur de `outward`. Le périmètre se
// parcourt dans le sens horaire vu du ciel : le côté nord d'ouest en est, l'est
// du nord au sud, le sud d'est en ouest, l'ouest du sud au nord.
[[nodiscard]] FieldPoint PerimeterPoint(const SpawnFrame& frame, FieldPoint hero,
                                        std::int64_t along, std::int32_t outward) noexcept;
// survivors_spawner.cpp. Un point de naissance tiré sur le périmètre du cadre,
// plus une marge, ramené au centre de la case praticable la plus proche.
// Chaque essai consomme exactement deux tirages du flux des apparitions ; rien
// après `spawn_tries_per_foe` essais refusés (aucune case proche, une case qui
// retombe dans le cadre, ou une impasse que le champ de poursuite connaît).
[[nodiscard]] std::optional<FieldPoint> DrawSpawnPoint(SurvivorsField& field);

// survivors_progress.cpp. Ce que BuryFoes fait des monstres à PV nuls, pour un
// objet qui tue hors de son passage (Holy Water).
void BuryFallen(SurvivorsField& field);
// survivors_loot.cpp. La carte que `foe`, de règle `rule`, lâche peut-être en
// tombant : à coup sûr pour le premier tué de son espèce de mini-boss ou de
// boss, sinon par un Chance(1, common_odds) du flux des cartes, tiré pour
// chaque tué qui porte une carte et pour lui seul. Posée au sol hors plafond,
// sous un numéro de carte.
void DropCard(SurvivorsField& field, const SurvivorsFoe& foe, const FoeRule& rule);

// survivors_progress.cpp. `hero.stats` (loadout + cartes), `max_hp` (un gain
// s'ajoute aux PV courants), `units_per_step` et `magnet_units` du héros.
void RecomputeStats(SurvivorsField& field);
// survivors_progress.cpp. Remplace `offers` par un tirage du flux des offres :
// `caps.offers`, plus une si la chance sourit, au plus kMostOffers. Un Id banni
// n'est plus offert, ni en nouveauté ni en montée de niveau.
void BuildOffers(SurvivorsField& field);
// survivors_progress.cpp. Valide et applique une réponse au choix ouvert, puis
// rouvre un choix ou rend la partie à kRunning. Faux, sans rien changer, pour
// une réponse refusée (voir SurvivorsRun::Answer). Appelée en kChoosing seul.
// Un skill acquis prend `ready_step = step` ; une orbite acquise ou montée de
// niveau prend aussi `phase_step = step`, et repart au début de sa phase active.
[[nodiscard]] bool ApplyAnswer(SurvivorsField& field, SurvivorsAnswer answer);

// survivors_hero.cpp. `damage` moins l'armure du héros, au moins 1, retiré de
// ses PV et compté dans `hero_hurt` ; rien pendant l'invulnérabilité d'une
// relève. PV à 0 : une vie de réserve le relève (ReviveRule) et rend faux ;
// sans vie, kOver, kFell, et vrai.
bool HurtHero(SurvivorsField& field, std::int32_t damage);

// Des dégâts calculés contre un monstre, et l'efficacité d'élément appliquée.
struct SurvivorsDamage {
  std::int32_t amount = 0;
  std::int32_t efficiency_pct = 100;
};
// survivors_skills.cpp. `base` majoré des bonus de dégâts du héros (cartes et
// niveau, plafonné), puis de l'élément contre l'élément de défense de `foe`
// (Water s'il est gelé) ; au moins 1 si l'élément ne l'annule pas.
[[nodiscard]] SurvivorsDamage DamageAgainst(const SurvivorsField& field, std::int32_t base,
                                            SurvivorsElement attack, const SurvivorsFoe& foe);
// Ce qu'un coup inflige en plus des dégâts : un gel, et un recul au cap
// `knock_heading`.
struct SurvivorsStatus {
  std::uint32_t freeze_ms = 0;
  std::int32_t knockback_units = 0;
  std::uint8_t knock_heading = 0;
};
// survivors_skills.cpp. Retire les PV, pose `hurt_step`, ajoute `damage_dealt`,
// applique `status` à un monstre qui survit (ApplyStatus), publie un
// SurvivorsHit qui dit s'il a gelé ou repoussé.
void HurtFoe(SurvivorsField& field, SurvivorsFoe& foe, SurvivorsDamage damage,
             std::uint16_t skill_id, const SurvivorsStatus& status = {});
// Les salves des archétypes d'aire et de zone, que FireSkills aiguille : vrai
// quand une salve part, faux, sans rien émettre, quand le skill doit attendre
// (aucune cible, plafond atteint). survivors_shots.cpp : le projectile droit.
[[nodiscard]] bool FireLinear(SurvivorsField& field, std::uint8_t slot,
                              const SurvivorsSkillRule& rule, const SkillLevel& level);
// survivors_blasts.cpp : l'éclat visé et la nova.
[[nodiscard]] bool FireClusterBlast(SurvivorsField& field, const SurvivorsSkillRule& rule,
                                    const SkillLevel& level);
[[nodiscard]] bool FireNova(SurvivorsField& field, const SurvivorsSkillRule& rule,
                            const SkillLevel& level);
// survivors_zones.cpp : le mur et le cercle de piliers.
[[nodiscard]] bool FireWall(SurvivorsField& field, std::uint8_t slot,
                            const SurvivorsSkillRule& rule, const SkillLevel& level);
[[nodiscard]] bool FireRing(SurvivorsField& field, std::uint8_t slot,
                            const SurvivorsSkillRule& rule, const SkillLevel& level);
// survivors_skills.cpp. Les orbes du DERNIER pas exécuté (`field.step − 1`, ou
// le pas 0 avant le premier), par le même calcul que FireSkills : celles qui
// ont touché, alignées sur la position des monstres. Une orbite dont la phase
// part au pas suivant (acquise ou montée pendant le choix) n'en montre aucune.
// Ajoutées à `out`.
void CollectOrbs(const SurvivorsField& field, std::vector<SurvivorsOrb>& out);

}  // namespace ro::sim::survivors
