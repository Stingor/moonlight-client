#pragma once

#include <cstdint>
#include <unordered_map>

namespace ro::sim {

// LES ITEMS QUI SE MONTRENT PAR UN EFFET DE CHAPEAU, tels que le serveur les a
// relevés dans leurs scripts. Voir `ro/proto/hat_effect_packets.h`.
//
// ⚠ UNE ABSENCE NE PROUVE RIEN TANT QUE LA LISTE N'EST PAS ARRIVÉE : elle ne
// l'est qu'une fois le client reconnu par le serveur.
class HatEffectItems {
 public:
  // La liste du serveur, qui REMPLACE la précédente.
  void Clear() noexcept {
    ordinals_.clear();
    ++revision_;
  }
  void Tell(std::uint32_t item_id, std::int16_t ordinal) {
    ordinals_[item_id] = ordinal;
    ++revision_;
  }
  // Le rang de la liste : il change à chaque retouche.
  [[nodiscard]] std::uint32_t revision() const noexcept { return revision_; }

  // L'ordinal de l'effet que porte cet item, zéro pour aucun.
  [[nodiscard]] std::int16_t OrdinalOf(std::uint32_t item_id) const {
    const auto found = ordinals_.find(item_id);
    return found == ordinals_.end() ? std::int16_t{0} : found->second;
  }

 private:
  std::unordered_map<std::uint32_t, std::int16_t> ordinals_;
  std::uint32_t revision_ = 0;
};

}  // namespace ro::sim
