#include "ro/render/ground_pick.h"

#include "ro/render/terrain_mesh.h"

namespace ro::render {
namespace {

// De combien le rayon peut s'éloigner avant qu'on renonce.
//
// Bornée par la CARTE, pas par le plan lointain de la caméra : celui-ci est à
// cent mille unités, et marcher jusque-là coûterait cinquante mille pas pour
// rien. La largeur plus la profondeur plus le recul de la caméra couvrent tout
// ce qu'un rayon parti de l'œil peut rencontrer.
float SearchLength(const formats::GndMap& gnd, const Camera& camera) {
  const float width = static_cast<float>(gnd.width()) * kWorldUnitsPerCube;
  const float depth = static_cast<float>(gnd.height()) * kWorldUnitsPerCube;
  return width + depth + camera.distance;
}

// Négatif sous le sol, positif au-dessus.
//
// Le sol est celui sur lequel on MARCHE quand la carte porte sa grille — un
// pont y passe au-dessus du terrain, et c'est le pont qu'on vise.
float HeightAbove(const formats::GndMap& gnd, const formats::GatMap* gat, const glm::vec3& point) {
  if (gat != nullptr && gat->width() > 0 && gat->height() > 0) {
    const std::array<float, 2> cell = FractionalCellAt(gnd, point.x, point.z);
    return point.y - WalkableHeightAt(*gat, cell[0], cell[1]);
  }
  return point.y - GroundHeightAt(gnd, point.x, point.z);
}

bool InsideMap(const formats::GndMap& gnd, const glm::vec3& point) {
  const float width = static_cast<float>(gnd.width()) * kWorldUnitsPerCube;
  const float depth = static_cast<float>(gnd.height()) * kWorldUnitsPerCube;
  // Le maillage occupe [0, largeur] en X, et [c, profondeur + c] en Z : les
  // rangées descendent en Z depuis profondeur + c. Voir CellPositionAt.
  return point.x >= 0.0F && point.x <= width && point.z >= kWorldUnitsPerCube &&
         point.z <= depth + kWorldUnitsPerCube;
}

}  // namespace

ScreenRay MakeScreenRay(const Camera& camera, float aspect_ratio, float screen_x, float screen_y,
                        float viewport_width, float viewport_height) {
  ScreenRay ray;
  if (viewport_width <= 0.0F || viewport_height <= 0.0F) {
    return ray;
  }

  // Coordonnées normalisées : -1 à +1, l'axe des Y vers le HAUT. La fenêtre
  // compte le sien vers le bas, d'où le retournement.
  const float ndc_x = 2.0F * screen_x / viewport_width - 1.0F;
  const float ndc_y = 1.0F - 2.0F * screen_y / viewport_height;

  const glm::mat4 inverse =
      glm::inverse(camera.ProjectionMatrix(aspect_ratio) * camera.ViewMatrix());
  // Les deux bouts du segment visible sous ce pixel. Partir de la position de
  // la caméra serait plus court, mais cesserait de valoir le jour où la
  // projection ne serait plus perspective : en orthographique, l'œil n'est pas
  // sur le rayon.
  const glm::vec4 near_point = inverse * glm::vec4{ndc_x, ndc_y, -1.0F, 1.0F};
  const glm::vec4 far_point = inverse * glm::vec4{ndc_x, ndc_y, 1.0F, 1.0F};
  if (near_point.w == 0.0F || far_point.w == 0.0F) {
    return ray;
  }

  ray.origin = glm::vec3(near_point) / near_point.w;
  const glm::vec3 target = glm::vec3(far_point) / far_point.w;
  const glm::vec3 span = target - ray.origin;
  const float length = glm::length(span);
  ray.direction = length > 0.0F ? span / length : glm::vec3{0.0F, 0.0F, -1.0F};
  return ray;
}

std::optional<std::array<float, 3>> PickGround(const formats::GndMap& gnd,
                                               const formats::GatMap* gat, const Camera& camera,
                                               float aspect_ratio, float screen_x, float screen_y,
                                               float viewport_width, float viewport_height) {
  if (gnd.width() <= 0 || gnd.height() <= 0) {
    return std::nullopt;
  }

  const ScreenRay ray =
      MakeScreenRay(camera, aspect_ratio, screen_x, screen_y, viewport_width, viewport_height);
  // Le rayon part déjà sous le terrain : il n'y a pas de première rencontre à
  // trouver, et resserrer sur rien rendrait un point arbitraire.
  if (HeightAbove(gnd, gat, ray.origin) < 0.0F) {
    return std::nullopt;
  }

  const float reach = SearchLength(gnd, camera);
  float above = 0.0F;
  for (float ahead = kGroundPickStep; ahead <= reach; ahead += kGroundPickStep) {
    if (HeightAbove(gnd, gat, ray.origin + ray.direction * ahead) >= 0.0F) {
      above = ahead;
      continue;
    }

    // Le pas vient de traverser le sol : on resserre entre les deux bornes.
    float outside = above;
    float inside = ahead;
    for (int step = 0; step < kGroundPickRefinements; ++step) {
      const float middle = (outside + inside) * 0.5F;
      if (HeightAbove(gnd, gat, ray.origin + ray.direction * middle) >= 0.0F) {
        outside = middle;
      } else {
        inside = middle;
      }
    }

    const glm::vec3 hit = ray.origin + ray.direction * ((outside + inside) * 0.5F);
    return InsideMap(gnd, hit) ? std::optional<std::array<float, 3>>{{hit.x, hit.y, hit.z}}
                               : std::nullopt;
  }

  return std::nullopt;
}

}  // namespace ro::render
