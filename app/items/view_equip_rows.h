#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>

#include "ro/sim/inventory.h"

namespace moonclient {

// ── LES LIGNES DE « VOIR L'ÉQUIPEMENT » ─────────────────────────────────────
//
// Où chaque pièce d'un autre joueur se range, et dans quel ordre la fenêtre les
// lit. Rien ici ne dessine.

// LES DIX PLACES DE LA GRILLE, numérotées comme le client range son équipement :
// le rang d'un emplacement est le rang de son bit, du plus bas au plus haut.
enum class EquipPlace : std::uint8_t {
  kHeadLow = 0,
  kWeapon = 1,
  kGarment = 2,
  // ⚠ LES DEUX ACCESSOIRES PORTENT LE NOM QUE L'ÉCRAN LEUR DONNE, et c'est
  // l'inverse du nom du serveur : voir `ro/sim/equip_location.h`.
  kAccessoryLeft = 3,
  kArmor = 4,
  kShield = 5,
  kShoes = 6,
  kAccessoryRight = 7,
  kHeadTop = 8,
  kHeadMid = 9,
};

// La section où une pièce se montre.
enum class EquipSection : std::uint8_t {
  kEquipment,
  kCostume,
  // La munition et l'équipement d'OMBRE : hors grille, chacun sur sa ligne. Le
  // client d'origine rangerait l'ombre sur les places des costumes, où elle en
  // couvrirait un sans rien dire.
  kOther,
};

struct PiecePlace {
  EquipSection section = EquipSection::kEquipment;
  // Absent pour ce qui vit hors grille.
  std::optional<EquipPlace> place;
  bool ammo = false;
  bool shadow = false;
  // Elle occupe l'arme ET le bouclier : la ligne du bouclier dit pourquoi elle
  // est vide.
  bool two_handed = false;
};

// Où se range une pièce portée à `worn_at`.
//
// ⚠ UNE PIÈCE, UNE LIGNE : une coiffe qui couvre le haut et le milieu se range
// au HAUT, et une arme à deux mains à l'ARME. Le premier emplacement de l'ordre
// ci-dessous l'emporte.
[[nodiscard]] PiecePlace PlaceOf(std::uint32_t worn_at);

// L'ordre de lecture : de la tête aux pieds, comme on regarde quelqu'un.
inline constexpr std::array<EquipPlace, 10> kEquipmentOrder = {
    EquipPlace::kHeadTop,       EquipPlace::kHeadMid, EquipPlace::kHeadLow,
    EquipPlace::kArmor,         EquipPlace::kWeapon,  EquipPlace::kShield,
    EquipPlace::kGarment,       EquipPlace::kShoes,   EquipPlace::kAccessoryLeft,
    EquipPlace::kAccessoryRight};
inline constexpr std::array<EquipPlace, 4> kCostumeOrder = {
    EquipPlace::kHeadTop, EquipPlace::kHeadMid, EquipPlace::kHeadLow, EquipPlace::kGarment};

// Le nom d'une place, DÉJÀ TRADUIT.
[[nodiscard]] const char* EquipPlaceLabel(EquipPlace place);

// La pièce d'une liste qui occupe cette place de cette section, ou nul. Hors
// grille — munition, ombre — rien ne se trouve ainsi.
[[nodiscard]] const ro::sim::InventoryItem* PieceAt(std::span<const ro::sim::InventoryItem> pieces,
                                                    EquipPlace place, EquipSection section);

// Le rang de la pièce trouvée par `PieceAt` dans sa liste, ou rien.
[[nodiscard]] std::optional<std::size_t> RankAt(std::span<const ro::sim::InventoryItem> pieces,
                                                EquipPlace place, EquipSection section);

// LE BIT D'EMPLACEMENT d'une place de cette section, zéro hors grille.
//
// 🔴 C'EST PAR LUI QUE SE LIT MA COLONNE, emplacement par emplacement, et non
// par `PlaceOf` : ma pièce à deux mains répond à l'arme ET au bouclier, ma
// coiffe haute et moyenne aux deux lignes. La colonne d'en face, elle, range
// chaque pièce à sa première place.
[[nodiscard]] std::uint32_t LocationOf(EquipPlace place, EquipSection section);

}  // namespace moonclient
