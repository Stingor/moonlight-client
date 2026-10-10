#include <cmath>
#include <numbers>

#include "ro/render/effect_particle_quad.h"
#include "ro/render/effect_scene.h"
#include "ro/render/effect_track.h"
#include "ro/render/effect_track_figures.h"
#include "ro/render/effect_trail_palette.h"

namespace ro::render {
namespace {

[[nodiscard]] float DegreesToRadians(float degrees) {
  return degrees * std::numbers::pi_v<float> / 180.0F;
}

// COMMENT LE RUBAN LIT SA PISTE.
//
// ⚠ ONZE POINTS, PAS VINGT ET UN. C'est la première des trois choses qui le
// séparent de la jupe d'anneau, dont il partage tout le reste : le pas angulaire
// vaut le balayage divisé par DIX, et les cases au-delà de la dixième ne sont
// jamais lues. Le client borne d'ailleurs son index à dix explicitement.
constexpr std::int32_t kLastSegment = 10;
constexpr std::size_t kSegmentCount = static_cast<std::size_t>(kLastSegment);

// La coordonnée de texture au travers du ruban, comme pour la jupe.
constexpr float kTextureAtInnerEdge = 1.0F;
constexpr float kTextureAtOuterEdge = 0.0F;

// ⚠ ET IL NE SE SOULÈVE PAS. La jupe compare sa clé à deux flottants exacts pour
// s'élever d'une ou deux unités ; ce dessinateur-ci ne porte AUCUNE de ces deux
// constantes. Lui prêter
// l'élévation de sa sœur décollerait du sol un ruban que le client y laisse.

// ⚠ L'ORDONNÉE DE L'ANCRE EST UNE SENTINELLE, pas une hauteur, et c'est l'ADR
// 0017 sous sa forme la plus nette. À cette valeur EXACTE, le client cesse de la
// lire comme une coordonnée : il prend l'abscisse et la profondeur de l'ancre,
// les TOURNE du cap que la troisième piste porte, et ajoute le résultat aux deux
// points. C'est ainsi qu'un ruban suit la caméra sans qu'aucun champ ne le dise.
//
// La lire comme une hauteur poserait le ruban à mille deux cent trente-quatre
// unités au-dessus du sol, c'est-à-dire hors de l'écran.
constexpr float kAnchorHeightThatMeansTurn = 1234.5F;
constexpr std::size_t kTrackThatCarriesTheTurn = 2;

}  // namespace

std::vector<EffectWorldQuad> ComposeEffectTrackRibbon(const EffectTrack& track,
                                                      std::uint8_t variant, glm::vec3 anchor,
                                                      std::int16_t turn_degrees,
                                                      const EffectRingOrientation& orientation) {
  std::vector<EffectWorldQuad> quads;
  // Un balayage négatif n'est pas un arc à l'envers : le client abandonne la
  // piste. Même règle que la jupe, et pour la même raison.
  if (!track.active || track.degrees < 0) {
    return quads;
  }

  const EffectTrailVariant tint = EffectTrailVariantFor(variant);
  const std::int16_t alpha = tint.forced_alpha != 0 ? tint.forced_alpha : track.alpha;
  if (alpha <= 0) {
    return quads;
  }

  const std::int32_t sweep_degrees = track.degrees;
  const std::int32_t step = sweep_degrees / kLastSegment;
  const float tilt = DegreesToRadians(static_cast<float>(track.tilt_degrees));
  const float upward = std::sin(tilt);
  const float vers_le_large = std::cos(tilt);

  // ⚠ LA SENTINELLE SE LIT UNE FOIS, AVANT LA BOUCLE. Le décalage qu'elle donne
  // est le même pour tous les points : le recalculer par point coûterait sans
  // rien changer, et le lire par point inviterait à croire qu'il varie.
  glm::vec3 shift{0.0F, 0.0F, 0.0F};
  if (track.anchor.y == kAnchorHeightThatMeansTurn) {
    const float cap = DegreesToRadians(static_cast<float>(turn_degrees));
    const float cosinus = std::cos(cap);
    const float sinus = std::sin(cap);
    // ⚠ L'ÉCART ET SON CAP SONT CEUX DU CLIENT — le cap est le tour de panneau
    // de SA caméra quand le mouvement l'y range — : l'écart tourné revient au
    // nôtre par `S`, qui en nie la profondeur.
    shift = EffectPointFromClient({cosinus * track.anchor.x - sinus * track.anchor.z, 0.0F,
                                   sinus * track.anchor.x + cosinus * track.anchor.z});
  }

  glm::vec3 previous_inner{0.0F, 0.0F, 0.0F};
  glm::vec3 previous_outer{0.0F, 0.0F, 0.0F};
  bool has_previous = false;
  std::int32_t travelled = 0;
  for (std::int32_t segment = 0; segment <= kLastSegment; ++segment) {
    // LE DERNIER SEGMENT REPREND L'ANGLE DE DÉPART quand le tour est entier,
    // comme la jupe : accumuler le pas y laisserait une fente.
    const bool referme = sweep_degrees == kEffectTrackFullTurnDegrees && segment == kLastSegment;
    const std::int32_t angle =
        referme ? track.azimuth_degrees
                : (track.azimuth_degrees + travelled) % kEffectTrackFullTurnDegrees;
    const float theta = DegreesToRadians(static_cast<float>(angle));
    const float vers_x = std::cos(theta);
    // LA PROFONDEUR SE RETOURNE AVEC L'ORDONNÉE : le bord extérieur monte déjà
    // dans notre repère, et n'en retourner qu'une ferait du ruban un miroir.
    const float vers_z = -std::sin(theta);

    const float length = track.value[static_cast<std::size_t>(segment)];
    const float offset = length * vers_le_large;
    const glm::vec3 inner = anchor + shift +
                            orientation * glm::vec4{track.radius_at_anchor * vers_x, 0.0F,
                                                    track.radius_at_anchor * vers_z, 1.0F};
    const glm::vec3 outer =
        anchor + shift +
        orientation * glm::vec4{(track.radius_at_anchor + offset) * vers_x, length * upward,
                                (track.radius_at_anchor + offset) * vers_z, 1.0F};

    if (has_previous) {
      const float u_before = static_cast<float>(segment - 1) / static_cast<float>(kSegmentCount);
      const float u_after = static_cast<float>(segment) / static_cast<float>(kSegmentCount);
      EffectWorldQuad quad;
      quad.corners = {previous_inner, inner, outer, previous_outer};
      quad.texture_uv = {
          glm::vec2{u_before, kTextureAtInnerEdge}, glm::vec2{u_after, kTextureAtInnerEdge},
          glm::vec2{u_after, kTextureAtOuterEdge}, glm::vec2{u_before, kTextureAtOuterEdge}};
      quad.tint = {tint.color[0], tint.color[1], tint.color[2], static_cast<std::uint8_t>(alpha)};
      quad.blend_source = kEffectNodeBlendSource;
      quad.blend_destination = tint.blend_destination;
      quads.push_back(quad);
    }

    previous_inner = inner;
    previous_outer = outer;
    has_previous = true;
    travelled += step;
    if (travelled > sweep_degrees) {
      break;
    }
  }
  return quads;
}

std::int16_t EffectRibbonTurnOf(const std::array<EffectTrack, kEffectTracksPerNode>& tracks) {
  return tracks[kTrackThatCarriesTheTurn].style;
}

}  // namespace ro::render
