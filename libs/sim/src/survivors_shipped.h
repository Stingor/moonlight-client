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
inline constexpr std::uint16_t kSpiralPierceId = 397;
inline constexpr std::uint16_t kHeadCrushId = 398;
inline constexpr std::uint16_t kJointBeatId = 399;

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

// survivors_rules_mage.cpp : les classes, les skills et les cartes. Les skills
// sont ceux de la réserve du Magicien, puis les évolués de
// ShippedEvolvedSkills, puis ceux du Chevalier : une seule table, que
// FindSkill parcourt.
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
inline constexpr std::size_t kKnightPoolSkillCount = 7;
inline constexpr std::size_t kKnightEvolvedSkillCount = 3;
[[nodiscard]] const ClassRule& KnightClass();
[[nodiscard]] std::span<const SurvivorsSkillRule> KnightSkills();
[[nodiscard]] std::span<const EvolutionRule> KnightEvolutions();

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
