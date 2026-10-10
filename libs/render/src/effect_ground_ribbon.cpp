#include "ro/render/effect_ground_ribbon.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "ro/render/effect_track.h"

namespace ro::render {
namespace {

[[nodiscard]] float DegreesToRadians(float degrees) {
  return degrees * std::numbers::pi_v<float> / 180.0F;
}

constexpr std::int16_t kAddedBlend = 0;
constexpr std::int16_t kOrdinaryBlend = 1;

// LES DIX-HUIT STYLES RELEVÉS, dans l'ordre du `switch` du client.
//
// ⚠ CE N'EST PAS UNE PALETTE RÉGULIÈRE. Trois familles de trois teintes par
// paliers de vingt sur un canal — des bleus, des jaunes, des verts —, puis NEUF
// couleurs libres qui ne suivent aucune règle. Les déduire les unes des autres
// en inventerait six.
struct RibbonRow {
  std::uint8_t style;
  std::uint8_t red;
  std::uint8_t green;
  std::uint8_t blue;
};

constexpr std::array<RibbonRow, 18> kRibbonRows = {{
    {0, 225, 225, 255},
    {1, 205, 205, 255},
    {2, 185, 185, 255},
    {10, 255, 255, 225},
    {11, 255, 255, 205},
    {12, 255, 255, 185},
    {20, 225, 255, 225},
    {21, 205, 255, 205},
    {22, 185, 255, 185},
    {30, 105, 105, 255},
    {31, 65, 65, 255},
    {32, 25, 25, 255},
    {33, 103, 33, 177},
    {34, 66, 1, 104},
    {35, 178, 136, 236},
    {36, 241, 225, 178},
    {37, 202, 77, 65},
    {38, 148, 79, 73},
}};

// LE CAS SANS MARQUE, que le client traite à part de son `switch` : le même bleu
// pâle que le style zéro. Ce n'est pas une absence, c'est une branche.
constexpr RibbonRow kUnmarkedRow = {0, 225, 225, 255};

// UN CAP, retourné d'un demi-tour et ramené dans le tour — le geste exact du
// client, qui ajoute ou retranche selon le côté où il se trouve.
[[nodiscard]] float CapRetourne(std::int32_t cap) {
  const std::int32_t oppose = cap < kEffectTrackHalfTurnDegrees ? cap + kEffectTrackHalfTurnDegrees
                                                                : cap - kEffectTrackHalfTurnDegrees;
  return DegreesToRadians(static_cast<float>(oppose));
}

}  // namespace

EffectGroundRibbonStyle EffectGroundRibbonStyleFor(std::uint8_t style) {
  const auto found = std::ranges::find(kRibbonRows, style, &RibbonRow::style);
  const RibbonRow& row = found == kRibbonRows.end() ? kUnmarkedRow : *found;
  const bool additif = style >= kGroundRibbonAddedMark && found != kRibbonRows.end();
  return {{row.red, row.green, row.blue}, additif ? kAddedBlend : kOrdinaryBlend, additif};
}

std::optional<EffectWorldQuad> ComposeGroundRibbonQuad(const EffectTrack& track,
                                                       glm::vec3 node_origin,
                                                       const EffectOwnerTint& owner) {
  if (!track.active || track.alpha <= 0) {
    return std::nullopt;
  }

  const std::int32_t cap = track.tilt_degrees;
  const float half_width = track.lift_key;
  const float de_cote = track.value[kSlotOfRibbonSideways];
  const float near_point = track.value[kSlotOfRibbonNearEnd];
  const float far_point = near_point + track.value[kSlotOfRibbonSpan];
  const float mesure = track.radius_at_anchor;

  // ⚠ LA LARGEUR SE PREND À QUATRE-VINGT-DIX DEGRÉS DU CAP, et le client choisit
  // le sens selon le côté : plus un quart de tour sous quatre-vingt-dix, moins
  // trois quarts au-delà. Les deux donnent la même direction ; c'est sa table de
  // sinus qu'il ménage, pas la géométrie.
  const std::int32_t travers = cap >= 90 ? cap - 90 : cap + 270;
  const float vers_x = std::cos(DegreesToRadians(static_cast<float>(travers)));
  const float vers_z = std::sin(DegreesToRadians(static_cast<float>(travers)));
  // Le décalage latéral, puis un demi-pas de largeur de part et d'autre.
  const glm::vec2 base{de_cote * vers_x, de_cote * vers_z};
  const glm::vec2 edge{base.x + half_width * vers_x, base.y + half_width * vers_z};

  // ⚠ CHAQUE EXTRÉMITÉ CHOISIT SON SENS AU SEUIL DE CENT : au-dessus elle
  // s'écarte dans la direction du cap, en dessous le cap est retourné ET la
  // soustraction inversée. Les traiter pareil replierait le ruban.
  const auto carries = [&](float extremite) {
    const bool forward = extremite >= kGroundRibbonForwardThreshold;
    const float length = forward ? extremite - mesure : mesure - extremite;
    const float angle = forward ? DegreesToRadians(static_cast<float>(cap)) : CapRetourne(cap);
    return glm::vec2{length * std::cos(angle), length * std::sin(angle)};
  };
  const glm::vec2 near_carried = carries(near_point);
  const glm::vec2 far_carried = carries(far_point);

  // ⚠ LE CAP, LA HAUTEUR ET LES FORMULES SONT CEUX DU CLIENT, dans son repère :
  // chaque coin revient au nôtre. Sans cela le ruban se couchait sous le sol,
  // et pointait à l'opposé de sa cible dès qu'elle quittait l'axe des x.
  const auto corner = [&](const glm::vec2& cote, const glm::vec2& tip) {
    return EffectPointFromClient({cote.x + tip.x, kGroundRibbonHeight, cote.y + tip.y});
  };

  const EffectGroundRibbonStyle look = EffectGroundRibbonStyleFor(track.mark[kMarkOfRibbonStyle]);

  EffectParticleQuadRequest request;
  request.corners = {corner(edge, far_carried), corner(edge, near_carried),
                     corner(base, near_carried), corner(base, far_carried)};
  request.alpha = static_cast<std::uint8_t>(std::clamp<std::int16_t>(track.alpha, 0, 255));
  request.color = look.color;
  request.blend_mode = look.blend_mode;
  request.depth = look.flattens_depth ? EffectQuadDepth::kFrontmost : EffectQuadDepth::kProjected;
  return ComposeEffectParticleQuad(request, node_origin, owner);
}

}  // namespace ro::render
