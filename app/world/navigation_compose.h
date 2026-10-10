#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "ro/formats/navi_tables.h"
#include "ro/sim/navi_guide.h"
#include "ro/sim/navi_search.h"
#include "ro/ui/navigation_window.h"

#include "world/minimap_compose.h"

namespace moonclient {

// Le sprite d'une classe, rendu dans un carré de ce côté en pixels d'écran ;
// une image vide quand la classe n'a pas de sprite — un modèle 3D, une classe
// que l'installation ignore.
using NavigationSpriteAt =
    std::function<ro::ui::SkinImage(std::uint32_t sprite_class, int side_px)>;

// Ce que la fenêtre de navigation lit, en plus des résultats gardés ici.
struct NavigationSources {
  const ro::formats::NaviData* navi = nullptr;
  const ro::sim::NaviGuide* guide = nullptr;
  std::string_view current_map;
  std::string_view current_map_label;
  // La carte de bornage de l'état, son nom lisible.
  std::string_view bound_map_label;
  MinimapImageAt image_at;
  // L'aperçu du détail : la ligne choisie, et de quoi montrer son PNJ ou son
  // monstre. Seule la ligne choisie a un sprite — il s'anime, un par image.
  std::optional<ro::ui::NavigationRowRef> selected;
  int preview_side_px = 0;
  NavigationSpriteAt sprite_at;
};

// Les résultats d'une recherche et la vue qu'on en tire.
//
// ⚠ LA RECHERCHE EST FAITE ICI, UNE FOIS PAR TERME, et non à chaque image : la
// vue garde des `string_view` sur les libellés composés au moment de la
// recherche, et le numéro de série change avec eux — la sélection de la fenêtre
// appartient aux résultats qu'elle a vus.
class NavigationComposer {
 public:
  void Search(const ro::formats::NaviData& data, std::string_view term,
              ro::formats::NaviMapIndex only_map, ro::formats::NaviMapIndex prefer_map);
  void ShowContents(const ro::formats::NaviData& data, ro::formats::NaviMapIndex map);
  void Clear();

  [[nodiscard]] const ro::ui::NavigationView& Compose(const NavigationSources& from);

  // Le résultat d'une ligne, rien si la référence ne désigne plus rien.
  [[nodiscard]] std::optional<ro::sim::NaviHit> HitAt(ro::ui::NavigationRowRef row) const;

  // Les groupes de la dernière recherche — la carte du monde les surligne.
  [[nodiscard]] std::span<const ro::sim::NaviHitGroup> groups() const noexcept { return groups_; }

 private:
  void Rebuild(const ro::formats::NaviData& data);

  std::vector<ro::sim::NaviHitGroup> groups_;
  // Par groupe, ses lignes ; puis la vue des groupes. Reconstruits ensemble.
  std::vector<std::vector<ro::ui::NavigationEntryView>> entries_;
  std::vector<ro::ui::NavigationGroupView> group_views_;
  std::vector<std::string> map_paths_;
  // Les classes qui n'ont rien donné à montrer : leur aperçu est le plan, tout
  // de suite, sans carré vide d'attente.
  std::unordered_set<std::uint32_t> spriteless_;
  std::uint32_t serial_ = 0;

  std::vector<std::string> step_labels_;
  std::vector<ro::ui::NavigationRouteStep> steps_;
  ro::ui::NavigationView view_;
};

}  // namespace moonclient
