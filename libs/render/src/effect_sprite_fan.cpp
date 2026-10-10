#include "ro/render/effect_sprite_fan.h"

#include <cstddef>

#include "ro/render/effect_particle_quad.h"
#include "ro/render/effect_track.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

// L'ALPHA PLEIN d'un calque, dont le client prend les quatre-vingt-cinq pour
// cent.
constexpr float kFullAlpha = 255.0F;

}  // namespace

std::array<std::uint8_t, 4> EffectSpriteFanTint(std::uint8_t mark) {
  const auto alpha = static_cast<std::uint8_t>(kFullAlpha * kSpriteFanAlphaShare);
  if (mark == kSpriteFanBlueMark) {
    return {kSpriteFanBlue[0], kSpriteFanBlue[1], kSpriteFanBlue[2], alpha};
  }
  if (mark == kSpriteFanLayerMark) {
    // LA TEINTE DES CALQUES, que le sprite porte déjà : blanc par-dessus ne
    // change rien, et l'alpha seul est rabattu.
    return {255, 255, 255, alpha};
  }
  // ⚠ TOUTE AUTRE MARQUE — et la seule qu'un site donne est QUATRE — laisse le
  // client sur la teinte du quad précédent. Voir l'en-tête : c'est un défaut,
  // pas une lecture manquée.
  return {255, 255, 255, 255};
}

std::array<std::uint8_t, 4> EffectSpriteFanHaloTint(std::uint8_t mark) {
  const auto alpha = static_cast<std::uint8_t>(kFullAlpha * kSpriteFanHaloAlphaShare);
  if (mark == kSpriteFanBlueMark) {
    return {kSpriteFanBlue[0], kSpriteFanBlue[1], kSpriteFanBlue[2], alpha};
  }
  if (mark == kSpriteFanLayerMark) {
    // LE ROUGE DU CALQUE, que le sprite porte déjà ; le vert et le bleu tombent.
    return {255, kSpriteFanShade, kSpriteFanShade, alpha};
  }
  return EffectSpriteFanTint(mark);
}

namespace {

std::vector<EffectTrailStep> ComposePoses(
    const std::array<EffectTrack, kEffectTracksPerNode>& tracks,
    const std::vector<EffectTrailStep>& history, glm::vec3 anchor,
    std::array<std::uint8_t, 4> (*tint_of)(std::uint8_t)) {
  std::vector<EffectTrailStep> poses;
  for (const EffectTrack& track : tracks) {
    if (!track.active) {
      continue;
    }
    const std::array<std::uint8_t, 4> tint = tint_of(track.mark[kSpriteFanMarkOfColor]);
    // ⚠ UN HISTORIQUE VIDE VAUT UN CRAN À L'ORIGINE, et c'est le cas de cette
    // famille : le constructeur du client en déclare un seul, et le bâtisseur
    // ne le change pas. Sauter la piste sur un historique vide ne rendrait
    // rien du tout.
    const std::size_t crans = history.empty() ? 1 : history.size();
    for (std::size_t notch = 0; notch < crans; ++notch) {
      EffectTrailStep pose;
      // LA POSITION EST LA SOMME DES DEUX : le point du cran et l'ancre de la
      // piste. C'est ce croisement qui fait l'éventail. L'ancre vit dans le
      // repère du client et passe par `S` : sa hauteur négative est un point
      // AU-DESSUS du nœud, et sa profondeur se nie.
      const glm::vec3 from = history.empty() ? anchor : history[notch].ground_position;
      pose.ground_position = from + EffectPointFromClient(track.anchor);
      pose.sprite_scale = history.empty() ? 1.0F : history[notch].sprite_scale;
      pose.alpha = static_cast<float>(tint[3]);
      pose.color = {static_cast<float>(tint[0]), static_cast<float>(tint[1]),
                    static_cast<float>(tint[2])};
      poses.push_back(pose);
    }
  }
  return poses;
}

}  // namespace

std::vector<EffectTrailStep> ComposeSpriteFanPoses(
    const std::array<EffectTrack, kEffectTracksPerNode>& tracks,
    const std::vector<EffectTrailStep>& history, glm::vec3 anchor) {
  return ComposePoses(tracks, history, anchor, &EffectSpriteFanTint);
}

std::vector<EffectTrailStep> ComposeSpriteFanHaloPoses(
    const std::array<EffectTrack, kEffectTracksPerNode>& tracks,
    const std::vector<EffectTrailStep>& history, glm::vec3 anchor) {
  return ComposePoses(tracks, history, anchor, &EffectSpriteFanHaloTint);
}

void AdvanceSpriteFanTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks) {
  for (EffectTrack& track : tracks) {
    if (!track.active) {
      continue;
    }
    // ⚠ L'ÉCARTEMENT NE TOUCHE QUE LE PLAN DU SOL : le cosinus va à l'abscisse
    // et le sinus à la profondeur, DANS LE REPÈRE DU CLIENT où vit l'ancre, et
    // la hauteur reste où elle est. L'éventail s'ouvre à plat, il ne monte pas.
    const float step = track.value[kSpriteFanValueOfSpread];
    track.anchor.x += step * CosOfDegrees(track.azimuth_degrees);
    track.anchor.z += step * SinOfDegrees(track.azimuth_degrees);
  }
}

}  // namespace ro::render
