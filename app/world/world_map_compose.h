#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/formats/navi_tables.h"
#include "ro/sim/navi_search.h"
#include "ro/sim/party_roster.h"
#include "ro/ui/world_map_window.h"
#include "ro/vfs/map_display_names.h"
#include "ro/vfs/warp_agents.h"
#include "ro/vfs/world_view.h"

#include "world/minimap_compose.h"
#include "world/world_map_warp_agent.h"

namespace moonclient {

// UN MEMBRE DU GROUPE EN LIGNE, et la carte où il est, sans extension. Hors
// groupe, soi-même seul.
struct WorldMapMember {
  std::string_view name;
  std::string_view map_name;
  bool self = false;
};

// LES MEMBRES EN LIGNE DU GROUPE, ou soi-même seul hors groupe. Les noms de
// cartes, sans extension et en minuscules, vont dans `maps`, qui doit durer
// autant que la liste.
[[nodiscard]] std::vector<WorldMapMember> WorldMapPartyMembers(const ro::sim::PartyRoster& party,
                                                               std::uint32_t own_account_id,
                                                               std::string_view own_name,
                                                               std::string_view here_map,
                                                               std::vector<std::string>& maps);

// CE QUE LA CARTE DU MONDE LIT, pour une image.
struct WorldMapSources {
  const ro::vfs::WorldViewTables* tables = nullptr;
  // La carte où l'on est, par son nom interne.
  std::string_view here_map;
  // Les cartes de l'itinéraire, la destination en dernier : voir
  // `WorldRouteMaps`. Vide : aucun.
  std::span<const std::string> route;
  // Les noms affichés des cartes, sur les étiquettes des escales. Nul : ceux
  // de la carte du monde.
  const ro::vfs::MapDisplayNames* map_names = nullptr;
  // La carte de l'étape en cours du guidage : voir `WorldRouteStepMap`. Vide :
  // aucune.
  std::string_view step_map;
  // Le groupe, chacun sur sa carte.
  std::span<const WorldMapMember> party;
  // La carte que la souris désignait à l'image précédente : son aperçu, et son
  // Warp Agent si elle en a un.
  std::string_view preview_map;
  // Les cartes qui ont un Warp Agent, et sa figure composée pour cette image.
  // Nul : aucun.
  const ro::vfs::WarpAgents* warp_agents = nullptr;
  const WorldMapWarpAgent::Shown* warp_agent_sprite = nullptr;
  // Les tables de la navigation : les monstres de chaque carte. Nul : aucun.
  const ro::formats::NaviData* navi = nullptr;
  // La dernière recherche de la navigation : ses cartes se surlignent.
  std::span<const ro::sim::NaviHitGroup> search;
  // Le monde choisi dans la liste ; négatif : celui de la carte où l'on est,
  // et le premier si elle n'est sur aucun.
  int chosen_world = -1;
  // Une image sous la racine d'interface, par son nom.
  MinimapImageAt image_at;
};

// ── LA RECHERCHE SUR LA CARTE DU MONDE ─────────────────────────────────────
//
// Chaque carte que la recherche de la navigation trouve porte un COMPTE : zéro
// pour la carte elle-même, le nombre d'un monstre qui y apparaît, et la marque
// d'un PNJ ajoutée par-dessus. Plusieurs résultats sur une carte : la marque
// PNJ s'ajoute une fois, et le compte garde le plus grand nombre.
inline constexpr std::int32_t kWorldSearchNpcMark = 10000;

// Porte un résultat sur le compte d'une carte, que le premier a créé.
void AddWorldSearchHit(std::int32_t& kept, std::int32_t count);

// LE PALIER DE TEINTE d'un compte : 0 la carte seule, 1 à 5 les monstres —
// un à quatre, cinq à dix-neuf, vingt à quarante-neuf, cinquante à quatre-vingt-
// dix-neuf, cent et plus —, et les mêmes à partir de 10 pour une carte qui
// porte aussi un PNJ : 10 pour le PNJ seul.
inline constexpr int kWorldSearchNpcShade = 10;
[[nodiscard]] int WorldSearchShade(std::int32_t count);

// COMPOSE LA VUE DE LA CARTE DU MONDE dans des tampons qui durent d'une image à
// l'autre : la vue n'en garde que des tranches.
class WorldMapComposer {
 public:
  [[nodiscard]] const ro::ui::WorldMapView& Compose(const WorldMapSources& sources);

 private:
  void ComposeRoute(const WorldMapSources& sources, std::size_t shown);
  void ComposeAirLines(const WorldMapSources& sources, const ro::vfs::WorldView& world);
  void ComposeParty(const WorldMapSources& sources, std::size_t shown);
  void ComposeWarpAgent(const WorldMapSources& sources, const ro::vfs::WorldView& world);

  std::vector<const char*> world_names_;
  std::vector<ro::ui::WorldMapMonsterView> monsters_;
  std::vector<ro::ui::WorldMapAreaView> areas_;
  std::vector<ro::ui::WorldMapAreaView> floors_;
  std::vector<ro::ui::WorldMapDungeonView> dungeons_;
  std::vector<ro::ui::WorldMapRouteStep> route_;
  std::vector<ro::ui::WorldMapAirStop> air_stops_;
  std::vector<ro::ui::WorldMapAirTour> air_tours_;
  std::vector<ro::ui::WorldMapAirLine> air_lines_;
  std::vector<std::string> member_names_;
  std::vector<ro::ui::WorldMapMemberView> members_;
  std::vector<ro::ui::WorldMapPartyView> party_;
  ro::ui::WorldMapView view_;
};

}  // namespace moonclient
