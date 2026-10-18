#pragma once

#include <array>

#include <glm/glm.hpp>

namespace ro::render {

// UNE BOÎTE DU MONDE, alignée aux axes.
struct WorldBox {
  glm::vec3 lowest{0.0F};
  glm::vec3 highest{0.0F};
};

// Ce que voit une vue-projection : les six plans de son tronc, chacun tourné
// vers l'intérieur, dans le repère du monde.
class WorldFrustum {
 public:
  [[nodiscard]] static WorldFrustum Of(const glm::mat4& view_projection);

  // Faux seulement quand la sphère ou la boîte est entièrement hors du tronc.
  // Un objet qui en coupe un coin sans y entrer peut passer pour vu : le test
  // ne coûte que six plans, et une erreur de ce côté ne fait que dessiner de
  // trop.
  [[nodiscard]] bool SeesSphere(glm::vec3 center, float radius) const;
  [[nodiscard]] bool SeesBox(const WorldBox& box) const;

 private:
  static constexpr std::size_t kPlaneCount = 6;
  std::array<glm::vec4, kPlaneCount> planes_{};
};

// Vrai quand la sphère et la boîte se touchent.
[[nodiscard]] bool SphereMeetsBox(glm::vec3 center, float radius, const WorldBox& box);

// CE QUE VOIT UNE FACE D'UNE CARTE D'OMBRE PONCTUELLE : son tronc, borné par
// la sphère où sa lumière porte.
struct ShadowSight {
  WorldFrustum frustum;
  glm::vec3 center{0.0F};
  float radius = 0.0F;

  [[nodiscard]] bool Sees(const WorldBox& box) const {
    return SphereMeetsBox(center, radius, box) && frustum.SeesBox(box);
  }
  [[nodiscard]] bool SeesSphere(glm::vec3 at, float reach) const {
    return glm::distance(at, center) <= radius + reach && frustum.SeesSphere(at, reach);
  }
};

// La boîte qui contient `box` portée par `transform`.
[[nodiscard]] WorldBox TransformedBox(const WorldBox& box, const glm::mat4& transform);

}  // namespace ro::render
