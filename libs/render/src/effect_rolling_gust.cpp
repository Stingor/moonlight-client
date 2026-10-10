#include "ro/render/effect_rolling_gust.h"

#include <cstddef>

#include "ro/render/effect_track.h"

#include "effect_ground_gust_motion.h"
#include "effect_track_angles.h"

namespace ro::render {
namespace {

// L'ALPHA DU 126 : plus six jusqu'au dixième tic, plafond soixante, moins trois
// ensuite.
constexpr GroundGustLineProfile kRollingGustLineProfile = {10, 6, 60, 3};

}  // namespace

std::optional<EffectWorldQuad> ComposeRollingGustQuad(const EffectTrack& track,
                                                      const EffectOwnerTint& owner) {
  if (!track.active || track.alpha <= 0) {
    return std::nullopt;
  }
  // LE PLAN DE LA ROUE est celui de son cap, déjà dans notre repère.
  const float cap_x = CosOfDegrees(track.azimuth_degrees);
  const float cap_z = SinOfDegrees(track.azimuth_degrees);

  EffectParticleQuadRequest request;
  for (std::size_t corner = 0; corner < kRollingGustCorners; ++corner) {
    // ⚠ LE PREMIER COIN EST À UN QUART DE TOUR de l'inclinaison, comme chez
    // la bourrasque couchée : le client ajoute quatre-vingt-dix degrés avant de
    // poser A.
    const std::int32_t angle = WithinFullTurn(
        track.tilt_degrees + static_cast<std::int32_t>(corner + 1) * kRollingGustQuarterTurn);
    const float le_long = track.radius_at_anchor * CosOfDegrees(angle);
    // ⚠ LA HAUTEUR SE RETRANCHE : le client AJOUTE `rayon × sin` à son
    // ordonnée, qui descend. La recopier telle quelle tournerait la roue à
    // l'envers — ce qui, pour un carré, se voit à sa texture retournée.
    const float upward = track.radius_at_anchor * SinOfDegrees(angle);
    request.corners[corner] = {track.anchor.x + cap_x * le_long, track.anchor.y - upward,
                               track.anchor.z + cap_z * le_long};
  }
  request.absolute = true;
  request.alpha = static_cast<std::uint8_t>(track.alpha);
  request.color = kRollingGustColor;
  request.blend_mode = kRollingGustBlend;
  // ⚠ LES MÊMES QUATRE NOMBRES DE TEXTURE QUE LA BOURRASQUE (0, 1, 0, 1 aux
  // paramètres vingt-deux à vingt-cinq) : l'image est retournée de la même
  // façon.
  request.v_at_a_and_b = 1.0F;
  request.v_at_c_and_d = 0.0F;
  return ComposeEffectParticleQuad(request, glm::vec3{0.0F, 0.0F, 0.0F}, owner);
}

void AdvanceRollingGustTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks) {
  AdvanceGroundGustLine(tracks, kRollingGustLineProfile);
}

}  // namespace ro::render
