#pragma once

#include <cstdint>

#include "ro/sim/storage_facts.h"
#include "ro/ui/inventory_window.h"

namespace moonclient {

// LE SOUS-TYPE D'UN OBJET, POUR AFFINER UN ONGLET DU COFFRE.
//
// 🔴 UN ONGLET NE SUFFIT PLUS PASSÉ QUELQUES CENTAINES DE LIGNES. « Équipement »
// mêle les dagues, les armures et les accessoires ; devant six cents lignes
// oubliées, on cherche « mes dagues », pas « mon équipement ». C'est ce que la
// liste déroulante apporte, et elle ne tient que sur ce que le SERVEUR envoie
// avec le coffre — le type d'arme et le masque d'emplacement (`ZC 0x0F0F`).
//
// ⚠ UNE DIMENSION PAR ONGLET. Les dix onglets ont déjà séparé les armes des armures et l'apparat du
// reste : la sous-catégorie n'a donc plus à rattraper un onglet trop large, elle affine dans un
// onglet déjà net. Une arme se range par son TYPE, une armure par son EMPLACEMENT, une carte par
// l'emplacement qu'elle VISE.
enum class SubCategoryKind : std::uint8_t {
  kNone,
  // Par type d'arme (`e_weapon_type` du serveur).
  kWeaponType,
  // Par emplacement — pour les armures, et pour l'emplacement VISÉ d'une carte :
  // une carte ne se porte pas, elle se serte dans ce qui se porte là.
  kSlot,
  // Par emplacement d'APPARAT. ⚠ CE NE SONT PAS LES MÊMES BITS que les coiffes
  // ordinaires, et une même tête peut porter les deux.
  kCostumeSlot,
  // Par type de munition.
  kAmmoType,
};

// CE QU'UNE ENTRÉE DU COMBO PORTE.
//
// ⚠ LA CLÉ DÉCIDE DE L'ORDRE, et les plages ne se chevauchent pas : les
// emplacements d'abord, les types d'arme ensuite. Deux familles qui partageraient
// une clé feraient répondre l'une pour l'autre au filtrage.
struct SubCategory {
  static constexpr int kNothing = -1;
  int key = kNothing;
  // ⚠ DÉJÀ TRADUIT, et en ANGLAIS comme tout le jargon du jeu : « Dagger », pas
  // « Dague ». Les joueurs le lisent ainsi depuis des années.
  const char* label = "";

  [[nodiscard]] bool told() const noexcept { return key != kNothing; }
};

// La dimension d'un onglet — `kNone` quand il n'a rien à affiner.
[[nodiscard]] SubCategoryKind SubCategoryKindOf(ro::ui::ItemTab tab);

// Où tombe cet objet-là. ⚠ `facts` EST NUL tant que le serveur n'a rien dit :
// le combo reste alors vide, plutôt que de ranger tout sous « Other ».
[[nodiscard]] SubCategory SubCategoryOf(SubCategoryKind kind,
                                        const ro::sim::StorageItemFacts* facts);

}  // namespace moonclient
