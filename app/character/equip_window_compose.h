#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <imgui.h>

#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/render/actor_vignette.h"
#include "ro/render/actor_wardrobe.h"
#include "ro/render/game_images.h"
#include "ro/render/item_icons.h"
#include "ro/sim/inventory.h"
#include "ro/sim/world_fwd.h"
#include "ro/ui/equip_window.h"
#include "ro/vfs/item_table.h"
#include "ro/vfs/item_words.h"

#include "character/equip_window_places.h"
#include "items/creator_names.h"

namespace moonclient {

// LA FENÊTRE D'ÉQUIPEMENT DU JEU, VUE DE LA PARTIE : ses vingt places, sa
// munition, sa silhouette, et ce que ses boutons peuvent faire.
//
// ⚠ ELLE NE DOUBLE PAS LA FICHE : les gestes communs — ôter, porter, décrire,
// lier, les deux réglages, renvoyer un compagnon — passent par
// `CharacterView::Apply`. Ne vivent ici que la composition et ce que la fiche
// ne sait pas faire : tout retirer, et l'accusé qui le suit.

// Les places, leurs bits et le versement des gestes : `equip_window_places.h`.
class EquipWindowCompose {
 public:
  EquipWindowCompose(const ro::vfs::ItemTable* items, const ro::vfs::ItemWords* words,
                     ro::render::ItemIcons* icons, ro::render::ActorWardrobe* wardrobe,
                     ro::render::GameImages* images);

  // LES NOMS DES FORGERONS, confiés une fois la description ouverte.
  void SeeCreators(CreatorNames creators) noexcept { creators_ = creators; }

  // REFAIT LES PLACES ET CE QUE LES BOUTONS PEUVENT FAIRE. À appeler une fois
  // par image, avant le dessin, et seulement fenêtre ouverte.
  //
  // ⚠ LES NOMS ET LES LIBELLÉS SONT GARDÉS ICI : la vue pointe dedans.
  void Refresh(const ro::sim::World& world, ro::ui::EquipWindowView& view);

  // LA SILHOUETTE, à l'échelle un dans la boîte que la fenêtre demande, ses
  // pieds au bas. `dressed` est l'apparence DÉJÀ HABILLÉE de ses couleurs, ses
  // coiffes et sa cape déjà celles de l'onglet montré.
  void RefreshAvatar(const ro::sim::World& world, ImVec2 box, const ro::render::ActorLook& dressed,
                     std::uint32_t now_ms, ro::ui::SkinImage& image);

  // CE QUE LA FENÊTRE A DEMANDÉ ET QUE LA FICHE NE SAIT PAS FAIRE : tout
  // retirer, et la pièce traînée dont on entoure les places.
  void Apply(const ro::ui::EquipWindowAction& asked, ro::net::Connection& link,
             const ro::sim::World& world);

  // L'ACCUSÉ DE « TOUT RETIRER » (`ZC 0x0BAE`) : le bouton se rallume, et un
  // refus se dit dans la conversation.
  [[nodiscard]] ro::net::FrameOutcome HearTakeOffAll(const ro::net::Frame& frame,
                                                     ro::sim::World& world, std::uint32_t now_ms);

 private:
  void Compose(const ro::sim::InventoryItem* piece, ro::ui::EquipWindowPiece& into,
               std::string& name);
  void RefreshRiders(const ro::sim::World& world);

  const ro::vfs::ItemTable* items_;
  const ro::vfs::ItemWords* words_;
  ro::render::ItemIcons* icons_;
  ro::render::GameImages* images_;
  CreatorNames creators_;
  std::optional<ro::render::ActorVignette> avatar_;

  // Les noms des vingt places et de la munition : la vue pointe dedans.
  std::array<std::string, ro::ui::kEquipWindowPlaceCount> general_names_;
  std::array<std::string, ro::ui::kEquipWindowPlaceCount> costume_names_;
  std::string ammo_name_;
  std::vector<ro::ui::EquipWindowRider> riders_;

  // LA PIÈCE TRAÎNÉE SUR LA FENÊTRE à l'image précédente, ou zéro.
  std::uint16_t dragged_slot_ = 0;
  // « TOUT RETIRER » EST PARTI, et l'on attend l'accusé — ou que ce qui est
  // porté change, qui le rend caduc. ⚠ CE QUI ÉTAIT PORTÉ AU MOMENT DU CLIC,
  // par places du sac, sert de témoin.
  bool take_off_all_pending_ = false;
  std::vector<std::uint16_t> worn_when_asked_;
};

}  // namespace moonclient
