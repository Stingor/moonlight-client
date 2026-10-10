#include "ro/render/effect_rainbow_trail.h"

#include <algorithm>

#include "ro/render/effect_scene.h"
#include "ro/render/effect_track.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

// LES SEPT COULEURS, dans l'ordre des bandes. Le client les écrit une par une,
// sans règle : ce ne sont pas des teintes calculées mais sept constantes.
constexpr std::array<std::array<std::uint8_t, 3>, kRainbowTrailBands> kBandTints = {{
    {255, 0, 0},
    {255, 126, 0},
    {255, 255, 0},
    {0, 255, 0},
    {0, 0, 255},
    {115, 50, 200},
    {255, 0, 255},
}};

// LE MÉLANGE DE CETTE VOIE, dans la numérotation de Direct3D : elle n'a pas de
// mode à choisir — son seul drapeau vaut zéro chez son unique appelant, et donne
// l'ADDITION.
constexpr std::int32_t kAddedDestination = 2;

// ⚠ L'ORDONNÉE EST DOUBLÉE : le client écrit deux fois le sinus. Voir l'en-tête.
constexpr float kHeightDoubling = 2.0F;

}  // namespace

std::array<std::uint8_t, 3> EffectRainbowTrailBandTint(std::size_t band) {
  return kBandTints[std::min(band, kRainbowTrailBands - 1)];
}

std::vector<EffectWorldQuad> ComposeRainbowTrailQuads(const EffectTrack& track,
                                                      glm::vec3 node_origin) {
  std::vector<EffectWorldQuad> quads;
  const std::int32_t sweep_degrees = track.degrees;
  if (!track.active || track.alpha <= 0 || sweep_degrees < 0) {
    return quads;
  }
  const std::int32_t step = sweep_degrees / kRainbowTrailSegments;
  if (step <= 0) {
    // Voir l'en-tête : le client se figerait ici.
    return quads;
  }

  // ⚠ L'INCLINAISON EST UN CAP DU REPÈRE DU CLIENT, tiré de SA caméra : la
  // direction revient au nôtre par `S`, qui nie la profondeur.
  const float vers_x = CosOfDegrees(track.tilt_degrees);
  const float vers_z = -SinOfDegrees(track.tilt_degrees);
  const auto alpha = static_cast<std::uint8_t>(std::clamp<std::int16_t>(track.alpha, 0, 255));

  // UN POINT DE L'ARC : le rayon à l'angle donné, mis à l'échelle de la bande,
  // couché sur la direction que l'inclinaison désigne.
  //
  // ⚠ LA HAUTEUR SE NIE AVEC LA PROFONDEUR : le client écrit le sinus dans son
  // ordonnée qui DESCEND. Un sinus positif — le premier demi-tour de l'azimut —
  // passe sous le nœud, et l'arc est au-dessus quand son alpha culmine, au
  // second demi-tour.
  const auto point = [&](std::int32_t angle, float scale) {
    const float de_cote = track.radius_at_anchor * CosOfDegrees(angle) * scale;
    const float height = track.radius_at_anchor * SinOfDegrees(angle) * scale;
    return node_origin + glm::vec3{de_cote * vers_x, -kHeightDoubling * height, de_cote * vers_z};
  };

  for (std::size_t band_index = 0; band_index < kRainbowTrailBands; ++band_index) {
    // ⚠ LES DEUX ÉCHELLES SE SUIVENT : celle de la bande, et celle de la bande
    // suivante. C'est ce qui les emboîte sans fente — voir l'en-tête.
    const float dedans = 1.0F - static_cast<float>(band_index) * kRainbowTrailBandStep;
    const float dehors = 1.0F - static_cast<float>(band_index + 1) * kRainbowTrailBandStep;
    const std::array<std::uint8_t, 3> tint = EffectRainbowTrailBandTint(band_index);

    std::int32_t travelled = 0;
    std::int32_t rang = 0;
    while (travelled <= sweep_degrees) {
      const std::int32_t angle = WithinFullTurn(track.azimuth_degrees + travelled);
      const std::int32_t next = WithinFullTurn(track.azimuth_degrees + travelled + step);
      const float u_before = static_cast<float>(rang) / kRainbowTrailTextureSpan;
      const float u_after = static_cast<float>(rang + 1) / kRainbowTrailTextureSpan;

      EffectWorldQuad quad;
      quad.corners = {point(next, dedans), point(next, dehors), point(angle, dehors),
                      point(angle, dedans)};
      // ⚠ CETTE VOIE RANGE SES DEUX COORDONNÉES DANS L'AUTRE ORDRE que celle du
      // ruban : le bord de bande va en abscisse, le rang de segment en ordonnée.
      quad.texture_uv = {glm::vec2{1.0F, u_before}, glm::vec2{1.0F, u_after},
                         glm::vec2{0.0F, u_after}, glm::vec2{0.0F, u_before}};
      quad.tint = {tint[0], tint[1], tint[2], alpha};
      quad.blend_source = kEffectNodeBlendSource;
      quad.blend_destination = kAddedDestination;
      quads.push_back(quad);

      // ⚠ LE RANG DE TEXTURE S'ÉCRÊTE À VINGT, et le client le dit : un balayage
      // plus long que vingt pas reprendrait la dernière bande d'image jusqu'au
      // bout plutôt que de sortir de l'image.
      rang = std::min(rang + 1, kRainbowTrailSegments);
      travelled += step;
    }
  }
  return quads;
}

}  // namespace ro::render
