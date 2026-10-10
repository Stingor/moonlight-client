#include "ro/render/name_plate_layout.h"

#include <algorithm>
#include <cmath>

namespace ro::render {

NamePlateLayout PlaceNamePlate(float anchor_x, float text_top_y, float widest_line_px,
                               std::size_t line_count, float line_step_px, NamePlateLead lead) {
  // UN SEUL ARRONDI DU HAUT, celui de la ligne de base : le texte et l'image de
  // tête se lisent sur la même grille, et l'écart entre eux ne tremble pas
  // quand la caméra bouge.
  const float text_top = std::round(text_top_y);
  const float widget_top = text_top - kNamePlateTopPixels;
  // L'ANCRE TRONQUÉE, LA DEMI-LARGEUR PAR DIVISION ENTIÈRE : c'est ainsi que le
  // client centre son widget, et un arrondi de l'ensemble le poserait un pixel
  // plus à gauche pour toute largeur impaire.
  const float anchor = std::trunc(anchor_x);
  const auto left_of = [anchor](float width) { return anchor - std::floor(width / 2.0F); };
  const float framed_text = std::max(std::round(widest_line_px) + 2.0F * kNamePlateMarginPixels,
                                     kNamePlateLeastWidthPixels);

  NamePlateLayout placed;
  placed.text_top_y = text_top;
  switch (lead) {
    case NamePlateLead::kNone: {
      placed.text_left_x = left_of(framed_text) + kNamePlateMarginPixels;
      break;
    }
    case NamePlateLead::kBossBadge: {
      const float left = left_of(framed_text + kBossBadgeSizePixels);
      placed.lead_min = glm::vec2{left, widget_top};
      placed.text_left_x = left + kBossBadgeSizePixels + kNamePlateMarginPixels;
      break;
    }
    case NamePlateLead::kEmblem: {
      const float left = left_of(framed_text + kEmblemWidgetGrowPixels);
      // LA HAUTEUR DU WIDGET couvre les lignes et leurs marges, et au moins la
      // case de l'emblème : c'est sur elle que l'emblème se centre.
      const float lines_height = static_cast<float>(line_count) * line_step_px +
                                 kNamePlateTopPixels + kNamePlateBottomPixels;
      const float height = std::max(lines_height, kEmblemSlotPixels);
      placed.lead_min = glm::vec2{left + kEmblemLeftPixels,
                                  widget_top + std::floor((height - kEmblemSlotPixels) / 2.0F)};
      placed.text_left_x = left + kEmblemLeftPixels + kEmblemTextShiftPixels;
      break;
    }
  }
  return placed;
}

}  // namespace ro::render
