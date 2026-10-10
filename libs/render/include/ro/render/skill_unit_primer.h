#pragma once

#include <array>
#include <cstdint>
#include <span>

namespace ro::render {

// L'APPARENCE DU PIÈGE USÉ : celle que le serveur donne à un piège qui vient de
// se déclencher. C'est en la prenant qu'une unité joue son AMORCE.
inline constexpr std::uint32_t kUsedTrapUnit = 140;

// L'AMORCE D'UNE UNITÉ AU SOL : l'effet qu'elle retient en paraissant et
// qu'elle ne joue qu'au moment où elle prend l'apparence du piège usé — la
// glace d'un Freezing Trap, l'éclair d'un Flasher, la poussière d'un Sandman.
//
// ⚠ L'UNITÉ LA RETIENT SOUS L'APPARENCE DE SA POSE, pas sous celle qu'elle
// prend : une fois déclenché, tout piège porte le même numéro, et seul celui
// qu'il avait en paraissant dit ce qu'il était.
//
// ⚠ UN TIC PEUT LA RÉÉCRIRE : une unité qui sème son amorce elle-même en
// retient ensuite une autre, et c'est celle-là qui joue au déclenchement.
struct SkillUnitPrimerEntry {
  std::uint16_t unit_id = 0;
  // LES NIVEAUX de la compétence où cette amorce vaut, bornes comprises.
  std::uint8_t level_min = 0;
  std::uint8_t level_max = 0;
  std::uint16_t effect_id = 0;
};

// La table entière, triée par unité puis par niveau.
[[nodiscard]] std::span<const SkillUnitPrimerEntry> SkillUnitPrimerTable();

// ⚠ UN AJOUT DE CE CLIENT, à la demande du joueur : LES AMORCES DE LAND MINE
// ET DE SHOCKWAVE TRAP.
//
// Le client d'origine ne donne aucune amorce à ces deux pièges : déclenchés,
// ils ne montrent que leur modèle qui joue. Ce client-ci leur en donne une, et
// la joue comme les autres — sur le piège, qui l'emporte en disparaissant :
// l'explosion d'un pilier de feu pour la Land Mine, l'onde de choc pour le
// Shockwave Trap. Elles ne valent que là où l'unité n'en a pas d'autre.
inline constexpr std::uint16_t kLandMineUnit = 147;
inline constexpr std::uint16_t kShockwaveTrapUnit = 148;
inline constexpr std::uint16_t kFirePillarBombEffect = 97;
inline constexpr std::uint16_t kShockwaveHitEffect = 146;
inline constexpr std::array<SkillUnitPrimerEntry, 2> kClientAddedPrimers = {{
    {kLandMineUnit, 0, 255, kFirePillarBombEffect},
    {kShockwaveTrapUnit, 0, 255, kShockwaveHitEffect},
}};

// L'amorce d'une unité posée sous cette apparence et à ce niveau, ou ZÉRO :
// celle de la table, sinon celle que ce client ajoute.
[[nodiscard]] std::uint16_t SkillUnitPrimer(std::uint32_t placed_unit_id, std::uint8_t skill_level);

// L'amorce que joue une unité qui vient de prendre l'apparence `unit_id`, ou
// ZÉRO : seule l'apparence du piège usé la fait jouer.
[[nodiscard]] std::uint16_t SkillUnitPrimerOnLook(std::uint32_t placed_unit_id,
                                                  std::uint8_t skill_level, std::uint32_t unit_id);

}  // namespace ro::render
