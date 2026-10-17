#include "ro/render/effect_concentric_square.h"

#include <cmath>

#include "ro/render/effect_particle_quad.h"
#include "ro/render/effect_quad_rotation.h"
#include "ro/render/effect_track.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

// OÙ LA FIGURE LIT CE QU'ELLE DESSINE, dans les vingt et une cases.
constexpr std::size_t kSlotOfBreath = 8;

// LE MÉLANGE DE LA COUCHE PLEINE quand le champ du nœud l'allume.
constexpr std::int16_t kBrightBlend = 10;
constexpr std::int16_t kPlainBlend = 0;

// LES MARQUES QUI CONSULTENT LE CHAMP DE MÉLANGE. Les autres passent zéro sans
// regarder — mesuré : le client ne lit le champ que dans ces quatre branches.
[[nodiscard]] bool ReadsBlendField(std::uint8_t mark) { return mark >= 10 && mark <= 13; }

// LA TEINTE DE LA COUCHE PLEINE, par marque.
[[nodiscard]] std::array<std::uint8_t, 3> FullLayerTint(std::uint8_t mark) {
  switch (mark) {
    case 1:
      return {68, 42, 30};
    case 10:
      return {255, 50, 50};
    case 11:
      return {200, 200, 50};
    case 12:
      return {50, 50, 255};
    case 13:
      return {255, 255, 255};
    case 20:
    case 21:
      return {40, 40, 255};
    default:
      return {0, 0, 255};
  }
}

// LA TEINTE DE LA COUCHE ÉTROITE, par marque. Elle ne suit PAS la précédente :
// six marques sur huit y changent de valeur.
[[nodiscard]] std::array<std::uint8_t, 3> NarrowLayerTint(std::uint8_t mark) {
  switch (mark) {
    case 1:
      return {255, 255, 255};
    case 10:
      return {255, 50, 50};
    case 11:
      return {200, 200, 50};
    case 12:
      return {50, 50, 255};
    case 13:
      return {100, 150, 100};
    case 20:
      return {150, 150, 255};
    case 21:
      return {250, 250, 255};
    default:
      return {255, 255, 0};
  }
}

// LA TEINTE DES COUCHES LARGES : bleu pur, quelle que soit la marque.
constexpr std::array<std::uint8_t, 3> kHaloColor = {0, 0, 255};

// UNE COUCHE : son échelle, sa teinte, son alpha et son mélange.
struct Layer {
  float scale = 1.0F;
  std::array<std::uint8_t, 3> color = {255, 255, 255};
  std::int16_t alpha = 0;
  std::int16_t blend_mode = kPlainBlend;
};

}  // namespace

std::vector<EffectWorldQuad> ComposeConcentricSquareQuads(
    const EffectConcentricSquareRequest& request, glm::vec3 node_origin) {
  std::vector<EffectWorldQuad> quads;
  if (request.alpha <= 0 || request.radius == 0.0F) {
    return quads;
  }

  // LES COUCHES, de la plus large à la plus étroite. Les deux premières
  // n'existent que pour deux marques.
  std::vector<Layer> layers;
  if (request.mark == kConcentricSquareMarkWithTwoHalos) {
    // ⚠ LA PLUS LARGE N'A QUE LA MOITIÉ DE L'ALPHA, et elle seule.
    layers.push_back(Layer{kConcentricSquareWidest, kHaloColor,
                           static_cast<std::int16_t>(request.alpha / 2), kPlainBlend});
    layers.push_back(Layer{kConcentricSquareWide, kHaloColor, request.alpha, kPlainBlend});
  } else if (request.mark == kConcentricSquareMarkWithOneHalo) {
    layers.push_back(Layer{kConcentricSquareMedium, kHaloColor, request.alpha, kPlainBlend});
  }
  // ⚠ LE CHAMP DU NŒUD RETIRE LE MÉLANGE AU LIEU DE L'AJOUTER : dix par défaut,
  // zéro quand il est posé. Et seules quatre marques le consultent.
  const std::int16_t blend =
      ReadsBlendField(request.mark) && !request.plain_blend ? kBrightBlend : kPlainBlend;
  layers.push_back(Layer{kConcentricSquareFull, FullLayerTint(request.mark), request.alpha, blend});
  layers.push_back(
      Layer{kConcentricSquareNarrow, NarrowLayerTint(request.mark), request.alpha, kPlainBlend});

  // ⚠ LE SOUFFLE EST LE MÊME POUR TOUTES LES COUCHES : seule l'échelle les
  // sépare. Le faire varier par couche les ferait battre en désordre.
  const float breath =
      request.radius * std::sin(DegreesToRadians(request.breath_degrees)) * kConcentricSquareBreath;

  for (const Layer& layer : layers) {
    const float radius = request.radius * layer.scale + breath;
    // LES QUATRE COINS, à zéro, quatre-vingt-dix, cent quatre-vingts et deux
    // cent soixante-dix degrés — un carré, et dans l'ordre où l'on en fait le
    // tour.
    //
    // ⚠ LE CARRÉ EST COUCHÉ : le cosinus va en abscisse, le sinus en PROFONDEUR,
    // l'ordonnée reste nulle. C'est la rotation de billboard qui le redresse vers
    // l'œil ; posé debout, elle le présenterait par la tranche — un trait.
    std::array<glm::vec3, 4> corners = {
        glm::vec3{radius, 0.0F, 0.0F}, glm::vec3{0.0F, 0.0F, radius},
        glm::vec3{-radius, 0.0F, 0.0F}, glm::vec3{0.0F, 0.0F, -radius}};
    RotateEffectQuadCorners(corners, request.turn_degrees, request.tilt_degrees);

    EffectParticleQuadRequest spawn_request;
    for (std::size_t corner = 0; corner < corners.size(); ++corner) {
      // ⚠ LE REPÈRE SE RETOURNE ICI : le client range son centre et tourne ses
      // coins, par les angles de SA caméra, dans un repère dont l'ordonnée
      // DESCEND, et nous rendons le tout dans le nôtre par `S` entier. Une
      // seule conversion, à la toute fin, plutôt qu'une par terme — deux se
      // seraient annulées.
      const glm::vec3 pose = corners[corner] + request.center;
      spawn_request.corners[corner] = EffectPointFromClient(pose);
    }
    spawn_request.alpha = static_cast<std::uint8_t>(layer.alpha);
    spawn_request.color = layer.color;
    spawn_request.blend_mode = layer.blend_mode;
    quads.push_back(ComposeEffectParticleQuad(spawn_request, node_origin));
  }
  return quads;
}

EffectConcentricSquareRequest EffectConcentricSquareRequestOf(const EffectTrack& first,
                                                              const EffectTrack& second,
                                                              bool plain_blend) {
  EffectConcentricSquareRequest spawn_request;
  spawn_request.radius = first.radius_at_anchor;
  spawn_request.breath_degrees = first.value[kSlotOfBreath];
  spawn_request.alpha = first.active ? first.alpha : 0;
  spawn_request.turn_degrees = first.style;
  // ⚠ LE SECOND ANGLE VIENT DE LA PISTE UN, et d'elle seule.
  spawn_request.tilt_degrees = second.style;
  spawn_request.mark = first.mark[0];
  spawn_request.plain_blend = plain_blend;
  spawn_request.center = first.anchor;
  return spawn_request;
}

}  // namespace ro::render
