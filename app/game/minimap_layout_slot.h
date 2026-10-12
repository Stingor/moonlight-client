#pragma once

#include <cstdint>
#include <filesystem>
#include <utility>

#include "ro/ui/minimap_settings.h"
#include "ro/ui/window_places.h"

#include "world/navi_file.h"

namespace moonclient {

// ── LA PRÉSENTATION DE LA MINIMAP, VIVANTE ─────────────────────────────────
//
// Celle que la partie dessine et que le joueur règle, tenue À LA COQUILLE : la
// disposition traverse les parties, et le profil d'un personnage se pose avant
// que sa partie n'ouvre la navigation. `LayoutPort` la relève et la pose avec
// le reste de la disposition.
//
// ⚠ UN PROFIL SANS MINIMAP HÉRITE DE `navigation.yml` — voir
// `InheritedMinimap` —, pas des défauts d'usine : c'est là qu'elle vivait
// avant de suivre le profil. Avant tout profil posé, c'est elle aussi.
class MinimapLayoutSlot {
 public:
  explicit MinimapLayoutSlot(std::filesystem::path client_root)
      : client_root_(std::move(client_root)),
        minimap_(InheritedMinimap(client_root_)),
        seen_(minimap_) {}

  [[nodiscard]] ro::ui::MinimapLayout& minimap() noexcept { return minimap_; }

  // Grandit à chaque changement de la présentation : on compare, et l'on compte.
  [[nodiscard]] std::uint64_t Revision() {
    if (minimap_ != seen_) {
      seen_ = minimap_;
      ++revision_;
    }
    return revision_;
  }

  void Capture(ro::ui::LayoutProfile& into) const { into.minimap = minimap_; }

  void Pose(const ro::ui::LayoutProfile& profile) {
    minimap_ = profile.minimap.has_value() ? *profile.minimap : InheritedMinimap(client_root_);
  }

 private:
  std::filesystem::path client_root_;
  ro::ui::MinimapLayout minimap_;
  ro::ui::MinimapLayout seen_;
  std::uint64_t revision_ = 0;
};

}  // namespace moonclient
