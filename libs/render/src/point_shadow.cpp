#include "ro/render/point_shadow.h"

#include <algorithm>
#include <array>
#include <cstddef>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace ro::render {
namespace {

// Un quart de l'espace par face : un champ de 90°, carré.
constexpr float kFaceFieldDegrees = 90.0F;
constexpr float kFaceAspectRatio = 1.0F;

struct FaceAxes {
  glm::vec3 forward;
  glm::vec3 up;
};

// LE HAUT DE CHAQUE FACE est celui que la table des textures cubiques d'OpenGL
// impose : les faces latérales ont le haut vers −Y, celles du dessus et du
// dessous vers ±Z.
constexpr std::array<FaceAxes, kCubeFaceCount> kFaceAxes = {{
    {{1.0F, 0.0F, 0.0F}, {0.0F, -1.0F, 0.0F}},
    {{-1.0F, 0.0F, 0.0F}, {0.0F, -1.0F, 0.0F}},
    {{0.0F, 1.0F, 0.0F}, {0.0F, 0.0F, 1.0F}},
    {{0.0F, -1.0F, 0.0F}, {0.0F, 0.0F, -1.0F}},
    {{0.0F, 0.0F, 1.0F}, {0.0F, -1.0F, 0.0F}},
    {{0.0F, 0.0F, -1.0F}, {0.0F, -1.0F, 0.0F}},
}};

[[nodiscard]] PaintedPointLight PaintedOf(const PointLight& light) {
  return PaintedPointLight{.world_position = light.world_position,
                           .range_world = light.range_world};
}

}  // namespace

std::size_t PointShadowTierOf(float distance_world, std::optional<std::size_t> held) {
  std::size_t wanted = 0;
  while (wanted < kPointShadowTierReachWorld.size() &&
         distance_world >= kPointShadowTierReachWorld[wanted]) {
    ++wanted;
  }
  if (!held.has_value() || *held == wanted || *held >= kPointShadowTierCount) {
    return wanted;
  }
  // Les bornes du palier tenu, élargies de la marge des deux côtés.
  const std::size_t tier = *held;
  const float nearest =
      tier == 0 ? 0.0F : kPointShadowTierReachWorld[tier - 1] * (1.0F - kPointShadowTierMargin);
  const bool beyond =
      tier < kPointShadowTierReachWorld.size() &&
      distance_world >= kPointShadowTierReachWorld[tier] * (1.0F + kPointShadowTierMargin);
  return distance_world >= nearest && !beyond ? tier : wanted;
}

std::vector<std::optional<PointShadowSlots::Assignment>> PointShadowSlots::Assign(
    std::span<const PointLight> lights, glm::vec3 eye) {
  ++round_;
  std::vector<std::optional<Assignment>> assigned(lights.size());
  const auto casts = [](const PointLight& light) {
    return light.casts_shadow && light.shadow_depth > 0.0F && light.range_world > 0.0F;
  };

  // LES CARTES GARDÉES : une lumière qui tient déjà une carte d'un palier où
  // elle a encore sa place la reprend telle quelle.
  for (std::size_t rank = 0; rank < lights.size(); ++rank) {
    if (!casts(lights[rank])) {
      continue;
    }
    const PaintedPointLight painted = PaintedOf(lights[rank]);
    const float distance_world = glm::length(lights[rank].world_position - eye);
    for (std::size_t tier = 0; tier < kPointShadowTierCount && !assigned[rank]; ++tier) {
      std::vector<Slot>& slots = tiers_[tier];
      for (std::size_t layer = 0; layer < slots.size(); ++layer) {
        Slot& slot = slots[layer];
        if (slot.painted != painted || slot.last_round == round_) {
          continue;
        }
        if (PointShadowTierOf(distance_world, tier) == tier) {
          slot.last_round = round_;
          assigned[rank] = Assignment{.slot = {.tier = static_cast<std::int32_t>(tier),
                                               .layer = static_cast<std::uint32_t>(layer)},
                                      .fresh = false};
        }
        break;
      }
    }
  }

  // LES CARTES DONNÉES, aux plus proches d'abord : dans le palier demandé, ou
  // le suivant s'il est plein. Une carte libre passe avant la plus anciennement
  // donnée, qu'aucune lumière de l'image n'a reprise.
  for (std::size_t rank = 0; rank < lights.size(); ++rank) {
    if (!casts(lights[rank]) || assigned[rank]) {
      continue;
    }
    const PaintedPointLight painted = PaintedOf(lights[rank]);
    const float distance_world = glm::length(lights[rank].world_position - eye);
    for (std::size_t tier = PointShadowTierOf(distance_world); tier < kPointShadowTierCount;
         ++tier) {
      std::vector<Slot>& slots = tiers_[tier];
      std::optional<std::size_t> chosen;
      for (std::size_t layer = 0; layer < slots.size(); ++layer) {
        const Slot& slot = slots[layer];
        if (slot.last_round == round_) {
          continue;
        }
        if (slot.painted == painted) {
          chosen = layer;
          break;
        }
        const auto older = [&slots](std::size_t left, std::size_t right) {
          if (slots[left].painted.has_value() != slots[right].painted.has_value()) {
            return !slots[left].painted.has_value();
          }
          return slots[left].last_round < slots[right].last_round;
        };
        if (!chosen.has_value() || older(layer, *chosen)) {
          chosen = layer;
        }
      }
      // UNE COUCHE NEUVE, tant que le palier n'est pas plein, plutôt que de
      // reprendre une carte qui porte encore une lumière.
      const bool reusable = chosen.has_value() && (slots[*chosen].painted == painted ||
                                                   !slots[*chosen].painted.has_value());
      if (!reusable && slots.size() < kPointShadowTierMaxLayers.at(tier)) {
        slots.emplace_back();
        chosen = slots.size() - 1;
      }
      if (!chosen.has_value()) {
        continue;
      }
      Slot& slot = slots[*chosen];
      const bool fresh = slot.painted != painted;
      slot.painted = painted;
      slot.last_round = round_;
      assigned[rank] = Assignment{.slot = {.tier = static_cast<std::int32_t>(tier),
                                           .layer = static_cast<std::uint32_t>(*chosen)},
                                  .fresh = fresh};
      break;
    }
  }
  return assigned;
}

float PointShadowFar(float range_world) {
  constexpr float kNearestFarShare = 2.0F;
  return std::max(range_world, kPointShadowClearanceWorld * kNearestFarShare);
}

std::array<glm::mat4, kCubeFaceCount> PointShadowFaceViewProjections(glm::vec3 light_position,
                                                                     float far_world) {
  const glm::mat4 projection = glm::perspective(glm::radians(kFaceFieldDegrees), kFaceAspectRatio,
                                                kPointShadowClearanceWorld, far_world);
  std::array<glm::mat4, kCubeFaceCount> faces{};
  for (std::size_t face = 0; face < kCubeFaceCount; ++face) {
    faces[face] = projection * glm::lookAt(light_position, light_position + kFaceAxes[face].forward,
                                           kFaceAxes[face].up);
  }
  return faces;
}

glm::vec3 CubeFaceForward(std::size_t face) { return kFaceAxes.at(face).forward; }

float PointShadowDepth(float axis_distance_world, float far_world) {
  // La profondeur normalisée d'une perspective, de −1 à 1, ramenée de 0 à 1.
  const float near_world = kPointShadowClearanceWorld;
  const float normalized =
      ((far_world + near_world) / (far_world - near_world)) -
      ((2.0F * far_world * near_world) / ((far_world - near_world) * axis_distance_world));
  return (normalized * 0.5F) + 0.5F;
}

}  // namespace ro::render
