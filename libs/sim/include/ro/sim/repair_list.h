#pragma once

// LA LISTE DE REPAIR WEAPON : les pièces abîmées que le serveur propose, et
// l'issue de la réparation demandée.

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace ro::sim {

// UNE PIÈCE RÉPARABLE, recopiée telle que le serveur l'a écrite : la demande la
// lui renvoie à l'identique.
struct RepairableItem {
  std::int16_t index = 0;
  std::uint32_t item_id = 0;
  std::array<std::uint32_t, 4> cards = {};
  std::uint8_t refine = 0;
  std::uint8_t grade = 0;
};

// LE MATÉRIAU QU'UNE RÉPARATION CONSOMME, d'après la règle du serveur : selon le
// niveau d'arme, Iron Ore, Iron, Steel ou Rough Oridecon ; Steel pour une
// armure, dont le niveau d'arme est nul. Zéro hors de ces cas.
inline constexpr std::array<std::uint32_t, 4> kRepairWeaponMaterials = {
    1002,  // Iron Ore
    998,   // Iron
    999,   // Steel
    756,   // Rough Oridecon
};
inline constexpr std::uint32_t kRepairArmorMaterial = 999;  // Steel

[[nodiscard]] constexpr std::uint32_t RepairMaterialFor(int weapon_level) noexcept {
  if (weapon_level == 0) {
    return kRepairArmorMaterial;
  }
  if (weapon_level < 1 || weapon_level > static_cast<int>(kRepairWeaponMaterials.size())) {
    return 0;
  }
  return kRepairWeaponMaterials[static_cast<std::size_t>(weapon_level - 1)];
}

class RepairListOffer {
 public:
  // Une liste vide n'ouvre rien : le serveur n'en envoie pas, il refuse la
  // compétence à la place.
  void Offer(std::vector<RepairableItem> items) {
    items_ = std::move(items);
    open_ = !items_.empty();
  }

  // Un choix fait, ou un refus : la liste s'en va.
  void Close() noexcept {
    open_ = false;
    items_.clear();
  }

  // L'issue de la réparation, que le serveur dit après coup.
  void TellOutcome(bool repaired) noexcept { outcome_ = repaired; }
  [[nodiscard]] std::optional<bool> TakeOutcome() noexcept {
    return std::exchange(outcome_, std::nullopt);
  }

  [[nodiscard]] bool open() const noexcept { return open_; }
  [[nodiscard]] std::span<const RepairableItem> items() const noexcept { return items_; }

 private:
  bool open_ = false;
  std::vector<RepairableItem> items_;
  std::optional<bool> outcome_;
};

}  // namespace ro::sim
