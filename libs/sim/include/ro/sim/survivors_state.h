#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_rules.h"

namespace ro::sim {

// L'ÉTAT D'UNE PARTIE DU MODE SURVIVORS, pièce par pièce. Tout y est entier, et
// chaque liste garde l'ordre de naissance ou d'émission : c'est cet ordre, et
// lui seul, qui départage deux éléments, pour que deux machines rejouent la
// même partie.

enum class SurvivorsPhase : std::uint8_t { kRunning, kChoosing, kOver };
enum class SurvivorsOutcome : std::uint8_t { kRunning, kFell, kSurvived, kVictory, kAbandoned };
enum class OfferKind : std::uint8_t { kNewSkill, kSkillLevel, kNewCard, kCardLevel, kHeal, kZeny };

struct SurvivorsOffer {
  OfferKind kind = OfferKind::kHeal;
  std::uint16_t id = 0;  // skill_id ou card_item_id ; 0 pour kHeal et kZeny
  std::uint8_t next_level = 0;
  std::uint8_t hero_index = 0;  // le héros qui la reçoit ; 0 en solo
  [[nodiscard]] bool operator==(const SurvivorsOffer&) const = default;
};
struct OwnedSkill {
  std::uint16_t skill_id = 0;
  std::uint8_t level = 1;
  std::uint32_t ready_step = 0;  // prochaine salve possible
  // Origine de la phase d'une orbite : le pas de son acquisition, puis de
  // chaque montée de niveau, pour qu'elle reparte au début de sa phase active.
  std::uint32_t phase_step = 0;
  // Une posture dure tant que `step < stance_until_step` ; 0 : éteinte.
  std::uint32_t stance_until_step = 0;
};
struct OwnedCard {
  std::uint16_t card_item_id = 0;
  std::uint8_t level = 1;
  [[nodiscard]] bool operator==(const OwnedCard&) const = default;
};
struct SurvivorsStats {
  std::array<std::int32_t, kSurvivorsStatCount> values = {};
  [[nodiscard]] std::int32_t Of(SurvivorsStat stat) const noexcept {
    return values[static_cast<std::size_t>(stat)];
  }
  [[nodiscard]] bool operator==(const SurvivorsStats&) const = default;
};
// LE BOND DE BACK SLIDE : le héros glisse de tant de cases dans le sens de sa
// marche, en tant de pas, le regard tourné vers ce qu'il a frappé.
inline constexpr std::int32_t kBackSlideLeapCells = 4;
inline constexpr std::uint8_t kBackSlideLeapSteps = 15;

struct SurvivorsHero {
  FieldPoint position;
  FieldPoint previous_position;
  std::uint8_t facing = 0;  // cap 0..63
  // LE BOND DE BACK SLIDE en cours : les pas qui restent, et le cap où il
  // emporte le héros, qui regarde à l'opposé ce qu'il vient de frapper.
  std::uint8_t leap_steps_left = 0;
  std::uint8_t leap_heading = 0;
  std::int32_t hp = 0;
  std::int32_t max_hp = 0;
  std::uint16_t level = 1;
  std::uint32_t exp = 0;          // dans le niveau en cours
  std::uint32_t regen_carry = 0;  // PV par minute accumulés, en pas
  std::uint32_t zeny = 0;
  std::vector<OwnedSkill> skills;  // l'emplacement est l'index
  std::vector<OwnedCard> cards;
  SurvivorsStats stats;  // classe + méta + cartes
  // Dérivés des statistiques par RecomputeStats : la marche du héros, en
  // sous-unités par pas, et la portée de son aimant, en sous-unités.
  std::int32_t units_per_step = 0;
  std::int32_t magnet_units = 0;
  // Les objets à effet durable : l'XP majorée (Battle Manual) et la marche
  // majorée (Speed Potion), tant que `step < …_until_step`.
  std::uint32_t exp_boost_until_step = 0;
  std::uint32_t haste_until_step = 0;
  std::uint8_t evolutions = 0;  // skills évolués par un coffre
  // Relevé, il ne prend aucun coup tant que `step < invulnerable_until_step`.
  std::uint32_t invulnerable_until_step = 0;
  // Le réflexe de survie, tant que `step < veil_until_step` : il le rend
  // invulnérable. `veil_sheathes` : il marche mais ne lance plus de skill ; ce
  // qui est déjà parti arrive.
  // `critical_due` : son premier coup après ce pas sera un critique.
  std::uint32_t veil_until_step = 0;
  bool veil_sheathes = false;
  bool critical_due = false;
  // Les relèves que ses cartes lui ont déjà données (kExtraLives).
  std::int32_t card_lives_granted = 0;
  // LE VOL DE VIE de la seconde en cours, commencée au pas `leech_since_step` :
  // ce qu'il a déjà rendu, sous le plafond du réglage.
  std::uint32_t leech_since_step = 0;
  std::int32_t leeched_hp = 0;
  // GELÉ, tant que `step < frost_until_step` : il marche `frost_slow_pct` %
  // moins vite. Un second gel garde le plus fort et le plus long.
  std::uint32_t frost_until_step = 0;
  std::int32_t frost_slow_pct = 0;
  // En coop : tombé sans vie de réserve. Il ne bouge, ne tire ni ne ramasse
  // plus, et la horde l'ignore. Jamais vrai en solo.
  bool down = false;
};

// LE MÉTIER QUE LE HÉROS MONTRE : celui de sa classe, de son évolution dès la
// première, de sa classe transcendante une fois rené, et monté tant qu'il
// tient la carte de sa monture.
[[nodiscard]] inline std::uint16_t HeroJobOf(const ClassRule& hero_class,
                                             const SurvivorsHero& hero) noexcept {
  const bool reborn = Reborn(hero_class, hero.level) && hero_class.reborn_job_id != 0;
  const bool evolved = hero.evolutions > 0 && hero_class.evolved_job_id != 0;
  bool mounted = false;
  if (hero_class.mount_card_item_id != 0) {
    for (const OwnedCard& card : hero.cards) {
      mounted = mounted || card.card_item_id == hero_class.mount_card_item_id;
    }
  }
  if (mounted) {
    const std::uint16_t ridden =
        reborn ? hero_class.reborn_mounted_job_id : hero_class.mounted_job_id;
    if (ridden != 0) return ridden;
  }
  if (reborn) return hero_class.reborn_job_id;
  return evolved ? hero_class.evolved_job_id : hero_class.job_id;
}

struct SurvivorsFoe {
  std::uint32_t serial = 0;  // numéro de naissance, unique dans la partie
  std::uint16_t foe_id = 0;
  // Son rang dans `SurvivorsRules::foes`, posé à sa naissance : un raccourci que
  // `FoeRuleOf` vérifie. Il ne fixe rien de la partie.
  std::uint16_t rule_rank = 0;
  FieldPoint position;
  FieldPoint previous_position;
  std::int32_t hp = 0;
  std::int32_t max_hp = 0;
  std::int32_t pending_damage = 0;  // frappes en vol déjà promises
  std::int32_t units_per_step = 0;  // 0 : immobile
  std::uint32_t born_step = 0;
  std::uint32_t touch_ready_step = 0;
  std::uint32_t chill_until_step = 0;
  std::uint32_t hurt_step = 0;  // dernier coup reçu, pour l'animation
  // Par héros puis par emplacement de skill : le pas où ce skill peut retoucher
  // ce monstre. Voir `RehitIndex`.
  std::array<std::uint32_t, kMostHeroes * kMostSkillSlots> rehit_ready_step = {};
  std::uint8_t facing = 0;
  std::int32_t armor_pct = 0;  // celle de sa règle, posée à sa naissance
  // Gelé tant que `step < freeze_until_step` : immobile, sans contact, et
  // d'élément de défense Water.
  std::uint32_t freeze_until_step = 0;
  // Aveuglé tant que `step < blind_until_step` : il erre au lieu de
  // poursuivre. Maudit tant que `step < curse_until_step`.
  std::uint32_t blind_until_step = 0;
  std::uint32_t curse_until_step = 0;
  // Un recul en cours : le monstre glisse au cap `knock_heading` tant qu'il
  // lui reste des sous-unités, à la place de sa marche.
  std::uint8_t knock_heading = 0;
  std::int32_t knock_left_units = 0;
  // L'acte de comportement (tir, charge, bond, cachette). 0 : pas encore
  // armé ; SteerFoes l'arme alors à sa phase de départ.
  std::uint32_t act_ready_step = 0;
  // Un acte annoncé joue à ce pas ; 0 : aucun acte annoncé.
  std::uint32_t telegraph_until_step = 0;
  std::uint32_t act_until_step = 0;  // cachette ou charge en cours
  std::uint8_t act_heading = 0;      // le cap d'une charge
  FieldPoint act_target;             // fin de charge, point de chute, point visé
  // Un essaim ou un raid file droit à ce cap, et quitte la partie, sans mort
  // ni gemme, au-delà de `recall_units` du héros.
  std::uint8_t drift_heading = kNoHeading;
  std::uint32_t carried_exp = 0;  // l'XP des gemmes qu'un voleur a prises
  // Vrai quand SteerFoes a mené le pas du monstre : MoveHorde ne le fait pas
  // poursuivre. Reposé à chaque pas.
  bool steered = false;
  // Une élite : PV majorés à sa naissance, contact et XP majorés, points de
  // score multipliés, et un coffre à sa chute.
  bool elite = false;
  // Le hurlement d'un boss : la marche majorée de `boost_pct` pour cent tant
  // que `step < boost_until_step`.
  std::uint32_t boost_until_step = 0;
  std::int32_t boost_pct = 0;
  // Vrai tant qu'un coup de boss (bond, charge, fuite) mène son corps :
  // RunBossScripts le déplace, ni SteerFoes ni la poursuite ne le font.
  bool scripted = false;
  // Empoisonné tant que `step < poison_until_step` (0 : il ne l'est pas) : le
  // prochain tic tombe à `poison_next_step`, et chaque tic est le coup de
  // `poison_skill_id`, porté par le héros `poison_hero`.
  std::uint32_t poison_until_step = 0;
  std::uint32_t poison_next_step = 0;
  std::int32_t poison_tick_damage = 0;
  std::uint16_t poison_skill_id = 0;
  std::uint8_t poison_hero = 0;
  // Le dernier héros qui l'a blessé : sa chute lui est créditée.
  std::uint8_t last_hurt_hero = 0;
};
// La place, dans `rehit_ready_step`, de l'emplacement `slot` du héros
// `hero_index`.
[[nodiscard]] constexpr std::size_t RehitIndex(std::size_t hero_index, std::size_t slot) noexcept {
  return hero_index * kMostSkillSlots + slot;
}
// Aucun coup de boss ne tient le corps du boss.
inline constexpr std::uint8_t kNoBossMove = 0xFF;
// LE SUIVI D'UN BOSS EN VIE : la règle qui l'a fait naître, et le pas où
// chaque coup de son script peut repartir, au même index que ses coups.
struct SurvivorsBossTrack {
  std::uint32_t serial = 0;
  std::uint16_t rule_index = 0;  // dans SurvivorsRules::bosses
  std::array<std::uint32_t, kMostBossMoves> ready_step = {};
  // Ses PV au passage précédent : une baisse dit qu'il a été touché.
  std::int32_t seen_hp = 0;
  // Le coup qui tient son corps, ou kNoBossMove.
  std::uint8_t held_move = kNoBossMove;
  bool escorted = false;  // son escorte est née
};
// Une plante posée au départ : le contact du héros ou l'aire d'un skill la
// casse, et elle laisse son objet sur place.
struct SurvivorsPlant {
  std::uint32_t serial = 0;
  std::uint16_t foe_id = 0;  // classe RO : nom et sprite côté app
  FieldPoint position;
};
// Un objet au sol, ramassé au contact du héros, sans aimant.
struct SurvivorsPickup {
  std::uint32_t serial = 0;
  PickupKind kind = PickupKind::kGold;
  // Identifiant d'objet RO : nom et icône côté app. Pour kCard, la carte.
  std::uint16_t item_id = 0;
  FieldPoint position;
};
// Un tir de monstre : il file droit, et touche le héros.
struct SurvivorsFoeShot {
  FieldPoint position;
  FieldPoint previous_position;
  FieldPoint velocity;  // par pas
  std::int32_t damage = 0;
  std::int32_t radius_units = 0;
  std::uint32_t expires_step = 0;
  std::uint32_t from_serial = 0;
};
enum class HazardKind : std::uint8_t { kQuake, kPuddle };
// Une aire au sol qui blesse le héros : à partir de `armed_step`, jusqu'à
// `until_step` exclu, au plus une fois toutes les `period_steps` (une seule
// fois pour une période nulle).
struct SurvivorsHazard {
  HazardKind kind = HazardKind::kPuddle;
  FieldPoint center;
  std::int32_t radius_units = 0;
  std::int32_t damage = 0;
  std::uint32_t armed_step = 0;
  std::uint32_t until_step = 0;
  std::uint32_t period_steps = 0;
  std::uint32_t next_hurt_step = 0;
  // Le gel qu'elle pose sur le héros qu'elle blesse ; 0 : aucun.
  std::int32_t slow_pct = 0;
  std::uint32_t slow_ms = 0;
};
struct SurvivorsShot {
  std::uint32_t serial = 0;
  std::uint8_t skill_slot = 0;
  std::uint16_t skill_id = 0;
  FieldPoint position;
  FieldPoint previous_position;
  FieldPoint velocity;  // par pas
  std::int32_t damage = 0;
  std::int32_t hit_radius_units = 0;
  std::uint32_t expires_step = 0;
  std::uint8_t pierce_left = 0;
  std::uint32_t target_serial = 0;
  // Les monstres déjà touchés, dans l'ordre des coups : le tir ne les vise ni
  // ne les touche plus.
  std::array<std::uint32_t, kMostShotHits> hit_serials = {};
  std::uint8_t hit_count = 0;
  // Ce que chaque coup inflige en plus des dégâts ; le recul suit le vol.
  std::uint32_t freeze_ms = 0;
  std::int32_t knockback_units = 0;
  std::uint32_t curse_ms = 0;
  // L'éclat de chaque coup : les AUTRES monstres que ce disque, autour du
  // monstre touché, chevauche prennent `splash_pct` % des dégâts. Rayon nul :
  // aucun éclat.
  std::int32_t splash_radius_units = 0;
  std::int32_t splash_pct = 0;
  // Un projectile droit : il garde sa vitesse de départ, sans se tourner vers
  // un monstre.
  bool straight = false;
  std::uint32_t poison_ms = 0;
  std::int32_t poison_tick_damage = 0;
};
struct SurvivorsStrike {
  std::uint32_t foe_serial = 0;
  std::uint32_t land_step = 0;
  std::int32_t base_damage = 0;  // avant élément
  std::int32_t promised = 0;     // part de pending_damage à rendre
  std::uint16_t skill_id = 0;
  SurvivorsElement element = SurvivorsElement::kNeutral;
  std::uint32_t chill_ms = 0;
  std::uint32_t freeze_ms = 0;
  std::int32_t knockback_units = 0;  // dans le sens du héros vers le monstre
  // Les rebonds, joués au pas où la frappe tombe.
  std::uint8_t bounces = 0;
  std::int32_t bounce_pct = 0;
  std::int32_t bounce_reach_units = 0;
  std::uint32_t poison_ms = 0;
  std::int32_t poison_tick_damage = 0;
};
// Un éclat d'aire en chute : celui d'un skill, visé sur un monstre, ou une nova
// posée sur le héros. Aucune promesse : une aire ne se répartit pas entre ses
// monstres.
struct SurvivorsBlast {
  FieldPoint center;
  std::uint32_t land_step = 0;
  std::int32_t damage = 0;           // avant élément, au centre
  std::int32_t radius_units = 0;     // déjà majoré de l'aire du héros
  std::int32_t splash_pct = 0;       // hors du centre
  std::int32_t knockback_units = 0;  // en s'écartant du centre
  std::uint32_t freeze_ms = 0;
  std::uint32_t center_serial = 0;  // le monstre visé ; 0 : aucun centre
  std::uint16_t skill_id = 0;
  SurvivorsElement element = SurvivorsElement::kNeutral;
};
// Une zone posée au sol : un mur (segment de `from` à `to`) ou un pilier
// (`from` == `to`).
struct SurvivorsZone {
  std::uint32_t serial = 0;
  std::uint8_t skill_slot = 0;
  std::uint16_t skill_id = 0;
  SkillArchetype archetype = SkillArchetype::kWall;
  FieldPoint from;
  FieldPoint to;
  std::int32_t radius_units = 0;  // portée de touche, autour du segment ou du point
  std::int32_t damage = 0;        // avant élément
  std::int32_t knockback_units = 0;
  std::uint32_t freeze_ms = 0;
  std::uint32_t expires_step = 0;
  // Mur : un même monstre n'est retouché qu'après ces pas ; pilier : il se
  // réarme après eux.
  std::uint32_t rehit_steps = 0;
  std::uint32_t armed_step = 0;  // pilier : premier pas où il peut se déclencher
  // Les coups qui restent à une zone `limited` ; une zone sans limite ne
  // s'éteint qu'à `expires_step`.
  std::uint8_t hits_left = 0;
  bool limited = false;
  std::uint8_t push_heading = 0;  // le cap d'un recul
  SurvivorsElement element = SurvivorsElement::kNeutral;
  // Orage : les vagues déjà tombées, et celle à partir de laquelle elles
  // gèlent (0 : toutes) ; l'aveuglement et la malédiction de chaque coup.
  std::uint8_t waves_done = 0;
  std::uint8_t freeze_from_wave = 0;
  std::uint32_t blind_ms = 0;
  std::uint32_t curse_ms = 0;
  // Barrière : les PV qui restent au bloc ; à zéro, il éclate.
  std::int32_t block_hp = 0;
  // Orage : le poison de chaque coup.
  std::uint32_t poison_ms = 0;
  std::int32_t poison_tick_damage = 0;
};
// UN ALLIÉ INVOQUÉ par un skill du héros (kSummon) : la horde ne le voit ni ne
// le touche, et il traverse le décor. Ses coups se lisent au niveau courant du
// skill de son emplacement ; il disparaît quand ce skill n'y est plus.
struct SurvivorsSummon {
  std::uint32_t serial = 0;
  std::uint8_t skill_slot = 0;
  std::uint16_t skill_id = 0;
  std::uint16_t summon_class = 0;  // classe RO : nom et sprite côté app
  FieldPoint position;
  FieldPoint previous_position;
  std::uint8_t facing = 0;
  // Il disparaît au pas `expires_step` ; 0 : jamais.
  std::uint32_t expires_step = 0;
  // Son prochain coup ou soin, puis son prochain échange de place.
  std::uint32_t ready_step = 0;
  std::uint32_t swap_ready_step = 0;
};
struct SurvivorsGem {
  FieldPoint position;
  FieldPoint previous_position;
  std::uint32_t exp = 0;
  std::int32_t pull_units = 0;  // 0 tant que l'aimant ne l'a pas saisie
  std::uint8_t pull_hero = 0;   // le héros qui l'a saisie, le seul qu'elle suit
};
// Le bilan d'une partie : ce que le score lit, et ce que le rejeu compare.
struct SurvivorsTally {
  std::uint32_t survived_steps = 0;
  std::uint32_t kills = 0;
  std::uint16_t boss_kills = 0;
  std::uint32_t kill_points = 0;
  std::uint16_t level = 1;
  std::uint32_t exp_total = 0;
  std::int32_t hp_left = 0;
  std::uint64_t damage_dealt = 0;
  std::uint32_t gems_gathered = 0;
  std::uint16_t foes_alive = 0;
  SurvivorsOutcome outcome = SurvivorsOutcome::kRunning;
  std::uint16_t elite_kills = 0;
  std::uint16_t chests_opened = 0;
  std::uint16_t evolutions = 0;
  [[nodiscard]] bool operator==(const SurvivorsTally&) const = default;
};
// Une valeur cumulée par identifiant.
struct SurvivorsCount {
  std::uint16_t id = 0;
  std::uint64_t value = 0;
  [[nodiscard]] bool operator==(const SurvivorsCount&) const = default;
};
// LE RÉCAPITULATIF D'UNE PARTIE, pour l'écran de fin : les tués par espèce
// (`foe_id`) et les dégâts par skill (`skill_id`), chacun dans l'ordre de
// première apparition. Il s'écrit aux seuls endroits qui écrivent
// `tally.kills` et `tally.damage_dealt`, et ses sommes leur sont égales. Hors
// du bilan : le rejeu ne le compare pas, et la version des règles n'en dépend
// pas.
// Il porte aussi ce que la méta relève : les cartes ramassées
// (`card_item_id`, dans l'ordre de ramassage, une entrée par carte) et les
// glaçages (un monstre vivant qui passe de non ralenti à ralenti).
struct SurvivorsRecap {
  std::vector<SurvivorsCount> kills_by_foe;
  std::vector<SurvivorsCount> damage_by_skill;
  std::vector<std::uint16_t> cards_found;
  std::uint64_t chills = 0;
  // Les monstres vivants passés de non gelés à gelés : un gel qui en prolonge
  // un autre ne recompte pas.
  std::uint64_t freezes = 0;
  [[nodiscard]] bool operator==(const SurvivorsRecap&) const = default;
};
// Ajoute `amount` à l'entrée `id`, posée en fin de liste à sa première
// apparition : quelques dizaines d'identifiants au plus, lus en ligne.
inline void CountInRecap(std::vector<SurvivorsCount>& counts, std::uint16_t id,
                         std::uint64_t amount) {
  for (SurvivorsCount& count : counts) {
    if (count.id == id) {
      count.value += amount;
      return;
    }
  }
  counts.push_back(SurvivorsCount{.id = id, .value = amount});
}

}  // namespace ro::sim
