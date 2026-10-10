#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>

#include "ro/sim/actor.h"

namespace moonclient {

// Combien de butins à pilier on retient. Au-delà, le souvenir le plus ancien
// s'efface : une carte n'en porte jamais autant à la fois, et un butin tombé
// il y a des centaines de colonnes a depuis longtemps quitté le sol.
inline constexpr std::size_t kRememberedPillarsMax = 512;

// LE PILIER QU'UN OBJET AU SOL PORTE, même quand le paquet qui l'annonce ne le
// dit pas.
//
// 🔴 LE SERVEUR NE DIT LE PILIER QU'À LA CHUTE. `ZC_ITEM_FALL_ENTRY5 0x0ADD`
// porte son mode ; `ZC_ITEM_ENTRY 0x009D`, qui annonce un objet DÉJÀ posé, ne
// le porte pas. Or toute arrivée — changement de carte comme téléport sur la
// même — vide le monde, et le serveur réannonce par ce second paquet les
// butins restés à terre. Sans souvenir, leur colonne s'éteindrait à chaque
// arrivée, comme à chaque sortie de vue suivie d'un retour.
//
// ⚠ UN IDENTIFIANT NE SUFFIT PAS : le serveur réattribue ceux des objets
// ramassés. Le souvenir ne vaut que pour le même objet, sur la même carte, à
// la même case — trois champs comparés, rien de haché.
//
// ⚠ UN OBJET QU'ON N'A JAMAIS VU TOMBER RESTE SANS PILIER : rien ne dit le sien.
//
// ⚠ IL SURVIT AU CHANGEMENT DE CARTE, c'est sa raison d'être, et non à la
// partie : le retour à la sélection de personnage détruit le jeu qui le tient.
class ItemPillarMemory {
 public:
  // Le mode de pilier que montre cet objet, zéro pour aucun.
  //
  // Une annonce de CHUTE fait foi : son mode se retient, et une chute sans
  // pilier efface ce qu'on savait d'un identifiant réattribué. Une annonce
  // d'objet DÉJÀ POSÉ rend le mode retenu du même objet.
  [[nodiscard]] std::uint16_t ModeOf(const std::string& map_name, const ro::sim::Actor& item);

  [[nodiscard]] std::size_t remembered() const noexcept { return by_floor_id_.size(); }

 private:
  struct Remembered {
    std::string map_name;
    std::uint32_t item_id = 0;
    std::uint16_t cell_x = 0;
    std::uint16_t cell_y = 0;
    std::uint16_t mode = 0;
    // L'ordre d'arrivée du souvenir : le plus petit part le premier.
    std::uint64_t remembered_rank = 0;
  };

  void Remember(std::uint32_t floor_id, Remembered remembered);

  std::unordered_map<std::uint32_t, Remembered> by_floor_id_;
  std::uint64_t next_rank_ = 0;
};

}  // namespace moonclient
