// LES ENTREPÔTS : la part du monde qui note ce que le serveur en dit.
//
// Elle vit à part de `world.cpp` pour tenir le plafond de huit cents lignes ;
// ces méthodes ne touchent que leurs propres champs. La banque, elle, a sa
// classe : voir `BankSession`.

#include <cstdint>
#include <utility>
#include <vector>

#include "ro/sim/world.h"

namespace ro::sim {

void World::TellStorages(std::vector<StorageShelf> shelves, std::uint8_t open_id) {
  storage_shelves_ = std::move(shelves);
  open_storage_id_ = open_id;
}

void World::CloseStorage() {
  storage_open_ = false;
  // ⚠ LA LISTE RESTE, LE MARQUEUR PART : les coffres qu'on PEUT ouvrir ne
  // dépendent pas de celui qui l'est. Garder le numéro laisserait un onglet
  // marqué sur un coffre refermé, et le serveur, lui, renvoie sa liste avec
  // 0xFF à la prochaine occasion.
  open_storage_id_ = kNoStorageOpen;
  storage_.Clear();
}

}  // namespace ro::sim
