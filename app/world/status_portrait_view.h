#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include <imgui.h>

#include "ro/render/actor_vignette.h"
#include "ro/render/actor_wardrobe.h"
#include "ro/sim/world_fwd.h"
#include "ro/ui/stat_bars.h"
#include "ro/ui/status_portrait.h"
#include "ro/vfs/job_names.h"

#include "options/status_portrait_options.h"
#include "world/actor_styles.h"

namespace moonclient {

// CE QUE LE PORTRAIT DE STATUT MONTRE, recomposé à chaque image : le joueur,
// son métier par la table du client, ses niveaux, et sa vignette composée par
// la garde-robe. `ui` ne voit que le résultat.
class PortraitHudView {
 public:
  // La garde-robe peut manquer : le portrait reste alors vide, et les
  // étiquettes vivent. `jobs` doit survivre à la vue.
  PortraitHudView(ro::render::ActorWardrobe* wardrobe, const ro::vfs::JobNames& jobs);

  // `gauges` : les jauges de la même page, sur lesquelles les cadres
  // s'aimantent aussi. `screen` : la taille de l'écran, qui borne celle du
  // cadre comme le dessin la borne — voir `ro::ui::HudOnScreen`.
  void Refresh(const ro::sim::World& world, const StatusPortraitOptions& options,
               const ro::ui::StatBarsLook& gauges, ActorStyles& styles, std::uint32_t now_ms,
               ImVec2 screen);

  // ⚠ LES VUES POINTENT DANS CET OBJET ET DANS LE MONDE : elles ne survivent
  // pas au `Refresh` suivant.
  [[nodiscard]] const ro::ui::StatusPortraitView& view() const noexcept { return view_; }

 private:
  void RefreshPortrait(const ro::sim::World& world, const StatusPortraitOptions& options,
                       ActorStyles& styles, std::uint32_t now_ms, ImVec2 screen);

  ro::render::ActorWardrobe* wardrobe_;
  const ro::vfs::JobNames* jobs_;
  std::optional<ro::render::ActorVignette> portrait_;
  // Les jauges visibles, que la vue désigne.
  std::vector<ro::ui::HudRect> neighbours_;
  ro::ui::StatusPortraitView view_;
};

}  // namespace moonclient
