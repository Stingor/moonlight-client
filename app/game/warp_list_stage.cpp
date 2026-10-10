// Les destinations d'une compétence de déplacement, dans la boucle. Voir
// l'en-tête.

#include "game/warp_list_stage.h"

#include <cstddef>
#include <span>
#include <string>
#include <string_view>

#include "ro/core/log.h"
#include "ro/net/warp_list.h"
#include "ro/sim/warp_menu.h"
#include "ro/sim/world.h"
#include "ro/vfs/warp_list_words.h"

namespace moonclient {

void StageWarpList(ro::sim::World& world, const ro::vfs::ItemWords& words,
                   const ro::vfs::MapDisplayNames* map_names, UiStage::Windows& windows) {
  ro::sim::WarpMenu& menu = world.warp_menu();
  menu.CloseIfMapChanged(world.map_entries());
  windows.warp_list.open = menu.open();
  if (!menu.open()) {
    return;
  }
  // LES LIGNES NE SE RECOMPOSENT QU'À UNE LISTE NOUVELLE : ce sont elles que la
  // vue désigne, et elles ne changent pas tant que la question tient.
  if (windows.warp_list_view.offer_seq != menu.offer_seq() || windows.warp_list_rows.empty()) {
    windows.warp_list_rows.clear();
    for (const std::string& destination : menu.offer().destinations) {
      windows.warp_list_rows.push_back(
          ro::vfs::WarpDestinationLabel(words, map_names, destination));
    }
    // LA DERNIÈRE LIGNE EST L'ANNULATION, que le client ajoute de lui-même.
    windows.warp_list_rows.push_back(
        ro::vfs::WarpDestinationLabel(words, map_names, ro::net::kWarpCancelName));
  }
  windows.warp_list_view.title = ro::vfs::WarpListTitle(words);
  windows.warp_list_view.rows = windows.warp_list_rows;
  windows.warp_list_view.offer_seq = menu.offer_seq();
}

void ApplyWarpList(const ro::ui::WarpListWindowAction& asked, ro::net::Connection& link,
                   ro::sim::World& world) {
  ro::sim::WarpMenu& menu = world.warp_menu();
  if (asked.verb == ro::ui::WarpListVerb::kNothing || !menu.open()) {
    return;
  }
  const std::vector<std::string>& destinations = menu.offer().destinations;
  // 🔴 LA LIGNE CHOISIE REND LE NOM TEL QUE LE SERVEUR L'A ÉCRIT, et la ligne
  // d'annulation — la dernière — le mot d'annulation, comme le bouton.
  std::string_view destination = ro::net::kWarpCancelName;
  if (asked.verb == ro::ui::WarpListVerb::kChoose && asked.row >= 0 &&
      static_cast<std::size_t>(asked.row) < destinations.size()) {
    destination = destinations[static_cast<std::size_t>(asked.row)];
  }
  if (auto sent = ro::net::RequestWarpPoint(link, menu.offer().skill_id, destination); !sent) {
    ro::core::LogWarning("destination : choix non émis — {}", ro::core::Describe(sent.error()));
  }
  menu.Close();
}

}  // namespace moonclient
