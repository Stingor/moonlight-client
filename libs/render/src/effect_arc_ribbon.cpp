#include "ro/render/effect_arc_ribbon.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "ro/render/effect_particle_quad.h"
#include "ro/render/effect_quad_rotation.h"
#include "ro/render/effect_track.h"

namespace ro::render {
namespace {

// COMMENT L'ARC DE RUBAN LIT SA PISTE. Les cases n'ont pas de sens en
// elles-mêmes — voir `effect_track.h` — et c'est ici que cette figure donne le
// sien aux siennes.
constexpr std::size_t kSlotOfSpinDegrees = 8;
constexpr std::size_t kSlotOfShape = 10;
constexpr std::size_t kMarkOfTexture = 0;
constexpr std::size_t kMarkOfStyle = 2;

// DE COMBIEN LE RAYON RESPIRE, au rythme de l'angle propre de la piste.
constexpr float kRadiusBreathAmplitude = 0.050000001F;

// LES TROIS JEUX D'ÉCARTS entre coins, en degrés, et la case qui les choisit.
// Le quatrième côté prend ce qui reste du tour : c'est lui, l'arc.
constexpr std::size_t kCornerCount = 4;
constexpr std::size_t kGapCount = kCornerCount - 1;
constexpr std::array<std::int32_t, kGapCount> kGapsOfSquare = {90, 90, 90};
constexpr std::array<std::int32_t, kGapCount> kGapsOfKite = {60, 120, 60};
constexpr std::array<std::int32_t, kGapCount> kGapsOfFlatKite = {130, 50, 130};

// LES DEUX DÉCALAGES QUE CERTAINES MARQUES AJOUTENT. Le premier pousse la figure
// de côté, le second la soulève ; la marque trois prend les DEUX.
constexpr float kNudgeAside = 3.0999999F;
constexpr float kLift = 6.0F;
constexpr std::uint8_t kMarkThatNudgesAside = 3;
constexpr std::uint8_t kFirstMarkThatLifts = 17;
constexpr std::uint8_t kLastMarkThatLifts = 31;

// L'image entière, dans l'ordre où le client range ses quatre nombres.
constexpr float kTextureAtLeft = 0.0F;
constexpr float kTextureAtRight = 1.0F;
constexpr float kTextureAtNear = 1.0F;
constexpr float kTextureAtFar = 0.0F;

// LA PILE DE COULEURS QUE LE CLIENT ENCHAÎNE, de la plus haute marque à la plus
// basse. Une marque prend cette pile À PARTIR de son rang : la quinze n'en prend
// que la dernière, la deux les prend toutes.
constexpr std::size_t kCascadeDepth = 6;
constexpr std::array<EffectArcRibbonLayer, kCascadeDepth> kCascade = {{
    {{130, 130, 250}, 0},
    {{255, 255, 255}, 0},
    {{125, 255, 125}, 0},
    {{150, 150, 150}, 0},
    {{50, 50, 50}, 0},
    {{51, 153, 153}, 0},
}};

// LES MARQUES QUI NE PASSENT PAS PAR LA PILE, chacune avec ses couches.
constexpr std::array<EffectArcRibbonLayer, 1> kPlainWhiteOverwrite = {{{{255, 255, 255}, 1}}};
constexpr std::array<EffectArcRibbonLayer, 1> kPlainWhite = {{{{255, 255, 255}, 0}}};
constexpr std::array<EffectArcRibbonLayer, 1> kPlainRed = {{{{255, 5, 5}, 0}}};
constexpr std::array<EffectArcRibbonLayer, 2> kBlueThenPale = {
    {{{5, 5, 255}, 0}, {{155, 155, 255}, 0}}};
constexpr std::array<EffectArcRibbonLayer, 2> kGreenThenPale = {
    {{{5, 255, 5}, 0}, {{155, 255, 155}, 0}}};
constexpr std::array<EffectArcRibbonLayer, 2> kRedThenPale = {
    {{{255, 5, 5}, 0}, {{255, 155, 155}, 0}}};

[[nodiscard]] float RadiansOfDegrees(std::int32_t degrees) {
  return static_cast<float>(degrees) * std::numbers::pi_v<float> / 180.0F;
}

[[nodiscard]] std::int32_t WithinFullTurn(std::int32_t degrees) {
  const std::int32_t remainder = degrees % kEffectTrackFullTurnDegrees;
  return remainder < 0 ? remainder + kEffectTrackFullTurnDegrees : remainder;
}

// Le jeu d'écarts que la piste demande. Le carré par défaut : le bâtisseur ne
// pose que trois valeurs, et toute autre laisserait les quatre coins au même
// angle — un quad sans aire.
[[nodiscard]] std::span<const std::int32_t> GapsOf(float shape) {
  if (shape == 1.0F) {
    return kGapsOfKite;
  }
  if (shape == 2.0F) {
    return kGapsOfFlatKite;
  }
  return kGapsOfSquare;
}

}  // namespace

EffectArcRibbonStyle EffectArcRibbonStyleFor(std::uint8_t mark) {
  switch (mark) {
    case 0:
    case 3:
      // LES DEUX SEULES QUI GARDENT L'ORDRE DES COINS, et qui écrasent au lieu
      // de s'ajouter.
      return {kPlainWhiteOverwrite, false, false};
    case 16:
      return {kPlainWhiteOverwrite, true, true};
    case 17:
      return {kPlainWhite, true, false};
    case 32:
      return {kPlainRed, true, false};
    case 18:
      return {kBlueThenPale, true, false};
    case 19:
      return {kGreenThenPale, true, false};
    case 20:
      return {kRedThenPale, true, false};
    case 2:
      return {kCascade, true, false};
    case 4:
      return {std::span<const EffectArcRibbonLayer>(kCascade).subspan(1), true, false};
    case 5:
      return {std::span<const EffectArcRibbonLayer>(kCascade).subspan(2), true, false};
    case 10:
      return {std::span<const EffectArcRibbonLayer>(kCascade).subspan(3), true, false};
    case 12:
      return {std::span<const EffectArcRibbonLayer>(kCascade).subspan(4), true, false};
    case 15:
      return {std::span<const EffectArcRibbonLayer>(kCascade).subspan(5), true, false};
    default:
      // DIX-NEUF MARQUES NE DESSINENT RIEN, et c'est une réponse.
      return {};
  }
}

std::vector<EffectWorldQuad> ComposeArcRibbonQuads(const EffectTrack& track,
                                                   std::int16_t billboard_turn_degrees,
                                                   std::int16_t billboard_tilt_degrees,
                                                   glm::vec3 node_origin) {
  std::vector<EffectWorldQuad> quads;
  if (!track.active || track.alpha <= 0) {
    return quads;
  }
  const std::uint8_t style_mark = track.mark[kMarkOfStyle];
  const EffectArcRibbonStyle style = EffectArcRibbonStyleFor(style_mark);
  if (style.layers.empty()) {
    return quads;
  }

  // LE RAYON RESPIRE au rythme de l'angle propre de la piste.
  const std::int32_t own =
      WithinFullTurn(static_cast<std::int32_t>(track.value[kSlotOfSpinDegrees]));
  const float radius = track.radius_at_anchor + track.radius_at_anchor *
                                                    std::sin(RadiansOfDegrees(own)) *
                                                    kRadiusBreathAmplitude;

  const std::span<const std::int32_t> ecarts = GapsOf(track.value[kSlotOfShape]);
  const bool lifted = style_mark == kMarkThatNudgesAside ||
                      (style_mark >= kFirstMarkThatLifts && style_mark <= kLastMarkThatLifts);

  std::array<glm::vec3, kCornerCount> corners;
  std::int32_t angle = track.degrees;
  for (std::size_t corner = 0; corner < kCornerCount; ++corner) {
    if (corner > 0) {
      angle = WithinFullTurn(angle - ecarts[corner - 1]);
    }
    const float theta = RadiansOfDegrees(angle);
    corners[corner] = {radius * std::cos(theta), 0.0F, radius * std::sin(theta)};
    if (style_mark == kMarkThatNudgesAside) {
      corners[corner].x += kNudgeAside;
    }
    if (lifted) {
      corners[corner].z += kLift;
    }
  }
  RotateEffectQuadCorners(corners, billboard_turn_degrees, billboard_tilt_degrees);

  // ⚠ L'ÉCHANGE DES PAIRES EST UNE AUTRE FIGURE, pas un détail : la texture s'y
  // pose retournée, et deux marques sur trente-trois gardent l'ordre d'origine.
  const std::array<std::size_t, kCornerCount> order =
      style.swaps_corners ? std::array<std::size_t, kCornerCount>{1, 0, 3, 2}
                          : std::array<std::size_t, kCornerCount>{0, 1, 2, 3};

  EffectParticleQuadRequest request;
  for (std::size_t corner = 0; corner < kCornerCount; ++corner) {
    const glm::vec3 local = corners[order[corner]];
    // ⚠ LE REPÈRE SE RETOURNE ICI, et seulement ici : les coins se calculent
    // dans celui du client, tournés par les angles de SA caméra ; l'ancre est
    // déjà dans le nôtre.
    request.corners[corner] = track.anchor + EffectPointFromClient(local);
  }
  request.absolute = true;
  request.alpha = static_cast<std::uint8_t>(std::min<std::int16_t>(track.alpha, 255));
  request.texture = track.mark[kMarkOfTexture];
  request.u_at_a_and_d = kTextureAtLeft;
  request.u_at_b_and_c = kTextureAtRight;
  request.v_at_a_and_b = kTextureAtNear;
  request.v_at_c_and_d = kTextureAtFar;
  request.depth =
      style.recentres_depth ? EffectQuadDepth::kScreenRecentred : EffectQuadDepth::kProjected;

  quads.reserve(style.layers.size());
  for (const EffectArcRibbonLayer& layer : style.layers) {
    request.color = layer.color;
    request.blend_mode = layer.blend_mode;
    quads.push_back(ComposeEffectParticleQuad(request, node_origin));
  }
  return quads;
}

}  // namespace ro::render
