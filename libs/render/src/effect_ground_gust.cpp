#include "ro/render/effect_ground_gust.h"

#include <algorithm>
#include <cstddef>

#include "ro/render/effect_track.h"

#include "effect_ground_gust_motion.h"
#include "effect_track_angles.h"

namespace ro::render {
namespace {

// CE QUE LE MOUVEMENT DU SOUS-TYPE 119 POSE.

// De combien le carré tourne par tic.
constexpr std::int16_t kTurnStepDegrees = 30;
// L'ALPHA DU 119 : il monte de vingt-cinq jusqu'au dixième tic, plafonne à
// deux cent cinquante, puis retombe de cinq.
constexpr GroundGustLineProfile kGroundGustLineProfile = {10, 25, 250, 5};
// De combien la vitesse enfle, pendant la montée puis après elle.
constexpr float kSpeedGrowthWhileRising = 1.02F;
constexpr float kSpeedGrowthWhileFading = 1.04F;
// LE TIC OÙ LE CAP SE PREND, et une seule fois.
constexpr std::int32_t kHeadingTick = 1;

// L'ÉCHO : tous les combien de tics il passe, et ce qu'il retient de l'alpha.
constexpr std::int32_t kEchoPeriodTicks = 3;
constexpr float kEchoAlphaShare = 0.8F;

}  // namespace

std::array<std::uint8_t, 3> EffectGroundGustColor(std::uint8_t mark) {
  return kGroundGustPalette[mark];
}

std::int16_t EffectGroundGustBlendMode(std::uint8_t mark) {
  return mark < kGroundGustFirstOrdinaryColor ? kGroundGustAddedBlend : kGroundGustOrdinaryBlend;
}

std::optional<EffectWorldQuad> ComposeGroundGustQuad(const EffectTrack& track,
                                                     const EffectOwnerTint& owner) {
  if (!track.active || track.alpha <= 0) {
    return std::nullopt;
  }
  const std::uint8_t tint = track.mark[kGroundGustMarkOfColor];
  // ⚠ AU-DELÀ DES QUATRE TEINTES, LE CLIENT NE DESSINE RIEN : son aiguillage
  // n'a pas de cas par défaut.
  if (tint >= kGroundGustColors) {
    return std::nullopt;
  }

  EffectParticleQuadRequest request;
  for (std::size_t corner = 0; corner < kGroundGustCorners; ++corner) {
    // ⚠ LE PREMIER COIN EST DÉJÀ À UN QUART DE TOUR de l'inclinaison : le
    // client ajoute quatre-vingt-dix degrés avant de poser A, puis autant
    // entre chaque coin. Partir de l'inclinaison nue tournerait le carré d'un
    // huitième de tour.
    const std::int32_t angle = WithinFullTurn(
        track.tilt_degrees + static_cast<std::int32_t>(corner + 1) * kGroundGustQuarterTurn);
    // LES QUATRE COINS SONT À LA MÊME HAUTEUR : la figure est COUCHÉE.
    // ⚠ L'ANCRE EST DANS NOTRE REPÈRE, l'écart au coin s'écrit dans celui du
    // client : sa profondeur se retourne. La garder ferait tourner le carré à
    // l'envers.
    request.corners[corner] = {track.anchor.x + track.radius_at_anchor * CosOfDegrees(angle),
                               track.anchor.y,
                               track.anchor.z - track.radius_at_anchor * SinOfDegrees(angle)};
  }
  // ⚠ ABSOLUS : l'ancre porte déjà la position du monde.
  request.absolute = true;
  request.alpha = static_cast<std::uint8_t>(track.alpha);
  request.color = EffectGroundGustColor(tint);
  request.blend_mode = EffectGroundGustBlendMode(tint);
  // ⚠ L'IMAGE EST RETOURNÉE : le client donne un aux deux premiers coins et
  // zéro aux deux derniers, là où le défaut fait l'inverse.
  request.v_at_a_and_b = 1.0F;
  request.v_at_c_and_d = 0.0F;
  return ComposeEffectParticleQuad(request, glm::vec3{0.0F, 0.0F, 0.0F}, owner);
}

void AdvanceGroundGustTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks) {
  AdvanceGroundGustLine(tracks, kGroundGustLineProfile);
}

void AdvanceGroundGustLine(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                           const GroundGustLineProfile& profile) {
  EffectTrack& leading_track = tracks.front();
  // ⚠ LA PISTE ZÉRO COMMANDE LES QUATRE : sa mort arrête le nœud entier, échos
  // compris. C'est ce qui les fige au dernier alpha qu'ils ont reçu.
  if (!leading_track.active) {
    return;
  }
  ++leading_track.ticks;
  if (leading_track.ticks <= 0) {
    return;
  }

  leading_track.tilt_degrees =
      static_cast<std::int16_t>(WithinFullTurn(leading_track.tilt_degrees + kTurnStepDegrees));

  // ⚠ LE TIC DU CAP NE TOUCHE NI L'ALPHA NI LA VITESSE. Le client y branche
  // ailleurs : rien ne se montre encore, puisque l'alpha est nul.
  if (leading_track.ticks != kHeadingTick) {
    if (leading_track.ticks <= profile.rise_until_tick) {
      leading_track.alpha = std::min<std::int16_t>(
          static_cast<std::int16_t>(leading_track.alpha + profile.rise), profile.ceiling);
      leading_track.value[kGroundGustSlotOfSpeed] *= kSpeedGrowthWhileRising;
    } else {
      leading_track.alpha = static_cast<std::int16_t>(leading_track.alpha - profile.fall);
      leading_track.value[kGroundGustSlotOfSpeed] *= kSpeedGrowthWhileFading;
      if (leading_track.alpha < 0) {
        leading_track.alpha = 0;
        leading_track.active = false;
      }
    }
  }

  DriftGroundGustLeader(leading_track);
  EchoGroundGustTrail(tracks);
}

void DriftGroundGustLeader(EffectTrack& leader) {
  // LA DÉRIVE : l'ancre file le long du cap, à plat. Sa hauteur ne bouge pas.
  const float speed = leader.value[kGroundGustSlotOfSpeed];
  leader.anchor.x += speed * CosOfDegrees(leader.azimuth_degrees);
  leader.anchor.z += speed * SinOfDegrees(leader.azimuth_degrees);
}

void EchoGroundGustTrail(std::array<EffectTrack, kEffectTracksPerNode>& tracks) {
  if (tracks.front().ticks % kEchoPeriodTicks != 0) {
    return;
  }
  // ⚠ DANS L'ORDRE DESCENDANT : la piste trois reçoit la deux avant que la
  // deux ne reçoive la une. Remonter l'ordre donnerait aux trois échos la
  // même image.
  for (std::size_t rang = kEffectTracksPerNode - 1; rang > 0; --rang) {
    const EffectTrack& source = tracks[rang - 1];
    EffectTrack& echo = tracks[rang];
    echo.anchor = source.anchor;
    echo.tilt_degrees = source.tilt_degrees;
    echo.alpha = static_cast<std::int16_t>(static_cast<float>(source.alpha) * kEchoAlphaShare);
  }
}

}  // namespace ro::render
