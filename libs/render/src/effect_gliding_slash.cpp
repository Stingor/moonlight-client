#include "ro/render/effect_gliding_slash.h"

#include "ro/render/effect_particle_quad.h"
#include "ro/render/effect_track.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

// LA MARQUE QUI PORTE LA TEINTE : la SECONDE, pas la première.
constexpr std::size_t kSlotOfPaintMark = 1;

// LE MÉLANGE de cette figure : l'alpha de la source, pour les trois marques.
constexpr std::int16_t kBlendBySourceAlpha = 1;

}  // namespace

std::array<std::uint8_t, 3> EffectGlidingSlashColor(std::uint8_t mark) {
  if (mark == kGlidingSlashAnimatedMark) {
    return kGlidingSlashPale;
  }
  if (mark == kGlidingSlashGreenMark) {
    return kGlidingSlashGreen;
  }
  return kGlidingSlashPlain;
}

std::size_t EffectGlidingSlashFrame(std::uint8_t mark, std::int32_t ticks) {
  // ⚠ LE COMPTEUR EST NÉGATIF PENDANT LE RETARD, et le client le traite à
  // part : il rend la PREMIÈRE image plutôt qu'un index négatif.
  if (mark != kGlidingSlashAnimatedMark || ticks <= 0) {
    return 0;
  }
  return static_cast<std::size_t>(ticks / kGlidingSlashTicksPerFrame) % kGlidingSlashFrameCount;
}

std::optional<EffectWorldQuad> ComposeGlidingSlashQuad(const EffectTrack& track) {
  if (!track.active || track.alpha <= 0) {
    return std::nullopt;
  }
  const std::uint8_t style_mark = track.mark[kSlotOfPaintMark];

  EffectParticleQuadRequest request;
  for (std::size_t corner = 0; corner < request.corners.size(); ++corner) {
    // ⚠ L'AZIMUT EST NÔTRE, l'opposé de celui du client : le huitième de tour
    // et le pas d'un coin au suivant, qu'il ajoute, se RETRANCHENT ici. Les
    // ajouter ferait tourner les coins à rebours et retournerait l'image.
    const std::int32_t angle =
        WithinFullTurn(track.azimuth_degrees - kGlidingSlashFirstCornerDegrees -
                       kGlidingSlashDegreesPerCorner * static_cast<std::int32_t>(corner));
    // ⚠ LES QUATRE COINS SONT À LA MÊME HAUTEUR : le carré est PLAT, et c'est
    // son ancre qui porte sa hauteur. Y mettre zéro le poserait au sol.
    request.corners[corner] = {track.anchor.x + track.radius_at_anchor * CosOfDegrees(angle),
                               track.anchor.y,
                               track.anchor.z + track.radius_at_anchor * SinOfDegrees(angle)};
  }
  request.absolute = true;
  request.alpha = static_cast<std::uint8_t>(track.alpha);
  request.color = EffectGlidingSlashColor(style_mark);
  request.texture = EffectGlidingSlashFrame(style_mark, track.ticks);
  request.blend_mode = kBlendBySourceAlpha;
  // ⚠ L'IMAGE EST RETOURNÉE : le client donne un aux deux premiers coins et
  // zéro aux deux derniers, là où le défaut fait l'inverse.
  request.v_at_a_and_b = 1.0F;
  request.v_at_c_and_d = 0.0F;
  return ComposeEffectParticleQuad(request, {});
}

}  // namespace ro::render
