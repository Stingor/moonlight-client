#pragma once

#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/net/connection.h"
#include "ro/render/item_icons.h"
#include "ro/sim/world_fwd.h"
#include "ro/ui/cart_window.h"
#include "ro/vfs/item_table.h"
#include "ro/vfs/item_words.h"

#include "items/creator_names.h"

namespace moonclient {

// LA CHARRETTE, VUE DE LA PARTIE.
//
// ⚠ ELLE CROISE LES MÊMES QUATRE SOURCES QUE LE SAC et les croise de la même
// façon : c'est `BuildItemCells` qui le fait pour les deux. Ce qui lui est
// propre tient en deux choses — son pied n'a pas de bourse, et son seul verbe
// sort un objet vers le sac.
class CartView {
 public:
  // LES NOMS DES FORGERONS, confiés une fois la description ouverte.
  void SeeCreators(CreatorNames creators) noexcept { creators_ = creators; }

  CartView(const ro::vfs::ItemTable* items, const ro::vfs::ItemWords* words,
           ro::render::ItemIcons* icons);

  // ⚠ LES CASES NE SE REFONT QUE FENÊTRE OUVERTE, comme celles du sac. Le pied,
  // lui, se relève toujours : c'est sa capacité qui dit si la charrette existe,
  // et la fenêtre se referme sur elle.
  void Refresh(bool open, const ro::sim::World& world, ro::ui::ItemTab tab,
               std::string_view filter);

  [[nodiscard]] std::span<const ro::ui::InventoryCellView> cells() const noexcept { return cells_; }
  [[nodiscard]] ro::ui::CartTally tally() const noexcept { return tally_; }

  // ⚠ ÉMET ET S'ARRÊTE LÀ. La charrette ne bouge qu'à la réponse du serveur.
  //
  // 🔴 ET LA DESTINATION DÉPEND DU COFFRE : ouvert, le serveur refuse le retour
  // vers le sac et n'accepte que le rangement vers le coffre ; fermé, c'est
  // l'inverse. La fenêtre a déjà choisi le verbe — ici on ne fait que l'émettre.
  void Apply(const ro::ui::CartWindowAction& asked, ro::net::Connection& link,
             const ro::sim::World& world);

 private:
  const ro::vfs::ItemTable* items_;
  const ro::vfs::ItemWords* words_;
  CreatorNames creators_;
  ro::render::ItemIcons* icons_;

  std::vector<std::string> names_;
  std::vector<ro::ui::InventoryCellView> cells_;
  ro::ui::CartTally tally_;
};

}  // namespace moonclient
