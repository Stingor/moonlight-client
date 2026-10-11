#pragma once

// LE PLAN D'UNE CARTE dans une infobulle : celui que le radar montre, et
// l'infobulle d'un lieu qui le porte. Partagés par les liens de la conversation
// et par les cartes d'apparition de la fiche de monstre : un lieu se montre de
// la même façon d'où qu'on le survole.

#include <string_view>

#include "ro/ui/navigation_window.h"
#include "ro/ui/skin_art.h"

#include "chat/chat_place_link.h"

namespace moonclient {

struct GamePieces;

// Le plan de cette carte, chargé au besoin ; vide quand il manque.
[[nodiscard]] ro::ui::SkinImage MapPlanOf(std::string_view map, GamePieces& pieces);

// L'infobulle d'un lieu : son nom lisible, sa case, et son plan.
[[nodiscard]] ro::ui::NavigationPlaceTip PlaceTipOf(const PlaceLink& place, GamePieces& pieces);

}  // namespace moonclient
