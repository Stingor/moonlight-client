#include "items/pickup_refusal.h"

#include <cstdint>
#include <optional>

#include "ro/vfs/item_words.h"

namespace moonclient {

std::optional<ro::vfs::ItemPickupRefusal> PickupRefusalOf(std::uint8_t result) {
  switch (static_cast<PickupResult>(result)) {
    case PickupResult::kGranted:
      // Un succès n'est pas un refus : c'est l'autre chemin qui l'annonce.
      return std::nullopt;
    case PickupResult::kInvalid:
    case PickupResult::kRefusedForNow:
      // Le client ne distingue pas ces deux-là : même phrase, qui ne dit pas
      // pourquoi.
      return ro::vfs::ItemPickupRefusal::kCannotGet;
    case PickupResult::kOverweight:
      return ro::vfs::ItemPickupRefusal::kTooHeavy;
    case PickupResult::kTooManyItems:
      return ro::vfs::ItemPickupRefusal::kOverWeightLimit;
    case PickupResult::kOverAmount:
      return ro::vfs::ItemPickupRefusal::kOneKindLimit;
    case PickupResult::kStackLimit:
      return ro::vfs::ItemPickupRefusal::kSameItemLimit;
  }
  // Un code que ce serveur n'émet pas. Le client se tait aussi dans ce cas :
  // aucune phrase vaut mieux qu'une raison inventée.
  return std::nullopt;
}

}  // namespace moonclient
