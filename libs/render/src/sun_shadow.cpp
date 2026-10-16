#include "ro/render/sun_shadow.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

#include <glm/gtc/matrix_transform.hpp>

namespace ro::render {
namespace {

// Au-delà, le soleil est presque au zénith et l'axe vertical ne peut plus
// servir de « haut » à sa vue.
constexpr float kZenithCosine = 0.99F;
// Un rayon qui descend moins vite ne rencontre pas le sol dans le champ.
constexpr float kLeastDescent = 1e-4F;

// Les huit coins d'une boîte, un bit par axe.
constexpr int kBoxCorners = 8;

// Les coins de l'écran, en coordonnées normalisées.
constexpr std::array<glm::vec2, 4> kScreenCorners{glm::vec2(-1.0F, -1.0F), glm::vec2(1.0F, -1.0F),
                                                  glm::vec2(-1.0F, 1.0F), glm::vec2(1.0F, 1.0F)};

[[nodiscard]] glm::vec3 Unproject(const glm::mat4& inverse_view_projection, glm::vec2 corner,
                                  float ndc_depth) {
  const glm::vec4 point = inverse_view_projection * glm::vec4(corner, ndc_depth, 1.0F);
  return glm::vec3(point) / point.w;
}

// En dessous, un éclairement est noir, et le rapport n'a plus de sens.
constexpr float kDarkestLight = 1e-4F;

// L'éclairement d'un sol plat sous `light` — au soleil, puis à l'ombre.
struct FlatGround {
  glm::vec3 lit;
  glm::vec3 shaded;
};

[[nodiscard]] FlatGround FlatGroundOf(const SceneLight& light) {
  const glm::vec3 one(1.0F);
  const float incidence = std::clamp(glm::normalize(light.direction).y, 0.0F, 1.0F);
  const glm::vec3 ambient_factor = (one - light.ambient) * light.ambient;
  const glm::vec3 ambient = light.ambient - ambient_factor + ambient_factor * light.diffuse;
  const glm::vec3 diffuse_factor = (one - light.diffuse) * light.diffuse;
  const glm::vec3 diffuse = light.diffuse - diffuse_factor + diffuse_factor * light.ambient;
  const glm::vec3 brightest = glm::max(light.diffuse, light.ambient);
  const glm::vec3 ceiling =
      glm::min(brightest + (one - brightest) * glm::min(light.diffuse, light.ambient), one);
  return FlatGround{
      .lit =
          glm::clamp(glm::min(glm::min(incidence * diffuse + ambient, one), ceiling), 0.0F, 1.0F),
      .shaded = glm::clamp(glm::min(ambient, ceiling), 0.0F, 1.0F)};
}

}  // namespace

glm::vec3 FlatGroundLightOf(const SceneLight& light) { return FlatGroundOf(light).lit; }

glm::vec3 SunShadeOf(const SceneLight& light) {
  const FlatGround ground = FlatGroundOf(light);
  return glm::clamp(ground.shaded / glm::max(ground.lit, glm::vec3(kDarkestLight)), 0.0F, 1.0F);
}

SunShadowFrame FrameSunShadow(const glm::vec3& light_direction, const Camera& camera,
                              float aspect_ratio, const glm::vec3& bounds_min,
                              const glm::vec3& bounds_max, int map_texels) {
  const glm::vec3 toward_sun = glm::normalize(light_direction);
  const glm::vec3 up = std::abs(toward_sun.y) > kZenithCosine ? glm::vec3(0.0F, 0.0F, 1.0F)
                                                              : glm::vec3(0.0F, 1.0F, 0.0F);
  // UNE ROTATION PURE, œil à l'origine : la grille des texels reste fixe dans
  // le monde, et le cadre peut s'y aligner.
  const glm::mat4 light_view = glm::lookAt(glm::vec3(0.0F), -toward_sun, up);

  // CE QUE LA CAMÉRA VOIT : ses quatre rayons de coin, arrêtés aux deux plans
  // qui bornent le relief autour du point de visée.
  const glm::mat4 inverse =
      glm::inverse(camera.ProjectionMatrix(aspect_ratio) * camera.ViewMatrix());
  const float reach = camera.distance * kSunShadowReachPerViewDistance;
  glm::vec2 seen_min(std::numeric_limits<float>::max());
  glm::vec2 seen_max(std::numeric_limits<float>::lowest());
  const auto see = [&](const glm::vec3& world) {
    const glm::vec2 lit = glm::vec2(light_view * glm::vec4(world, 1.0F));
    seen_min = glm::min(seen_min, lit);
    seen_max = glm::max(seen_max, lit);
  };
  see(camera.look_at);
  for (const glm::vec2& corner : kScreenCorners) {
    const glm::vec3 near_point = Unproject(inverse, corner, -1.0F);
    const glm::vec3 ray = glm::normalize(Unproject(inverse, corner, 1.0F) - near_point);
    for (const float height :
         {camera.look_at.y - kSunShadowReliefMargin, camera.look_at.y + kSunShadowReliefMargin}) {
      float travel = reach;
      if (ray.y < -kLeastDescent) {
        travel = std::clamp((height - near_point.y) / ray.y, 0.0F, reach);
      }
      see(near_point + ray * travel);
    }
  }

  const glm::vec2 extent = seen_max - seen_min;
  const float side =
      std::ceil(std::max({extent.x, extent.y, kSunShadowSideStep}) / kSunShadowSideStep) *
      kSunShadowSideStep;
  const float texel = side / static_cast<float>(std::max(map_texels, 1));
  const glm::vec2 center = glm::floor((seen_min + seen_max) * 0.5F / texel) * texel;

  // LA PROFONDEUR : toute l'emprise du terrain, et le décor qui le dépasse.
  float nearest = std::numeric_limits<float>::max();
  float farthest = std::numeric_limits<float>::lowest();
  const glm::vec3 top = bounds_max + glm::vec3(0.0F, kSunShadowCasterRise, 0.0F);
  for (int corner = 0; corner < kBoxCorners; ++corner) {
    const glm::vec3 world((corner & 1) != 0 ? top.x : bounds_min.x,
                          (corner & 2) != 0 ? top.y : bounds_min.y,
                          (corner & 4) != 0 ? top.z : bounds_min.z);
    // La vue regarde le long de -z : la distance est l'opposée.
    const float depth = -(light_view * glm::vec4(world, 1.0F)).z;
    nearest = std::min(nearest, depth);
    farthest = std::max(farthest, depth);
  }

  const float half = side * 0.5F;
  const glm::mat4 projection = glm::ortho(center.x - half, center.x + half, center.y - half,
                                          center.y + half, nearest, farthest);
  return SunShadowFrame{.view_projection = projection * light_view, .texel_world = texel};
}

}  // namespace ro::render
