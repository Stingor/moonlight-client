#include "ro/render/effect_screen_quad.h"

#include <cmath>

#include "ro/render/map_effect_math.h"

namespace ro::render {
namespace {

// LES COORDONNÉES D'IMAGE DES QUATRE COINS, dans l'ordre CYCLIQUE.
//
// ⚠ LE CLIENT SOUMET SES SOMMETS PAR TRIANGLES, et son second triangle reprend
// deux sommets du premier : son ordre est donc (0,1), (0,0), (1,1) puis (1,1),
// (0,0), (1,0). Recopier cette suite dans un quad à quatre coins donnerait un
// papillon — le renderer, lui, referme le quad en tournant autour, et attend
// donc les coins EN CYCLE. Les deux derniers s'échangent.
constexpr std::array<glm::vec2, 4> kTextureCorners = {glm::vec2{0.0F, 1.0F}, glm::vec2{0.0F, 0.0F},
                                                      glm::vec2{1.0F, 0.0F}, glm::vec2{1.0F, 1.0F}};

}  // namespace

std::vector<EffectScreenQuad> ComposeScreenQuad(const EffectScreenQuadRequest& request) {
  std::vector<EffectScreenQuad> quads;
  if (request.half_width == 0.0F || request.half_height == 0.0F) {
    return quads;
  }

  // ⚠ L'ANCRAGE DÉPLACE L'ORIGINE, PAS LA TAILLE. Les deux bornes s'écartent
  // toujours de deux demi-hauteurs ; c'est leur milieu qui change.
  const float bottom =
      request.hangs_below_anchor ? -2.0F * request.half_height : -request.half_height;
  const float top = request.hangs_below_anchor ? 0.0F : request.half_height;

  // 🔴 LA ROTATION EST LA ROTATION USUELLE, lue dans les tables du client : un
  // point local (u, v) sort en (u·cos − v·sin, u·sin + v·cos), et un angle nul
  // laisse la figure d'aplomb. Le dessinateur la calcule par deux lectures de
  // table dont les NOMS sont échangés — celle qu'on croirait du cosinus rend le
  // sinus —, et les prendre au mot faisait tourner chaque quad d'un quart de
  // tour. L'angle se lit comme dans ces tables, un degré de travers quand il est
  // négatif : voir `NativeTableRadians`.
  const float theta = NativeTableRadians(request.turn_degrees);
  const float sinus = std::sin(theta);
  const float cosinus = std::cos(theta);
  const auto rotated = [&](float u, float v) {
    return glm::vec2{u * cosinus - v * sinus, u * sinus + v * cosinus};
  };

  // ⚠ EN CYCLE : haut-gauche, bas-gauche, bas-droit, haut-droit. Le renderer
  // découpe un quad en deux triangles qui partagent la diagonale du premier
  // coin au troisième ; des coins rangés en paires y feraient un papillon.
  const std::array<glm::vec2, 4> locaux = {
      rotated(-request.half_width, top), rotated(-request.half_width, bottom),
      rotated(request.half_width, bottom), rotated(request.half_width, top)};

  EffectScreenQuad quad;
  for (std::size_t corner = 0; corner < quad.corners.size(); ++corner) {
    // Le demi-pixel se retire APRÈS la translation, comme le client le fait.
    quad.corners[corner] = locaux[corner] + request.screen_position -
                           glm::vec2{kEffectScreenTexelOffset, kEffectScreenTexelOffset};
  }
  quad.texture_uv = kTextureCorners;
  quad.tint = request.tint;
  quad.blend_mode = request.blend_mode;
  quad.texture = request.texture;
  quads.push_back(quad);
  return quads;
}

std::optional<glm::vec2> EffectScreenPositionOf(const Camera& camera, glm::vec3 world_position,
                                                float viewport_width, float viewport_height) {
  if (viewport_width <= 0.0F || viewport_height <= 0.0F) {
    return std::nullopt;
  }
  const glm::mat4 to_screen =
      camera.ProjectionMatrix(viewport_width / viewport_height) * camera.ViewMatrix();
  const glm::vec4 projete = to_screen * glm::vec4{world_position, 1.0F};
  // ⚠ DERRIÈRE LA CAMÉRA, la coordonnée homogène est nulle ou négative : diviser
  // enverrait le point à l'opposé de l'écran.
  if (projete.w <= 0.0F) {
    return std::nullopt;
  }
  const glm::vec3 normalise = glm::vec3{projete} / projete.w;
  // Du repère normalisé à celui de la fenêtre : l'origine passe au coin
  // HAUT-GAUCHE, et l'ordonnée SE RETOURNE.
  return glm::vec2{(normalise.x + 1.0F) * 0.5F * viewport_width,
                   (1.0F - normalise.y) * 0.5F * viewport_height};
}

}  // namespace ro::render
