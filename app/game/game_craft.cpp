// LES ATELIERS, vus de la boucle : ce que leurs gestes demandent au reste de la
// partie — une description, l'aperçu d'un survol, leur page de réglages,
// l'Atlas.

#include "ro/sim/inventory.h"
#include "ro/ui/craft_atlas_window.h"

#include "game/game.h"
#include "options/weapon_refine_options.h"

namespace moonclient {

void Game::FollowCraftGestures(const CraftGestures& asked, std::uint32_t now_ms) {
  if (asked.switch_auto_refine) {
    settings_->SetRowSwitch(WeaponRefineRowId(WeaponRefineRow::kAutoRefine), asked.auto_refine_on,
                            OptionsTargets(), now_ms);
  }
  if (asked.open_make_settings) {
    GameSettings::OpenOptionsAt(windows_, OptionSection::kMakeItem);
  }
  if (asked.open_refine_settings) {
    GameSettings::OpenOptionsAt(windows_, OptionSection::kWeaponRefine);
  }
  if (asked.open_atlas) {
    if (asked.atlas_item_id != 0) {
      ro::ui::OpenCraftAtlasOn(windows_.craft_atlas.state, asked.atlas_item_id);
    } else {
      windows_.craft_atlas.state.open = true;
      windows_.craft_atlas.state.raise = true;
    }
  }

  // ── LA DESCRIPTION ──────────────────────────────────────────────────────────
  //
  // Une arme de la liste d'affinage se décrit PAR SA PLACE : c'est le sac qui
  // donne son raffinement et ses cartes. Un produit ou un minerai, par son seul
  // identifiant.
  std::uint32_t described = asked.describe_item_id;
  std::uint16_t from_slot = 0;
  if (asked.describe_slot != 0) {
    if (const ro::sim::InventoryItem* item = world_.inventory().Find(asked.describe_slot);
        item != nullptr) {
      described = item->item_id;
      from_slot = asked.describe_slot;
    }
  }
  if (described != 0) {
    windows_.description.item_id = described;
    windows_.description.from_slot = from_slot;
    windows_.description.from_holder = ro::ui::ItemHolder::kInventory;
    windows_.description.open = true;
    windows_.description.place_at_mouse = true;
    windows_.description.raise = true;
  }

  // L'APERÇU AU SURVOL, si aucune autre fenêtre ne l'a pris.
  if (hovered_item_ == 0 && asked.hovered_id != 0) {
    hovered_item_ = asked.hovered_id;
    hovered_slot_ = asked.hovered_slot;
    hovered_holder_ = ro::ui::ItemHolder::kInventory;
  }
}

}  // namespace moonclient
