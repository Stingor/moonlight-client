#include "ro/render/effect_magic_target.h"

#include <cstddef>

#include "ro/render/effect_track.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

// L'ÉCART D'UN COIN au centre du bloc, en cases : le coin de rang zéro est au
// coin de petites coordonnées de la case, le rang trois à celui de grandes.
[[nodiscard]] glm::vec2 CornerOffset(std::int32_t column, std::int32_t row, std::size_t corner) {
  const bool abscisse_suivante = corner % 2 == 1;
  const bool ordonnee_suivante = corner >= 2;
  return glm::vec2{
      static_cast<float>(column) + (abscisse_suivante ? 1.0F : 0.0F) - kMagicTargetGridCenter,
      static_cast<float>(row) + (ordonnee_suivante ? 1.0F : 0.0F) - kMagicTargetGridCenter};
}

// LA PORTION D'IMAGE d'un coin : son écart au centre TOURNE, se met à l'échelle,
// puis se ramène dans la planche.
//
// ⚠ ELLE SORT DE L'INTERVALLE UNITAIRE dès que l'échelle dépasse un, ce qui est
// le cas partout sauf au point bas de la course. Ce n'est pas une erreur de
// transcription : c'est ainsi que le cercle rétrécit, puisque la portion d'image
// grandit quand ce qu'elle montre rapetisse.
//
// ⚠ ET CE QUI SE VOIT AU-DELÀ NE DÉPEND PAS DE NOUS mais du SERRAGE : le
// renderer de sol téléverse ses images en `kSprite`, c'est-à-dire en
// `GL_CLAMP_TO_EDGE`. Le bord de `magic_target.tga` a été mesuré — les quatre
// côtés de ses 256 × 256 sont entièrement transparents, sans une seule
// exception — si bien que serrer n'étale rien : hors du cercle, il n'y a rien.
// Une répétition, elle, collerait un second cercle en quartiers tout autour.
[[nodiscard]] glm::vec2 CornerPortion(glm::vec2 offset, float cosinus, float sinus, float scale) {
  const float abscisse = offset.x * cosinus + offset.y * sinus;
  const float ordonnee = offset.y * cosinus - offset.x * sinus;
  const float cote = static_cast<float>(kMagicTargetGridSide);
  return glm::vec2{(abscisse * scale + kMagicTargetGridCenter) / cote,
                   (ordonnee * scale + kMagicTargetGridCenter) / cote};
}

[[nodiscard]] EffectGroundCellPaint CellPaint(std::int32_t column, std::int32_t row, float cosinus,
                                              float sinus, float scale,
                                              const std::array<std::uint8_t, 4>& tint) {
  EffectGroundCellPaint peinture;
  peinture.cell_offset_x = kMagicTargetGridFirstCell + column;
  peinture.cell_offset_y = kMagicTargetGridFirstCell + row;
  peinture.color = tint;
  for (std::size_t corner = 0; corner < peinture.texture_uv.size(); ++corner) {
    peinture.texture_uv[corner] =
        CornerPortion(CornerOffset(column, row, corner), cosinus, sinus, scale);
  }
  return peinture;
}

}  // namespace

std::vector<EffectGroundCellPaint> ComposeMagicTargetGridCells(const EffectTrack& track) {
  std::vector<EffectGroundCellPaint> cases;
  if (!track.active) {
    return cases;
  }
  const std::int32_t angle = WithinFullTurn(track.azimuth_degrees);
  const float cosinus = CosOfDegrees(angle);
  const float sinus = SinOfDegrees(angle);
  // ⚠ L'ÉCHELLE EST LA CLÉ DE SOULÈVEMENT, et elle ne soulève rien ici : c'est
  // la case que le mouvement fait battre, et que la figure lit comme un facteur
  // de texture. Le sens d'un champ appartient à qui le lit — ADR 0017.
  const float scale = track.lift_key;
  cases.reserve(static_cast<std::size_t>(kMagicTargetGridSide * kMagicTargetGridSide * 2));
  for (std::int32_t row = 0; row < kMagicTargetGridSide; ++row) {
    for (std::int32_t column = 0; column < kMagicTargetGridSide; ++column) {
      cases.push_back(CellPaint(column, row, cosinus, sinus, scale, kMagicTargetNearColor));
      cases.push_back(CellPaint(column, row, cosinus, sinus, scale + kMagicTargetSecondPassScale,
                                kMagicTargetFarColor));
    }
  }
  return cases;
}

void AdvanceMagicTargetGridTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks) {
  for (EffectTrack& effect_track : tracks) {
    if (!effect_track.active) {
      continue;
    }
    // ⚠ L'ANGLE AVANCE AVANT D'ÊTRE LU : le client incrémente puis indexe sa
    // table. Lire avant d'avancer décalerait l'échelle d'un tic sur la rotation,
    // et les deux ne battraient plus ensemble.
    const std::int32_t angle = WithinFullTurn(effect_track.azimuth_degrees + 1);
    effect_track.azimuth_degrees = static_cast<std::int16_t>(angle);
    effect_track.lift_key = SinOfDegrees(angle) * kMagicTargetScaleSwing + kMagicTargetScaleFloor;
  }
}

}  // namespace ro::render
