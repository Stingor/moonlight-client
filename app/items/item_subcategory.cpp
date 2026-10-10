#include "items/item_subcategory.h"

#include "ro/core/i18n.h"
#include "ro/sim/equip_location.h"

namespace moonclient {
namespace {

using ro::core::Tr;

// LES DEUX PLAGES DE CLÉS. ⚠ ELLES NE SE CHEVAUCHENT PAS : sous « Équipement »,
// les emplacements et les types d'arme paraissent dans la MÊME liste, et deux
// familles qui partageraient une clé se filtreraient l'une pour l'autre.
constexpr int kSlotKeyBase = 0;
constexpr int kWeaponKeyBase = 100;
// Ce qu'on n'a pas su ranger : toujours en DERNIER, d'où une clé au-delà des
// deux plages.
constexpr int kOtherKey = 900;

// LE TYPE D'UNE ARME, tel que le serveur le numérote (`e_weapon_type`, W_*).
//
// ⚠ EN ANGLAIS, comme tout le jargon du jeu : un joueur cherche « Katar », pas
// « griffe ». Ils passent quand même par `Tr` — c'est ce qui les fait entrer au
// catalogue, et une langue pourra les rendre autrement si elle le veut.
[[nodiscard]] const char* WeaponLabel(std::uint8_t subtype) {
  switch (subtype) {
    case 1:
      return Tr("Dagger");
    case 2:
      return Tr("One-handed sword");
    case 3:
      return Tr("Two-handed sword");
    case 4:
      return Tr("One-handed spear");
    case 5:
      return Tr("Two-handed spear");
    case 6:
      return Tr("One-handed axe");
    case 7:
      return Tr("Two-handed axe");
    case 8:
      return Tr("Mace");
    case 9:
      return Tr("Two-handed mace");
    case 10:
      return Tr("Staff");
    case 11:
      return Tr("Bow");
    case 12:
      return Tr("Knuckle");
    case 13:
      return Tr("Musical instrument");
    case 14:
      return Tr("Whip");
    case 15:
      return Tr("Book");
    case 16:
      return Tr("Katar");
    case 17:
      return Tr("Revolver");
    case 18:
      return Tr("Rifle");
    case 19:
      return Tr("Gatling gun");
    case 20:
      return Tr("Shotgun");
    case 21:
      return Tr("Grenade launcher");
    case 22:
      return Tr("Huuma shuriken");
    case 23:
      return Tr("Two-handed staff");
    default:
      return Tr("Other");
  }
}

// LE TYPE D'UNE MUNITION (`e_ammo_type`, A_*).
[[nodiscard]] const char* AmmoLabel(std::uint8_t subtype) {
  switch (subtype) {
    case 1:
      return Tr("Arrow");
    case 2:
      return Tr("Throwing dagger");
    case 3:
      return Tr("Bullet");
    case 4:
      return Tr("Cartridge");
    case 5:
      return Tr("Grenade");
    case 6:
      return Tr("Shuriken");
    case 7:
      return Tr("Kunai");
    case 8:
      return Tr("Cannonball");
    case 9:
      return Tr("Throwing weapon");
    default:
      return Tr("Other");
  }
}

// L'EMPLACEMENT PRINCIPAL d'un masque.
//
// 🔴 UNE PIÈCE PEUT EN DÉCLARER PLUSIEURS — un accessoire va d'un côté ou de
// l'autre, une arme à deux mains prend les deux —, et il faut bien n'en garder
// qu'un : c'est une LISTE, pas un croisement. L'ordre est celui du mannequin, du
// haut vers le bas, et il tranche.
//
// ⚠ L'ORDRE EST CELUI DU MANNEQUIN, DU HAUT VERS LE BAS : la liste déroulante
// se lit comme on s'habille. L'apparat et l'ombre sont en QUEUE — ce sont des
// places à part, que l'onglet « Costumes » a déjà sorties de là où cette liste
// se montre.
[[nodiscard]] SubCategory PrimarySlot(std::uint32_t equip) {
  const auto slot = [](int rank, const char* label) {
    return SubCategory{.key = kSlotKeyBase + rank, .label = label};
  };
  if ((equip & ro::sim::kEquipHeadTop) != 0) {
    return slot(0, Tr("Head top"));
  }
  if ((equip & ro::sim::kEquipHeadMid) != 0) {
    return slot(1, Tr("Head mid"));
  }
  if ((equip & ro::sim::kEquipHeadLow) != 0) {
    return slot(2, Tr("Head bot"));
  }
  if ((equip & ro::sim::kEquipArmor) != 0) {
    return slot(3, Tr("Armor"));
  }
  if ((equip & ro::sim::kEquipGarment) != 0) {
    return slot(4, Tr("Garment"));
  }
  if ((equip & ro::sim::kEquipShoes) != 0) {
    return slot(5, Tr("Shoes"));
  }
  if ((equip & (ro::sim::kEquipAccessoryLeft | ro::sim::kEquipAccessoryRight)) != 0) {
    return slot(6, Tr("Accessory"));
  }
  if ((equip & ro::sim::kEquipShield) != 0) {
    return slot(7, Tr("Shield"));
  }
  if ((equip & ro::sim::kEquipWeapon) != 0) {
    return slot(8, Tr("Weapon"));
  }
  if ((equip & ro::sim::kEquipAmmo) != 0) {
    return slot(9, Tr("Ammo"));
  }
  if ((equip & ro::sim::kEquipCostumeMask) != 0) {
    return slot(10, Tr("Costume"));
  }
  if (ro::sim::IsShadowLocation(equip)) {
    return slot(11, Tr("Shadow gear"));
  }
  return SubCategory{.key = kOtherKey, .label = Tr("Other")};
}

// L'EMPLACEMENT D'UNE PIÈCE D'APPARAT.
//
// 🔴 SES BITS NE SONT PAS CEUX DES COIFFES ORDINAIRES, et c'est tout l'intérêt :
// une même tête porte une coiffe ET son apparat. Chercher un chapeau d'apparat
// par `kEquipHeadTop` ne rendrait rien.
//
// ⚠ LES LIBELLÉS SONT CEUX DE `PrimarySlot`, à dessein : la même place se dit du
// même mot, qu'on la regarde en apparat ou non.
[[nodiscard]] SubCategory CostumeSlot(std::uint32_t equip) {
  const auto slot = [](int rank, const char* label) {
    return SubCategory{.key = kSlotKeyBase + rank, .label = label};
  };
  if ((equip & ro::sim::kEquipCostumeHeadTop) != 0) {
    return slot(0, Tr("Head top"));
  }
  if ((equip & ro::sim::kEquipCostumeHeadMid) != 0) {
    return slot(1, Tr("Head mid"));
  }
  if ((equip & ro::sim::kEquipCostumeHeadLow) != 0) {
    return slot(2, Tr("Head bot"));
  }
  if ((equip & ro::sim::kEquipCostumeGarment) != 0) {
    return slot(3, Tr("Garment"));
  }
  return SubCategory{.key = kOtherKey, .label = Tr("Other")};
}

}  // namespace

SubCategoryKind SubCategoryKindOf(ro::ui::ItemTab tab) {
  switch (tab) {
    case ro::ui::ItemTab::kWeapon:
      return SubCategoryKind::kWeaponType;
    case ro::ui::ItemTab::kArmor:
      return SubCategoryKind::kSlot;
    case ro::ui::ItemTab::kCostume:
      return SubCategoryKind::kCostumeSlot;
    case ro::ui::ItemTab::kCard:
      return SubCategoryKind::kSlot;
    case ro::ui::ItemTab::kAmmo:
      return SubCategoryKind::kAmmoType;
    case ro::ui::ItemTab::kAll:
    case ro::ui::ItemTab::kConsumable:
    case ro::ui::ItemTab::kEtc:
    case ro::ui::ItemTab::kCash:
    case ro::ui::ItemTab::kFavorite:
      // ⚠ « TOUT » ET « FAVORIS » N'EN ONT PAS, et c'est délibéré : ils mêlent
      // les familles, donc une liste de sous-types y mêlerait des types d'arme
      // et des emplacements sans qu'aucun ne dise de quoi il parle.
      return SubCategoryKind::kNone;
    case ro::ui::ItemTab::kEquipment:
      // ⚠ LE COFFRE NE LE MONTRE PAS : ses dix onglets séparent les armes des
      // armures, et cet onglet-ci n'appartient qu'aux deux fenêtres à grille, qui
      // n'ont pas de sous-type. Le cas reste écrit pour que l'ajout d'un onglet
      // au coffre ne passe pas en silence.
      return SubCategoryKind::kNone;
  }
  return SubCategoryKind::kNone;
}

SubCategory SubCategoryOf(SubCategoryKind kind, const ro::sim::StorageItemFacts* facts) {
  if (kind == SubCategoryKind::kNone || facts == nullptr) {
    return SubCategory{};
  }
  switch (kind) {
    case SubCategoryKind::kWeaponType:
      return SubCategory{.key = kWeaponKeyBase + facts->subtype,
                         .label = WeaponLabel(facts->subtype)};
    case SubCategoryKind::kAmmoType:
      return SubCategory{.key = kWeaponKeyBase + facts->subtype,
                         .label = AmmoLabel(facts->subtype)};
    case SubCategoryKind::kSlot:
      // ⚠ POUR UNE CARTE, C'EST L'EMPLACEMENT QU'ELLE VISE : elle ne se porte
      // pas, elle se serte dans ce qui se porte là. Le masque dit les deux.
      return PrimarySlot(facts->equip_mask);
    case SubCategoryKind::kCostumeSlot:
      return CostumeSlot(facts->equip_mask);
    case SubCategoryKind::kNone:
      break;
  }
  return SubCategory{};
}

}  // namespace moonclient
