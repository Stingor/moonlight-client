// Le Warp Agent sur la carte du monde. Voir `world_map_flight.h`.
//
// La place suit l'image ; les marges et la taille du texte restent en pixels
// d'écran.

#include <algorithm>

#include "ro/core/i18n.h"

#include "world_map_flight.h"
#include "world_map_marks.h"

namespace ro::ui {
namespace {

// La figure ne monte jamais plus haut que six pixels sous le haut de l'image,
// plus sa hauteur.
constexpr float kSpriteTopRoom = 6.0F;
// Le nom, à quatre pixels à droite des pieds, en blanc cerné de noir.
constexpr float kLabelGap = 4.0F;
constexpr float kLabelFontPixels = 12.0F;
constexpr ImU32 kLabelColor = IM_COL32(255, 255, 255, 255);
constexpr ImU32 kLabelOutline = IM_COL32(0, 0, 0, 255);

}  // namespace

WorldMapWarpAgentPlace PlaceWorldMapWarpAgent(const WorldMapWarpAgentView& agent,
                                              const WorldMapRect& town, float image_top,
                                              float font_pixels) {
  const float feet_y = std::max(town.top, image_top + agent.sprite_height + kSpriteTopRoom);
  return WorldMapWarpAgentPlace{
      .sprite_corner = ImVec2(town.right - agent.sprite_feet.x, feet_y - agent.sprite_feet.y),
      .label_at =
          ImVec2(town.right + kLabelGap, feet_y - (agent.sprite_height + font_pixels) * 0.5F)};
}

void DrawWorldMapWarpAgent(ImDrawList* into, const WorldMapFit& fit, const WorldMapView& view,
                           std::string_view hovered_map) {
  // LA VILLE DÉSIGNÉE, si elle a un Warp Agent. Sa vue est celle de l'image
  // précédente : une autre ville attend la sienne.
  if (!view.warp_agent.has_value() || hovered_map.empty() ||
      view.warp_agent->town_map != hovered_map) {
    return;
  }
  const WorldMapWarpAgentView& agent = *view.warp_agent;
  const WorldMapRect town{.left = fit.origin.x + agent.town.left * fit.scale,
                          .top = fit.origin.y + agent.town.top * fit.scale,
                          .right = fit.origin.x + agent.town.right * fit.scale,
                          .bottom = fit.origin.y + agent.town.bottom * fit.scale};
  const WorldMapWarpAgentPlace place =
      PlaceWorldMapWarpAgent(agent, town, fit.origin.y, kLabelFontPixels);
  if (agent.sprite.usable()) {
    into->AddImage(agent.sprite.id, place.sprite_corner,
                   ImVec2(place.sprite_corner.x + agent.sprite.width,
                          place.sprite_corner.y + agent.sprite.height));
  }
  WriteWorldMapOutlined(into, kLabelFontPixels, place.label_at, kLabelColor, kLabelOutline,
                        core::Tr("Warp Agent"));
}

}  // namespace ro::ui
