#include "ro/render/model_vignette.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

namespace ro::render {
namespace {

constexpr std::size_t kChannels = 4;
constexpr std::size_t kAlpha = 3;
constexpr int kSupersampleFactor = 2;
constexpr float kHalf = 0.5F;
constexpr float kByteMax = 255.0F;
constexpr float kRounding = 0.5F;
// Un pixel presque transparent ne peint ni n'occulte : sans ce seuil, le carré
// de la texture masquerait ce qu'il y a derrière — les faces d'un cristal se
// cacheraient entre elles.
constexpr std::uint8_t kLeastPaintedAlpha = 8;
// Le gris d'un maillage sans texture.
constexpr std::uint8_t kUntexturedGrey = 200;
// Une aire ou une longueur plus petite ne se divise pas.
constexpr float kDegenerate = 1e-6F;

// La lumière, fixe par rapport à la caméra : du haut, de l'avant, de la gauche.
constexpr float kLightX = -0.42F;
constexpr float kLightY = 0.66F;
constexpr float kLightZ = -0.62F;

struct Viewed {
  float x = 0.0F;
  float y = 0.0F;
  float z = 0.0F;
};

// Le repère du modèle : X et Y au sol, Z vers le haut. Le lacet tourne autour
// de Z, puis la plongée bascule.
//
// 🔴 LE SIGNE SE VÉRIFIE SUR LA PROFONDEUR, pas sur la hauteur : une caméra
// placée au-dessus voit le sommet PLUS PRÈS que la base, et le coefficient de
// `z` dans la profondeur est négatif. Le signe opposé montre le monstre vu de
// DESSOUS — image plausible, que seul un modèle connu trahit.
[[nodiscard]] Viewed ViewTransform(const glm::vec3& point, float cos_yaw, float sin_yaw,
                                   float cos_pitch, float sin_pitch) {
  const float x = point.x * cos_yaw - point.y * sin_yaw;
  const float y = point.x * sin_yaw + point.y * cos_yaw;
  const float z = point.z;
  return Viewed{.x = x, .y = z * cos_pitch + y * sin_pitch, .z = y * cos_pitch - z * sin_pitch};
}

[[nodiscard]] std::uint8_t ToByte(float value) {
  const float clamped = std::clamp(value, 0.0F, 1.0F);
  return static_cast<std::uint8_t>(clamped * kByteMax + kRounding);
}

struct Projection {
  float scale = 0.0F;
  float origin_x = 0.0F;
  float origin_y = 0.0F;
};

// Le cadrage : la boîte englobante de la pose, vue de la caméra, tient dans le
// cadre moins sa marge, et s'y centre.
[[nodiscard]] bool Frame(std::span<const ModelVignetteMesh> meshes, const ModelVignetteView& view,
                         int width, int height, float cos_yaw, float sin_yaw, float cos_pitch,
                         float sin_pitch, Projection* out) {
  float least_x = std::numeric_limits<float>::max();
  float most_x = std::numeric_limits<float>::lowest();
  float least_y = least_x;
  float most_y = most_x;
  for (const ModelVignetteMesh& mesh : meshes) {
    for (const PosedVertex& vertex : mesh.vertices) {
      const Viewed seen = ViewTransform(vertex.position, cos_yaw, sin_yaw, cos_pitch, sin_pitch);
      least_x = std::min(least_x, seen.x);
      most_x = std::max(most_x, seen.x);
      least_y = std::min(least_y, seen.y);
      most_y = std::max(most_y, seen.y);
    }
  }
  if (most_x <= least_x || most_y <= least_y) {
    return false;
  }
  const float usable = 1.0F - 2.0F * view.margin;
  out->scale = std::min(static_cast<float>(width) * usable / (most_x - least_x),
                        static_cast<float>(height) * usable / (most_y - least_y));
  const float middle_x = (least_x + most_x) * kHalf;
  const float middle_y = (least_y + most_y) * kHalf;
  out->origin_x = static_cast<float>(width) * kHalf - middle_x * out->scale;
  // L'axe vertical de l'image descend, celui du modèle monte : le signe
  // s'inverse ici et nulle part ailleurs.
  out->origin_y = static_cast<float>(height) * kHalf + middle_y * out->scale;
  return true;
}

// Le texel au plus proche, en répétition : le filtre du jeu, qui garde un
// modèle agrandi dans la famille visuelle des sprites qui l'entourent.
void Sample(const formats::GrannyTextureImage& texture, float u, float v, std::uint8_t* rgba) {
  const int width = texture.width;
  const int height = texture.height;
  int texel_x = static_cast<int>(u * static_cast<float>(width));
  int texel_y = static_cast<int>(v * static_cast<float>(height));
  texel_x = ((texel_x % width) + width) % width;
  texel_y = ((texel_y % height) + height) % height;
  const std::size_t at = (static_cast<std::size_t>(texel_y) * static_cast<std::size_t>(width) +
                          static_cast<std::size_t>(texel_x)) *
                         kChannels;
  std::copy_n(texture.rgba.begin() + static_cast<std::ptrdiff_t>(at), kChannels, rgba);
}

// Réduit de moitié en moyennant des couleurs PRÉMULTIPLIÉES par l'alpha : sans
// cela les pixels transparents, de couleur arbitraire, délavent les bords et
// cernent le modèle d'un halo.
[[nodiscard]] std::vector<std::uint8_t> Halve(const std::vector<std::uint8_t>& big, int width,
                                              int height) {
  const int big_width = width * kSupersampleFactor;
  std::vector<std::uint8_t> small(
      static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * kChannels, 0);
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      std::array<int, kChannels> sum{};
      for (int dy = 0; dy < kSupersampleFactor; ++dy) {
        for (int dx = 0; dx < kSupersampleFactor; ++dx) {
          const std::size_t at = (static_cast<std::size_t>(y * kSupersampleFactor + dy) *
                                      static_cast<std::size_t>(big_width) +
                                  static_cast<std::size_t>(x * kSupersampleFactor + dx)) *
                                 kChannels;
          const int alpha = big[at + kAlpha];
          for (std::size_t channel = 0; channel < kAlpha; ++channel) {
            sum[channel] += big[at + channel] * alpha;
          }
          sum[kAlpha] += alpha;
        }
      }
      if (sum[kAlpha] <= 0) {
        continue;
      }
      std::uint8_t* out = &small[(static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                                  static_cast<std::size_t>(x)) *
                                 kChannels];
      for (std::size_t channel = 0; channel < kAlpha; ++channel) {
        out[channel] = static_cast<std::uint8_t>(sum[channel] / sum[kAlpha]);
      }
      out[kAlpha] =
          static_cast<std::uint8_t>(sum[kAlpha] / (kSupersampleFactor * kSupersampleFactor));
    }
  }
  return small;
}

}  // namespace

formats::Image RasterModelVignette(std::span<const ModelVignetteMesh> meshes,
                                   const ModelVignetteView& view) {
  if (view.width <= 0 || view.height <= 0 || meshes.empty()) {
    return {};
  }
  const int factor = view.supersample ? kSupersampleFactor : 1;
  const int width = view.width * factor;
  const int height = view.height * factor;
  const float cos_yaw = std::cos(view.yaw_radians);
  const float sin_yaw = std::sin(view.yaw_radians);
  const float cos_pitch = std::cos(view.pitch_radians);
  const float sin_pitch = std::sin(view.pitch_radians);
  Projection projection;
  if (!Frame(meshes, view, width, height, cos_yaw, sin_yaw, cos_pitch, sin_pitch, &projection)) {
    return {};
  }

  const auto pixel_count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
  std::vector<std::uint8_t> image(pixel_count * kChannels, 0);
  std::vector<float> depth(pixel_count, std::numeric_limits<float>::max());

  for (const ModelVignetteMesh& mesh : meshes) {
    const formats::GrannyTextureImage* const texture =
        mesh.texture != nullptr && mesh.texture->width > 0 && mesh.texture->height > 0 &&
                mesh.texture->rgba.size() >= static_cast<std::size_t>(mesh.texture->width) *
                                                 static_cast<std::size_t>(mesh.texture->height) *
                                                 kChannels
            ? mesh.texture
            : nullptr;
    for (std::size_t first = 0; first + 2 < mesh.indices.size(); first += 3) {
      std::array<const PosedVertex*, 3> corner{};
      bool whole = true;
      for (std::size_t k = 0; k < corner.size(); ++k) {
        const std::uint16_t index = mesh.indices[first + k];
        if (index >= mesh.vertices.size()) {
          whole = false;
          break;
        }
        corner[k] = &mesh.vertices[index];
      }
      if (!whole) {
        continue;
      }
      std::array<Viewed, 3> screen{};
      for (std::size_t k = 0; k < corner.size(); ++k) {
        const Viewed seen =
            ViewTransform(corner[k]->position, cos_yaw, sin_yaw, cos_pitch, sin_pitch);
        screen[k] = Viewed{.x = projection.origin_x + seen.x * projection.scale,
                           .y = projection.origin_y - seen.y * projection.scale,
                           .z = seen.z};
      }
      // L'aire signée : elle sert au test barycentrique ET donne l'orientation.
      const float area = (screen[1].x - screen[0].x) * (screen[2].y - screen[0].y) -
                         (screen[2].x - screen[0].x) * (screen[1].y - screen[0].y);
      if (std::fabs(area) < kDegenerate) {
        continue;
      }
      const float inverse_area = 1.0F / area;
      const int left = std::max(
          0, static_cast<int>(std::floor(std::min({screen[0].x, screen[1].x, screen[2].x}))));
      const int right =
          std::min(width - 1,
                   static_cast<int>(std::ceil(std::max({screen[0].x, screen[1].x, screen[2].x}))));
      const int top = std::max(
          0, static_cast<int>(std::floor(std::min({screen[0].y, screen[1].y, screen[2].y}))));
      const int bottom =
          std::min(height - 1,
                   static_cast<int>(std::ceil(std::max({screen[0].y, screen[1].y, screen[2].y}))));

      for (int y = top; y <= bottom; ++y) {
        for (int x = left; x <= right; ++x) {
          const float px = static_cast<float>(x) + kHalf;
          const float py = static_cast<float>(y) + kHalf;
          const float w0 =
              ((screen[1].x - px) * (screen[2].y - py) - (screen[2].x - px) * (screen[1].y - py)) *
              inverse_area;
          const float w1 =
              ((screen[2].x - px) * (screen[0].y - py) - (screen[0].x - px) * (screen[2].y - py)) *
              inverse_area;
          const float w2 = 1.0F - w0 - w1;
          if (w0 < 0.0F || w1 < 0.0F || w2 < 0.0F) {
            continue;
          }
          const float z = w0 * screen[0].z + w1 * screen[1].z + w2 * screen[2].z;
          const std::size_t at = static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                                 static_cast<std::size_t>(x);
          if (z >= depth[at]) {
            continue;
          }
          std::array<std::uint8_t, kChannels> texel{kUntexturedGrey, kUntexturedGrey,
                                                    kUntexturedGrey,
                                                    static_cast<std::uint8_t>(kByteMax)};
          if (texture != nullptr) {
            const glm::vec2 uv = w0 * corner[0]->uv + w1 * corner[1]->uv + w2 * corner[2]->uv;
            Sample(*texture, uv.x, uv.y, texel.data());
          }
          if (texel[kAlpha] < kLeastPaintedAlpha) {
            continue;
          }
          const glm::vec3 normal =
              w0 * corner[0]->normal + w1 * corner[1]->normal + w2 * corner[2]->normal;
          const Viewed seen_normal = ViewTransform(normal, cos_yaw, sin_yaw, cos_pitch, sin_pitch);
          const float length =
              std::sqrt(seen_normal.x * seen_normal.x + seen_normal.y * seen_normal.y +
                        seen_normal.z * seen_normal.z);
          float lambert = 0.0F;
          if (length > kDegenerate) {
            // ÉCLAIRÉ DES DEUX CÔTÉS, puisqu'aucune face n'est écartée.
            lambert = std::fabs(
                (seen_normal.x * kLightX + seen_normal.y * kLightY + seen_normal.z * kLightZ) /
                length);
          }
          const float light = view.ambient + view.diffuse * lambert;
          depth[at] = z;
          std::uint8_t* out = &image[at * kChannels];
          for (std::size_t channel = 0; channel < kAlpha; ++channel) {
            out[channel] = ToByte(static_cast<float>(texel[channel]) / kByteMax * light);
          }
          out[kAlpha] = texel[kAlpha];
        }
      }
    }
  }

  formats::Image result;
  result.width = view.width;
  result.height = view.height;
  result.pixels = factor == 1 ? std::move(image) : Halve(image, view.width, view.height);
  return result;
}

}  // namespace ro::render
