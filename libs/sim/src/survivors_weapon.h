#pragma once

// LE COUP DE L'ARME : le skill que toute classe porte au premier emplacement,
// et ce que ses cartes y ajoutent — recharge raccourcie, perforation, rebond.
// Les autres skills n'en reçoivent rien.
//
// En-tête PRIVÉ de `libs/sim/src`.

#include <cstdint>

#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"
#include "ro/sim/survivors_state.h"

namespace ro::sim::survivors {

// Le coup de l'arme de la classe, posé au premier emplacement d'un héros neuf,
// avant son skill de départ. Sans effet pour une classe sans coup de l'arme.
void ArmHero(const SurvivorsRules& rules, const ClassRule& hero_class, SurvivorsHero& hero);

// Le coup de l'arme du héros au niveau de son arme : 2 dès sa première
// évolution, sans dépasser ses niveaux.
void RaiseWeapon(SurvivorsField& field);

// Vrai pour le skill `skill_id`, s'il est un coup de l'arme.
[[nodiscard]] bool IsWeaponSkillId(const SurvivorsField& field, std::uint16_t skill_id);

// LE NIVEAU QUE LE COUP DE L'ARME JOUE : celui des règles, plus la perforation
// des cartes. Un tir traverse des monstres de plus, sous sa mémoire ; une
// frappe enchaîne sur ceux qui jouxtent sa cible ; un cône porte plus loin ; un
// éclat s'élargit.
[[nodiscard]] SkillLevel WeaponLevelOf(const SurvivorsField& field, const SurvivorsSkillRule& rule,
                                       const SkillLevel& level);

// La recharge du coup de l'arme : celle d'un skill, raccourcie en plus par les
// cartes de l'arme, sous le même plafond.
[[nodiscard]] std::uint32_t WeaponReadyAfter(const SurvivorsField& field, const SkillLevel& level);

// LE REBOND : après un coup de l'arme sur `struck`, la chance des cartes de
// frapper aussi le monstre le plus proche de lui, à kWeaponSplitReachCells, pour
// kWeaponSplitDamagePct % de `base_damage`. Rien n'est tiré sans chance ; un
// coup qui n'est pas de l'arme ne rebondit pas.
void SplitWeaponHit(SurvivorsField& field, std::uint16_t skill_id, std::int32_t base_damage,
                    SurvivorsElement element, const SurvivorsFoe& struck);

}  // namespace ro::sim::survivors
