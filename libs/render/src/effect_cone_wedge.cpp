#include "ro/render/effect_cone_wedge.h"

#include <algorithm>
#include <cmath>

#include "ro/render/effect_track.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

constexpr std::int16_t kAddedBlend = 0;
constexpr std::int16_t kOrdinaryBlend = 1;

// LES CINQ VARIANTES RELEVÉES, chacune avec les DEUX teintes de ses deux coins.
struct WedgeRow {
  float variant;
  std::array<std::uint8_t, 3> narrow;
  std::array<std::uint8_t, 3> wide;
  std::int16_t narrow_blend;
  std::int16_t wide_blend;
  bool flat;
};

constexpr std::array<WedgeRow, 5> kWedgeRows = {{
    {2.0F, {0, 0, 250}, {250, 250, 0}, kAddedBlend, kAddedBlend, false},
    {3.0F, {250, 250, 250}, {250, 250, 250}, kAddedBlend, kOrdinaryBlend, false},
    {4.0F, {0, 0, 250}, {250, 250, 0}, kAddedBlend, kOrdinaryBlend, false},
    {5.0F, {50, 50, 50}, {250, 250, 250}, kAddedBlend, kOrdinaryBlend, false},
    // ⚠ LA SEULE QUI APLATIT SA PROFONDEUR, et pour ses deux coins.
    {6.0F, {0, 0, 255}, {50, 50, 255}, kAddedBlend, kAddedBlend, true},
}};

// LE CAS PAR DÉFAUT DU CLIENT : un cœur cyan dans un bord rouge. Ce n'est pas une
// absence de teinte — c'est la variante la plus contrastée de toutes.
constexpr WedgeRow kDefaultRow = {0.0F,        {0, 250, 250}, {250, 0, 0},
                                  kAddedBlend, kAddedBlend,   false};

}  // namespace

EffectConeWedgeStyle EffectConeWedgeStyleFor(float variant) {
  const auto found = std::ranges::find(kWedgeRows, variant, &WedgeRow::variant);
  const WedgeRow& row = found == kWedgeRows.end() ? kDefaultRow : *found;
  return {row.narrow, row.wide, row.narrow_blend, row.wide_blend, row.flat};
}

std::vector<EffectWorldQuad> ComposeConeWedgeQuads(const EffectTrack& track, glm::vec3 node_origin,
                                                   const EffectOwnerTint& owner) {
  std::vector<EffectWorldQuad> quads;
  if (!track.active || track.alpha <= 0) {
    return quads;
  }

  const float variant_value = track.value[kSlotOfWedgeVariant];
  const EffectConeWedgeStyle style = EffectConeWedgeStyleFor(variant_value);
  const bool serre = variant_value == kConeWedgeTightVariant;
  const float radius = track.radius_at_anchor;
  const float apex = track.value[kSlotOfWedgeApexHeight];
  // L'AZIMUT FAIT PIVOTER le coin autour de l'axe vertical ; l'inclinaison dit
  // où il pointe.
  const float vers_x = std::cos(DegreesToRadians(static_cast<float>(track.azimuth_degrees)));
  const float vers_y = std::sin(DegreesToRadians(static_cast<float>(track.azimuth_degrees)));

  // UN COIN : deux points sur le cône, et l'apex répété. C'est un triangle que
  // la voie A transporte sous la forme d'un quad.
  const auto place = [&](std::int32_t ouverture, const std::array<std::uint8_t, 3>& tint,
                         std::int16_t blend) {
    const float before = static_cast<float>(
        WithinFullTurnOnce(static_cast<std::int32_t>(track.tilt_degrees) - ouverture));
    const float after = static_cast<float>(
        WithinFullTurnOnce(static_cast<std::int32_t>(track.tilt_degrees) + ouverture));
    const float reach_before = radius * std::cos(DegreesToRadians(before));
    const float reach_after = radius * std::cos(DegreesToRadians(after));
    // LES FORMULES SONT CELLES DU CLIENT, dans son repère : l'apex négatif du
    // semis s'y dresse au-dessus de l'ancre. Chaque coin revient au nôtre.
    const glm::vec3 vertex = EffectPointFromClient({0.0F, apex, 0.0F});

    EffectParticleQuadRequest request;
    request.corners = {
        EffectPointFromClient({vers_x * reach_before, vers_y * reach_before + apex,
                               radius * std::sin(DegreesToRadians(before))}),
        EffectPointFromClient({vers_x * reach_after, vers_y * reach_after + apex,
                               radius * std::sin(DegreesToRadians(after))}),
        vertex,
        vertex,
    };
    request.alpha = static_cast<std::uint8_t>(std::clamp<std::int16_t>(track.alpha, 0, 255));
    request.color = tint;
    request.blend_mode = blend;
    request.depth =
        style.flattens_depth ? EffectQuadDepth::kFrontmost : EffectQuadDepth::kProjected;
    quads.push_back(ComposeEffectParticleQuad(request, node_origin, owner));
  };

  place(serre ? kConeWedgeTightNarrowDegrees : kConeWedgeNarrowDegrees, style.narrow_color,
        style.narrow_blend);
  place(serre ? kConeWedgeTightWideDegrees : kConeWedgeWideDegrees, style.wide_color,
        style.wide_blend);
  return quads;
}

}  // namespace ro::render
