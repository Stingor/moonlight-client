#pragma once

#include <cstdint>
#include <span>

namespace ro::render {

// CE QU'UNE UNITÉ AU SOL FAIT À SON APPARITION, dans l'ordre : les effets
// qu'elle fait naître sur elle, et ceux qu'elle retire d'abord.
//
// Le retrait vise l'effet que l'unité porte déjà sous le même identifiant :
// une unité qui réapparaît ne double pas sa figure.
enum class SkillUnitPoseKind : std::uint8_t {
  kEffect,
  kRemoveEffect,
};

struct SkillUnitPoseStep {
  std::uint16_t unit_id = 0;
  SkillUnitPoseKind kind = SkillUnitPoseKind::kEffect;
  std::uint32_t effect_id = 0;
  // LES NIVEAUX de la compétence où l'étape joue, bornes comprises : le paquet
  // d'apparition porte le niveau, et une unité peut choisir son effet selon
  // lui.
  std::uint8_t level_min = 0;
  std::uint8_t level_max = 255;
  // Vrai quand l'unité ne sème l'effet que si elle ne le porte pas déjà.
  bool unless_carried = false;
  // ⚠ EN QUEUE, comme tout ce qui s'ajoute : la table se bâtit par position.
  //
  // LA ROTATION DE L'IMAGE à l'écran, en degrés dans le sens des aiguilles
  // d'une montre ; zéro la laisse droite. Voir `DrawnEffect::spin_degrees`.
  float spin_degrees = 0.0F;
  // VRAI QUAND LA ROTATION SUIT LA VUE : chaque degré dont la caméra tourne
  // s'y ajoute, et l'image reste fixe PAR RAPPORT AU SOL — une fissure orientée
  // dans le monde.
  bool spin_anchored_to_ground = false;
};

// Vrai si cette étape joue pour une unité de ce niveau.
[[nodiscard]] constexpr bool SkillUnitPoseStepPlays(const SkillUnitPoseStep& step,
                                                    std::uint8_t skill_level) {
  return skill_level >= step.level_min && skill_level <= step.level_max;
}

// La table entière, triée par unité, l'ordre du client gardé dans chacune.
[[nodiscard]] std::span<const SkillUnitPoseStep> SkillUnitPoseTable();

// Les étapes d'une unité, vides quand elle ne fait rien naître.
[[nodiscard]] std::span<const SkillUnitPoseStep> SkillUnitPoseOf(std::uint32_t unit_id);

}  // namespace ro::render
