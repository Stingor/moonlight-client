// Voir l'en-tête.

#include "game/pet_staff_words.h"

#include <algorithm>
#include <string>

#include "ro/sim/pet.h"
#include "ro/ui/pet_window.h"

namespace moonclient {

std::string PetStaffHungerCommand(int hunger) {
  return "@pethungry " + std::to_string(std::clamp(hunger, 0, ro::sim::kPetHungerMax));
}

std::string PetStaffIntimacyCommand(int intimacy) {
  return "@petfriendly " + std::to_string(std::clamp(intimacy, ro::ui::kPetStaffIntimacyLeast,
                                                     ro::sim::kPetIntimacyMax));
}

}  // namespace moonclient
