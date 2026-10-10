#pragma once

#include <cstdint>
#include <optional>

#include "ro/render/actor_vignette.h"
#include "ro/render/actor_wardrobe.h"
#include "ro/render/status_icons.h"
#include "ro/sim/world_fwd.h"
#include "ro/ui/target_hud.h"
#include "ro/vfs/item_words.h"
#include "ro/vfs/status_table.h"

#include "character/status_view.h"
#include "options/target_options.h"
#include "world/actor_styles.h"
#include "world/target_facts.h"
#include "world/targeting.h"

namespace moonclient {

// CE QUE LE HUD DE CIBLE MONTRE, recomposé à chaque image : le croisement du
// monde, du dossier du serveur, des tables d'états et de la garde-robe. `ui` ne
// voit que le résultat.
class TargetHudView {
 public:
  // La garde-robe peut manquer : le portrait reste alors vide, et le reste vit.
  // `words` donne leurs mots à la race, à l'élément et à la taille de la cible.
  // Il doit survivre à la vue.
  TargetHudView(const ro::vfs::StatusTable* statuses, ro::render::StatusIcons* icons,
                ro::render::ActorWardrobe* wardrobe, const ro::vfs::ItemWords& words);

  // `ground_skill_armed` : un sort au sol attend sa case, et les cadres laissent
  // alors passer le clic.
  void Refresh(const ro::sim::World& world, const Targeting& targeting,
               const TargetOptions& options, bool ground_skill_armed, ActorStyles& styles,
               std::uint32_t now_ms);

  // ⚠ LES VUES POINTENT DANS CET OBJET : elles ne survivent pas au `Refresh`
  // suivant.
  [[nodiscard]] const ro::ui::TargetView& view() const noexcept { return view_; }

 private:
  void RefreshPortrait(const ro::sim::World& world, const TargetOptions& options,
                       ActorStyles& styles, std::uint32_t now_ms);

  ro::render::ActorWardrobe* wardrobe_;
  const ro::vfs::ItemWords* words_;
  TargetFacts facts_;
  StatusView statuses_;
  std::optional<ro::render::ActorVignette> portrait_;
  ro::ui::TargetView view_;
};

}  // namespace moonclient
