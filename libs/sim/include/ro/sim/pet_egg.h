#pragma once

#include <array>
#include <cstdint>

namespace ro::sim {

// L'ŒUF D'UN FAMILIER, vu comme un objet.
//
// Ses quatre « cartes » ne sont pas des cartes : c'est la fiche du familier
// qu'il contient. 🔴 ET CE QUI ARRIVE N'EST PAS CE QUE LE SERVEUR GARDE : il
// réécrit les quatre cases avant de les mettre sur le fil.
//
//   base du serveur                       sur le fil
//   card[0] = marqueur de familier           card[0] = 0
//   card[1] = mot bas de l'identifiant       card[1] = 0
//   card[2] = mot haut de l'identifiant      card[2] = rang d'intimité, 1 à 5
//   card[3] = renommé | rang << 1            card[3] = renommé
//
// Le marqueur ni l'identifiant n'atteignent donc jamais le client : un œuf
// habité se reconnaît à son rang ou à son renommage, un œuf VIERGE (sorti d'une
// commande de staff) arrive entièrement à zéro. Et `damaged` n'y dit pas
// « cassé », il dit « éclos ».

// Les œufs de familier, par leur seule plage d'identifiants — le test du client.
inline constexpr std::uint32_t kPetEggFirstId = 9001;
inline constexpr std::uint32_t kPetEggLastId = 9499;
[[nodiscard]] constexpr bool IsPetEgg(std::uint32_t item_id) noexcept {
  return item_id >= kPetEggFirstId && item_id <= kPetEggLastId;
}

// Les cinq rangs d'intimité, de 1 (le plus distant) à 5 (le plus fidèle).
inline constexpr int kPetIntimacyRanks = 5;

struct PetEggFacts {
  // Un familier est bien dedans.
  bool has_pet = false;
  bool renamed = false;
  // De 1 à 5 ; zéro quand le rang est inconnu.
  int intimacy_rank = 0;
};

[[nodiscard]] PetEggFacts DecodePetEgg(const std::array<std::uint32_t, 4>& cards) noexcept;

}  // namespace ro::sim
