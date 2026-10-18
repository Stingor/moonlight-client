#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/core/error.h"
#include "ro/sim/npc_shop_saved_carts.h"

namespace moonclient {

// LES PANIERS D'ACHAT ENREGISTRÉS SUR LE DISQUE, un fichier pour tous les
// comptes de l'installation, dans `savedata\`.
//
// ⚠ PAR COMPTE, ET NON PAR PERSONNAGE : un panier décrit des courses —
// « Potions » —, que tous les personnages du compte peuvent refaire. La clé
// est l'identifiant de compte que le serveur de connexion a donné.

inline constexpr std::string_view kNpcShopCartFileName = "npc_shop_carts.yml";

// Les paniers de chaque compte, par identifiant.
using SavedCartShelf = std::map<std::uint32_t, std::vector<ro::sim::SavedCart>>;

[[nodiscard]] std::filesystem::path NpcShopCartFilePath(const std::filesystem::path& client_root);

// LE TEXTE DU FICHIER, dans les deux sens, sans rien lire ni écrire.
//
// Un panier sans nom, ou dont aucune ligne ne nomme un objet et une quantité,
// se saute ; au-delà du plafond, les paniers d'un compte ne se lisent pas. Un
// texte que le YAML refuse rend une erreur, et le fichier ne doit alors pas
// être réécrit.
[[nodiscard]] ro::core::Result<SavedCartShelf> ReadSavedCartShelf(std::string_view text);
[[nodiscard]] std::string WriteSavedCartShelf(const SavedCartShelf& shelf);

// Les paniers de ce compte ; aucun quand le fichier manque ou est abîmé.
[[nodiscard]] std::vector<ro::sim::SavedCart> LoadSavedCarts(
    const std::filesystem::path& client_root, std::uint32_t account_id);

// Remplace les paniers de ce compte et garde ceux des autres. Un fichier
// abîmé ne se réécrit pas : il porte ceux des autres comptes. Rend faux quand
// rien n'a été écrit.
[[nodiscard]] bool SaveSavedCarts(const std::filesystem::path& client_root,
                                  std::uint32_t account_id,
                                  std::span<const ro::sim::SavedCart> carts);

}  // namespace moonclient
