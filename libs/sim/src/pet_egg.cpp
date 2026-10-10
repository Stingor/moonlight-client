#include "ro/sim/pet_egg.h"

namespace ro::sim {
namespace {

constexpr std::size_t kRankCard = 2;
constexpr std::size_t kRenamedCard = 3;
constexpr std::uint32_t kRenamedBit = 1;

}  // namespace

PetEggFacts DecodePetEgg(const std::array<std::uint32_t, 4>& cards) noexcept {
  PetEggFacts facts;
  const std::uint32_t rank = cards[kRankCard];
  facts.intimacy_rank = rank >= 1 && rank <= static_cast<std::uint32_t>(kPetIntimacyRanks)
                            ? static_cast<int>(rank)
                            : 0;
  facts.renamed = (cards[kRenamedCard] & kRenamedBit) != 0;
  // Un œuf habité porte au moins son rang, que le serveur pose dès la création
  // et rafraîchit à chaque retour à l'œuf. Zéro partout : personne dedans.
  facts.has_pet = facts.intimacy_rank != 0 || facts.renamed;
  return facts;
}

}  // namespace ro::sim
