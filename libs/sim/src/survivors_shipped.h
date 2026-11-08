#pragma once

// LES TABLES LIVRÉES DU MODE SURVIVORS, une par fonction. Chacune rend un span
// sur un tableau constexpr de portée de fichier : son initialisation est
// constante, donc sans ordre d'initialisation entre unités de traduction.
// ShippedSurvivorsRules les assemble.
//
// En-tête PRIVÉ de `libs/sim/src`.

#include <cstddef>
#include <cstdint>
#include <span>

#include "ro/sim/survivors_rules.h"

namespace ro::sim::survivors {

// ── Les Id que plusieurs fichiers du catalogue nomment ─────────────────────
// Compétence RO pour un skill, objet RO pour une carte ou un coffre, classe RO
// (monster_list) pour un monstre.

inline constexpr std::uint16_t kFireBoltId = 19;
inline constexpr std::uint16_t kSightId = 10;
inline constexpr std::uint16_t kFireWallId = 18;
inline constexpr std::uint16_t kFrostDiverId = 15;
inline constexpr std::uint16_t kColdBoltId = 14;
inline constexpr std::uint16_t kLightningBoltId = 20;
inline constexpr std::uint16_t kSoulStrikeId = 13;
inline constexpr std::uint16_t kNapalmBeatId = 11;
inline constexpr std::uint16_t kEarthSpikeId = 90;
inline constexpr std::uint16_t kThunderstormId = 21;
inline constexpr std::uint16_t kFireBallId = 17;
inline constexpr std::uint16_t kWaterBallId = 86;
inline constexpr std::uint16_t kIceWallId = 87;
// Les évolués.
inline constexpr std::uint16_t kMeteorStormId = 83;
inline constexpr std::uint16_t kSightrasherId = 81;
inline constexpr std::uint16_t kFirePillarId = 80;
inline constexpr std::uint16_t kFrostNovaId = 88;
inline constexpr std::uint16_t kStormGustId = 89;
inline constexpr std::uint16_t kJupitelThunderId = 84;
inline constexpr std::uint16_t kLordOfVermilionId = 85;
inline constexpr std::uint16_t kHeavensDriveId = 91;
inline constexpr std::uint16_t kNapalmVulcanId = 400;
inline constexpr std::uint16_t kCrimsonRockId = 2211;
inline constexpr std::uint16_t kJackFrostId = 2204;
// Le Chevalier, puis ses évolués.
inline constexpr std::uint16_t kBashId = 5;
inline constexpr std::uint16_t kMagnumBreakId = 7;
inline constexpr std::uint16_t kPierceId = 56;
inline constexpr std::uint16_t kBrandishSpearId = 57;
inline constexpr std::uint16_t kSpearStabId = 58;
inline constexpr std::uint16_t kSpearBoomerangId = 59;
inline constexpr std::uint16_t kBowlingBashId = 62;
inline constexpr std::uint16_t kEndureId = 8;
inline constexpr std::uint16_t kTwoHandQuickenId = 60;
inline constexpr std::uint16_t kSpiralPierceId = 397;
inline constexpr std::uint16_t kHeadCrushId = 398;
inline constexpr std::uint16_t kJointBeatId = 399;
inline constexpr std::uint16_t kAuraBladeId = 355;
inline constexpr std::uint16_t kConcentrationId = 357;
inline constexpr std::uint16_t kFrenzyId = 359;
inline constexpr std::uint16_t kIgnitionBreakId = 2006;
// Le Voleur, puis ses évolués.
inline constexpr std::uint16_t kDoubleAttackId = 48;
inline constexpr std::uint16_t kEnvenomId = 52;
inline constexpr std::uint16_t kThrowStoneId = 152;
inline constexpr std::uint16_t kSprinkleSandId = 149;
inline constexpr std::uint16_t kBackSlideId = 150;
inline constexpr std::uint16_t kGrimtoothId = 137;
inline constexpr std::uint16_t kVenomDustId = 140;
inline constexpr std::uint16_t kEnchantPoisonId = 138;
inline constexpr std::uint16_t kImproveDodgeId = 49;
inline constexpr std::uint16_t kHidingId = 51;
inline constexpr std::uint16_t kSonicBlowId = 136;
inline constexpr std::uint16_t kSoulDestroyerId = 379;
inline constexpr std::uint16_t kVenomSplasherId = 141;
inline constexpr std::uint16_t kEnchantDeadlyPoisonId = 378;
inline constexpr std::uint16_t kCloakingId = 135;
inline constexpr std::uint16_t kThrowVenomKnifeId = 1004;
inline constexpr std::uint16_t kMeteorAssaultId = 406;
// L'Archer, puis ses évolués.
inline constexpr std::uint16_t kDoubleStrafeId = 46;
inline constexpr std::uint16_t kArrowShowerId = 47;
inline constexpr std::uint16_t kChargeArrowId = 148;
inline constexpr std::uint16_t kImproveConcentrationId = 45;
inline constexpr std::uint16_t kBlitzBeatId = 129;
inline constexpr std::uint16_t kAnkleSnareId = 117;
inline constexpr std::uint16_t kLandMineId = 116;
inline constexpr std::uint16_t kSkidTrapId = 115;
inline constexpr std::uint16_t kFreezingTrapId = 121;
inline constexpr std::uint16_t kBeastStrafingId = 499;
inline constexpr std::uint16_t kSharpShootingId = 382;
inline constexpr std::uint16_t kFalconAssaultId = 381;
inline constexpr std::uint16_t kTrueSightId = 380;
inline constexpr std::uint16_t kClaymoreTrapId = 123;
inline constexpr std::uint16_t kBlastMineId = 122;
inline constexpr std::uint16_t kPhantasmicArrowId = 1009;

inline constexpr std::uint16_t kHornetCardId = 4019;
inline constexpr std::uint16_t kChonchonCardId = 4009;
inline constexpr std::uint16_t kGolemCardId = 4072;
inline constexpr std::uint16_t kMasteringCardId = 4197;
inline constexpr std::uint16_t kPupaCardId = 4003;
inline constexpr std::uint16_t kPoringCardId = 4001;
inline constexpr std::uint16_t kSporeCardId = 4022;
inline constexpr std::uint16_t kThiefBugCardId = 4016;
inline constexpr std::uint16_t kEclipseCardId = 4266;
inline constexpr std::uint16_t kDragonFlyCardId = 4179;
inline constexpr std::uint16_t kVagabondWolfCardId = 4183;
inline constexpr std::uint16_t kBaphometCardId = 4147;
inline constexpr std::uint16_t kPecoPecoCardId = 4031;
inline constexpr std::uint16_t kSmokieCardId = 4044;
inline constexpr std::uint16_t kWhisperCardId = 4102;
inline constexpr std::uint16_t kArcherSkeletonCardId = 4094;
inline constexpr std::uint16_t kHunterFlyCardId = 4115;
inline constexpr std::uint16_t kPhreeoniCardId = 4121;
inline constexpr std::uint16_t kWillowCardId = 4010;
inline constexpr std::uint16_t kRockerCardId = 4021;
inline constexpr std::uint16_t kYoyoCardId = 4051;

inline constexpr std::uint16_t kLunaticId = 1063;
inline constexpr std::uint16_t kPoringId = 1002;
inline constexpr std::uint16_t kChonchonId = 1011;
inline constexpr std::uint16_t kPupaId = 1008;
inline constexpr std::uint16_t kHornetId = 1004;
inline constexpr std::uint16_t kFamiliarId = 1005;
inline constexpr std::uint16_t kThiefBugId = 1051;
inline constexpr std::uint16_t kSporeId = 1014;
inline constexpr std::uint16_t kWolfId = 1013;
inline constexpr std::uint16_t kGoblinId = 1122;
inline constexpr std::uint16_t kGhoulId = 1036;
inline constexpr std::uint16_t kBaphometJrId = 1101;
inline constexpr std::uint16_t kMasteringId = 1090;
inline constexpr std::uint16_t kEclipseId = 1093;
inline constexpr std::uint16_t kDragonFlyId = 1091;
inline constexpr std::uint16_t kVagabondWolfId = 1092;
inline constexpr std::uint16_t kBaphometId = 1039;
// Ceux de la prairie que le désert fait naître aussi.
inline constexpr std::uint16_t kDropsId = 1113;
inline constexpr std::uint16_t kCondorId = 1009;
inline constexpr std::uint16_t kGolemId = 1040;
inline constexpr std::uint16_t kMummyId = 1041;

inline constexpr std::uint32_t kMinuteMs = 60000;

// Les règles de terrain de la prairie et du désert, que nomment les arènes
// générées.
inline constexpr std::uint16_t kPlainsTerrainId = 1;
inline constexpr std::uint16_t kDesertTerrainId = 2;
inline constexpr std::uint16_t kSnowTerrainId = 3;

// survivors_rules_mage.cpp : les classes, les skills et les cartes. Les skills
// sont ceux de la réserve du Magicien, puis les évolués de
// ShippedEvolvedSkills, puis ceux du Chevalier et ceux du Voleur : une seule
// table, que FindSkill parcourt.
[[nodiscard]] std::span<const ClassRule> ShippedClasses();
[[nodiscard]] std::span<const SurvivorsSkillRule> ShippedSkills();
[[nodiscard]] std::span<const CardRule> ShippedCards();

// survivors_rules_wizard.cpp : les skills évolués, que seul un coffre donne,
// et les évolutions qui les nomment.
inline constexpr std::size_t kShippedEvolvedSkillCount = 11;
[[nodiscard]] std::span<const SurvivorsSkillRule> ShippedEvolvedSkills();
[[nodiscard]] std::span<const EvolutionRule> ShippedEvolutions();

// survivors_rules_knight.cpp : le Chevalier, ses skills — la réserve, puis les
// évolués — et ses évolutions. ShippedClasses, ShippedSkills et
// ShippedEvolutions les mettent après ceux du Magicien.
inline constexpr std::size_t kKnightPoolSkillCount = 9;
inline constexpr std::size_t kKnightEvolvedSkillCount = 7;
// Ses cartes propres, que ShippedCards met après celles du Magicien.
inline constexpr std::size_t kKnightCardCount = 1;
[[nodiscard]] const ClassRule& KnightClass();
[[nodiscard]] std::span<const CardRule> KnightCards();
[[nodiscard]] std::span<const SurvivorsSkillRule> KnightSkills();
[[nodiscard]] std::span<const EvolutionRule> KnightEvolutions();

// survivors_rules_thief.cpp : le Voleur, sur le même plan, après le Chevalier.
inline constexpr std::size_t kThiefPoolSkillCount = 10;
inline constexpr std::size_t kThiefEvolvedSkillCount = 7;
inline constexpr std::size_t kThiefCardCount = 2;
[[nodiscard]] const ClassRule& ThiefClass();
[[nodiscard]] std::span<const CardRule> ThiefCards();
[[nodiscard]] std::span<const SurvivorsSkillRule> ThiefSkills();
[[nodiscard]] std::span<const EvolutionRule> ThiefEvolutions();

// survivors_rules_archer.cpp : l'Archer, sur le même plan, après le Voleur.
inline constexpr std::size_t kArcherPoolSkillCount = 9;
inline constexpr std::size_t kArcherEvolvedSkillCount = 7;
inline constexpr std::size_t kArcherCardCount = 6;
[[nodiscard]] const ClassRule& ArcherClass();
[[nodiscard]] std::span<const CardRule> ArcherCards();
[[nodiscard]] std::span<const SurvivorsSkillRule> ArcherSkills();
[[nodiscard]] std::span<const EvolutionRule> ArcherEvolutions();

// survivors_rules_foes.cpp : les monstres, leurs vagues, les arènes, la courbe
// d'XP et la table des éléments.
[[nodiscard]] std::span<const FoeRule> ShippedFoes();
[[nodiscard]] std::span<const WaveRule> ShippedWaves();
[[nodiscard]] std::span<const ArenaRule> ShippedArenas();
[[nodiscard]] std::span<const ExpSegment> ShippedExpCurve();
[[nodiscard]] std::span<const ElementRow> ShippedElements();

// survivors_rules_desert.cpp : les monstres propres au désert de Morroc, et
// ses vagues, ses boss et ses événements. ShippedFoes les met après ceux de
// la prairie ; l'arène du désert porte le contenu.
inline constexpr std::size_t kDesertFoeCount = 23;
[[nodiscard]] std::span<const FoeRule> DesertFoes();
[[nodiscard]] ArenaContent DesertContent();

// survivors_rules_snow.cpp : les monstres propres aux neiges de Lutie, et
// leurs vagues, leurs boss et leurs événements. ShippedFoes les met après
// ceux du désert ; l'arène de la neige porte le contenu.
inline constexpr std::size_t kSnowFoeCount = 19;
[[nodiscard]] std::span<const FoeRule> SnowFoes();
[[nodiscard]] ArenaContent SnowContent();

// survivors_rules_bosses.cpp : les mini-boss, le boss final et leurs coups.
[[nodiscard]] std::span<const BossRule> ShippedBosses();

// survivors_rules_events.cpp : les événements, les élites, le coffre, les
// plantes et les objets au sol.
[[nodiscard]] std::span<const EventRule> ShippedEvents();
[[nodiscard]] EliteRule ShippedElites();
[[nodiscard]] ChestRule ShippedChest();
[[nodiscard]] std::span<const PlantRule> ShippedPlants();
[[nodiscard]] std::span<const PickupRule> ShippedPickups();

// survivors_rules_terrain.cpp : les règles de terrain des arènes générées.
[[nodiscard]] std::span<const TerrainRule> ShippedTerrains();

}  // namespace ro::sim::survivors
