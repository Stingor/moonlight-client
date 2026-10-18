#include "world/map_sprites.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <limits>
#include <numbers>

#include <glm/glm.hpp>

#include "ro/mapedit/map_sprite.h"
#include "ro/render/map_effect_math.h"
#include "ro/render/sprite_frame.h"
#include "ro/render/terrain_mesh.h"

namespace moonclient {
namespace {

constexpr double kMillisecondsPerSecond = 1000.0;
constexpr float kDegreesPerHalfTurn = 180.0F;
constexpr float kOpaqueAlpha = 255.0F;

// Deux noms égaux à la casse près : la table écrit « SAVAGE » comme « savage ».
[[nodiscard]] bool SameName(std::string_view first, std::string_view second) {
  return first.size() == second.size() &&
         std::ranges::equal(first, second, [](char left, char right) {
           return std::tolower(static_cast<unsigned char>(left)) ==
                  std::tolower(static_cast<unsigned char>(right));
         });
}

}  // namespace

void RollQuads(ro::render::ComposedActor& composed, float degrees) {
  if (composed.quads.empty() || degrees == 0.0F) {
    return;
  }
  glm::vec2 lowest(std::numeric_limits<float>::max());
  glm::vec2 highest(std::numeric_limits<float>::lowest());
  for (const ro::render::SpriteQuad& quad : composed.quads) {
    for (const glm::vec2& corner : quad.corners) {
      lowest = glm::min(lowest, corner);
      highest = glm::max(highest, corner);
    }
  }
  const glm::vec2 centre = (lowest + highest) * 0.5F;
  // L'ordonnée des coins descend : la rotation directe y tourne dans le sens
  // des aiguilles d'une montre.
  const float radians = degrees * std::numbers::pi_v<float> / kDegreesPerHalfTurn;
  const float cosine = std::cos(radians);
  const float sine = std::sin(radians);
  for (ro::render::SpriteQuad& quad : composed.quads) {
    for (glm::vec2& corner : quad.corners) {
      const glm::vec2 offset = corner - centre;
      corner = centre +
               glm::vec2(offset.x * cosine - offset.y * sine, offset.x * sine + offset.y * cosine);
    }
  }
}

std::string MapSpriteName(const ro::render::SpriteCatalog& catalog, std::string_view typed) {
  if (const std::optional<std::uint32_t> number = ro::mapedit::SpriteClassNumber(typed)) {
    if (const auto named = catalog.tables.class_sprites.find(*number);
        named != catalog.tables.class_sprites.end() && !named->second.empty()) {
      return named->second;
    }
  }
  return std::string(typed);
}

std::optional<std::uint32_t> MapSprites::ClassOf(const ro::render::SpriteCatalog& catalog,
                                                 std::string_view sprite) {
  if (const std::optional<std::uint32_t> number = ro::mapedit::SpriteClassNumber(sprite)) {
    return number;
  }
  if (const auto known = classes_.find(sprite); known != classes_.end()) {
    return known->second;
  }
  // LA PLUS PETITE CLASSE qui porte ce nom : plusieurs le partagent, et le
  // choix ne doit pas dépendre de l'ordre de la table.
  std::optional<std::uint32_t> found;
  for (const auto& [job, name] : catalog.tables.class_sprites) {
    if (SameName(name, sprite) && (!found.has_value() || job < *found)) {
      found = job;
    }
  }
  return classes_.emplace(std::string(sprite), found).first->second;
}

ro::render::ActorOutfit* MapSprites::OutfitOf(ro::render::ActorWardrobe& wardrobe,
                                              std::string_view sprite) {
  const std::optional<std::uint32_t> job = ClassOf(wardrobe.catalog(), sprite);
  if (!job.has_value()) {
    return nullptr;
  }
  return wardrobe.OutfitFor(ro::render::ActorLook{.job = *job});
}

MapSprites::ActCounts MapSprites::CountsOf(ro::render::ActorWardrobe& wardrobe,
                                           std::string_view sprite, std::uint32_t action) {
  const ro::render::ActorOutfit* outfit = OutfitOf(wardrobe, sprite);
  if (outfit == nullptr || outfit->pieces().empty() ||
      outfit->pieces().front().animation == nullptr) {
    return {};
  }
  const std::size_t actions = outfit->pieces().front().animation->actions().size();
  return ActCounts{.action_count = actions / ro::mapedit::kSpriteDirectionCount,
                   .frame_count = outfit->FrameCount(action)};
}

void MapSprites::Update(const MapSpriteFrame& frame) {
  Forget();
  if (frame.document == nullptr || frame.wardrobe == nullptr || frame.gnd == nullptr ||
      frame.document->sprites().empty()) {
    return;
  }
  const double seconds = static_cast<double>(frame.now_ms) / kMillisecondsPerSecond;
  const ro::render::MapEffectFrame map{frame.gnd->width(), frame.gnd->height()};
  // ⚠ LA PLACE AVANT LES POINTEURS : `drawn_` pointe dans `composed_`, qui ne
  // doit plus bouger.
  composed_.resize(frame.document->sprites().size());
  std::vector<ro::render::DrawnActor> translucent;
  for (std::size_t slot = 0; slot < frame.document->sprites().size(); ++slot) {
    const ro::mapedit::MapSprite& sprite = frame.document->sprites()[slot];
    const float opacity = sprite.opacity.value_or(ro::mapedit::kFullOpacity);
    if (sprite.hidden || !(opacity > 0.0F)) {
      continue;
    }
    ro::render::ActorOutfit* outfit = OutfitOf(*frame.wardrobe, sprite.sprite);
    if (outfit == nullptr) {
      continue;
    }
    const float facing = ro::mapedit::SpriteFacingDegreesAt(sprite, seconds);
    // L'angle de face du client tourne à l'envers des directions du .act.
    const std::size_t direction =
        ro::render::SpriteDirectionOfDegrees(-facing, frame.camera_yaw_degrees, true);
    const std::size_t image = ro::mapedit::SpriteFrameAt(
        sprite, seconds, outfit->FrameCount(sprite.action), outfit->FrameIntervalMs(sprite.action));
    ro::render::ComposedActor& composed = composed_[slot];
    composed = outfit->Composed(sprite.action, direction, 0.0F, true, image);
    RollQuads(composed, ro::mapedit::SpriteRollDegreesAt(sprite, seconds));
    if (composed.quads.empty()) {
      continue;
    }
    const glm::vec3 placed =
        map.ToWorld(glm::vec3(sprite.position[0], sprite.position[1], sprite.position[2]));
    std::array<std::uint8_t, 4> tint = {
        255, 255, 255, static_cast<std::uint8_t>(std::lround(kOpaqueAlpha * opacity))};
    if (frame.light != nullptr) {
      const std::array<float, 2> cell =
          ro::render::FractionalCellAt(*frame.gnd, placed.x, placed.z);
      tint = ro::render::LitTint(tint, frame.light->Under(cell[0], cell[1]), frame.cold_light);
    }
    ro::render::DrawnActor drawn{.actor = &composed,
                                 .shadow = frame.round_shadow && !outfit->Shadow().quads.empty()
                                               ? &outfit->Shadow()
                                               : nullptr,
                                 .ground_position = placed,
                                 .tint = tint,
                                 .scale = sprite.scale};
    if (opacity < ro::mapedit::kFullOpacity) {
      translucent.push_back(drawn);
    } else {
      drawn_.push_back(drawn);
    }
  }
  opaque_count_ = drawn_.size();
  drawn_.insert(drawn_.end(), translucent.begin(), translucent.end());
}

}  // namespace moonclient
