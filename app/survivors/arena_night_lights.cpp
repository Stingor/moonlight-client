#include "survivors/arena_night_lights.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "ro/render/actor_layers.h"
#include "ro/render/terrain_mesh.h"

#include "survivors/arena_night.h"

namespace moonclient {
namespace {

// De `dusk` au premier palier de la nuit à `night` au dernier.
[[nodiscard]] float AlongNight(float share, float dusk, float night) noexcept {
  return dusk + (night - dusk) * share;
}

}  // namespace

float ArenaLanternRadiusCells(std::uint64_t presentation_ms) noexcept {
  const std::optional<float> share = ArenaNightShare(presentation_ms);
  if (!share.has_value()) {
    return 0.0F;
  }
  return AlongNight(*share, kArenaLanternDuskRadiusCells, kArenaLanternNightRadiusCells);
}

std::optional<ro::render::PointLight> ArenaLantern(std::uint64_t presentation_ms,
                                                   glm::vec3 hero_feet_world) {
  const std::optional<float> share = ArenaNightShare(presentation_ms);
  if (!share.has_value()) {
    return std::nullopt;
  }
  ro::render::PointLight lantern;
  lantern.world_position = hero_feet_world + glm::vec3(0.0F, kArenaLanternHeightWorld, 0.0F);
  lantern.color = kArenaLanternColor *
                  AlongNight(*share, kArenaLanternDuskStrength, kArenaLanternNightStrength);
  lantern.range_world = ArenaLanternRadiusCells(presentation_ms) * ro::render::kWorldUnitsPerCell;
  lantern.casts_shadow = false;
  return lantern;
}

std::optional<glm::vec3> ArenaHeroFeet(std::span<const ro::render::DrawnActor> drawn,
                                       std::span<const HordeTag> tags) {
  const std::size_t count = std::min(drawn.size(), tags.size());
  for (std::size_t index = 0; index < count; ++index) {
    if (tags[index].kind == HordeBodyKind::kHero) return drawn[index].ground_position;
  }
  return std::nullopt;
}

float ArenaItemHaloAlpha(std::uint64_t presentation_ms) noexcept {
  return AlongNight(ArenaNightShare(presentation_ms).value_or(0.0F), kArenaItemHaloDayAlpha,
                    kArenaItemHaloNightAlpha);
}

void ArenaItemHalos(std::span<const ro::render::DrawnActor> drawn, std::span<const HordeTag> tags,
                    std::uint64_t presentation_ms, std::vector<ro::render::DrawnActor>& out) {
  out.clear();
  const float halo_alpha = ArenaItemHaloAlpha(presentation_ms);
  const std::size_t count = std::min(drawn.size(), tags.size());
  for (std::size_t index = 0; index < count; ++index) {
    const HordeBodyKind kind = tags[index].kind;
    if (kind != HordeBodyKind::kGem && kind != HordeBodyKind::kPickup) continue;
    const ro::render::DrawnActor& item = drawn[index];
    ro::render::DrawnActor halo = item;
    halo.shadow = nullptr;
    halo.shadow_ground_height.reset();
    halo.additive_parts = ro::render::kAllActorParts;
    halo.doubles_weapon = false;
    halo.fogged = false;
    halo.grow_screen_pixels = kArenaItemHaloScreenPixels;
    // UN OBJET QUI FOND emporte sa lueur : elle prend sa transparence.
    const float item_alpha = static_cast<float>(item.tint[3]) / kArenaOpaqueAlpha;
    halo.tint = {item.tint[0], item.tint[1], item.tint[2],
                 static_cast<std::uint8_t>(std::lround(halo_alpha * item_alpha))};
    out.push_back(halo);
  }
}

}  // namespace moonclient
