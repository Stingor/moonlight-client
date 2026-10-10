#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/sim/inventory.h"

#include "game/save_folder.h"

namespace moonclient {

// CE QUE LE CLIENT RETIENT DES ÉCHOPPES DU JOUEUR, SUR LE DISQUE.
//
// Deux réglages de la fenêtre de préparation — le stock en grille, la
// confirmation des prix élevés — et, PAR PERSONNAGE, la dernière échoppe
// ouverte : son nom, et chaque ligne avec sa pile et son prix.
//
// ⚠ C'EST ENTIÈREMENT CLIENT : aucun service web, tout reste dans `savedata\`,
// et le serveur n'en sait rien.
//
// ⚠ UNE LIGNE SE RECONNAÎT À SON OBJET ENTIER — identifiant, raffinement,
// cartes —, pas à son seul identifiant : le prix d'une épée +7 ne doit pas se
// poser sur une épée +0.

inline constexpr std::string_view kVendingMemoryFileName = "echoppes.yml";

struct RememberedVendingLine {
  std::uint32_t item_id = 0;
  int refine_level = 0;
  std::array<std::uint32_t, ro::sim::kItemCardSlots> cards{};
  int amount = 0;
  std::uint32_t price_zeny = 0;
};

struct RememberedVendingShop {
  // Le nom du personnage : un compte en porte plusieurs, un nom n'en désigne
  // qu'un sur le serveur.
  std::string character;
  std::string title;
  std::vector<RememberedVendingLine> lines;
};

struct VendingMemory {
  bool grid = false;
  bool confirm_high_prices = true;
  std::vector<RememberedVendingShop> shops;

  // La dernière échoppe de ce personnage, ou nulle.
  [[nodiscard]] const RememberedVendingShop* LastOf(std::string_view character) const;
  // Retient l'échoppe qui vient de s'ouvrir, à la place de la précédente.
  void Remember(RememberedVendingShop shop);
};

// UNE LIGNE À REPOSER : la pile du cart qui correspond, combien, à quel prix.
struct ResumedVendingLine {
  std::uint16_t cart_slot = 0;
  int amount = 0;
  std::uint32_t price_zeny = 0;
};

// LA DERNIÈRE ÉCHOPPE, REPOSÉE SUR LE CART D'AUJOURD'HUI.
//
// Chaque ligne retenue prend la première pile du cart qui porte le même objet
// et qu'aucune autre ligne n'a déjà prise ; sa quantité se borne à la pile. Ce
// que le cart ne contient plus ne se pose pas, et l'on s'arrête à
// `permitted_lines`.
[[nodiscard]] std::vector<ResumedVendingLine> ResumeVendingLines(
    const RememberedVendingShop& shop, std::span<const ro::sim::InventoryItem> cart,
    std::size_t permitted_lines);

[[nodiscard]] std::filesystem::path VendingMemoryPath(const std::filesystem::path& client_root);

// Un fichier absent ou illisible laisse les défauts : on joue quand même.
void LoadVendingMemory(const std::filesystem::path& client_root, VendingMemory& into);

[[nodiscard]] bool SaveVendingMemory(const std::filesystem::path& client_root,
                                     const VendingMemory& from);

}  // namespace moonclient
