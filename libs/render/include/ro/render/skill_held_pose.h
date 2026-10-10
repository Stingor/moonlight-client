#pragma once

#include <cstdint>
#include <optional>
#include <span>

namespace ro::render {

// LA POSE QU'UNE COMPÉTENCE IMPOSE À SON LANCEUR, à l'annonce ou à l'impact
// d'un coup.
//
// Le client ne déroule pas alors l'animation du fichier : il mène la pose
// lui-même. Il montre les images [first_frame, last_frame] du rang `play_rank`,
// une image toutes les `period_ticks` avances de l'acteur ; puis, sauf boucle,
// il TIENT l'image `hold_frame` du rang `hold_rank`. Tout s'arrête à `hold_ms`
// quand la pose est minutée. L'orientation reste celle de l'acteur.
//
// Une pose en boucle reprend sa plage au début au lieu de tenir.
struct SkillHeldPose {
  std::uint16_t skill_id = 0;
  std::uint8_t play_rank = 0;
  std::uint8_t first_frame = 0;
  std::uint8_t last_frame = 0;
  std::uint8_t hold_rank = 0;
  std::uint8_t hold_frame = 0;
  bool timed = false;
  std::uint32_t hold_ms = 0;
  bool loops = false;
  // Combien d'avances de l'acteur — une par tic de jeu — durent une image.
  std::uint8_t period_ticks = 0;
  // Vrai quand la compétence ne pose pas sa période : l'acteur garde celle de
  // sa dernière action, et la table donne la plus ordinaire.
  bool period_inherited = false;
  // LES SORTES DE COUP qui la déclenchent À L'IMPACT, un bit par sorte ; zéro
  // pour une pose d'annonce.
  std::uint16_t damage_types = 0;
  // Vrai quand elle dure le DÉLAI DE COMBO de son lanceur, que le serveur
  // annonce, et non `hold_ms` : les enchaînements du moine tiennent leur pose
  // jusqu'à ce que le coup suivant soit permis.
  bool hold_from_combo_delay = false;
};

// LA POSE D'ARRIVÉE d'Asura Strike, que l'arrivée suivant son impact fait tenir
// — voir `ro::sim::kArrivalPoseSkill` : l'image 2 du rang 12, trois secondes.
// La période est celle que l'impact a laissée ; une plage d'une image ne la lit
// pas.
inline constexpr SkillHeldPose kArrivalHeldPose{
    .skill_id = 271,
    .play_rank = 12,
    .first_frame = 2,
    .last_frame = 2,
    .hold_rank = 12,
    .hold_frame = 2,
    .timed = true,
    .hold_ms = 3000,
    .period_ticks = 4,
};

// La pose qu'une compétence impose à l'IMPACT d'un coup de cette sorte, ou rien.
[[nodiscard]] const SkillHeldPose* SkillImpactPoseOf(std::uint16_t skill_id,
                                                     std::uint8_t damage_type);

// Ce que la pose montre à un instant : un rang et une image de ce rang.
struct HeldPoseFrame {
  std::uint8_t rank = 0;
  std::uint8_t frame = 0;
};

// La table entière, triée par compétence, l'annonce avant l'impact.
[[nodiscard]] std::span<const SkillHeldPose> SkillHeldPoseTable();

// LA DURÉE D'UNE POSE D'IMPACT, comptée depuis le coup : sa durée fixe, ou le
// délai de combo de son lanceur. Le client la pose au coup — maintenant plus le
// délai qu'il tient —, et une annonce de délai neuve la repousse à son tour :
// l'échéance est la plus tardive des deux.
[[nodiscard]] std::uint32_t ImpactPoseHoldMs(const SkillHeldPose& pose, std::uint32_t struck_ms,
                                             std::uint32_t combo_delay_ms,
                                             std::uint32_t combo_delay_since_ms);

// La pose d'une compétence À L'ANNONCE, ou rien.
[[nodiscard]] const SkillHeldPose* SkillHeldPoseOf(std::uint16_t skill_id);

// Ce que montre la pose `elapsed_ms` après l'annonce, ou rien quand elle est
// finie. `tick_ms` est la durée d'une avance de l'acteur.
[[nodiscard]] std::optional<HeldPoseFrame> HeldPoseFrameAt(const SkillHeldPose& pose,
                                                           std::uint32_t elapsed_ms,
                                                           std::uint32_t tick_ms);

}  // namespace ro::render
