#include "game/map_plan_tip.h"

#include <string>

#include "ro/render/game_images.h"
#include "ro/render/skin_pieces.h"

#include "game/game_pieces.h"
#include "game/skin_image_of.h"

namespace moonclient {
namespace {

constexpr std::string_view kMapPlanFolder = "map\\";
constexpr std::string_view kMapPlanExtension = ".bmp";

}  // namespace

ro::ui::SkinImage MapPlanOf(std::string_view map, GamePieces& pieces) {
  if (!pieces.game_images.has_value() || map.empty()) {
    return {};
  }
  std::string path(ro::render::kSkinRoot);
  path += kMapPlanFolder;
  path += map;
  path += kMapPlanExtension;
  return SkinImageOf(pieces.game_images->GetOurs(path));
}

ro::ui::NavigationPlaceTip PlaceTipOf(const PlaceLink& place, GamePieces& pieces) {
  ro::ui::NavigationPlaceTip tip = PlaceTipText(
      place, pieces.map_display_names.has_value() ? &*pieces.map_display_names : nullptr);
  tip.map_image = MapPlanOf(place.map, pieces);
  return tip;
}

}  // namespace moonclient
