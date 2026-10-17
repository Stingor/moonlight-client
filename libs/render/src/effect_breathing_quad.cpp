#include "ro/render/effect_breathing_quad.h"

#include <algorithm>
#include <cmath>

#include "ro/render/effect_track.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

constexpr std::int16_t kAddedBlend = 0;
constexpr std::int16_t kOrdinaryBlend = 1;

// LES DOUZE STYLES RELEVÉS, dans l'ordre où le client les teste.
struct StyleRow {
  std::int32_t style;
  std::uint8_t red;
  std::uint8_t green;
  std::uint8_t blue;
  std::int16_t blend;
  std::int32_t layers;
  EffectQuadDepth depth = EffectQuadDepth::kProjected;
};

constexpr std::array<StyleRow, 12> kStyleRows = {{
    // ⚠ CELUI-CI SE MÉLANGE AU LIEU DE S'AJOUTER, avec le cent dix, et ce sont
    // les deux seuls. Les traiter comme les autres les rendrait deux fois trop
    // vifs sur un fond clair.
    {0, 255, 255, 255, kOrdinaryBlend, 1},
    {15, 35, 255, 35, kAddedBlend, 1},
    {17, 105, 105, 255, kAddedBlend, 1},
    {18, 35, 35, 255, kAddedBlend, 1},
    // Deux styles peignent DEUX fois le même quad : c'est ainsi qu'ils
    // renforcent leur trait sans changer de teinte.
    {100, 255, 255, 255, kAddedBlend, 2},
    {101, 255, 0, 0, kAddedBlend, 2},
    {102, 255, 255, 0, kAddedBlend, 1},
    {110, 255, 255, 255, kOrdinaryBlend, 1},
    {111, 55, 55, 255, kAddedBlend, 1},
    {112, 105, 225, 105, kAddedBlend, 1},
    {113, 5, 5, 255, kAddedBlend, 1, EffectQuadDepth::kFrontmost},
    {114, 155, 5, 5, kAddedBlend, 1, EffectQuadDepth::kFrontmost},
}};

// LES CINQ STYLES QUI LISENT L'AZIMUT, et la condition exacte du client : cent,
// cent dix ou au-delà, dix, zéro — et quatre, mais SEULEMENT quand la case de
// variante vaut aussi quatre.
constexpr std::int32_t kStyleThatNeedsMatchingVariant = 4;
constexpr std::int32_t kLowestStyleAtAzimuth = 110;

[[nodiscard]] bool ReadsAzimuth(std::int32_t style, float variant) {
  if (style == 100 || style >= kLowestStyleAtAzimuth || style == 10 || style == 0) {
    return true;
  }
  return style == kStyleThatNeedsMatchingVariant &&
         variant == static_cast<float>(kStyleThatNeedsMatchingVariant);
}

}  // namespace

EffectBreathingQuadStyle EffectBreathingQuadStyleFor(std::int32_t style, float variant) {
  const auto found = std::ranges::find(kStyleRows, style, &StyleRow::style);
  EffectBreathingQuadStyle rendered;
  if (found == kStyleRows.end()) {
    // LE CAS PAR DÉFAUT DU CLIENT : un carré blanc qui s'ajoute. Ce n'est pas une
    // absence — c'est ce que reçoivent la plupart des six identifiants relevés.
    rendered.color = {255, 255, 255};
    rendered.blend_mode = kAddedBlend;
    rendered.layers = 1;
  } else {
    rendered.color = {found->red, found->green, found->blue};
    rendered.blend_mode = found->blend;
    rendered.layers = found->layers;
    rendered.depth = found->depth;
  }
  rendered.starts_at_azimuth = ReadsAzimuth(style, variant);
  return rendered;
}

std::vector<EffectWorldQuad> ComposeBreathingQuad(const EffectTrack& track, std::int32_t style,
                                                  std::size_t rank, glm::vec3 node_origin,
                                                  const EffectOwnerTint& owner) {
  std::vector<EffectWorldQuad> quads;
  if (!track.active || track.alpha <= 0) {
    return quads;
  }

  const EffectBreathingQuadStyle look =
      EffectBreathingQuadStyleFor(style, track.value[kSlotOfBreathingVariant]);

  // ⚠ LE RAYON RESPIRE ENTRE QUATRE CINQUIÈMES ET UN, jamais au-delà : le client
  // ajoute UN au sinus avant de le multiplier, si bien que le terme oscillant
  // reste positif. Oublier ce « plus un » ferait battre le carré autour de sa
  // taille de repos au lieu de la frôler.
  const float breath = std::sin(DegreesToRadians(static_cast<float>(track.tilt_degrees)));
  const float radius = track.radius_at_anchor * kBreathingQuadSteadyPart +
                       (breath + 1.0F) * (track.radius_at_anchor * kBreathingQuadSwingPart);
  // ⚠ NOTRE REPÈRE MONTE, CELUI DU CLIENT DESCEND : la hauteur change de signe.
  // Les semis la rangent telle que le client l'écrit — négative pour un carré
  // AU-DESSUS de son ancre. Recopiée sans la nier, elle couche le carré SOUS le
  // sol, où l'épreuve de profondeur l'efface tout entier.
  // ⚠ ET LA PROFONDEUR SE RETOURNE AVEC ELLE : chaque coin passe entier dans
  // notre repère. N'en retourner qu'une ferait du carré le miroir du sien.
  const float client_height = track.value[kSlotOfBreathingHeight];

  const std::int32_t start = look.starts_at_azimuth
                                 ? track.azimuth_degrees
                                 : kBreathingQuadDegreesPerTrack * static_cast<std::int32_t>(rank);

  std::array<glm::vec3, 4> corners{};
  for (std::size_t corner = 0; corner < corners.size(); ++corner) {
    const std::int32_t angle =
        (start + kBreathingQuadCornerDegrees * static_cast<std::int32_t>(corner)) %
        kEffectTrackFullTurnDegrees;
    const float theta = DegreesToRadians(static_cast<float>(angle));
    corners[corner] =
        EffectPointFromClient({radius * std::cos(theta), client_height, radius * std::sin(theta)});
  }

  quads.reserve(static_cast<std::size_t>(look.layers));
  for (std::int32_t layer = 0; layer < look.layers; ++layer) {
    EffectParticleQuadRequest request;
    request.corners = corners;
    request.alpha = static_cast<std::uint8_t>(std::clamp<std::int16_t>(track.alpha, 0, 255));
    request.color = look.color;
    request.blend_mode = look.blend_mode;
    request.depth = look.depth;
    quads.push_back(ComposeEffectParticleQuad(request, node_origin, owner));
  }
  return quads;
}

}  // namespace ro::render
