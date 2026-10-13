#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include <imgui.h>

#include "ro/render/actor_vignette.h"
#include "ro/render/actor_wardrobe.h"
#include "ro/render/sprite_vignette.h"
#include "ro/sim/actor.h"
#include "ro/sim/world_fwd.h"
#include "ro/ui/stat_bars.h"
#include "ro/ui/status_portrait.h"
#include "ro/vfs/job_names.h"

#include "items/doll_body.h"
#include "items/doll_wearing.h"
#include "options/status_portrait_options.h"
#include "world/actor_styles.h"

namespace moonclient {

class DollEffects;
struct MapStage;

// CE QUE LE PORTRAIT DE STATUT MONTRE, recomposé à chaque image : le joueur,
// son métier par la table du client, ses niveaux, et sa vignette composée par
// la garde-robe. `ui` ne voit que le résultat.
class PortraitHudView {
 public:
  // La garde-robe peut manquer : le portrait reste alors vide, et les
  // étiquettes vivent. `jobs` doit survivre à la vue.
  PortraitHudView(ro::render::ActorWardrobe* wardrobe, const ro::vfs::JobNames& jobs);

  // `gauges` : les jauges de la même page, sur lesquelles les cadres
  // s'aimantent aussi, et dont le verrou les fige. `screen` : la taille de
  // l'écran, qui borne celle du cadre comme le dessin la borne — voir
  // `ro::ui::HudOnScreen`.
  void Refresh(const ro::sim::World& world, const StatusPortraitOptions& options,
               const ro::ui::StatBarsLook& gauges, ActorStyles& styles, std::uint32_t now_ms,
               ImVec2 screen);

  // LES EFFETS DE CHAPEAU DU JOUEUR SUR SON PORTRAIT, par `effects`, au cadrage
  // que la dernière `Refresh` a imposé à la vignette, et découpés à
  // l'intérieur du cadre. Le rappel se pose sur la vue. ⚠ APRÈS `Refresh`, qui
  // l'efface : sans portrait, ou sur une classe en modèle 3D, `effects` se
  // repose.
  void ShowHatEffects(DollEffects& effects, const ro::sim::World& world, const MapStage& stage,
                      std::uint32_t now_ms);

  // Le cadre du portrait est-il à l'écran ? Sinon, son moteur d'effets se rend.
  [[nodiscard]] bool portrait_shown() const noexcept { return portrait_shown_; }
  // Les effets de chapeau portés à cette image, sur un portrait qui peut les
  // montrer : leur moteur naît au premier qui a sa fiche.
  [[nodiscard]] std::span<const ro::sim::HatEffectId> hat_effects() const noexcept {
    return hat_effects_;
  }

  // ⚠ LES VUES POINTENT DANS CET OBJET ET DANS LE MONDE : elles ne survivent
  // pas au `Refresh` suivant.
  [[nodiscard]] const ro::ui::StatusPortraitView& view() const noexcept { return view_; }

 private:
  void RefreshPortrait(const ro::sim::World& world, const StatusPortraitOptions& options,
                       ActorStyles& styles, std::uint32_t now_ms, ImVec2 screen);

  ro::render::ActorWardrobe* wardrobe_;
  const ro::vfs::JobNames* jobs_;
  std::optional<ro::render::ActorVignette> portrait_;
  bool portrait_shown_ = false;
  // CE QUE LES EFFETS SUIVENT, relevé à chaque `Refresh` : les effets portés,
  // le cadrage IMPOSÉ de la vignette dans l'intérieur du cadre, la direction
  // qu'elle montre, et le corps qu'elle a composé — désigné, valide jusqu'à
  // la prochaine composition.
  std::vector<ro::sim::HatEffectId> hat_effects_;
  ro::render::VignetteFraming framing_;
  DollCanvas canvas_;
  DollBodySource body_;
  std::size_t sprite_direction_ = 0;
  // Les jauges visibles, que la vue désigne.
  std::vector<ro::ui::HudRect> neighbours_;
  ro::ui::StatusPortraitView view_;
};

}  // namespace moonclient
