// Ce que le combat du champ de bataille montre. Voir l'en-tête.

#include "world/battlefield_view.h"

#include "ro/render/actor_pick.h"
#include "ro/sim/map_mode.h"
#include "ro/sim/world.h"

#include "world/actor_visibility.h"

namespace moonclient {
namespace {

// LES COULEURS DES CAMPS : le lion chaud, l'aigle froid, pour qu'un coup d'œil
// les sépare ; un camp que la règle ne tranche pas encore reste gris.
constexpr ImU32 kLionColor = IM_COL32(255, 150, 40, 255);
constexpr ImU32 kEagleColor = IM_COL32(70, 170, 255, 255);
constexpr ImU32 kUnknownSideColor = IM_COL32(200, 200, 200, 255);

[[nodiscard]] ro::render::BattleScoreHighlight HighlightOf(ro::sim::BattleSide side) {
  switch (side) {
    case ro::sim::BattleSide::kLion:
      return ro::render::BattleScoreHighlight::kLion;
    case ro::sim::BattleSide::kEagle:
      return ro::render::BattleScoreHighlight::kEagle;
    case ro::sim::BattleSide::kUnknown:
      break;
  }
  return ro::render::BattleScoreHighlight::kNone;
}

}  // namespace

std::vector<ro::render::ScreenQuad> EnemyMarkersOnScreen(
    const ro::render::Camera& camera, float aspect_ratio,
    std::span<const ro::render::DrawnActor> drawn, std::span<const std::uint32_t> ids,
    const ro::sim::World& world, const ro::render::BattlefieldArt& art, float viewport_width,
    float viewport_height) {
  std::vector<ro::render::ScreenQuad> markers;
  if (!world.map_mode().battlefield || art.enemy_marker.empty()) {
    return markers;
  }
  const ro::sim::Battlefield& field = world.battlefield();
  // LE DÉCALAGE SUIT LE RECUL ET LA TAILLE DU CORPS, comme les jauges et la
  // bulle : l'échelle d'ancrage, sans la part de la fenêtre, que la marque
  // compte déjà dans son décalage.
  const float apparent = ro::render::ApparentScale(camera);
  const glm::vec2 viewport{viewport_width, viewport_height};
  for (std::size_t index = 0; index < drawn.size() && index < ids.size(); ++index) {
    if (!field.IsEnemy(ids[index])) {
      continue;
    }
    const ro::sim::Actor* actor = world.Find(ids[index]);
    if (actor == nullptr || IsHidden(*actor, world)) {
      continue;
    }
    const std::optional<glm::vec2> feet = ro::render::ProjectToScreen(
        camera, aspect_ratio, drawn[index].ground_position, viewport_width, viewport_height);
    if (!feet.has_value()) {
      continue;
    }
    const float height_scale = ro::render::ScreenAnchorScale(ro::render::kClientReferenceHeight,
                                                             apparent, drawn[index].scale);
    markers.push_back(
        ro::render::ComposeEnemyMarker(art.enemy_marker, *feet, viewport, height_scale));
  }
  return markers;
}

std::optional<ro::render::BattleScoreFigures> BattleScoreOf(const ro::sim::World& world,
                                                            bool highlight_own) {
  if (!world.map_mode().battlefield) {
    return std::nullopt;
  }
  const ro::sim::Battlefield& field = world.battlefield();
  const ro::sim::BattleScore& score = field.score();
  if (!score.shown()) {
    return std::nullopt;
  }
  return ro::render::BattleScoreFigures{.lion = score.lion,
                                        .eagle = score.eagle,
                                        .highlight = highlight_own
                                                         ? HighlightOf(field.own_side())
                                                         : ro::render::BattleScoreHighlight::kNone};
}

ImU32 BattleSideColor(ro::sim::BattleSide side) {
  switch (side) {
    case ro::sim::BattleSide::kLion:
      return kLionColor;
    case ro::sim::BattleSide::kEagle:
      return kEagleColor;
    case ro::sim::BattleSide::kUnknown:
      break;
  }
  return kUnknownSideColor;
}

}  // namespace moonclient
