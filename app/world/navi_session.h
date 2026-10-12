#pragma once

#include <cstdint>
#include <filesystem>
#include <future>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "ro/formats/navi_tables.h"
#include "ro/sim/map_guidance.h"
#include "ro/sim/navi_guide.h"
#include "ro/sim/navigation_request.h"
#include "ro/vfs/town_info.h"
#include "ro/vfs/vfs.h"

#include "world/minimap_compose.h"
#include "world/monster_sprite.h"
#include "world/navi_file.h"
#include "world/navi_goal_terrain.h"
#include "world/navi_trail_view.h"
#include "world/navigation_compose.h"
#include "world/world_map_compose.h"
#include "world/world_map_warp_agent.h"

namespace moonclient {

// LA NAVIGATION D'UNE PARTIE : les données du graphe, le guidage en cours, les
// repères du serveur, et ce que le joueur a réglé.
//
// ⚠ LA TABLE SE LIT EN ARRIÈRE-PLAN. Quelques mégaoctets de texte :
// la lire sur le fil de rendu figerait l'entrée en jeu. Le
// fil de lecture monte SA PROPRE archive — un `Vfs` n'est pas fait pour être lu
// de deux fils à la fois —, et la partie adopte le résultat quand il est prêt.
// Une demande de guidage faite avant attend (`NaviGuideStatus::kPending`).
class NaviSession {
 public:
  NaviSession() = default;
  NaviSession(const NaviSession&) = delete;
  NaviSession& operator=(const NaviSession&) = delete;
  ~NaviSession();

  // Ce que le serveur montre sur la carte, rempli par la boucle réseau.
  ro::sim::MapGuidance guidance;
  ro::sim::NaviGuide guide;
  NaviPrefs prefs;
  // Les vues des deux fenêtres, composées dans des tampons qui durent d'une
  // image à l'autre.
  MinimapComposer minimap_view;
  NavigationComposer navigation_view;
  WorldMapComposer world_map_view;
  // La trace semée au sol le long du chemin, et les images de son jeu de
  // flèches.
  NaviTrailView trail_view;
  // L'aperçu du PNJ ou du monstre choisi dans la fenêtre, de face et animé —
  // en 3D pour une classe qui se dessine en modèle (Emperium, gardiens).
  // Vide sans garde-robe : l'aperçu montre alors le plan de la carte.
  std::optional<MonsterSprite> preview_sprite;
  // L'entrée de carte où l'aperçu a été composé : la garde-robe se vide à
  // chaque entrée, et l'aperçu doit alors tout oublier.
  std::uint32_t preview_map_entries = 0;
  // LA SILHOUETTE D'UN PNJ dans l'infobulle d'une recherche `<NAVS>` du chat,
  // posée au premier survol. ⚠ À PART DE L'APERÇU : la fenêtre peut composer le
  // sien dans la même image, et un seul sprite changerait de classe deux fois.
  std::optional<MonsterSprite> tip_sprite;
  std::uint32_t tip_map_entries = 0;
  // LE WARP AGENT DE LA CARTE DU MONDE, au survol d'une ville qui en a un, et
  // l'entrée de carte où il a été composé.
  std::optional<WorldMapWarpAgent> warp_agent;
  std::uint32_t warp_agent_map_entries = 0;

  // Relit les préférences du joueur, puis lance la lecture des tables — une
  // fois par partie.
  //
  // ⚠ DEUX DOSSIERS, PAS UN : les préférences vivent sous `client_root`, les
  // tables viennent des archives de `mount`. Ils ne se confondent qu'en jeu
  // ordinaire ; un client d'essai lit les archives d'une installation dont il
  // ne doit pas réécrire les réglages.
  void Open(const std::filesystem::path& client_root, const ro::vfs::MountOptions& mount);
  // L'archive de la partie, lue sur le fil de rendu quand le guidage calcule
  // un itinéraire : la grille de la carte visée. Nulle : une case ou un PNJ
  // d'une autre carte n'a pas d'itinéraire.
  void UseArchive(ro::vfs::Vfs* vfs);
  // Adopte les tables quand le fil les a lues. À chaque image, sans coût tant
  // que rien n'est prêt. `here_map` est la carte où l'on se tient : la
  // recherche retenue en attendant les tables y met ses résultats en tête.
  void Poll(std::string_view here_map);
  // Adopte des tables déjà lues, puis lance la recherche retenue. Des tables
  // nulles — une lecture qui a échoué — l'oublient.
  void Adopt(std::shared_ptr<const ro::formats::NaviData> data, std::string_view here_map);

  [[nodiscard]] bool loading() const noexcept { return pending_.valid(); }
  [[nodiscard]] const ro::formats::NaviData* data() const noexcept { return data_.get(); }
  [[nodiscard]] const ro::vfs::TownInfo* towns() const noexcept {
    return towns_.has_value() ? &*towns_ : nullptr;
  }

  // LA RECHERCHE DE LA FENÊTRE, d'où qu'elle vienne — commande, lien du chat,
  // fiche de monstre. `only_map` vide ne borne pas. Avant les tables, elle est
  // retenue et part à leur adoption ; une seule l'est, la dernière demandée.
  void Search(std::string_view term, std::string_view only_map, std::string_view here_map);
  [[nodiscard]] bool search_waiting() const noexcept { return waiting_search_.has_value(); }

  // ── Les demandes, d'où qu'elles viennent ────────────────────────────────────
  // Une case d'une carte — un lien de lieu, un clic sur la minimap. Sans
  // coordonnées sûres (x ou y nul), la carte entière.
  void GuideToPlace(std::string_view map_name, int cell_x, int cell_y);
  // Un PNJ du graphe.
  void GuideToNpc(std::uint32_t npc_id);
  // Une demande du serveur (`navigateto`).
  void GuideFromServer(const ro::sim::NavigationRequest& request);

  // ── Les marqueurs du joueur ──────────────────────────────────────────────────
  [[nodiscard]] std::span<const NaviMemo> MemosOn(std::string_view map_name) const;
  void AddMemo(std::string_view map_name, NaviMemo memo);
  void RemoveMemo(std::string_view map_name, std::size_t rank);

  void SavePrefs() const;

 private:
  struct Loaded {
    std::shared_ptr<const ro::formats::NaviData> data;
    std::optional<ro::vfs::TownInfo> towns;
  };
  struct WaitingSearch {
    std::string term;
    std::string only_map;
  };

  std::filesystem::path client_root_;
  std::optional<WaitingSearch> waiting_search_;
  std::future<Loaded> pending_;
  std::shared_ptr<const ro::formats::NaviData> data_;
  std::optional<ro::vfs::TownInfo> towns_;
  // ⚠ LE GUIDAGE EN GARDE L'ADRESSE : la session ne se copie ni ne se déplace.
  NaviGoalLoader goal_terrain_;
};

}  // namespace moonclient
