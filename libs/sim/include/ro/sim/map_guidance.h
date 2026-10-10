#pragma once

#include <cstdint>
#include <optional>

#include "ro/sim/map_viewpoints.h"
#include "ro/sim/navigation_request.h"

namespace ro::sim {

// LA CASE DU BOSS qu'un Convex Mirror révèle sur la carte.
struct BossCell {
  std::int32_t x = 0;
  std::int32_t y = 0;
};

// CE QUE LE SERVEUR MONTRE SUR LA CARTE, hors du monde : les repères posés
// (`viewpoint`), les demandes de navigation (`navigateto`) que personne n'a
// encore prises en charge, et le boss révélé.
//
// Le réseau les remplit, l'application les lit. Les repères et le boss se vident
// à chaque changement de carte ; les demandes de navigation, non : elles
// attendent d'être prises.
struct MapGuidance {
  MapViewpoints viewpoints;
  NavigationRequests navigation;
  // Rien quand aucun boss n'est révélé, et pendant qu'il est mort : le serveur
  // n'a alors plus de position à donner.
  std::optional<BossCell> boss;
};

}  // namespace ro::sim
