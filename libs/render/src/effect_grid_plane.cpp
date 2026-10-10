#include "ro/render/effect_grid_plane.h"

#include <algorithm>
#include <array>
#include <cmath>

#include "ro/render/effect_dark_veil.h"
#include "ro/render/effect_quad_rotation.h"
#include "ro/render/effect_track.h"

namespace ro::render {
namespace {

// LE MÉLANGE ORDINAIRE, dans la numérotation du seizième paramètre de la voie A :
// zéro y est l'ADDITION, un le mélange ordinaire. La nappe emploie le second, et
// c'est ce qui la rend opaque là où une figure additive éclaircirait le sol.
constexpr std::int16_t kOrdinaryBlend = 1;

// LES QUATRE MARQUES QUE LE CLIENT COMPARE, une à une et par égalité.
constexpr std::uint8_t kMarkOfWhite = 0;
constexpr std::uint8_t kMarkOfRed = 1;
constexpr std::uint8_t kMarkOfWhiteAgain = 3;

constexpr std::uint8_t kFull = 255;

}  // namespace

std::array<std::uint8_t, 3> EffectGridPlaneTintFor(std::uint8_t mark) {
  if (mark == kMarkOfWhite || mark == kMarkOfWhiteAgain) {
    return {kFull, kFull, kFull};
  }
  if (mark == kMarkOfRed) {
    return {kFull, 0, 0};
  }
  return {0, 0, kFull};
}

std::vector<EffectWorldQuad> ComposeGridPlaneQuads(const EffectTrack& track,
                                                   std::int16_t turn_degrees,
                                                   std::int16_t tilt_degrees, glm::vec3 node_origin,
                                                   const EffectOwnerTint& owner) {
  std::vector<EffectWorldQuad> quads;
  if (!track.active || track.alpha <= 0) {
    return quads;
  }

  // ⚠ LE CÔTÉ D'UN CARREAU EST LE RAYON DE LA PISTE, et le PAS des boucles est
  // écrit en dur : les deux valent trente chez son unique bâtisseur, et c'est ce
  // qui rend la nappe jointive. Les confondre marcherait aujourd'hui et
  // ouvrirait des fentes au premier bâtisseur qui les sépare.
  const float cote = track.radius_at_anchor;
  // ⚠ LA CLÉ SE RETRANCHE DES DEUX AXES À LA FOIS, et c'est mesuré : le client
  // ajoute la même valeur à l'abscisse et à la profondeur. Ce n'est pas une
  // origine à deux composantes, c'est un seul nombre employé deux fois.
  const float origin = track.lift_key;
  const std::array<std::uint8_t, 3> tint = EffectGridPlaneTintFor(track.mark[kMarkOfGridPlaneTint]);
  const auto alpha = static_cast<std::uint8_t>(std::clamp<std::int16_t>(track.alpha, 0, 255));

  for (std::int32_t first = kGridPlaneFirstAxisFrom; first < kGridPlaneFirstAxisTo;
       first += kGridPlaneStep) {
    for (std::int32_t second = kGridPlaneSecondAxisFrom; second < kGridPlaneSecondAxisTo;
         second += kGridPlaneStep) {
      const float x = origin + static_cast<float>(first);
      const float z = origin + static_cast<float>(second);
      EffectParticleQuadRequest request;
      request.corners = {
          glm::vec3{x, kGridPlaneHeight, z},
          glm::vec3{x + cote, kGridPlaneHeight, z},
          glm::vec3{x + cote, kGridPlaneHeight, z + cote},
          glm::vec3{x, kGridPlaneHeight, z + cote},
      };
      // ⚠ LES COINS SE TOURNENT AVANT D'ÊTRE RENDUS, et c'est la seule chose qui
      // couche la nappe : posée à plat, elle serait invisible de face.
      // Elle se tourne dans le repère du client, par les angles de SA caméra,
      // puis revient au nôtre par `S` entier.
      RotateEffectQuadCorners(request.corners, turn_degrees, tilt_degrees);
      for (glm::vec3& corner : request.corners) {
        corner = EffectPointFromClient(corner);
      }
      request.alpha = alpha;
      request.color = tint;
      request.blend_mode = kOrdinaryBlend;
      quads.push_back(ComposeEffectParticleQuad(request, node_origin, owner));
    }
  }
  return quads;
}

std::vector<EffectScreenQuad> ComposeScreenGridPlaneQuads(const EffectTrack& track,
                                                          std::optional<glm::vec2> bearer_pixel,
                                                          float viewport_width,
                                                          float viewport_height) {
  std::vector<EffectScreenQuad> quads;
  if (!track.active || track.alpha <= 0 || viewport_width <= 0.0F || viewport_height <= 0.0F ||
      track.radius_at_anchor <= 0.0F) {
    return quads;
  }
  // LA TAILLE D'UN CARREAU AU RECUL DU JEU : la conversion du voile de ténèbres,
  // qui ne lit pas la caméra.
  const float side = DarkVeilScreenRadiusPixels(track.radius_at_anchor, viewport_height);
  const float scale = side / track.radius_at_anchor;
  const glm::vec2 centre =
      bearer_pixel.value_or(glm::vec2{viewport_width * 0.5F, viewport_height * 0.5F});
  // ⚠ LE CARRELAGE DU JEU, prolongé : ses arêtes restent là où le jeu les pose
  // — l'origine de la piste, puis un carreau après l'autre depuis le bord de
  // la nappe —, et les carreaux ajoutés continuent la même grille. L'abscisse
  // de la nappe court vers la droite de l'écran, sa profondeur vers le bas.
  const float left =
      centre.x + (track.lift_key + static_cast<float>(kGridPlaneFirstAxisFrom)) * scale;
  const float top =
      centre.y + (track.lift_key + static_cast<float>(kGridPlaneSecondAxisFrom)) * scale;
  const float first_column = std::floor(-left / side);
  const float last_column = std::ceil((viewport_width - left) / side);
  const float first_row = std::floor(-top / side);
  const float last_row = std::ceil((viewport_height - top) / side);
  const std::array<std::uint8_t, 3> tint = EffectGridPlaneTintFor(track.mark[kMarkOfGridPlaneTint]);
  const auto alpha = static_cast<std::uint8_t>(std::clamp<std::int16_t>(track.alpha, 0, 255));
  for (float column = first_column; column < last_column; column += 1.0F) {
    for (float row = first_row; row < last_row; row += 1.0F) {
      const glm::vec2 top_left{left + column * side, top + row * side};
      const glm::vec2 bottom_right{top_left.x + side, top_left.y + side};
      EffectScreenQuad quad;
      quad.corners = {top_left, glm::vec2{top_left.x, bottom_right.y}, bottom_right,
                      glm::vec2{bottom_right.x, top_left.y}};
      quad.texture_uv = {glm::vec2{0.0F, 0.0F}, glm::vec2{0.0F, 1.0F}, glm::vec2{1.0F, 1.0F},
                         glm::vec2{1.0F, 0.0F}};
      quad.tint = {tint[0], tint[1], tint[2], alpha};
      quad.blend_mode = kOrdinaryBlend;
      quads.push_back(quad);
    }
  }
  return quads;
}

}  // namespace ro::render
