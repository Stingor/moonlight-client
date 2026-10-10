#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#include "ro/render/skill_sound_table.h"

namespace ro::render {

// LE BRUIT D'UNE UNITÉ POSÉE AU SOL, et il vient en DEUX MOMENTS.
//
// ⚠ CE N'EST PAS LE BRUIT DE SON EFFET. Le client le joue depuis le cas de
// l'UNITÉ — à la pose, puis à l'entretien —, et non depuis la fonction de l'effet. La table
// des sons d'effet n'en couvre aucun ; les y chercher ne donnait rien, et c'est
// pourquoi une unité au sol restait muette.

// Une entrée de l'une des deux tables des sons d'unité.
struct SkillUnitSoundEntry {
  std::uint16_t unit_id = 0;
  // Le chemin du fichier, relatif à kSoundFolder, écrit comme dans l'archive.
  std::string_view sound;
  // LE DÉNOMINATEUR D'UN TIRAGE AU SORT : le son ne s'entend qu'une fois sur
  // autant. UN veut dire qu'il s'entend à coup sûr — c'est le cas ordinaire.
  //
  // Le client tire pour une seule unité, et c'est voulu : un piège qui
  // crépiterait à chaque passe serait insupportable là où il y en a dix.
  std::uint8_t one_in = 1;
};

// Ce qu'une unité fait entendre AU MOMENT OÙ ON LA POSE, une fois.
[[nodiscard]] std::span<const SkillUnitSoundEntry> SkillUnitPlacementSoundTable();

// Et ce qu'elle fait entendre CHAQUE FOIS que le client resème son effet. Le
// son suit alors la cadence de l'image — voir SkillUnitVisual.
[[nodiscard]] std::span<const SkillUnitSoundEntry> SkillUnitSustainSoundTable();

// Le son de pose d'une unité, ou NUL pour une unité qui se pose en silence.
[[nodiscard]] const SkillUnitSoundEntry* SkillUnitPlacementSound(std::uint32_t unit_id);

// Le son que son entretien rejoue, ou NUL.
[[nodiscard]] const SkillUnitSoundEntry* SkillUnitSustainSound(std::uint32_t unit_id);

}  // namespace ro::render
