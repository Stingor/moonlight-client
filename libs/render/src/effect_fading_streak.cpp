#include "ro/render/effect_fading_streak.h"

#include <algorithm>
#include <cmath>

#include "ro/render/effect_quad_rotation.h"
#include "ro/render/effect_track.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

// LE MÉLANGE : zéro est l'ADDITION, dans la numérotation de la voie A.
constexpr std::int16_t kAddedBlend = 0;

// LES QUATRE TEINTES, par marque. Le bleu est toujours plein.
constexpr std::uint8_t kFullBlue = 255;
constexpr std::array<std::uint8_t, 4> kWhiteness = {55, 105, 75, 85};

// LES TROIS FORMES DE QUATRE COINS, et les écarts angulaires de chacune. Le
// premier coin est toujours à l'angle de la piste ; les trois autres suivent.
//
// ⚠ CE NE SONT PAS DES ÉCARTS ÉGAUX sauf pour la première : les deux autres
// font des LOSANGES, plus larges d'un côté. Les uniformiser rendrait trois
// carrés là où le client en montre un et deux losanges.
constexpr std::array<std::array<std::int32_t, 3>, 3> kCornerSteps = {{
    {90, 90, 90},
    {60, 120, 60},
    {130, 50, 130},
}};

}  // namespace

bool EffectFadingStreakTintFor(std::uint8_t mark, std::array<std::uint8_t, 3>& tint) {
  if (mark >= kWhiteness.size()) {
    // ⚠ PAS DE CAS PAR DÉFAUT CHEZ LE CLIENT : son choix ne couvre que ces
    // quatre marques, et la piste ne s'émet alors pas du tout.
    return false;
  }
  const std::uint8_t blancheur = kWhiteness[mark];
  tint = {blancheur, blancheur, kFullBlue};
  return true;
}

std::vector<EffectWorldQuad> ComposeFadingStreakQuads(
    const EffectTrack& track, std::int16_t turn_degrees, std::int16_t tilt_degrees,
    glm::vec3 node_origin, std::uint8_t shared_tint_mark, std::uint8_t shared_speed_key,
    const EffectOwnerTint& owner) {
  std::vector<EffectWorldQuad> quads;
  if (!track.active || track.alpha <= 0) {
    return quads;
  }
  std::array<std::uint8_t, 3> tint_color{};
  if (!EffectFadingStreakTintFor(shared_tint_mark, tint_color)) {
    return quads;
  }
  if (shared_speed_key == 0) {
    // ⚠ LA CLÉ DIVISE, et le client ne s'en garde pas : à zéro il diviserait par
    // zéro. Aucun bâtisseur n'en pose une nulle — celui du sous-type 134 tire
    // entre quarante et soixante —, et rendre un lot vide est la seule
    // transcription qui ne produise pas d'infini.
    return quads;
  }

  // ⚠ LE RAYON RESPIRE AUTOUR DE LUI-MÊME, il ne se multiplie pas : le client
  // AJOUTE un vingtième de son sinus. À case nulle, la respiration est nulle.
  const auto breath = static_cast<std::int32_t>(track.value[kSlotOfFadingStreakBreath]);
  const float half =
      track.radius_at_anchor + track.radius_at_anchor * SinOfDegrees(breath) * kFadingStreakBreath;

  const auto forme = static_cast<std::size_t>(track.value[kSlotOfFadingStreakShape]);
  const std::array<std::int32_t, 3>& ecarts =
      kCornerSteps[std::min(forme, kCornerSteps.size() - 1)];

  std::array<glm::vec3, 4> corners{};
  std::int32_t angle = track.degrees;
  for (std::size_t corner = 0; corner < corners.size(); ++corner) {
    if (corner > 0) {
      angle = WithinFullTurn(angle - ecarts[corner - 1]);
    }
    corners[corner] = {half * CosOfDegrees(angle), 0.0F, half * SinOfDegrees(angle)};
  }
  RotateEffectQuadCorners(corners, turn_degrees, tilt_degrees);

  // LA DISTANCE JUSQU'À LA CIBLE, à plat : c'est elle que le compteur de tics
  // parcourt, divisée par la clé.
  const float vers_x = track.anchor.x - node_origin.x;
  const float vers_z = track.anchor.z - node_origin.z;
  const float reach = std::sqrt(vers_x * vers_x + vers_z * vers_z);
  const float per_tick = reach / static_cast<float>(shared_speed_key);
  const float cap_x = CosOfDegrees(track.tilt_degrees);
  const float cap_z = SinOfDegrees(track.tilt_degrees);

  for (std::size_t copie = 0; copie < kFadingStreakCopies; ++copie) {
    const float advance = std::max(0.0F, per_tick * static_cast<float>(track.ticks) -
                                             static_cast<float>(copie) * kFadingStreakCopyGap);
    // ⚠ LE CENTRE EST DANS NOTRE REPÈRE, qui monte : la montée s'y AJOUTE. Le
    // cap, lui, a été tiré de nos positions au semis.
    const glm::vec3 centre{node_origin.x + advance * cap_x, node_origin.y + kFadingStreakRise,
                           node_origin.z + advance * cap_z};

    // ⚠ LA PREMIÈRE COPIE GARDE TOUT SON ALPHA, et les quatre autres en perdent
    // cent vingt d'un coup, puis vingt de plus chacune. Ce n'est pas un dégradé
    // régulier : la tête est franche et la queue presque éteinte.
    std::int16_t alpha = track.alpha;
    if (copie > 0) {
      alpha = static_cast<std::int16_t>(
          alpha -
          kFadingStreakFadeUnit * (kFadingStreakFadeBase + static_cast<std::int16_t>(copie)));
      if (alpha < 0) {
        alpha = 0;
      }
    }

    EffectParticleQuadRequest request;
    for (std::size_t corner = 0; corner < request.corners.size(); ++corner) {
      // Le carré, tourné par les angles de la caméra du client, revient au
      // nôtre par `S` entier avant de rejoindre son centre.
      request.corners[corner] = EffectPointFromClient(corners[corner]) + centre;
    }
    request.alpha = static_cast<std::uint8_t>(std::clamp<std::int16_t>(alpha, 0, 255));
    request.color = tint_color;
    request.blend_mode = kAddedBlend;
    request.texture = track.mark[kMarkOfFadingStreakImage];
    // ⚠ ABSOLUS : les coins portent déjà un point du monde.
    request.absolute = true;
    quads.push_back(ComposeEffectParticleQuad(request, glm::vec3{0.0F, 0.0F, 0.0F}, owner));
  }
  return quads;
}

}  // namespace ro::render
