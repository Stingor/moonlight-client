// Les VERBES du sac : ce que le joueur demande au serveur de faire d'un objet.
//
// ⚠ AUCUN N'AGIT SUR LE MONDE. Utiliser, porter, retirer, jeter : chacun ÉMET et
// s'arrête là. C'est le serveur qui décide, et le sac ne bouge qu'en recevant sa
// réponse — sans quoi une demande refusée laisserait le sac dans un état que
// personne d'autre ne partage.

#include "ro/net/inventory.h"
#include "ro/proto/manual_packets.h"
#include "ro/proto/map_packets.h"
#include "ro/proto/player_packets.h"

#include "send_encoded.h"

namespace ro::net {

core::Result<void> RequestUseItem(Connection& link, std::uint16_t client_slot,
                                  std::uint32_t account_id) {
  proto::UseItemRequest request;
  request.client_slot = client_slot;
  request.account_id = account_id;
  return SendEncoded(link, proto::Encode(request));
}

core::Result<void> RequestEquip(Connection& link, std::uint16_t client_slot,
                                std::uint32_t wearable_at) {
  // ⚠ CELUI-CI A UNE STRUCTURE CÔTÉ SERVEUR, donc il est engendré et non écrit à
  // la main. C'est la seule différence avec ses trois voisins.
  proto::CzReqWearEquip request;
  request.index = client_slot;
  // OÙ LA PIÈCE PEUT SE PORTER, pas où elle ira : le serveur choisit la main ou
  // l'emplacement libre, et n'emploie ce champ que pour trancher entre deux
  // places possibles.
  request.position = wearable_at;
  link.Send(proto::Encode(request));
  return {};
}

core::Result<void> RequestUnequip(Connection& link, std::uint16_t client_slot) {
  proto::UnequipItemRequest request;
  request.client_slot = client_slot;
  return SendEncoded(link, proto::Encode(request));
}

core::Result<void> RequestDropItem(Connection& link, std::uint16_t client_slot,
                                   std::uint16_t amount) {
  proto::DropItemRequest request;
  request.client_slot = client_slot;
  request.amount = amount;
  return SendEncoded(link, proto::Encode(request));
}

core::Result<void> RequestInventoryToCart(Connection& link, std::uint16_t client_slot,
                                          std::int32_t amount) {
  proto::CzMoveItemFromBodyToCart request;
  request.index = static_cast<std::int16_t>(client_slot);
  request.count = amount;
  link.Send(proto::Encode(request));
  return {};
}

core::Result<void> RequestCartToInventory(Connection& link, std::uint16_t client_slot,
                                          std::int32_t amount) {
  proto::CzMoveItemFromCartToBody request;
  request.index = client_slot;
  request.amount = amount;
  link.Send(proto::Encode(request));
  return {};
}

core::Result<void> RequestFavorite(Connection& link, std::uint16_t client_slot, bool favorite) {
  proto::CzInventoryTab request;
  request.index = static_cast<std::int16_t>(client_slot);
  // 🔴 INVERSÉ, ET C'EST LE SERVEUR QUI L'IMPOSE. Sa documentation le dit en
  // toutes lettres : « 0 = move item to personal tab, 1 = move item to normal
  // tab ». Le champ nomme donc l'onglet NORMAL, et envoyer vrai pour rendre un
  // objet favori fait exactement le contraire.
  request.favorite = !favorite;
  link.Send(proto::Encode(request));
  return {};
}

core::Result<void> RequestInventoryToStorage(Connection& link, std::uint16_t client_slot,
                                             std::int32_t amount) {
  proto::StorageStoreRequest request;
  request.client_slot = client_slot;
  request.amount = amount;
  return SendEncoded(link, proto::Encode(request));
}

core::Result<void> RequestStorageToInventory(Connection& link, std::uint16_t client_slot,
                                             std::int32_t amount) {
  proto::StorageTakeRequest request;
  request.client_slot = client_slot;
  request.amount = amount;
  return SendEncoded(link, proto::Encode(request));
}

core::Result<void> RequestCartToStorage(Connection& link, std::uint16_t client_slot,
                                        std::int32_t amount) {
  proto::CzMoveItemFromCartToStore request;
  request.index = client_slot;
  request.amount = amount;
  link.Send(proto::Encode(request));
  return {};
}

core::Result<void> RequestStorageToCart(Connection& link, std::uint16_t client_slot,
                                        std::int32_t amount) {
  proto::CzMoveItemFromStoreToCart request;
  request.index = client_slot;
  request.amount = amount;
  link.Send(proto::Encode(request));
  return {};
}

core::Result<void> RequestCloseStorage(Connection& link) {
  return SendEncoded(link, proto::Encode(proto::CloseStorageRequest{}));
}

core::Result<void> RequestViewEquip(Connection& link, std::uint32_t account_id) {
  proto::ViewEquipRequest request;
  request.account_id = account_id;
  return SendEncoded(link, proto::Encode(request));
}

}  // namespace ro::net
