#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ro/render/effect_sound_table.h"
#include "ro/render/sprite_catalog.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_state.h"

#include "survivors/survivors_gems.h"

// LA TABLE D'APPARENCE DES ACTEURS DU MODE SURVIVORS : ce que la simulation
// nomme par des identifiants (espèce, objet, classe) devient une apparence que
// la garde-robe habille. La simulation n'en sait rien ; `app` seul la lit.
namespace moonclient {

// Un monstre ou une plante : un sprite unique, nommé par sa classe.
[[nodiscard]] ro::render::ActorLook FoeLook(std::uint16_t foe_id) noexcept;

// LE TIR D'UN MONSTRE QUE LE CLIENT NE MUNIT PAS : l'effet qui vole de lui au
// point visé. Le Gazeti lance l'éclat de glace de Frost Diver ; rien pour les
// autres, que leur flèche dessine.
inline constexpr std::uint16_t kGazetiFoe = 1778;
inline constexpr std::uint32_t kFrostDiverBoltEffect = 27;
[[nodiscard]] std::optional<std::uint32_t> FoeShotEffectOf(std::uint16_t foe_id) noexcept;

// UN MONSTRE QUI NE SE MONTRE QU'EN ATTAQUANT : l'Iceicle n'a qu'une ombre au
// repos, et son glaçon ne paraît que dans sa chute d'attaque. Il se tient sur
// l'image `standing_frame` de son attaque, planté au sol, et meurt en rejouant
// l'attaque depuis `burst_frame`, où il éclate.
struct SurvivorsStillPose {
  std::size_t standing_frame = 0;
  std::size_t burst_frame = 0;
};
inline constexpr std::uint16_t kIceicleFoe = 1789;
[[nodiscard]] std::optional<SurvivorsStillPose> StillPoseOf(std::uint16_t foe_id) noexcept;

// Un objet au sol, gemme comprise : le sprite que la table des objets donne.
[[nodiscard]] ro::render::ActorLook ItemLook(std::uint16_t item_id) noexcept;

// LA CARTE AU SOL (Q6) : le sprite RO d'une carte posée, le même pour toutes.
// La Poring Card le porte comme les cartes de Gravity ; une carte dont la
// fiche nomme une autre ressource (la Mastering Card du serveur) ne se
// dessine pas autrement.
inline constexpr std::uint16_t kGroundCardItem = 4001;

// L'objet qui montre un objet au sol : le sien, la carte au sol pour une carte.
[[nodiscard]] std::uint16_t PickupLookOf(const ro::sim::SurvivorsPickup& pickup) noexcept;

// CE QUE LE HÉROS MONTRE de son état : le corps de son métier
// (`ro::sim::HeroJobOf` : rené, monté), et s'il a évolué, ce qui change son arme.
struct HeroBody {
  std::uint16_t job = 0;  // nul : le métier de la classe
  bool evolved = false;
  [[nodiscard]] bool operator==(const HeroBody&) const = default;
};
[[nodiscard]] HeroBody HeroBodyOf(const ro::sim::ClassRule& hero_class,
                                  const ro::sim::SurvivorsHero& hero) noexcept;

// LE HÉROS (U7) : la tête, la coiffure, les coiffes, la cape et les couleurs
// du personnage (`player`), sur le corps `body.job`, avec l'arme du départ, ou
// celle de l'évolution une fois évolué quand la classe en a une.
//
// ⚠ CE QUI CHANGERAIT LE CORPS EST RETIRÉ : costume, monture, habit de GM,
// combinaison, tête d'orc et bouclier sont ceux du personnage, pas du mode.
[[nodiscard]] ro::render::ActorLook HeroLook(const ro::render::ActorLook& player,
                                             const ro::sim::ClassRule& hero_class,
                                             HeroBody body) noexcept;

// LE CORPS DE LA RENCONTRE : le métier et l'arme du départ de la classe, sous une
// tête ordinaire, sans rien du personnage du joueur.
[[nodiscard]] ro::render::ActorLook MeetingBodyLook(const ro::sim::ClassRule& hero_class) noexcept;

// LE RANG D'ANIMATION DU GESTE D'ATTAQUE d'un héros d'apparence `look`.
//
// Une classe qui change d'arme avec son corps suit le jeu (`AttackAnimation`) :
// un katar n'a d'images qu'à son rang, et un geste au rang ordinaire le rendrait
// invisible. Toute autre classe garde le rang ordinaire d'un joueur.
[[nodiscard]] std::size_t HeroAttackAnimation(const ro::render::SpriteCatalog& catalog,
                                              const ro::sim::ClassRule& hero_class,
                                              const ro::render::ActorLook& look);

// ── LA TABLE D'APPARENCE DES SKILLS ─────────────────────────────────────────

// LES TROIS REPLIS ÉCRITS : ces skills ne montrent rien par les tables du
// client, parce que leur effet y naît d'un état (Sight), d'une suite sans
// dégâts (Napalm Beat) ou d'une unité posée (Fire Wall, l'unité 127).
inline constexpr std::uint16_t kSightSkill = 10;
inline constexpr std::uint16_t kNapalmBeatSkill = 11;
inline constexpr std::uint16_t kFireWallSkill = 18;
inline constexpr std::uint32_t kSightFallbackEffect = 22;
inline constexpr std::uint32_t kNapalmBeatFallbackEffect = 32;
inline constexpr std::uint32_t kFireWallFallbackEffect = 25;

// L'EFFET EMPRUNTÉ : Napalm Vulcan naît, dans le jeu, sur sa cible, que la horde
// du mode n'est pas ; ses esprits qui poursuivent se montrent par l'effet de
// Soul Strike, porté par le héros. Son icône reste la sienne.
inline constexpr std::uint16_t kNapalmVulcanSkill = 400;
inline constexpr std::uint16_t kSoulStrikeSkill = 13;
// De même, Brandish Spear et Spiral Pierce naissent sur leur cible : le
// balayage se montre par l'effet de Spear Stab, les lances par celui de Pierce,
// tous deux portés par le héros.
inline constexpr std::uint16_t kBrandishSpearSkill = 57;
inline constexpr std::uint16_t kSpearStabSkill = 58;
inline constexpr std::uint16_t kSpiralPierceSkill = 397;
inline constexpr std::uint16_t kPierceSkill = 56;
// Frenzy ne fait que teinter le corps dans le jeu, et le mode ne teint pas son
// héros : il se montre par l'éclat de Giant Growth, dont le fichier est celui de
// la furie. Sans bruit à lui ni à cet éclat, il sonne comme Two-Hand Quicken,
// le skill dont il évolue.
inline constexpr std::uint16_t kFrenzySkill = 359;
inline constexpr std::uint16_t kGiantGrowthSkill = 2014;
inline constexpr std::uint16_t kTwoHandQuickenSkill = 60;

// LE VOLEUR. Double Attack n'a dans le jeu que l'étincelle de ses coups, comme
// Bash : le mode la pose sur le monstre frappé.
inline constexpr std::uint16_t kDoubleAttackSkill = 48;
inline constexpr std::uint32_t kDoubleAttackFallbackEffect = 1;
// Sprinkle Sand et Enchant Poison naissent dans le jeu sur leur cible, que la
// horde n'est pas : leur effet reste le leur, écrit ici et porté par le héros.
inline constexpr std::uint16_t kSprinkleSandSkill = 149;
inline constexpr std::uint32_t kSprinkleSandFallbackEffect = 310;
inline constexpr std::uint16_t kEnchantPoisonSkill = 138;
inline constexpr std::uint32_t kEnchantPoisonFallbackEffect = 125;
// Throw Stone et Throw Venom Knife : l'objet qui vole du lanceur à sa cible,
// effet du jeu qu'aucune table de compétence ne nomme.
inline constexpr std::uint16_t kThrowStoneSkill = 152;
inline constexpr std::uint32_t kThrownStoneEffect = 308;
inline constexpr std::uint16_t kThrowVenomKnifeSkill = 1004;
inline constexpr std::uint32_t kThrownVenomKnifeEffect = 600;
// Back Slide n'a pas d'effet dans le jeu : le recul se montre par l'élan de
// Bowling Bash, qui repousse comme lui.
inline constexpr std::uint16_t kBackSlideSkill = 150;
inline constexpr std::uint16_t kBowlingBashSkill = 62;
// Improve Dodge n'en a pas non plus : sa marche se montre par Wind Walk.
inline constexpr std::uint16_t kImproveDodgeSkill = 49;
inline constexpr std::uint16_t kWindWalkSkill = 383;
// Cloaking se montre par Hiding, le skill dont il évolue.
inline constexpr std::uint16_t kCloakingSkill = 135;
inline constexpr std::uint16_t kHidingSkill = 51;
// Sonic Blow : l'effet de son lancer n'a pas de bruit à lui, et le repli des
// tables en donnerait un qui n'est pas le sien. Il sonne comme l'effet de ses
// coups.
inline constexpr std::uint16_t kSonicBlowSkill = 136;
inline constexpr std::uint32_t kSonicBlowSoundingEffect = 122;

// L'ARCHER. Ses tirs droits se voient par la flèche de l'arc, qui vole du héros
// à sa cible (`SkillLook::shoots_arrow`). Ceux de base n'ont pas d'autre effet,
// et l'étincelle du coup n'est que leur repli écrit ; Sharp Shooting garde le
// sien, que les tables lui donnent, et Beast Strafing et Phantasmic Arrow le
// lui empruntent.
inline constexpr std::uint16_t kDoubleStrafeSkill = 46;
inline constexpr std::uint16_t kChargeArrowSkill = 148;
inline constexpr std::uint16_t kBeastStrafingSkill = 499;
inline constexpr std::uint16_t kSharpShootingSkill = 382;
inline constexpr std::uint16_t kPhantasmicArrowSkill = 1009;
inline constexpr std::uint32_t kArrowFallbackEffect = 1;
// L'arc que la flèche suppose : celui de l'Archer.
inline constexpr std::uint16_t kArrowShooterJob = 3;
inline constexpr std::uint16_t kArrowShooterWeapon = 1701;
// LE COUP DE BAGUETTE du Magicien tire le trait que le jeu donne aux bâtons
// (`ro::render::ShootsWandBolts`), et non une flèche.
inline constexpr std::uint16_t kRodShotSkill = 60001;
inline constexpr std::uint16_t kWandShooterJob = 2;
inline constexpr std::uint16_t kWandShooterWeapon = 1601;
// LE BRUIT D'UNE FLÈCHE QUI TOUCHE : celui que le jeu donne à l'arc, à l'impact
// et non au lâcher.
inline constexpr std::string_view kArrowHitSound = "_hit_arrow.wav";
// Arrow Shower (kArrowShowerSkill) : la pluie de flèches du jeu, au sol.
inline constexpr std::uint32_t kArrowShowerEffect = 64;
// Blitz Beat et Falcon Assault : le piqué du faucon, qui vole jusqu'où il
// frappe.
inline constexpr std::uint16_t kBlitzBeatSkill = 129;
inline constexpr std::uint16_t kFalconAssaultSkill = 381;
inline constexpr std::uint32_t kFalconDiveEffect = 115;
// LES PIÈGES : une marque au sol tant qu'ils attendent (`SkillLook::trap`), et
// l'explosion de leur amorce sur chaque monstre qu'ils frappent. Ankle Snare et
// Skid Trap n'ont pas d'amorce : l'étoile de coup ordinaire.
inline constexpr std::uint16_t kSkidTrapSkill = 115;
inline constexpr std::uint16_t kLandMineSkill = 116;
inline constexpr std::uint16_t kAnkleSnareSkill = 117;
inline constexpr std::uint16_t kFreezingTrapSkill = 121;
inline constexpr std::uint16_t kBlastMineSkill = 122;
inline constexpr std::uint16_t kClaymoreTrapSkill = 123;
inline constexpr std::uint32_t kLandMinePrimerEffect = 97;
inline constexpr std::uint32_t kBlastMinePrimerEffect = 106;
inline constexpr std::uint32_t kClaymorePrimerEffect = 107;
inline constexpr std::uint32_t kFreezingTrapPrimerEffect = 108;

// L'UNITÉ AU SOL du jeu qu'un piège armé pose, et dont le modèle se tient au
// milieu de sa zone ; rien pour un piège qui n'en a pas.
[[nodiscard]] constexpr std::optional<std::uint32_t> TrapUnitOf(std::uint16_t skill_id) noexcept {
  constexpr std::uint32_t kBlastMineUnit = 143;
  constexpr std::uint32_t kSkidTrapUnit = 144;
  constexpr std::uint32_t kAnkleSnareUnit = 145;
  constexpr std::uint32_t kLandMineUnit = 147;
  constexpr std::uint32_t kFreezingTrapUnit = 151;
  constexpr std::uint32_t kClaymoreTrapUnit = 152;
  switch (skill_id) {
    case kBlastMineSkill:
      return kBlastMineUnit;
    case kSkidTrapSkill:
      return kSkidTrapUnit;
    case kAnkleSnareSkill:
      return kAnkleSnareUnit;
    case kLandMineSkill:
      return kLandMineUnit;
    case kFreezingTrapSkill:
      return kFreezingTrapUnit;
    case kClaymoreTrapSkill:
      return kClaymoreTrapUnit;
    default:
      return std::nullopt;
  }
}

// UN GEL QUI N'EN EST PAS UN : Ankle Snare retient le monstre, et la mine le
// secoue un instant. La simulation les compte comme un gel ; le monstre ne
// bleuit pas pour autant.
[[nodiscard]] constexpr bool FreezeHolds(std::uint16_t skill_id) noexcept {
  return skill_id == kAnkleSnareSkill || skill_id == kLandMineSkill || skill_id == kBlastMineSkill;
}

// LE MARCHAND. Ses skills n'ont presque rien aux tables du client : ils
// empruntent l'effet d'un skill voisin, ou se replient sur l'étincelle du coup.
// La Marine Sphere est un piège, et saute comme la mine.
inline constexpr std::uint16_t kMammoniteSkill = 42;
inline constexpr std::uint16_t kDemonstrationSkill = 229;
inline constexpr std::uint16_t kAcidTerrorSkill = 230;
inline constexpr std::uint16_t kPotionPitcherSkill = 231;
inline constexpr std::uint16_t kBioCannibalizeSkill = 232;
inline constexpr std::uint16_t kSphereMineSkill = 233;
// La classe du monstre Marine Sphere, que Sphere Mine pose sur sa mine.
inline constexpr std::uint16_t kMarineSphereClass = 1142;
inline constexpr std::uint16_t kChemicalProtectionArmorSkill = 236;
inline constexpr std::uint16_t kSlimPotionPitcherSkill = 478;
inline constexpr std::uint16_t kFullChemicalProtectionSkill = 479;
inline constexpr std::uint16_t kAcidDemonstrationSkill = 490;
inline constexpr std::uint16_t kPlantCultivationSkill = 491;
inline constexpr std::uint16_t kCartCannonSkill = 2477;
// Les deux orbites dont l'orbe est le chariot.
inline constexpr std::uint16_t kCartRevolutionSkill = 153;
inline constexpr std::uint16_t kCartTornadoSkill = 2476;
inline constexpr std::uint16_t kTouchOfHealSkill = 8001;
inline constexpr std::uint16_t kMentalChangeSkill = 8004;
inline constexpr std::uint16_t kCastlingSkill = 8005;
inline constexpr std::uint16_t kMoonlightSkill = 8009;
inline constexpr std::uint16_t kSbr44Skill = 8012;
inline constexpr std::uint16_t kCapriceSkill = 8013;
inline constexpr std::uint16_t kBioExplosionSkill = 8016;
// Les skills dont ils empruntent l'effet : le soin, la boule de feu, l'éclair
// de feu, Earth Spike et Sonic Blow. Les protections chimiques gardent le
// leur, que les tables donnent à l'annonce.
inline constexpr std::uint16_t kHealBorrowed = 28;
inline constexpr std::uint16_t kFireBallBorrowed = 17;
inline constexpr std::uint16_t kFireBoltBorrowed = 19;
inline constexpr std::uint16_t kEarthSpikeBorrowed = 90;
// La bombe de Demonstration brûle de son propre feu ; la fiole d'Acid Terror
// vole comme la pierre de Throw Stone.
inline constexpr std::uint32_t kDemonstrationFallbackEffect = 302;
// Acid Demonstration : les anneaux d'acide que le jeu fait naître sur la cible,
// hors des tables que lit le mode.
inline constexpr std::uint32_t kAcidDemonstrationEffect = 537;
inline constexpr std::uint32_t kStrikeSparkFallbackEffect = 1;

// MAGNUM BREAK a deux effets : ses anneaux, à l'annonce, et l'anneau de choc de
// chaque victime, au coup. Le lancer du mode montre les anneaux.
inline constexpr std::uint16_t kMagnumBreakSkill = 7;

// Bash n'a pas d'effet de lancer dans le jeu, seulement l'étincelle de son
// coup : le mode la pose sur le monstre frappé.
inline constexpr std::uint16_t kBashSkill = 5;
inline constexpr std::uint32_t kBashFallbackEffect = 1;

// L'UNITÉ QU'UN SKILL DE ZONE POSE AU SOL dans le jeu : la zone du mode en
// montre le visuel entretenu (`SkillLook::ground_effect_id`) tant qu'elle dure.
// Fire Pillar pose le pilier en attente, dont le cercle rouge marque la case.
inline constexpr std::uint16_t kFirePillarSkill = 80;
inline constexpr std::uint32_t kFirePillarWaitingUnit = 135;
// Ice Wall : chaque bloc est l'unité du mur de glace du jeu, un bloc permanent
// tant que sa zone dure.
inline constexpr std::uint16_t kIceWallSkill = 87;
inline constexpr std::uint32_t kIceWallUnit = 141;
// Son lancer n'a pas d'effet au tableau : il se montre par le bloc lui-même.
inline constexpr std::uint32_t kIceWallFallbackEffect = 74;
// Venom Dust : le nuage de poison de l'unité du jeu, tant que sa zone dure.
inline constexpr std::uint16_t kVenomDustSkill = 140;
inline constexpr std::uint32_t kVenomDustUnit = 146;
inline constexpr std::uint32_t kVenomDustFallbackEffect = 171;
// Demonstration : le feu de l'unité du jeu, tant que sa zone dure ; son lancer
// est ce même feu.
inline constexpr std::uint32_t kDemonstrationUnit = 177;

// HEAVEN'S DRIVE pose un orage sans unité au sol : son champ de pointes se joue
// à sa naissance, au point de l'orage, pour que la horde voie où il blesse.
inline constexpr std::uint16_t kHeavensDriveSkill = 91;

// L'EFFET D'UNE ÉVOLUTION : le changement de métier, sur le héros.
inline constexpr std::uint32_t kJobChangeEffect = 156;

// L'ÉCLAT D'UNE CARTE RAMASSÉE (Q6) : l'étincelle du soin, brève, sur le
// héros qui vient de la prendre.
inline constexpr std::uint32_t kCardPickupEffect = 309;

// L'ÉTOILE DE COUP : la gerbe ordinaire, que la table des impacts donne par
// défaut, et la seule que « Effets réduits » garde.
inline constexpr std::uint32_t kPlainHitEffect = 0;

// OÙ L'EFFET D'UN SKILL NAÎT.
//
// 🔴 JAMAIS SUR UN MONSTRE : la horde n'entre pas dans le monde des effets. Ce
// que le client fait naître sur la cible se pose au sol, là où elle se tient.
enum class SkillBearer : std::uint8_t {
  // Porté par le héros, qu'il suit.
  kHero,
  // Posé au sol, au point que la nouvelle nomme.
  kGround,
};

struct SkillLook {
  std::uint16_t skill_id = 0;
  // Le skill dont les tables donnent l'effet : le sien, sauf un effet emprunté.
  std::uint16_t effect_skill_id = 0;
  // L'effet du sort, toujours connu pour un skill des règles livrées.
  std::uint32_t effect_id = 0;
  SkillBearer bearer = SkillBearer::kGround;
  // L'effet vient d'un des trois replis écrits, et non des tables.
  bool fallback = false;
  // L'IMPACT sur un monstre touché : la gerbe ordinaire quand la table n'en
  // nomme pas d'autre. « Effets réduits » le remplace par l'étoile de coup.
  std::uint32_t hit_effect_id = 0;
  // CE QUE SA ZONE MONTRE AU SOL tant qu'elle dure, sous les flammes : le
  // visuel entretenu de l'unité que le skill pose dans le jeu, ou zéro.
  std::uint32_t ground_effect_id = 0;
  // Son orage joue l'effet du sort à sa naissance, au point où il se pose.
  bool storm_sown = false;
  // Le skill dont l'icône se montre : le sien.
  std::uint16_t icon_skill_id = 0;
  // Ses projectiles se dessinent en flèches d'arc, et un lancer sans effet de
  // table ne joue rien de plus.
  bool shoots_arrow = false;
  // Ces projectiles sont le trait d'une baguette, et non la flèche d'un arc.
  bool shoots_wand_bolt = false;
  // Sa zone est un piège : une marque au sol, sans flamme.
  bool trap = false;
  // SON ORBE EST LE CHARIOT du héros, qui tourne : il ne porte aucun effet, et
  // l'effet du sort part une fois sur le héros, au début du tour.
  bool cart_spin = false;
};

// L'ARME QUE LE COUP DE L'ARME DU HÉROS MONTRE : l'objet de son niveau, celui
// que le héros tient ; 0 pour un skill qui n'est pas un coup de l'arme.
[[nodiscard]] std::uint16_t WeaponItemShown(const ro::sim::SurvivorsSkillRule& skill,
                                            const ro::sim::SurvivorsHero& hero) noexcept;

// L'apparence d'un skill des règles : l'objet que son lancer fait voler, sinon
// le repli écrit d'un skill que le jeu fait naître sur sa cible, sinon l'effet du
// coup, celui de la table ordinaire, celui de l'annonce, et le repli écrit de ce
// que les tables ne nomment pas. Où il naît se lit
// sur l'archétype : une orbite, un projectile qui part du héros et une nova
// sont portés par lui, tout le reste se pose au sol. L'effet au sol d'une zone
// est celui que la table des unités entretient pour l'unité du skill.
//
// LE COUP DE L'ARME n'a pas de compétence au jeu : la bille de baguette se
// montre par l'effet de Soul Strike, la flèche par la flèche d'arc, le balayage
// de lame par l'effet de Spear Stab, et une frappe par l'étincelle du coup.
[[nodiscard]] SkillLook SkillLookOf(const ro::sim::SurvivorsSkillRule& skill);

// LES FICHIERS QU'UN LANCER FAIT ENTENDRE, variantes tirées de `draw` : ceux de
// la compétence, sinon ceux de son effet. Rien pour un sort qui n'en a aucun :
// il sonne à l'impact (`SkillThudsOnHit`).
//
// 🔴 UN NOM À VARIANTES SE FORME ICI : le `%d` qu'il porte à la place du numéro
// ne désigne aucun fichier, et un bolt joué sous ce nom resterait muet.
//
// ⚠ RIEN POUR UN EFFET DONT LA TROUPE SONNE : chaque pièce semée fait son bruit
// (`ArenaEffects::TakeSounds`), et en ajouter un au lancer serait un doublon —
// voir `ro::render::EffectSoundPlaysAtSowing`.
[[nodiscard]] std::vector<std::string> SkillSoundFilesOf(const SkillLook& look, std::uint32_t draw);

// Vrai pour un sort sans bruit, ni à lui, ni à son effet, ni à la troupe de son
// effet : chaque coup qu'il porte fait entendre le coup sourd du jeu à
// l'impact d'une compétence (`ro::render::SkillBlowSound`), et un lancer qui
// ne touche rien reste muet.
[[nodiscard]] bool SkillThudsOnHit(const SkillLook& look);

// LE BRUIT QU'UN COUP DE CE SORT FAIT À L'IMPACT : la flèche d'arc pour un sort
// qui tire des flèches, le coup sourd pour un sort sans bruit
// (`SkillThudsOnHit`), rien sinon. `draw` tire la variante du coup sourd.
[[nodiscard]] std::string SkillHitSoundOf(const SkillLook& look, std::uint32_t draw);

}  // namespace moonclient
