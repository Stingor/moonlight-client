// LE SOUS-TYPE D'UN OBJET DU COFFRE.
//
// 🔴 CE QUE CE BANC TIENT, ET QU'AUCUN ÉCRAN NE MONTRERAIT VITE :
//
//   · chaque onglet a SA dimension — une arme se range par son TYPE, une armure
//     par sa PLACE, une carte par la place qu'elle VISE, un apparat par sa place
//     d'APPARAT, qui n'est pas la même ;
//   · les deux familles de clés NE SE CHEVAUCHENT PAS : une même liste peut les
//     mêler, et une collision ferait filtrer les dagues avec les capes ;
//   · un masque porte SOUVENT PLUSIEURS BITS — un accessoire va d'un côté ou de
//     l'autre, une arme à deux mains prend les deux mains — et il faut n'en
//     garder qu'un, toujours le même.

#include "items/item_subcategory.h"

#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/equip_location.h"
#include "ro/sim/storage_facts.h"
#include "ro/ui/inventory_window.h"

using moonclient::SubCategory;
using moonclient::SubCategoryKind;
using moonclient::SubCategoryKindOf;
using moonclient::SubCategoryOf;
using ro::sim::StorageItemFacts;
using ro::ui::ItemTab;

namespace {

[[nodiscard]] SubCategory Of(SubCategoryKind kind, std::uint8_t subtype, std::uint32_t equip) {
  const StorageItemFacts facts{.item_id = 501, .subtype = subtype, .equip_mask = equip};
  return SubCategoryOf(kind, &facts);
}

}  // namespace

TEST_CASE("Sous-type : une dimension par onglet, et cinq n'en ont pas", "[app][coffre]") {
  // ⚠ « TOUT » ET « FAVORIS » N'EN ONT PAS : ils mêlent les familles, donc leur
  // liste mêlerait des types d'arme et des emplacements sans qu'aucun ne dise de
  // quoi il parle.
  CHECK(SubCategoryKindOf(ItemTab::kWeapon) == SubCategoryKind::kWeaponType);
  CHECK(SubCategoryKindOf(ItemTab::kArmor) == SubCategoryKind::kSlot);
  CHECK(SubCategoryKindOf(ItemTab::kCostume) == SubCategoryKind::kCostumeSlot);
  CHECK(SubCategoryKindOf(ItemTab::kCard) == SubCategoryKind::kSlot);
  CHECK(SubCategoryKindOf(ItemTab::kAmmo) == SubCategoryKind::kAmmoType);
  CHECK(SubCategoryKindOf(ItemTab::kAll) == SubCategoryKind::kNone);
  CHECK(SubCategoryKindOf(ItemTab::kFavorite) == SubCategoryKind::kNone);
  CHECK(SubCategoryKindOf(ItemTab::kConsumable) == SubCategoryKind::kNone);
  CHECK(SubCategoryKindOf(ItemTab::kCash) == SubCategoryKind::kNone);
  CHECK(SubCategoryKindOf(ItemTab::kEtc) == SubCategoryKind::kNone);
  // ⚠ ET « ÉQUIPEMENT » NON PLUS : le coffre ne le montre plus — ses dix onglets
  // ont séparé les armes des armures —, et les deux fenêtres à grille qui le
  // gardent n'ont pas de sous-type.
  CHECK(SubCategoryKindOf(ItemTab::kEquipment) == SubCategoryKind::kNone);
}

TEST_CASE("Sous-type : une arme se range par son type", "[app][coffre]") {
  const SubCategory dagger = Of(SubCategoryKind::kWeaponType, 1, ro::sim::kEquipWeapon);
  const SubCategory katar = Of(SubCategoryKind::kWeaponType, 16, ro::sim::kEquipWeapon);
  CHECK(std::string_view(dagger.label) == "Dagger");
  CHECK(std::string_view(katar.label) == "Katar");
  CHECK(dagger.key != katar.key);

  // ⚠ ET UNE ARME À DEUX MAINS RESTE UNE ARME, pas un bouclier : c'est son type
  // qu'on lit, jamais son masque.
  const SubCategory claymore = Of(SubCategoryKind::kWeaponType, 3, ro::sim::kEquipBothHands);
  CHECK(std::string_view(claymore.label) == "Two-handed sword");
}

TEST_CASE("Sous-type : une armure se range par sa place", "[app][coffre]") {
  const SubCategory armor = Of(SubCategoryKind::kSlot, 0, ro::sim::kEquipArmor);
  CHECK(std::string_view(armor.label) == "Armor");

  // 🔴 LES DEUX FAMILLES DE CLÉS NE SE CHEVAUCHENT PAS : rien n'interdit qu'une
  // même liste les mêle, et deux entrées de même clé se filtreraient l'une pour
  // l'autre.
  const SubCategory dagger = Of(SubCategoryKind::kWeaponType, 1, ro::sim::kEquipWeapon);
  CHECK(armor.key != dagger.key);
  CHECK(armor.key < dagger.key);
}

TEST_CASE("Sous-type : un masque à plusieurs bits ne rend qu'une place", "[app][coffre]") {
  // Un accessoire déclare les DEUX côtés : il faut n'en garder qu'un.
  const SubCategory accessory =
      Of(SubCategoryKind::kSlot, 0, ro::sim::kEquipAccessoryLeft | ro::sim::kEquipAccessoryRight);
  CHECK(std::string_view(accessory.label) == "Accessory");
}

TEST_CASE("Sous-type : l'apparat a ses PROPRES places", "[app][coffre]") {
  // 🔴 SES BITS NE SONT PAS CEUX DES COIFFES ORDINAIRES, et les deux se portent
  // sur la même tête. Chercher un chapeau d'apparat par le bit de la coiffe ne
  // rendrait rien.
  CHECK(
      std::string_view(Of(SubCategoryKind::kCostumeSlot, 0, ro::sim::kEquipCostumeHeadTop).label) ==
      "Head top");
  CHECK(
      std::string_view(Of(SubCategoryKind::kCostumeSlot, 0, ro::sim::kEquipCostumeGarment).label) ==
      "Garment");
  // ⚠ ET UNE COIFFE ORDINAIRE N'EN EST PAS UNE : sous cet onglet, elle n'a pas de
  // place — ce qui ne peut pas arriver, l'onglet ne la montrant pas.
  CHECK(std::string_view(Of(SubCategoryKind::kCostumeSlot, 0, ro::sim::kEquipHeadTop).label) ==
        "Other");
}

TEST_CASE("Sous-type : l'apparat et l'ombre ferment la liste des places", "[app][coffre]") {
  // ⚠ L'ORDRE EST CELUI DU MANNEQUIN, du haut vers le bas, et ces deux-là sont
  // des places À PART : elles passent en queue.
  const SubCategory head = Of(SubCategoryKind::kSlot, 0, ro::sim::kEquipHeadTop);
  const SubCategory costume = Of(SubCategoryKind::kSlot, 0, ro::sim::kEquipCostumeHeadTop);
  CHECK(std::string_view(costume.label) == "Costume");
  CHECK(head.key < costume.key);

  // L'ombre ne se montre nulle part ailleurs dans le client : c'est ici qu'on la
  // retrouve, et en dernier.
  constexpr std::uint32_t kShadowArmor = 0x010000;
  const SubCategory shadow = Of(SubCategoryKind::kSlot, 0, kShadowArmor);
  CHECK(std::string_view(shadow.label) == "Shadow gear");
  CHECK(costume.key < shadow.key);
}

TEST_CASE("Sous-type : une carte se range là où elle SE SERTE", "[app][coffre]") {
  // Une carte ne se porte pas : son masque dit ce qu'elle vise.
  const SubCategory card = Of(SubCategoryKind::kSlot, 0, ro::sim::kEquipShoes);
  CHECK(std::string_view(card.label) == "Shoes");
}

TEST_CASE("Sous-type : une munition se range par son type", "[app][coffre]") {
  CHECK(std::string_view(Of(SubCategoryKind::kAmmoType, 1, ro::sim::kEquipAmmo).label) == "Arrow");
  CHECK(std::string_view(Of(SubCategoryKind::kAmmoType, 3, ro::sim::kEquipAmmo).label) == "Bullet");
}

TEST_CASE("Sous-type : sans les faits du serveur, il n'y a rien à dire", "[app][coffre]") {
  // 🔴 LA TABLE ARRIVE APRÈS LA LISTE DU COFFRE, et une image durant elle
  // manque. Ranger tout sous « Other » ferait paraître un combo d'une seule
  // entrée, qui disparaîtrait à l'image suivante.
  CHECK_FALSE(SubCategoryOf(SubCategoryKind::kSlot, nullptr).told());
  CHECK_FALSE(SubCategoryOf(SubCategoryKind::kNone, nullptr).told());
  const StorageItemFacts facts{.item_id = 501};
  CHECK_FALSE(SubCategoryOf(SubCategoryKind::kNone, &facts).told());
}
