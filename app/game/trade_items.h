#pragma once

#include <cstdint>
#include <string>

#include "ro/net/connection.h"
#include "ro/render/item_icons.h"
#include "ro/sim/command_settings.h"
#include "ro/sim/inventory.h"
#include "ro/sim/world_fwd.h"
#include "ro/ui/skin_art.h"
#include "ro/vfs/item_table.h"
#include "ro/vfs/item_words.h"

#include "items/creator_names.h"

namespace moonclient {

// LES OBJETS DES COMMERCES, VUS DE L'ÉCRAN : leur icône et leur nom.
//
// Boutique de PNJ, échoppes, troc : chacun montre des objets qu'il ne possède
// pas toujours, et que le fil ne nomme jamais — un identifiant, un
// raffinement, des cartes. Le nom se COMPOSE avec les mots de l'installation,
// comme dans le sac ; ce petit atelier est ce qui garde les quatre fenêtres
// d'accord entre elles et avec le sac.
//
// Tout est facultatif : sans table, « #id » ; sans icônes, une case vide.
class TradeItems {
 public:
  void See(const ro::vfs::ItemTable* items, ro::render::ItemIcons* icons,
           const ro::vfs::ItemWords* words) noexcept;

  [[nodiscard]] ro::ui::SkinImage IconOf(std::uint32_t item_id) const;

  // Le nom d'un objet qu'on n'a pas en main : celui de la table, avec ses
  // emplacements de cartes, comme un catalogue le montre.
  [[nodiscard]] std::string NameOf(std::uint32_t item_id) const;

  // Le nom d'un objet TENU — le sien ou celui d'un autre joueur — : raffinement,
  // cartes, forge, grade compris. `creators` pose le nom du forgeron quand il
  // est connu.
  [[nodiscard]] std::string NameOf(const ro::sim::InventoryItem& item,
                                   const CreatorNames& creators) const;

  // Combien de cartes la pièce peut recevoir, d'après la table du client.
  [[nodiscard]] int CardSlotsOf(std::uint32_t item_id) const;

  // La table des objets du client, nulle sans installation.
  [[nodiscard]] const ro::vfs::ItemTable* table() const noexcept { return items_; }

 private:
  const ro::vfs::ItemTable* items_ = nullptr;
  ro::render::ItemIcons* icons_ = nullptr;
  const ro::vfs::ItemWords* words_ = nullptr;
};

// CE QU'UN ÉTAGE DE COMMERCE REÇOIT À CHAQUE IMAGE : le fil, le monde, les mots
// du jeu, les forgerons connus, l'atelier des noms, et le verrou de vente des
// favoris — un réglage du joueur, que le serveur ne connaît pas.
//
// ⚠ PASSÉ PAR RÉFÉRENCES, et ne vit que le temps de l'appel : l'étage ne
// retient rien de ce qu'il ne possède pas.
struct TradeContext {
  ro::net::Connection& link;
  ro::sim::World& world;
  const ro::vfs::ItemWords& words;
  const CreatorNames& creators;
  const TradeItems& items;
  bool sell_locked = false;
  std::uint32_t now_ms = 0;
  // Les réglages de commande du serveur (`@sellstuff`, `@sellitem`…), qui
  // décident de ce que la liste de vente d'un PNJ montre. Nul hors partie.
  const ro::sim::CommandSettings* commands = nullptr;
};

}  // namespace moonclient
