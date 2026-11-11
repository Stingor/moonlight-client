#include "effects/effect_ground_scope.h"

#include <algorithm>
#include <utility>

#include "ro/render/effect_frame.h"
#include "ro/render/ground_scope.h"

namespace moonclient {
namespace {

// Le dossier où vivent les images que le client nomme depuis `effect\`.
constexpr std::string_view kTextureFolder = "data\\texture\\";

}  // namespace

void GroundScopes::Place(std::uint16_t skill_id, glm::vec3 ground_position,
                         std::uint32_t started_ms, std::uint32_t duration_ms,
                         bool minimal_effects) {
  const std::int32_t side = ro::render::GroundScopeSide(skill_id);
  if (side <= 0) {
    return;
  }
  scopes_.push_back(Scope{
      .skill_id = skill_id,
      .side = side,
      .ground_position = ground_position,
      .started_ms = started_ms,
      .ends_ms = started_ms + duration_ms,
      .texture_path = std::string(kTextureFolder) +
                      std::string(ro::render::GroundScopeTexture(skill_id, minimal_effects))});
}

std::span<const DrawnGroundCells> GroundScopes::Compose(std::uint32_t now_ms) {
  // LE CLIENT LE LÈVE quand l'heure de fin est passée.
  std::erase_if(scopes_, [now_ms](const Scope& scope) { return scope.ends_ms < now_ms; });
  cell_lots_.clear();
  drawn_.clear();
  for (const Scope& scope : scopes_) {
    const ro::formats::Image& texture = TextureFor(scope.texture_path);
    if (texture.empty()) {
      continue;
    }
    const std::uint32_t ticks =
        now_ms >= scope.started_ms
            ? (now_ms - scope.started_ms) / static_cast<std::uint32_t>(ro::render::kEffectTickMs)
            : 0;
    const float angle = ro::render::GroundScopeAngle(scope.side, ticks);
    cell_lots_.push_back(ro::render::ComposeGroundScopeCells(
        scope.side, angle, ro::render::GroundScopeColor(scope.skill_id, angle)));
    drawn_.push_back(DrawnGroundCells{
        {}, scope.ground_position, &textures_.find(scope.texture_path)->first, &texture});
  }
  // Les cases se rattachent une fois les deux listes finies de grandir.
  for (std::size_t index = 0; index < drawn_.size(); ++index) {
    drawn_[index].cells = cell_lots_[index];
  }
  return drawn_;
}

const ro::formats::Image& GroundScopes::TextureFor(const std::string& path) {
  if (const auto found = textures_.find(path); found != textures_.end()) {
    return found->second;
  }
  ro::formats::Image image;
  if (auto raw = vfs_->Read(path); raw) {
    if (auto decoded = ro::formats::DecodeImage(path, *raw); decoded) {
      image = std::move(*decoded);
      ro::formats::ApplyEffectTextureColorKey(image);
      ro::formats::QuantizeToEffectTextureDepth(image);
    }
  }
  return textures_.emplace(path, std::move(image)).first->second;
}

}  // namespace moonclient
