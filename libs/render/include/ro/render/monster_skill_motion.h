#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

#include "ro/render/sprite_catalog.h"

namespace ro::render {

// ── LE GESTE D'UNE CRÉATURE QUI EMPLOIE UNE COMPÉTENCE ──────────────────────
//
// Un joueur prend sa posture d'un ÉTAT, que deux résolveurs tirent de la
// compétence. Une créature ou un PNJ n'y passe JAMAIS : son geste vient de la
// table des gestes (`data\monsterskillinfo.xml`), sur les cinq chemins de
// compétence — l'incantation, l'emploi, l'emploi au sol, l'emploi en un point
// et le coup. Une compétence que la table ne nomme pas pour son espèce ne
// change RIEN à sa posture.

// Vrai quand la classe AFFICHÉE est une créature ou un PNJ : de 1001 à 3998, et
// de 20021 à 32042.
[[nodiscard]] bool IsMonsterOrNpcClass(std::uint32_t job) noexcept;

// LA COMPÉTENCE QUE LE CHEMIN DE L'EMPLOI POSE PAR LA TABLE, QUI QUE SOIT LE
// LANCEUR : la neuvième. Pour un joueur, la table ne nomme pas son métier, et
// il garde donc sa posture.
inline constexpr std::uint16_t kSkillPosedByTableForAll = 9;

// Vrai quand l'emploi de cette compétence par cette classe prend son geste
// dans la table, et non dans l'état d'emploi.
[[nodiscard]] bool SkillUsePosedByTable(std::uint32_t job, std::uint16_t skill_id) noexcept;

struct MonsterSkillMotion {
  // Le rang d'animation : l'action de la table, qui compte à partir de un.
  std::size_t animation = 0;
  // Vrai quand l'action tourne jusqu'au geste suivant ; faux quand elle se joue
  // une fois et rend l'acteur à l'attente.
  bool loops = false;
};

// Le geste de cette classe pour cette compétence, ou RIEN : un métier joueur,
// une espèce ou une compétence que la table ne nomme pas, ou une action qui ne
// désigne aucun rang.
[[nodiscard]] std::optional<MonsterSkillMotion> MonsterSkillMotionFor(const SpriteCatalog& catalog,
                                                                      std::uint32_t job,
                                                                      std::uint16_t skill_id);

// LA MÉTAMORPHOSE RÈGLE SA CADENCE SUR SON INCANTATION : une image dure la
// durée d'incantation divisée par trois cents — en millisecondes, division
// entière —, comptée en pas de vingt-quatre millisecondes. Rien pour une autre
// compétence, ou quand ce quotient est nul. Cela ne vaut que si la table a
// donné un geste.
inline constexpr std::uint16_t kMetamorphosisSkill = 193;
inline constexpr std::uint32_t kMetamorphosisCastDivisorMs = 300;
[[nodiscard]] std::optional<float> MonsterCastCadenceSteps(std::uint16_t skill_id,
                                                           std::uint32_t cast_ms) noexcept;

}  // namespace ro::render
