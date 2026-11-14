#include <cstddef>
#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/inventory.h"

using ro::sim::Inventory;
using ro::sim::InventoryItem;
using ro::sim::ItemCategory;
using ro::sim::ItemCategoryIsGear;
using ro::sim::ItemCategoryIsUsable;
using ro::sim::ItemCategoryOf;
using ro::sim::kObtainedItemsKept;
using ro::sim::kRefusedPickupsKept;

namespace {

[[nodiscard]] ItemCategory FamilyOf(std::uint8_t type) {
  return ItemCategoryOf(InventoryItem{.type = type});
}

// Un objet tel qu'un ramassage le décrit : `amount` y est la quantité REÇUE, et
// non ce que la case porte une fois l'ajout fait.
[[nodiscard]] InventoryItem Picked(std::uint16_t slot, std::int16_t amount) {
  return InventoryItem{.client_slot = slot, .item_id = 501, .amount = amount};
}

}  // namespace

TEST_CASE("Famille d'un objet : les sept listes du serveur", "[sim][sac]") {
  // Consommables : soin, usage, usage différé.
  CHECK(FamilyOf(0) == ItemCategory::kConsumable);
  CHECK(FamilyOf(1) == ItemCategory::kConsumable);
  CHECK(FamilyOf(2) == ItemCategory::kConsumable);

  // 🔴 LE CASH A SA PROPRE FAMILLE, et il en faut une : le coffre lui donne un
  // onglet à lui. Le noyer dans les consommables l'aurait rendu introuvable là
  // où on vient justement le chercher.
  CHECK(FamilyOf(0x12) == ItemCategory::kCash);

  // 🔴 LES ARMES ET LES ARMURES SE SÉPARENT, pour la même raison : le coffre a un
  // onglet pour chacune. « Équipement » n'est plus une famille mais un
  // REGROUPEMENT, celui des deux fenêtres à grille.
  CHECK(FamilyOf(5) == ItemCategory::kWeapon);
  CHECK(FamilyOf(8) == ItemCategory::kWeapon);
  CHECK(FamilyOf(0xF) == ItemCategory::kWeapon);
  CHECK(FamilyOf(4) == ItemCategory::kArmor);
  CHECK(FamilyOf(0xB) == ItemCategory::kArmor);
  CHECK(FamilyOf(0xE) == ItemCategory::kArmor);

  CHECK(FamilyOf(3) == ItemCategory::kEtc);
  CHECK(FamilyOf(7) == ItemCategory::kEtc);
  CHECK(FamilyOf(6) == ItemCategory::kCard);
}

TEST_CASE("Famille d'un objet : le gros grain regroupe ce que le sac réunit", "[sim][sac]") {
  // ⚠ C'EST LE DÉCOUPAGE DU CLIENT D'ORIGINE, et le sac comme la charrette le
  // gardent : « Conso » y prend le cash, « Equip » les armes et les armures.
  // Sans ces deux noms, chaque fenêtre le recomposerait à sa façon.
  CHECK(ItemCategoryIsUsable(ItemCategory::kConsumable));
  CHECK(ItemCategoryIsUsable(ItemCategory::kCash));
  CHECK_FALSE(ItemCategoryIsUsable(ItemCategory::kEtc));

  CHECK(ItemCategoryIsGear(ItemCategory::kWeapon));
  CHECK(ItemCategoryIsGear(ItemCategory::kArmor));
  CHECK_FALSE(ItemCategoryIsGear(ItemCategory::kAmmo));
  CHECK_FALSE(ItemCategoryIsGear(ItemCategory::kCard));
}

TEST_CASE("Famille d'un objet : munitions et cartes QUITTENT « divers »", "[sim][sac]") {
  // 🔴 LE SERVEUR LES RANGE DANS « DIVERS », ET L'INTERFACE NON. Chacune a son
  // onglet ; les laisser des deux côtés les ferait compter deux fois, et un
  // joueur verrait ses flèches à deux endroits.
  CHECK(FamilyOf(10) == ItemCategory::kAmmo);
  CHECK(FamilyOf(0x10) == ItemCategory::kAmmo);
  CHECK(FamilyOf(0x11) == ItemCategory::kAmmo);
  CHECK(FamilyOf(0x13) == ItemCategory::kAmmo);
  CHECK(FamilyOf(6) != ItemCategory::kEtc);
}

TEST_CASE("Famille d'un objet : un type inconnu n'est forcé dans aucune", "[sim][sac]") {
  // ⚠ IL EXISTE, et le nommer est ce qui empêche de le ranger au hasard : un
  // type hors des listes ne se montre que là où tout se montre.
  CHECK(FamilyOf(0xFE) == ItemCategory::kOther);
  CHECK(FamilyOf(200) == ItemCategory::kOther);
  CHECK_FALSE(ItemCategoryIsUsable(ItemCategory::kOther));
  CHECK_FALSE(ItemCategoryIsGear(ItemCategory::kOther));
}

TEST_CASE("Famille d'un objet : elle ignore tout le reste de l'objet", "[sim][sac]") {
  // Ni le favori, ni le raffinement, ni les cartes n'y entrent : la famille est
  // un fait du TYPE, et c'est ce qui la rend lisible partout.
  //
  // ⚠ L'EMPLACEMENT NON PLUS. Une coiffe d'apparat reste de type « armure » ;
  // c'est son MASQUE qui la range sous « Costumes », et cela ne se décide pas
  // ici — voir `ItemBelongsToTab`.
  InventoryItem sword{.item_id = 1101, .type = 4};
  sword.favorite = true;
  sword.refine_level = 9;
  sword.cards = {4001, 4001, 0, 0};
  CHECK(ItemCategoryOf(sword) == ItemCategory::kArmor);
}

TEST_CASE("Journal des ramassages : le rang part à un et ne recule pas", "[sim][sac]") {
  // 🔴 LE RANG COMPTE LES RAMASSAGES, PAS LES ENTRÉES GARDÉES. L'anneau écrête
  // par le début ; si le rang suivait la taille du tampon, il reculerait à
  // chaque éviction et un témoin qui compare au sien croirait relire ce qu'il a
  // déjà vu.
  constexpr std::uint32_t kFirstPickedMs = 100;
  Inventory bag;
  CHECK(bag.obtained().empty());
  CHECK(bag.last_obtained_seq() == 0);

  bag.RecordObtained(Picked(2, 3), kFirstPickedMs);
  REQUIRE(bag.obtained().size() == 1);
  CHECK(bag.obtained().front().seq == 1);
  CHECK(bag.obtained().front().obtained_ms == kFirstPickedMs);
  CHECK(bag.last_obtained_seq() == 1);

  // Un ramassage de plus que l'anneau n'en garde : le plus ancien s'en va.
  const std::size_t picked_count = kObtainedItemsKept + 1;
  for (std::size_t rank = 1; rank < picked_count; ++rank) {
    bag.RecordObtained(Picked(static_cast<std::uint16_t>(rank), 1),
                       kFirstPickedMs + static_cast<std::uint32_t>(rank));
  }
  CHECK(bag.obtained().size() == kObtainedItemsKept);
  CHECK(bag.last_obtained_seq() == picked_count);
  // Les rangs ne se renumérotent pas : le premier gardé est celui qui SUIT
  // l'évincé, et le dernier vaut le nombre total de ramassages.
  CHECK(bag.obtained().front().seq == 2);
  CHECK(bag.obtained().back().seq == picked_count);
}

TEST_CASE("Journal des ramassages : ni le vidage ni un relistage ne l'effacent", "[sim][sac]") {
  // 🔴 C'EST UNE HISTOIRE D'ÉVÈNEMENTS, PAS UN CONTENU. Le serveur reliste le
  // sac à chaque entrée sur une carte ; l'effacer alors ferait disparaître le
  // bandeau d'un objet ramassé la seconde d'avant.
  constexpr std::uint32_t kPickedMs = 100;
  constexpr int kCartSlots = 100;
  Inventory bag;
  bag.RecordObtained(Picked(2, 3), kPickedMs);
  bag.Put(Picked(2, 3));
  REQUIRE(bag.size() == 1);

  bag.Clear();
  CHECK(bag.size() == 0);
  CHECK(bag.obtained().size() == 1);
  CHECK(bag.last_obtained_seq() == 1);

  // Le relistage vide le contenu par la même porte, et n'en efface pas plus.
  bag.BeginFill("");
  bag.EndFill();
  CHECK(bag.obtained().size() == 1);

  // Et le contenant qu'on REND emporte sa capacité, pas ce qui en est sorti.
  bag.SetCapacity(kCartSlots);
  bag.Forget();
  CHECK(bag.capacity() == 0);
  CHECK(bag.obtained().size() == 1);
  CHECK(bag.last_obtained_seq() == 1);
}

TEST_CASE("Journal des refus : le rang part à un et ne recule pas", "[sim][sac]") {
  // Même contrat que le journal des gains, et pour la même raison : le rang
  // compte les REFUS, pas les entrées gardées. L'anneau y est plus court, parce
  // qu'un refus se répète à l'identique et que l'appelant en écrit une ligne de
  // conversation chacun.
  constexpr std::uint32_t kFirstRefusedMs = 100;
  // ADDITEM_OVERWEIGHT : le même code à chaque pas, comme en surcharge.
  constexpr std::uint8_t kOverweight = 2;
  Inventory bag;
  CHECK(bag.refused().empty());
  CHECK(bag.last_refused_seq() == 0);

  bag.RecordRefused(kOverweight, kFirstRefusedMs);
  REQUIRE(bag.refused().size() == 1);
  CHECK(bag.refused().front().seq == 1);
  CHECK(bag.refused().front().refused_ms == kFirstRefusedMs);
  CHECK(bag.refused().front().result == kOverweight);
  CHECK(bag.last_refused_seq() == 1);

  // Un refus de plus que l'anneau n'en garde : le plus ancien s'en va.
  const std::size_t refused_count = kRefusedPickupsKept + 1;
  for (std::size_t rank = 1; rank < refused_count; ++rank) {
    bag.RecordRefused(kOverweight, kFirstRefusedMs + static_cast<std::uint32_t>(rank));
  }
  CHECK(bag.refused().size() == kRefusedPickupsKept);
  CHECK(bag.last_refused_seq() == refused_count);
  CHECK(bag.refused().front().seq == 2);
  CHECK(bag.refused().back().seq == refused_count);
}

TEST_CASE("Journal des refus : ni le vidage ni un relistage ne l'effacent", "[sim][sac]") {
  // Le relistage du sac arrive à chaque entrée sur une carte : effacer là ferait
  // disparaître la ligne rouge d'un ramassage refusé la seconde d'avant.
  constexpr std::uint32_t kRefusedMs = 100;
  constexpr std::uint8_t kOverweight = 2;
  Inventory bag;
  bag.RecordRefused(kOverweight, kRefusedMs);

  bag.Clear();
  CHECK(bag.refused().size() == 1);
  CHECK(bag.last_refused_seq() == 1);

  bag.BeginFill("");
  bag.EndFill();
  CHECK(bag.refused().size() == 1);

  bag.Forget();
  CHECK(bag.refused().size() == 1);
  CHECK(bag.last_refused_seq() == 1);
}

TEST_CASE("Journaux : le rang d'arrivée entrelace les gains et les refus", "[sim][sac]") {
  // 🔴 C'EST LE SEUL LIEN ENTRE LES DEUX JOURNAUX, et ce qui rend leur ordre
  // relatif connaissable. Ni le rang propre à chacun — ils comptent séparément —
  // ni l'horloge ne le donnent : l'appelant date toute une fournée de trames du
  // même instant, celui où il l'a drainée. Qui relit les deux journaux pour en
  // faire des lignes de conversation ne peut donc les remettre en ordre que
  // par là.
  constexpr std::uint32_t kDrainedMs = 500;
  constexpr std::uint8_t kOverweight = 2;
  Inventory bag;

  // Un refus, puis un gain, puis un refus — TOUS du même instant, comme un seul
  // drain les apporte.
  bag.RecordRefused(kOverweight, kDrainedMs);
  bag.RecordObtained(Picked(2, 3), kDrainedMs);
  bag.RecordRefused(kOverweight, kDrainedMs);

  REQUIRE(bag.refused().size() == 2);
  REQUIRE(bag.obtained().size() == 1);
  CHECK(bag.refused().front().arrival == 1);
  CHECK(bag.obtained().front().arrival == 2);
  CHECK(bag.refused().back().arrival == 3);

  // Les rangs PROPRES, eux, ne disent rien de cet ordre : les deux journaux
  // comptent chacun de leur côté, et c'est bien pourquoi il en fallait un autre.
  CHECK(bag.refused().front().seq == 1);
  CHECK(bag.obtained().front().seq == 1);

  // Et l'horloge non plus.
  CHECK(bag.refused().front().refused_ms == bag.obtained().front().obtained_ms);
}
