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

inline constexpr std::uint16_t kHornetCardId = 4019;
inline constexpr std::uint16_t kChonchonCardId = 4009;
inline constexpr std::uint16_t kGolemCardId = 4072;
inline constexpr std::uint16_t kMasteringCardId = 4197;
inline constexpr std::uint16_t kPupaCardId = 4003;
inline constexpr std::uint16_t kPoringCardId = 4001;
inline constexpr std::uint16_t kSporeCardId = 4022;
inline constexpr std::uint16_t kThiefBugCardId = 4016;

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

inline constexpr std::uint32_t kMinuteMs = 60000;

// survivors_rules_mage.cpp : la classe, ses skills et ses cartes. Les skills
// sont ceux de la réserve, puis les évolués de ShippedEvolvedSkills : une
// seule table, que FindSkill parcourt.
[[nodiscard]] std::span<const ClassRule> ShippedClasses();
[[nodiscard]] std::span<const SurvivorsSkillRule> ShippedSkills();
[[nodiscard]] std::span<const CardRule> ShippedCards();

// survivors_rules_wizard.cpp : les skills évolués, que seul un coffre donne,
// et les évolutions qui les nomment.
inline constexpr std::size_t kShippedEvolvedSkillCount = 4;
[[nodiscard]] std::span<const SurvivorsSkillRule> ShippedEvolvedSkills();
[[nodiscard]] std::span<const EvolutionRule> ShippedEvolutions();

// survivors_rules_foes.cpp : les monstres, leurs vagues, l'arène, la courbe
// d'XP et la table des éléments.
[[nodiscard]] std::span<const FoeRule> ShippedFoes();
[[nodiscard]] std::span<const WaveRule> ShippedWaves();
[[nodiscard]] std::span<const ArenaRule> ShippedArenas();
[[nodiscard]] std::span<const ExpSegment> ShippedExpCurve();
[[nodiscard]] std::span<const ElementRow> ShippedElements();

// survivors_rules_bosses.cpp : les mini-boss, le boss final et leurs coups.
[[nodiscard]] std::span<const BossRule> ShippedBosses();

// survivors_rules_events.cpp : les événements, les élites, le coffre, les
// plantes et les objets au sol.
[[nodiscard]] std::span<const EventRule> ShippedEvents();
[[nodiscard]] EliteRule ShippedElites();
[[nodiscard]] ChestRule ShippedChest();
[[nodiscard]] std::span<const PlantRule> ShippedPlants();
[[nodiscard]] std::span<const PickupRule> ShippedPickups();

}  // namespace ro::sim::survivors
