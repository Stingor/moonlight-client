#pragma once

// LA RÉPARATION DE REPAIR WEAPON, DANS LA BOUCLE DE LA PARTIE : ce que la
// fenêtre montre de la liste du serveur, ce que sa réponse devient sur le fil,
// et l'issue que le serveur rend, écrite dans le chat.

#include <cstdint>
#include <string>
#include <vector>

#include "ro/net/connection.h"
#include "ro/render/item_icons.h"
#include "ro/sim/repair_list.h"
#include "ro/sim/world_fwd.h"
#include "ro/ui/repair_window.h"
#include "ro/vfs/item_table.h"
#include "ro/vfs/item_words.h"

#include "items/craft_book.h"
#include "items/creator_names.h"

namespace moonclient {

class RepairStage {
 public:
  [[nodiscard]] ro::sim::RepairListOffer& offer() noexcept { return offer_; }

  // Ce que la fenêtre lit ailleurs. Tous facultatifs ; les forgerons nomment
  // les armes forgées de la liste.
  void See(const ro::vfs::ItemTable* items, const ro::vfs::ItemWords* words,
           ro::render::ItemIcons* icons, const CraftBook* book, CreatorNames creators);

  // Recopie la liste dans l'état et la vue de la fenêtre : le nom composé de
  // chaque pièce, son icône, et le matériau que sa réparation consommera, avec
  // ce que le sac en porte.
  void Stage(const ro::sim::World& world, ro::ui::RepairWindowState& state,
             ro::ui::RepairView& view);

  // Envoie la pièce choisie, ou le refus, et ferme la liste.
  void Apply(const ro::ui::RepairWindowAction& asked, ro::net::Connection& link);

  // Écrit dans le chat l'issue que le serveur vient de rendre.
  void SayOutcome(ro::sim::World& world, std::uint32_t now_ms);

 private:
  [[nodiscard]] std::string NameOf(const ro::sim::RepairableItem& item, int weapon_level) const;
  [[nodiscard]] ro::ui::SkinImage IconOf(std::uint32_t item_id) const;

  ro::sim::RepairListOffer offer_;
  const ro::vfs::ItemTable* items_ = nullptr;
  const ro::vfs::ItemWords* words_ = nullptr;
  ro::render::ItemIcons* icons_ = nullptr;
  const CraftBook* book_ = nullptr;
  CreatorNames creators_;
  std::vector<std::string> names_;
  std::vector<std::string> materials_;
  std::vector<ro::ui::RepairRowView> rows_;
};

}  // namespace moonclient
