#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "ro/core/error.h"
#include "ro/sim/packed_position.h"
#include "ro/sim/survivors_content.h"
#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_terrain.h"

namespace ro::sim {

// LES RÈGLES DU MODE SURVIVORS : des données, injectées dans une partie.
// Si l'essai de rejeu en or change, montez kSurvivorsRulesetVersion : un journal
// ne se rejoue que contre la version de règles qui l'a produit.
inline constexpr std::uint16_t kSurvivorsRulesetVersion = 5;
inline constexpr std::size_t kMostSkillLevels = 8;
inline constexpr std::size_t kMostCardLevels = 5;
inline constexpr std::uint8_t kMostOffers = 4;
// Les héros d'une partie : un en solo, quatre au plus en coop.
inline constexpr std::size_t kMostHeroes = 4;
inline constexpr std::int32_t kMostArenaSideCells = 512;
// Les emplacements de skill qu'un monstre suit un à un (`rehit_ready_step`) :
// CheckSurvivorsRules refuse un `caps.skill_slots` plus grand.
inline constexpr std::size_t kMostSkillSlots = 6;
// Les monstres qu'un tir retient avoir touchés (`hit_serials`), pour n'en
// toucher aucun deux fois : CheckSurvivorsRules refuse un niveau dont
// `pierce + 1` dépasse ce compte.
inline constexpr std::size_t kMostShotHits = 4;
// Les objets d'une salve (frappes, tirs ou orbes), quantité du niveau et
// statistique comprises : CheckSurvivorsRules refuse un niveau qui en demande
// plus, et la simulation n'en émet jamais davantage.
inline constexpr std::uint32_t kMostVolley = 64;
// La plus grande valeur, en valeur absolue, d'un bonus de statistique (loadout
// ou niveau de carte) et d'une majoration en pour cent du catalogue : les
// calculs qui l'appliquent tiennent alors en 64 bits avec une large marge.
inline constexpr std::int32_t kMostStatBonus = 10000;
// La Cursed Water la plus forte qu'un départ puisse porter (0 : aucune).
inline constexpr std::uint8_t kMostCursedWater = 5;

// Dans l'ordre des constantes du serveur.
enum class SurvivorsElement : std::uint8_t {
  kNeutral,
  kWater,
  kEarth,
  kFire,
  kWind,
  kPoison,
  kHoly,
  kDark,
  kGhost,
  kUndead,
};
inline constexpr std::size_t kSurvivorsElementCount = 10;
static_assert(static_cast<std::size_t>(SurvivorsElement::kUndead) + 1 == kSurvivorsElementCount);
// Pourcentage de dégâts d'une attaque (ligne) contre une défense (colonne).
using ElementRow = std::array<std::int16_t, kSurvivorsElementCount>;

// Un ajout se fait en FIN d'énumération.
enum class SkillArchetype : std::uint8_t {
  kTargetStrike,
  kHoming,
  kOrbit,
  kClusterBlast,  // éclats autour d'un point visé
  kLinear,        // projectile droit vers une cible
  kWall,          // mur perpendiculaire au cap du héros
  kNova,          // aire autour du héros
  kRing,          // piliers sur un cercle autour du héros
  kStorm,         // aire posée qui frappe tout son disque, par vagues
  kCone,          // cône devant le héros, dans son cap
  kBarrier,       // blocs posés entre le héros et la horde, qu'elle ne traverse pas
  kStance,        // posture : un état sur le héros, `life_ms` toutes les `cooldown_ms`
  // Réflexe de survie : un coup reçu sous `reflex_below_hp_pct` % de PV le rend
  // intouchable `life_ms`, à sa recharge. Il ne tire rien de lui-même.
  kReflex,
  // INVOCATION : des alliés posés près du héros, au plus `amount` à la fois, un
  // de plus à chaque recharge. Chacun frappe le monstre le plus proche à
  // `reach_units` toutes les `interval_ms`, ou soigne (`heal_pct`). Sans
  // `speed_units_per_s`, il ne bouge pas ; sinon il suit le héros et fond sur
  // ce qui entre dans sa laisse. `life_ms` nul : il dure toute la partie.
  kSummon,
  // SOIN : le héros debout le plus blessé à `reach_units`, lui compris, reprend
  // `heal_pct` % de ses PV max, à la recharge. Rien ne part sans blessé.
  kMend,
};
// Vrai pour un coup lancé de loin : projectile, tir guidé ou éclats visés. Les
// frappes, les zones posées et les aires autour du héros n'en sont pas.
[[nodiscard]] constexpr bool IsRangedArchetype(SkillArchetype archetype) noexcept {
  return archetype == SkillArchetype::kLinear || archetype == SkillArchetype::kHoming ||
         archetype == SkillArchetype::kClusterBlast;
}
// kHero : une aire centrée sur le héros, et non sur un monstre visé.
enum class TargetRule : std::uint8_t { kNearest, kRandom, kStrongest, kDensest, kHero };
enum class SurvivorsStat : std::uint8_t {
  kDamagePct,
  kMaxHp,
  kArmor,
  kRegenPerMinute,
  kWalkPct,
  kCooldownPct,
  kAmount,
  kAreaPct,
  kDurationPct,
  kLuckPct,
  kMagnetPct,
  kExpPct,
  kCritPct,   // la chance, en %, qu'un coup du héros soit critique : il compte double
  kDodgePct,  // la chance, en %, qu'un coup reçu par le héros le manque
  // Les dégâts en plus, en %, des coups à distance (IsRangedArchetype).
  kRangedDamagePct,
  // La part, en %, des dégâts d'un coup du héros qui lui revient en PV, sous le
  // plafond `leech_most_pct_per_second` du réglage.
  kLifeStealPct,
  // Les relèves que les cartes donnent au héros : chaque gain s'ajoute une fois
  // à ses vies de réserve.
  kExtraLives,
  // Le poids, en %, des plantes que seules les cartes font pousser.
  kSproutPct,
  // Les zeny en plus, en %, de chaque Gold ramassé.
  kZenyPct,
  // La chance, en %, qu'un monstre qui touche le héros gèle.
  kFreezeRiposteChancePct,
  // Les dégâts en plus, en %, des coups des invocations.
  kSummonDamagePct,
};
inline constexpr std::size_t kSurvivorsStatCount = 21;
static_assert(static_cast<std::size_t>(SurvivorsStat::kSummonDamagePct) + 1 == kSurvivorsStatCount);
// Une chance se tire sur 100. CheckSurvivorsRules refuse une carte qui en sortirait, une
// posture qui la majorerait, et des cartes d'esquive qui rendraient un héros intouchable.
inline constexpr std::int32_t kWholeChancePct = 100;
// Le vol de vie d'une carte, et les relèves qu'elle donne, au plus.
inline constexpr std::int32_t kMostLifeStealPct = 25;
inline constexpr std::int32_t kMostCardLives = 3;
// LES STATISTIQUES QU'UN LOADOUT PEUT PORTER, les premières de l'énumération :
// ce sont elles que les journaux écrivent. Les suivantes ne viennent que des
// cartes et des postures, et un journal écrit avant elles se relit toujours.
inline constexpr std::size_t kSurvivorsLoadoutStatCount = 12;
static_assert(static_cast<std::size_t>(SurvivorsStat::kExpPct) + 1 == kSurvivorsLoadoutStatCount);
enum class FoeRank : std::uint8_t { kCommon, kMiniBoss, kBoss };
enum class FoeSize : std::uint8_t { kSmall, kMedium, kLarge };
inline constexpr std::size_t kFoeSizeCount = 3;
static_assert(static_cast<std::size_t>(FoeSize::kLarge) + 1 == kFoeSizeCount);

// Un niveau de skill, écrit en entier (pas de « gains » à cumuler).
struct SkillLevel {
  std::int32_t damage = 0;
  std::uint32_t cooldown_ms = 0;
  std::uint8_t amount = 1;             // frappes, projectiles ou orbes
  std::uint32_t interval_ms = 0;       // entre deux frappes d'une même salve
  std::int32_t reach_units = 0;        // portée depuis le héros ; rayon d'orbite
  std::int32_t speed_units_per_s = 0;  // projectile
  std::int32_t hit_radius_units = 0;   // projectile ou orbe
  std::uint32_t life_ms = 0;           // projectile ; phase active d'une orbite
  std::uint32_t rest_ms = 0;           // phase éteinte d'une orbite ; 0 = permanente
  std::uint32_t period_ms = 0;         // un tour d'orbite
  std::uint32_t rehit_ms = 0;          // un même monstre n'est retouché qu'après
  std::uint8_t pierce = 0;
  std::uint32_t chill_ms = 0;
  std::int32_t area_radius_units = 0;  // éclat, nova, aire d'un pilier
  std::int32_t splash_pct = 0;         // éclat, hors du centre
  std::int32_t length_units = 0;       // mur
  std::uint8_t zone_hits = 0;          // mur, pilier ; 0 = la vie seule
  std::int32_t knockback_units = 0;
  std::uint32_t freeze_ms = 0;
  std::uint8_t bounces = 0;
  std::int32_t bounce_pct = 0;
  std::int32_t bounce_reach_units = 0;
  // Orage : ses vagues gèlent à partir de celle-ci (1 : dès la première) ;
  // 0 avec un gel : toutes.
  std::uint8_t freeze_from_wave = 0;
  std::uint32_t blind_ms = 0;  // aveugle : le monstre erre au hasard
  std::uint32_t curse_ms = 0;  // maudit : plus lent, et plus blessé
  // Cône : sa demi-ouverture de part et d'autre du cap, en caps sur 64.
  std::uint8_t cone_half_steps = 0;
  // Barrière : les PV d'un bloc ; `length_units` est sa distance au héros.
  std::int32_t block_hp = 0;
  // Posture : ce qu'elle ajoute à la statistique `stance_stat` du skill, tant
  // qu'elle dure.
  std::int32_t stance_bonus = 0;
  // Le poison que le coup pose : `poison_tick_damage` avant bonus et élément,
  // à chaque tic de `poison_tick_ms`, pendant `poison_ms`.
  std::uint32_t poison_ms = 0;
  std::int32_t poison_tick_damage = 0;
  // Réflexe : le seuil de PV, en %, sous lequel un coup reçu le déclenche ; et
  // vrai quand le héros, intouchable, marche mais ne lance plus de skill (ce qui
  // est déjà parti arrive). Faux : il
  // garde ses coups, et son premier coup en sortant est un critique.
  std::int32_t reflex_below_hp_pct = 0;
  bool reflex_sheathes = false;
  // Invocation : la classe RO que l'allié montre (un monstre ou un homoncule),
  // côté app ; la laisse, autour du héros, d'un allié qui marche. Une
  // invocation qui porte `reflex_below_hp_pct` échange sa place avec celle du
  // héros frappé sous ce seuil, à sa recharge.
  std::uint16_t summon_class = 0;
  std::int32_t leash_units = 0;
  // Soin : la part, en %, des PV max rendue.
  std::int32_t heal_pct = 0;
};

// La vitesse d'un tir en sous-unités par pas, tronquée vers zéro : celle du
// vol, et celle que CheckSurvivorsRules exige non nulle.
[[nodiscard]] constexpr std::int32_t ShotUnitsPerStep(const SkillLevel& level) noexcept {
  return level.speed_units_per_s / static_cast<std::int32_t>(kStepsPerSecond);
}

// Préfixé : `ro::sim::SkillRule` est déjà la règle d'un arbre de compétences
// (skill_tree.h), et deux définitions d'un même nom se mêleraient à l'édition
// de liens sans un mot.
struct SurvivorsSkillRule {
  std::uint16_t skill_id = 0;    // identifiant de compétence RO : nom, icône, effet côté app
  const char* key = "";          // clé stable
  const char* description = "";  // TrText
  SkillArchetype archetype = SkillArchetype::kTargetStrike;
  TargetRule target = TargetRule::kNearest;
  SurvivorsElement element = SurvivorsElement::kNeutral;
  std::span<const SkillLevel> levels;  // niveau 1 en tête
  // Posture : la statistique que `stance_bonus` majore. Jamais les PV max :
  // la fin de la posture les retirerait au héros.
  SurvivorsStat stance_stat = SurvivorsStat::kArmor;
  // Cône : tourné vers l'arrière du cap du héros, et non vers l'avant.
  bool cone_behind = false;
  // LE GROUPE EXCLUSIF : un héros n'en tient qu'un skill ; dès qu'il en tient
  // un, les offres ne proposent plus les autres. 0 : aucun groupe.
  std::uint8_t exclusive_group = 0;
};

struct CardRule {
  std::uint16_t card_item_id = 0;  // identifiant d'objet RO : nom, icône, illustration côté app
  const char* key = "";
  const char* description = "";  // TrText
  SurvivorsStat stat = SurvivorsStat::kDamagePct;
  std::span<const std::int32_t> per_level;  // valeur au niveau n : per_level[n - 1]
};

struct FoeRule {
  std::uint16_t foe_id = 0;   // classe RO (monster_list) : nom et sprite côté app
  std::uint8_t ro_level = 1;  // points de score par tué
  FoeRank rank = FoeRank::kCommon;
  FoeSize size = FoeSize::kSmall;
  SurvivorsElement element = SurvivorsElement::kNeutral;
  std::int32_t hp = 0;
  std::uint32_t ro_walk_ms_per_cell = 0;  // WalkSpeed de db/pre-re/mob_db.yml ; 0 = immobile
  std::int32_t touch_damage = 0;
  std::uint32_t touch_interval_ms = 0;  // AttackDelay de db/pre-re/mob_db.yml
  std::uint32_t exp = 0;
  FoeBehaviour behaviour;
  std::uint16_t card_item_id = 0;  // la carte qu'il peut lâcher ; 0 : aucune
  // SON ARMURE : la part, en %, des dégâts reçus qu'il retranche, après
  // l'élément. 0 : aucune.
  std::int32_t armor_pct = 0;
};
// L'armure la plus forte qu'un monstre puisse porter : au-delà, un coup ne
// le toucherait presque plus.
inline constexpr std::int32_t kMostFoeArmorPct = 80;

// En vigueur de from_ms jusqu'à la vague suivante.
struct WaveRule {
  std::uint32_t from_ms = 0;
  std::span<const std::uint16_t> foe_ids;
  std::span<const std::uint32_t> weights;  // même taille que foe_ids
  std::uint16_t floor = 0;
  std::uint32_t interval_ms = 0;
  std::uint8_t batch = 0;
};
struct BossRule {
  std::uint32_t at_ms = 0;
  std::uint16_t foe_id = 0;
  bool final = false;                  // sa chute est la victoire ; le dernier boss
  std::uint32_t walk_ms_per_cell = 0;  // 0 : celle du FoeRule, sous boss_walk_scale_pct
  std::uint16_t escort_foe_id = 0;     // nés avec lui
  std::uint8_t escort_count = 0;
  std::span<const BossMoveRule> moves;  // son script
};
struct ArenaRule {
  std::uint16_t arena_id = 0;
  const char* key = "";
  const char* map_name = "";          // nom RO de la carte, lue par app
  std::optional<CellPosition> start;  // rien : le centre de la carte
  // La règle de terrain du sol généré ; 0 : la carte de `map_name`. Une arène
  // générée naît au centre du monde, autour de sa clairière : elle ne porte
  // pas de `start`.
  std::uint16_t terrain_id = 0;
  // Classée : la partie classée et le défi s'y jouent. Une arène hors
  // classement ne se joue qu'en partie libre ; les règles en classent une au
  // moins.
  bool ranked = false;
  // SON CONTENU PROPRE : ses vagues, et avec elles ses boss et ses
  // événements. Sans vagues, l'arène joue ceux des règles, et ses boss et
  // événements ne se lisent pas.
  std::span<const WaveRule> waves;
  std::span<const BossRule> bosses;
  std::span<const EventRule> events;
};
// LE CONTENU QU'UNE PARTIE JOUE : les vagues, les boss et les événements de
// son arène, ou ceux des règles.
struct ArenaContent {
  std::span<const WaveRule> waves;
  std::span<const BossRule> bosses;
  std::span<const EventRule> events;
};
struct ClassRule {
  std::uint16_t class_id = 0;
  const char* key = "";
  std::uint16_t job_id = 0;          // apparence, côté app
  std::uint16_t weapon_item_id = 0;  // apparence, côté app
  std::int32_t max_hp = 0;
  std::uint32_t walk_ms_per_cell = 0;
  std::int32_t magnet_units = 0;
  std::int32_t damage_pct_per_level = 0;
  std::int32_t damage_pct_cap = 0;
  std::span<const std::uint16_t> starter_skills;
  std::span<const std::uint16_t> skill_pool;
  std::span<const std::uint16_t> card_pool;
  std::uint16_t evolved_job_id = 0;  // apparence après une évolution, côté app
  // L'arme des corps évolué et rené, côté app ; 0 : celle du départ, sur tous.
  std::uint16_t evolved_weapon_item_id = 0;
  // Il lance ses skills d'un geste d'attaque plutôt que d'une incantation, côté app.
  bool attack_gesture = false;
  // Il traîne un chariot, côté app : ses orbites le font tourner.
  bool pushes_cart = false;
  // LA MONTURE : le héros qui tient cette carte monte, et montre
  // `mounted_job_id`, puis `reborn_mounted_job_id` une fois rené. 0 : aucune
  // monture.
  std::uint16_t mount_card_item_id = 0;
  std::uint16_t mounted_job_id = 0;
  // LA RENAISSANCE : au niveau `reborn_level`, le héros prend le corps de sa
  // classe transcendante, `reborn_job_id`, et ses skills `reborn_damage_pct` %
  // de dégâts en plus. 0 : aucune renaissance.
  std::uint16_t reborn_level = 0;
  std::uint16_t reborn_job_id = 0;
  std::uint16_t reborn_mounted_job_id = 0;
  std::int32_t reborn_damage_pct = 0;
  // LE BOSS FINAL FACE À CETTE CLASSE : ses PV majorés de ce pourcentage, dans
  // [0, kMostStatBonus] ; 0 le laisse tel que le contenu le décrit. À plusieurs,
  // la plus forte majoration de l'équipe vaut pour tous.
  std::int32_t final_boss_hp_bonus_pct = 0;
  // LES CARTES PRÊTÉES : des cartes de sa réserve que les offres proposent
  // aussi à tout coéquipier d'un héros de cette classe. Une équipe qui ne la
  // compte pas ne les voit jamais.
  std::span<const std::uint16_t> lent_cards;
  // HORS CLASSEMENT : la classe ne joue qu'en partie libre ; ni la partie
  // classée ni le défi ne la prennent.
  bool free_play_only = false;
};

// Vrai quand un héros de ce niveau est rené.
[[nodiscard]] constexpr bool Reborn(const ClassRule& hero_class, std::uint16_t level) noexcept {
  return hero_class.reborn_level != 0 && level >= hero_class.reborn_level;
}
// XP du niveau L au niveau L + 1 : base + per_level × (L − from_level), pour le
// dernier segment dont from_level <= L.
struct ExpSegment {
  std::uint16_t from_level = 1;
  std::uint32_t base = 0;
  std::uint32_t per_level = 0;
};
// Rectangle autour du héros, aligné sur la caméra figée au nord : les monstres
// naissent sur son périmètre, plus une marge tirée de 0 à margin_units.
struct SpawnFrame {
  std::int32_t half_width_units = 0;
  std::int32_t north_units = 0;
  std::int32_t south_units = 0;
  std::int32_t margin_units = 0;
};
// Les plafonds du contenu d'une arène complète, que CheckSurvivorsRules exige
// non nuls : ce défaut sert toute règle qui ne les pose pas.
inline constexpr std::uint16_t kDefaultMostBlasts = 256;
inline constexpr std::uint16_t kDefaultMostZones = 64;
inline constexpr std::uint16_t kDefaultMostFoeShots = 128;
inline constexpr std::uint16_t kDefaultMostHazards = 64;
inline constexpr std::uint16_t kDefaultMostPickups = 128;
inline constexpr std::uint16_t kDefaultMostSummons = 32;
struct SurvivorsCaps {
  std::uint16_t most_foes = 0;
  std::uint16_t most_shots = 0;
  std::uint16_t most_strikes = 0;
  std::uint16_t most_gems = 0;
  std::uint8_t skill_slots = 0;  // au plus kMostSkillSlots
  std::uint8_t card_slots = 0;
  // Dans [1, kMostOffers]. La chance ajoute une offre sans dépasser kMostOffers :
  // son tirage a lieu même quand la place manque, pour que le flux des offres
  // ne dépende pas du plafond.
  std::uint8_t offers = 0;
  std::uint16_t most_blasts = kDefaultMostBlasts;
  std::uint16_t most_zones = kDefaultMostZones;
  std::uint16_t most_foe_shots = kDefaultMostFoeShots;
  std::uint16_t most_hazards = kDefaultMostHazards;
  std::uint16_t most_pickups = kDefaultMostPickups;  // les coffres passent hors plafond
  // Les invocations de toute l'équipe.
  std::uint16_t most_summons = kDefaultMostSummons;
};
// LES CAPS DES PLANTES DU DÉPART, dans l'ordre de leur pose, en pas d'une
// case vers l'est et vers le nord : est, ouest, nord, sud, puis nord-est,
// sud-ouest, nord-ouest, sud-est.
struct PlantStartHeading {
  std::int8_t east = 0;
  std::int8_t north = 0;
};
inline constexpr std::array<PlantStartHeading, 8> kPlantStartHeadings{{
    {.east = 1, .north = 0},
    {.east = -1, .north = 0},
    {.east = 0, .north = 1},
    {.east = 0, .north = -1},
    {.east = 1, .north = 1},
    {.east = -1, .north = -1},
    {.east = -1, .north = 1},
    {.east = 1, .north = -1},
}};
struct SurvivorsTuning {
  std::int32_t foe_walk_scale_pct = 0;
  std::int32_t foe_hp_growth_pct_per_minute = 0;
  std::int32_t chill_slow_pct = 0;
  std::int32_t boss_chill_share_pct = 0;
  std::int32_t hero_radius_units = 0;
  std::array<std::int32_t, kFoeSizeCount> foe_radius_units = {};
  std::int32_t touch_reach_units = 0;
  std::int32_t direct_chase_units = 0;
  std::int32_t recall_units = 0;
  std::int32_t flow_radius_cells = 0;
  std::uint32_t flow_refresh_steps = 0;
  std::int32_t separation_max_push_units = 0;
  std::int32_t hero_shove_pct = 0;
  std::int32_t gem_pull_start_units = 0;
  std::int32_t gem_pull_accel_units = 0;
  std::uint32_t offer_weight_owned = 0;
  std::uint32_t offer_weight_fresh = 0;
  std::int32_t heal_offer_hp = 0;
  std::uint32_t zeny_offer = 0;
  std::uint16_t most_floor_spawns_per_step = 0;
  std::uint8_t spawn_tries_per_foe = 0;
  std::int32_t spawn_search_ring = 0;
  std::int32_t most_cooldown_pct = 0;
  // Le plus court repos d'une posture : quand la durée allongée rattrape la
  // recharge raccourcie, elle retombe encore ce temps avant de reprendre.
  std::uint32_t stance_least_rest_ms = 0;
  std::int32_t boss_hp_growth_pct_per_minute = 0;
  std::int32_t boss_walk_scale_pct = 0;
  std::int32_t knock_units_per_step = 0;  // vitesse d'un recul
  // LES PLANTES SE POSENT D'UNE DE DEUX FAÇONS, exclusives :
  // - par blocs, au départ, sur toute une carte (`plant_block_cells` et
  //   `plant_cells_per_plant` non nuls, `plant_kills_per_minute` nul) ;
  // - au fil des tués (`plant_kills_per_minute` non nul, les deux réglages de
  //   blocs nuls) : `plant_start_count` plantes autour du départ, puis une
  //   plante juste hors du cadre d'apparition chaque fois que les tués depuis
  //   la précédente atteignent `plant_kills_per_minute` × la minute en cours
  //   (1 de 0:00 à 0:59, 2 de 1:00 à 1:59…), au plus `most_live_plants` en vie
  //   (0 : sans plafond ; le rythme et la perte bornent alors la liste).
  std::int32_t plant_block_cells = 0;       // côté d'un bloc qui porte au plus une plante
  std::uint32_t plant_cells_per_plant = 0;  // cases praticables d'un bloc par plante
  std::uint32_t plant_kills_per_minute = 0;
  std::uint8_t plant_start_count = 0;
  // La distance, en cases, entre le héros et les plantes du départ, sur les
  // caps de kPlantStartHeadings.
  std::int32_t plant_start_cells = 0;
  std::uint8_t most_live_plants = 0;
  // Une plante plus loin du héros que cette distance de Tchebychev SE PERD,
  // sur tout sol ; 0 : aucune ne se perd, ce qui n'est permis qu'aux plantes
  // posées par blocs.
  std::int32_t plant_forget_units = 0;
  std::int32_t pickup_radius_units = 0;  // ramassage d'un objet au sol
  std::uint32_t final_overtime_ms = 0;   // au-delà de run_ms, avec un boss final
  // LES STATUTS D'AVEUGLEMENT ET DE MALÉDICTION, que seuls les communs
  // subissent. Un maudit marche `curse_slow_pct` % moins vite et prend
  // `curse_hurt_pct` % de dégâts de plus ; un aveuglé erre au hasard, et change
  // de cap tous les `blind_turn_ms`.
  std::int32_t curse_slow_pct = 0;
  std::int32_t curse_hurt_pct = 0;
  std::uint32_t blind_turn_ms = 0;
  // LE POISON, que seuls les communs subissent : un tic toutes les
  // `poison_tick_ms`.
  std::uint32_t poison_tick_ms = 0;
  // LE VOL DE VIE rend au plus ce pourcentage des PV max du héros par seconde.
  std::int32_t leech_most_pct_per_second = 0;
};

// LA CHUTE DES CARTES. Un tué qui porte une carte la lâche une fois sur
// `common_odds` ; le premier tué de chaque espèce de mini-boss ou de boss dans
// la partie la lâche à coup sûr. La carte ne change rien à la partie : elle se
// ramasse pour l'Album et pour les parties suivantes.
inline constexpr std::uint32_t kCommonCardOdds = 300;
struct CardDropRule {
  std::uint32_t common_odds = kCommonCardOdds;
};
// Un rang de Cursed Water : les PV de naissance et le contact des monstres,
// majorés de ces pourcentages.
struct CursedWaterRank {
  std::int32_t foe_hp_pct = 0;
  std::int32_t touch_pct = 0;
};
// LA RELÈVE d'un héros qui tombe avec une vie en réserve : il se relève sur
// place à `hp_pct` % de ses PV max, ne prend aucun coup pendant
// `invulnerable_ms`, et une onde repousse jusqu'à `push_units` de lui les
// monstres qu'un recul peut déplacer.
inline constexpr std::int32_t kReviveHpPct = 50;
inline constexpr std::uint32_t kReviveInvulnerableMs = 3000;
inline constexpr std::int32_t kRevivePushCells = 4;
struct ReviveRule {
  std::int32_t hp_pct = kReviveHpPct;
  std::uint32_t invulnerable_ms = kReviveInvulnerableMs;
  std::int32_t push_units = Cells(kRevivePushCells);
};

struct SurvivorsRules {
  std::uint16_t ruleset_version = 0;
  std::uint32_t run_ms = 0;
  std::span<const ClassRule> classes;
  std::span<const SurvivorsSkillRule> skills;
  std::span<const CardRule> cards;
  std::span<const FoeRule> foes;
  std::span<const WaveRule> waves;   // triées par from_ms, la première à 0
  std::span<const BossRule> bosses;  // triés par at_ms
  std::span<const ArenaRule> arenas;
  std::span<const ExpSegment> exp_curve;  // triée, la première à 1
  std::span<const ElementRow> elements;   // kSurvivorsElementCount lignes
  SpawnFrame spawn_frame;
  SurvivorsCaps caps;
  SurvivorsTuning tuning;
  std::span<const EventRule> events;  // triés par at_ms
  std::span<const EvolutionRule> evolutions;
  std::span<const PlantRule> plants;
  std::span<const PickupRule> pickups;  // une règle par sorte d'objet
  EliteRule elites;
  ChestRule chests;
  CardDropRule card_drops;
  // Un rang par entrée, de 0 à kMostCursedWater ; vide : aucune Cursed Water.
  std::span<const CursedWaterRank> cursed_water;
  ReviveRule revive;
  // Les règles de terrain que nomment les arènes générées.
  std::span<const TerrainRule> terrains;
};

// La table livrée : constante, construite une fois.
[[nodiscard]] const SurvivorsRules& ShippedSurvivorsRules();
// kInvalidArgument, avec le détail, pour tout catalogue incohérent : version ou
// durée nulles ; Id en double (classes, skills, cartes, monstres, arènes), ou
// dans une réserve ou les skills de départ d'une classe ; Id de réserve, de
// départ, de vague ou de boss inconnu ; Id présent à la fois dans la réserve de
// skills et dans celle de cartes d'une classe (un bannissement ne nomme qu'un
// Id) ; skill de départ hors de la réserve ; classe à PV max nuls, dont la
// marche ne bouge pas d'une sous-unité par pas, ou dont l'aimant ou la
// majoration de dégâts sort de ses bornes ; skill sans niveau ou avec plus de
// kMostSkillLevels ; niveau de skill à `amount` nul ou au-delà de kMostVolley ;
// niveau, de tout archétype, dont `pierce + 1` dépasse kMostShotHits ; niveau
// kOrbit à `period_ms` ou `life_ms` nul ; niveau kTargetStrike ou kHoming à
// `reach_units` nul ; niveau kHoming à `life_ms` nul ou dont la vitesse ne
// donne pas une sous-unité par pas ; portée, rayon ou vitesse au-delà de la
// plus grande arène ; carte sans niveau, avec plus de kMostCardLevels, ou dont
// une valeur dépasse kMostStatBonus ; poids d'une vague de taille autre que ses
// monstres, de somme nulle ou supérieure à 0xFFFFFFFF ; vague à lot non nul
// sans intervalle ; vagues non triées ou dont la première n'est pas à 0 ; boss
// non triés ; courbe d'XP vide, non triée, qui ne commence pas au niveau 1, ou
// dont un segment a une base nulle ; table des éléments de taille autre que
// kSurvivorsElementCount ; monstre à PV nuls, ou à dégâts de contact sans
// intervalle ; cadre d'apparition nul, ou dont une mesure dépasse
// Cells(kMostArenaSideCells) ; réglage hors de ses bornes (rayon du héros nul,
// mesure négative ou au-delà de la plus grande arène, aucun essai
// d'apparition, croissance des PV hors de [0, kMostStatBonus]) ; plafond nul ;
// plus de kMostSkillSlots emplacements de skill ; `offers` hors de [1,
// kMostOffers].
// Une vague à plancher et lot nuls est VALIDE : elle ne fait naître personne.
// Le contenu d'une arène complète est refusé ensuite si : un niveau
// kClusterBlast, kNova ou kRing n'a pas d'aire ; un niveau kWall n'a pas de
// longueur, ou un mur ou un pilier pas de vie ; un niveau kLinear ne bouge pas
// d'une sous-unité par pas ; un éclat, un rebond ou une majoration sort de
// [0, kMostStatBonus] ; un rebond n'a pas de portée, ou `bounces + 1` dépasse
// kMostShotHits ; kDensest sert autre chose qu'une frappe ou un éclat, ou
// kRandom un éclat ; une
// meute est vide ; un comportement à acte n'a pas de période ; une annonce
// n'est pas plus courte que sa période ; un tir ou une charge ne bouge pas
// d'une sous-unité par pas ; une flaque qui blesse n'a pas de période ; plus
// d'un boss final, ou un boss final qui n'est pas le dernier ; un boss ou un
// coup nomme une espèce inconnue ; un script dépasse kMostBossMoves coups, ou
// porte sur une espèce dont le comportement joue un acte ; un
// coup a des seuils de PV vides ou au-delà de 100, ou pas de période ; un
// recul sans vitesse de recul ; un événement est non
// trié, d'espèce inconnue, vide, ou un anneau sans rayon ; une élite naît sans
// majoration de PV ; une évolution nomme un Id inconnu, un évolué présent dans
// une réserve de classe, un skill ou un évolué déjà nommé ; un objet a deux
// règles ; une plante n'a pas de règle d'objet ; les poids des plantes ont une
// somme nulle ; des plantes ni par blocs (bloc et densité) ni au fil des tués,
// ou des deux façons ; des plantes au fil des tués sans plafond, sans oubli,
// avec un oubli qui atteindrait une plante à sa naissance, plus de plantes au
// départ que le plafond ou que kPlantStartHeadings, ou des plantes du départ
// hors de l'écran (leur distance plus l'anneau de recherche au-delà du cadre
// d'apparition) ; des élites sans objet de
// coffre ; un coffre promet de 1 à kMostChestGains gains hors de cet ordre ;
// la partie et son dépassement dépassent 32 bits de ms ; une mesure neuve
// dépasse la plus grande arène ; un plafond neuf est nul ; un poison sans durée
// ou sans dégâts, hors d'une frappe, d'un tir ou d'un orage, ou sans période de
// tic dans les réglages ; un réflexe sans durée, sans recharge, ou dont le seuil
// sort de [1, 100] % de PV ; un cône tourné vers l'arrière qui n'est pas un cône.
// La méta est refusée enfin si : un monstre porte une carte absente du
// catalogue ; la cote de chute est nulle ; la table de la Cursed Water n'est
// ni vide ni de kMostCursedWater + 1 rangs, son rang 0 majore quelque chose,
// ou une majoration sort de [0, kMostStatBonus] ; la relève rend des PV hors
// de [1, 100] % ou repousse au-delà de la plus grande arène.
// Les terrains sont refusés pour : un Id nul, un Id ou une clé en double, de
// terrain comme de pièce d'un terrain ; une emprise vide, sans case bloquée,
// de lignes inégales, d'un caractère autre que « . » et « # », plus grande que
// son emplacement moins sa bordure, ou qui enclave une case libre ; des poids
// de pièces ordinaires de somme nulle ou supérieure à 0xFFFFFFFF, de même pour
// les repères quand il y en a ; des repères sans période ; des densités hors de
// [0, 1000] ou inversées ; une période de densité nulle, ou une période au-delà
// du monde ; une clairière hors de [0, kMostArenaSideCells] ; une arène qui
// nomme un terrain inconnu.
[[nodiscard]] core::Result<void> CheckSurvivorsRules(const SurvivorsRules& rules);
// Recherches linéaires par Id ; nullptr pour un Id inconnu.
[[nodiscard]] const SurvivorsSkillRule* FindSkill(const SurvivorsRules& rules,
                                                  std::uint16_t skill_id);
[[nodiscard]] const CardRule* FindCard(const SurvivorsRules& rules, std::uint16_t card_item_id);
[[nodiscard]] const FoeRule* FindFoe(const SurvivorsRules& rules, std::uint16_t foe_id);
[[nodiscard]] const ClassRule* FindClass(const SurvivorsRules& rules, std::uint16_t class_id);
[[nodiscard]] const ArenaRule* FindArena(const SurvivorsRules& rules, std::uint16_t arena_id);
// LES CLASSES QUI PEUVENT OBTENIR UNE CARTE, dans l'ordre du catalogue : une
// carte ne tombe que si la réserve d'une classe de l'équipe la contient.
[[nodiscard]] std::vector<const ClassRule*> ClassesHoldingCard(const SurvivorsRules& rules,
                                                               std::uint16_t card_item_id);
// La première arène classée du catalogue, celle de la partie classée et du
// défi ; nulle quand aucune ne l'est.
[[nodiscard]] const ArenaRule* FirstRankedArena(const SurvivorsRules& rules);
[[nodiscard]] const TerrainRule* FindTerrain(const SurvivorsRules& rules, std::uint16_t terrain_id);
// L'évolution d'un skill de base, et celle qui donne un évolué.
[[nodiscard]] const EvolutionRule* FindEvolutionOf(const SurvivorsRules& rules,
                                                   std::uint16_t skill_id);
[[nodiscard]] const EvolutionRule* FindEvolutionTo(const SurvivorsRules& rules,
                                                   std::uint16_t evolved_skill_id);
// La règle de la sorte d'objet `kind`, ou nullptr.
[[nodiscard]] const PickupRule* FindPickup(const SurvivorsRules& rules, PickupKind kind);
// Vrai si un boss est marqué final : sa chute clôt la partie en victoire.
[[nodiscard]] bool HasFinalBoss(const SurvivorsRules& rules);
[[nodiscard]] bool HasFinalBoss(std::span<const BossRule> bosses);
// Le contenu que joue l'arène `arena` : le sien quand elle a des vagues,
// sinon celui des règles.
[[nodiscard]] ArenaContent ContentOf(const SurvivorsRules& rules, const ArenaRule& arena);
// Le contenu des règles, puis celui de chaque arène qui a le sien : de quoi
// parcourir toutes les espèces que les règles font naître.
[[nodiscard]] std::vector<ArenaContent> AllContents(const SurvivorsRules& rules);
// L'XP qui mène du niveau `level` au suivant, dans [1, 0xFFFFFFFF] : jamais 0,
// pour qu'une boucle de montées finisse toujours. Sans segment applicable
// (courbe vide, niveau 0), 0xFFFFFFFF : aucune montée.
[[nodiscard]] std::uint32_t ExpToNext(const SurvivorsRules& rules, std::uint16_t level);
// Négatif dans la table : 0 (le mode ne soigne pas un monstre). Sans table
// complète, 100.
[[nodiscard]] std::int32_t ElementPct(const SurvivorsRules& rules, SurvivorsElement attack,
                                      SurvivorsElement defence);

}  // namespace ro::sim
