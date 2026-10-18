#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace ro::ui {

// ── LES PANIERS D'ACHAT ENREGISTRÉS, VUS DE LA BOUTIQUE D'UN PNJ ─────────────
//
// `app` tient le carnet du compte et son fichier ; la fenêtre le montre sous
// « Paniers ▾ » et émet ce que le joueur demande : enregistrer le panier
// courant, renommer, supprimer. CHARGER ne sort pas de la fenêtre : il remplit
// le panier, qui est à elle, par le même chemin qu'un clic sur une ligne.

// Le nom saisi, terminateur compris : celui que le carnet range.
inline constexpr std::size_t kNpcShopCartNameInputBytes = 32;

// Une ligne d'un panier enregistré.
struct NpcShopSavedCartLineView {
  std::uint32_t item_id = 0;
  std::int64_t amount = 0;
  // Le nom montré à l'enregistrement : il dit l'objet que cette boutique ne
  // vend pas.
  std::string_view item_name;
};

struct NpcShopSavedCartView {
  std::string_view name;
  std::span<const NpcShopSavedCartLineView> lines;
};

// Ce que le menu garde d'une image à l'autre.
struct NpcShopSavedCartsState {
  std::array<char, kNpcShopCartNameInputBytes> name = {};
  // Le panier qu'on renomme, et la saisie ; moins de zéro si aucun.
  int renaming = -1;
  std::array<char, kNpcShopCartNameInputBytes> rename = {};
  // Le panier dont on confirme la suppression ; moins de zéro si aucun.
  int removing = -1;
  // CE QUE LE DERNIER CHARGEMENT N'A PAS PU METTRE AU PANIER, déjà écrit :
  // vide quand tout y est entré.
  std::string load_note;
};

enum class NpcShopCartBookVerb : std::uint8_t {
  kNothing,
  // Enregistrer le panier courant sous `name` ; un nom déjà porté le met à jour.
  kSave,
  // Renommer le panier `index` en `name`.
  kRename,
  kRemove,
};

struct NpcShopCartBookAsk {
  NpcShopCartBookVerb verb = NpcShopCartBookVerb::kNothing;
  std::size_t index = 0;
  std::string name;
};

}  // namespace ro::ui
