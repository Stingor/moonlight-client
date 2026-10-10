#pragma once

// LES ATELIERS, DANS LA BOUCLE DE LA PARTIE : la fabrication et le refine
// d'arme — ce que le serveur propose, ce que les fenêtres en montrent, et ce que
// leurs gestes deviennent sur le fil.

#include <cstdint>
#include <optional>

#include "ro/net/connection.h"
#include "ro/render/item_icons.h"
#include "ro/sim/world_fwd.h"
#include "ro/ui/make_item_window.h"
#include "ro/ui/weapon_refine_window.h"
#include "ro/vfs/item_table.h"
#include "ro/vfs/item_words.h"

#include "items/craft_book.h"
#include "items/creator_names.h"
#include "items/make_item_flow.h"
#include "items/weapon_refine_flow.h"

namespace moonclient {

// CE QUE LES GESTES DES ATELIERS DEMANDENT AU RESTE DE LA PARTIE : une
// description, l'aperçu d'un survol, une page de réglages, l'Atlas.
struct CraftGestures {
  std::uint32_t describe_item_id = 0;
  // La place d'une arme du sac, pour la décrire avec son raffinement et ses
  // cartes. Zéro : on décrit par identifiant.
  std::uint16_t describe_slot = 0;
  std::uint32_t hovered_id = 0;
  std::uint16_t hovered_slot = 0;
  bool open_make_settings = false;
  bool open_refine_settings = false;
  // Ouvrir l'Atlas, sur ce produit s'il n'est pas nul.
  bool open_atlas = false;
  std::uint32_t atlas_item_id = 0;
  // Le bouton de la chaîne a basculé le réglage « Refiner automatiquement » :
  // la partie l'écrit par le même chemin que la page d'options.
  bool switch_auto_refine = false;
  bool auto_refine_on = false;
};

class CraftStage {
 public:
  // Ce que les ateliers lisent ailleurs. Tous facultatifs.
  void See(const ro::vfs::ItemTable* items, const ro::vfs::ItemWords* words,
           ro::render::ItemIcons* icons, const CraftBook* book, CreatorNames creators);

  // 🔴 CE QUE LE JOUEUR ENVOIE, observé à la source : la liste qu'un skill ou un
  // objet ouvre ne dit pas lequel l'a ouverte. À refaire à CHAQUE nouvelle
  // liaison — un changement de serveur de carte en donne une neuve.
  void Watch(ro::net::Connection& link, ro::sim::World& world);

  // Compose les deux vues. À appeler une fois par image, avant le dessin.
  void Stage(ro::sim::World& world, ro::net::Connection& link,
             ro::ui::MakeItemWindowState& make_state, ro::ui::WeaponRefineWindowState& refine_state,
             const MakeItemSettings& make_settings, const WeaponRefineSettings& refine_settings,
             std::uint32_t now_ms);

  [[nodiscard]] const ro::ui::MakeItemView* make_view() const noexcept {
    return make_.has_value() ? &make_->view() : nullptr;
  }
  [[nodiscard]] const ro::ui::WeaponRefineView* refine_view() const noexcept {
    return refine_.has_value() ? &refine_->view() : nullptr;
  }

  // UN ATELIER RÉCLAME-T-IL ENTRÉE ? La fabrication ouverte avec son réglage ;
  // le refine ouvert avec le sien, ou tant que sa confirmation attend — elle
  // valide une action qui peut détruire l'arme. Alors la touche ne va pas au
  // chat.
  [[nodiscard]] static bool ClaimsEnter(const ro::ui::MakeItemWindowState& make_state,
                                        const ro::ui::WeaponRefineWindowState& refine_state,
                                        const MakeItemSettings& make_settings,
                                        const WeaponRefineSettings& refine_settings) {
    const bool confirming =
        refine_state.confirm_index != ro::ui::WeaponRefineWindowAction::kNoRefineRow;
    return (make_state.open && make_settings.enter_key) ||
           (refine_state.open && (refine_settings.enter_key || confirming));
  }

  // Route ce que les deux fenêtres ont demandé.
  [[nodiscard]] CraftGestures Apply(const ro::ui::MakeItemWindowAction& make_asked,
                                    const ro::ui::WeaponRefineWindowAction& refine_asked,
                                    ro::ui::MakeItemWindowState& make_state,
                                    ro::ui::WeaponRefineWindowState& refine_state,
                                    const MakeItemSettings& make_settings,
                                    const WeaponRefineSettings& refine_settings,
                                    ro::net::Connection& link, ro::sim::World& world,
                                    std::uint32_t now_ms);

 private:
  std::optional<MakeItemFlow> make_;
  std::optional<WeaponRefineFlow> refine_;
  // L'horloge de la dernière image, pour dater ce que l'observateur voit
  // partir entre deux images.
  std::uint32_t now_ms_ = 0;
  // Ouvertes à l'image précédente : une fenêtre refermée autrement que par ses
  // boutons — Échap — répond quand même au serveur.
  bool make_was_open_ = false;
  bool refine_was_open_ = false;
};

}  // namespace moonclient
