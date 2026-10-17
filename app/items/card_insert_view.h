#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "ro/net/connection.h"
#include "ro/render/item_icons.h"
#include "ro/sim/inventory.h"
#include "ro/sim/world_fwd.h"
#include "ro/ui/card_insert_window.h"
#include "ro/ui/inventory_window.h"
#include "ro/vfs/item_table.h"
#include "ro/vfs/item_words.h"

#include "items/creator_names.h"

namespace moonclient {

// LE SERTISSAGE, VU DE LA PARTIE : la fenêtre, le sous-menu rapide, et la ligne
// que le client écrit à chaque accusé.
//
// 🔴 LES FICHES SE RELISENT À CHAQUE IMAGE DANS LE SAC, jamais une copie : le
// serveur ne renvoie une liste que s'il en a une, et après le dernier sertissage
// possible il se tait. Une copie resterait figée sur l'état d'avant.
//
// ⚠ C'EST AUSSI ICI QUE LA SESSION SE REFERME SEULE, quand toutes les pièces
// qu'on retrouve sont pleines : personne d'autre ne le ferait.
class CardInsertView {
 public:
  // LES NOMS DES FORGERONS, confiés une fois la description ouverte.
  void SeeCreators(CreatorNames creators) noexcept { creators_ = creators; }

  CardInsertView(const ro::vfs::ItemTable* items, const ro::vfs::ItemWords* words,
                 ro::render::ItemIcons* icons);

  // Compose la fenêtre et le sous-menu. À appeler une fois par image, avant le
  // dessin. ⚠ LE MONDE EN ÉCRITURE : une session dont toutes les pièces sont
  // pleines se referme ici.
  void Refresh(ro::sim::World& world);

  [[nodiscard]] const ro::ui::CardInsertView& view() const noexcept { return view_; }
  [[nodiscard]] std::span<const ro::ui::QuickInsertCardView> quick_cards() const noexcept {
    return quick_cards_;
  }

  // Route ce que la fenêtre a demandé.
  void Apply(const ro::ui::CardInsertWindowAction& asked, ro::ui::CardInsertWindowState& state,
             ro::net::Connection& link, ro::sim::World& world);

  // Le sous-menu rapide est ouvert sur cette pièce : la question part, UNE
  // fois par pièce. Zéro ne demande rien.
  static void AskCompatible(std::uint16_t equip_slot, ro::net::Connection& link,
                            ro::sim::World& world);

  // UNE LIGNE PAR ACCUSÉ ARRIVÉ, écrite une fois dans la conversation : quelle
  // carte dans quelle pièce (voir `CardInsertLine`).
  void NoteVerdict(ro::sim::World& world, std::uint32_t now_ms);

 private:
  [[nodiscard]] std::string NameOf(const ro::sim::InventoryItem& item) const;
  [[nodiscard]] ro::ui::SkinImage IconOf(std::uint32_t item_id) const;
  [[nodiscard]] int SlotsOf(std::uint32_t item_id) const;

  const ro::vfs::ItemTable* items_;
  const ro::vfs::ItemWords* words_;
  CreatorNames creators_;
  ro::render::ItemIcons* icons_;

  // Les noms, gardés le temps que la fenêtre les lise : les vues POINTENT dessus.
  std::string card_name_;
  std::vector<std::string> row_names_;
  std::vector<ro::ui::CardInsertRowView> rows_;
  std::vector<std::string> quick_names_;
  std::vector<ro::ui::QuickInsertCardView> quick_cards_;
  ro::ui::CardInsertView view_;
};

}  // namespace moonclient
