// La question des destinations. Voir l'en-tête.

#include "ro/sim/warp_menu.h"

#include <utility>

namespace ro::sim {

void WarpMenu::Offer(std::uint16_t skill_id, std::vector<std::string> destinations,
                     std::uint32_t map_entry) {
  offer_ = WarpOffer{
      .skill_id = skill_id, .destinations = std::move(destinations), .map_entry = map_entry};
  open_ = true;
  ++offer_seq_;
}

}  // namespace ro::sim
