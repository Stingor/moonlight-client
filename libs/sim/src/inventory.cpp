#include "ro/sim/inventory.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace ro::sim {
namespace {

// Les objets sont rangés par numéro de case : la recherche est dichotomique, et
// l'insertion garde l'ordre.
[[nodiscard]] auto LowerBoundBySlot(std::vector<InventoryItem>& items, std::uint16_t client_slot) {
  return std::lower_bound(
      items.begin(), items.end(), client_slot,
      [](const InventoryItem& item, std::uint16_t slot) { return item.client_slot < slot; });
}

// LES FAMILLES, PAR LES TYPES QUE LE SERVEUR ENVOIE.
//
// Ce sont ceux de `itemtype()` (`src/map/clif.cpp`), et non ceux de sa base :
// il les replie avant de les mettre sur le fil.
//
// ⚠ LES MUNITIONS ET LES CARTES SONT SORTIES DE « DIVERS », où le serveur les
// range. Chacune a son propre onglet, et les laisser des deux côtés les ferait
// compter deux fois.
constexpr std::array<std::uint8_t, 3> kConsumableTypes = {0, 1, 2};
constexpr std::array<std::uint8_t, 4> kWeaponTypes = {5, 8, 9, 0xF};
constexpr std::array<std::uint8_t, 5> kArmorTypes = {4, 0xB, 0xC, 0xD, 0xE};
constexpr std::array<std::uint8_t, 4> kAmmoTypes = {10, 0x10, 0x11, 0x13};
constexpr std::array<std::uint8_t, 2> kEtcTypes = {3, 7};
constexpr std::array<std::uint8_t, 1> kCardTypes = {6};
constexpr std::array<std::uint8_t, 1> kCashTypes = {0x12};

[[nodiscard]] bool TypeIsOneOf(std::uint8_t type, std::span<const std::uint8_t> family) {
  return std::ranges::find(family, type) != family.end();
}

}  // namespace

ItemCategory ItemCategoryOf(const InventoryItem& item) noexcept {
  if (TypeIsOneOf(item.type, kConsumableTypes)) {
    return ItemCategory::kConsumable;
  }
  if (TypeIsOneOf(item.type, kWeaponTypes)) {
    return ItemCategory::kWeapon;
  }
  if (TypeIsOneOf(item.type, kArmorTypes)) {
    return ItemCategory::kArmor;
  }
  if (TypeIsOneOf(item.type, kAmmoTypes)) {
    return ItemCategory::kAmmo;
  }
  if (TypeIsOneOf(item.type, kEtcTypes)) {
    return ItemCategory::kEtc;
  }
  if (TypeIsOneOf(item.type, kCardTypes)) {
    return ItemCategory::kCard;
  }
  if (TypeIsOneOf(item.type, kCashTypes)) {
    return ItemCategory::kCash;
  }
  return ItemCategory::kOther;
}

bool ItemIsForged(const InventoryItem& item) noexcept {
  return item.cards[0] != 0 && item.cards[0] <= kHighestForgeMark;
}

std::uint32_t CreatorIdOfCards(const std::array<std::uint32_t, kItemCardSlots>& cards) noexcept {
  constexpr std::size_t kLowWordCard = 2;
  constexpr std::size_t kHighWordCard = 3;
  constexpr std::uint32_t kWordMask = 0xFFFF;
  constexpr unsigned kWordBits = 16;
  // La règle d'`ItemIsForged` : une première case non nulle mais petite.
  if (cards[0] == 0 || cards[0] > kHighestForgeMark) {
    return 0;
  }
  return (cards[kLowWordCard] & kWordMask) | ((cards[kHighWordCard] & kWordMask) << kWordBits);
}

std::uint32_t ItemCreatorId(const InventoryItem& item) noexcept {
  return CreatorIdOfCards(item.cards);
}

std::size_t ItemCardCount(const InventoryItem& item) noexcept {
  if (ItemIsForged(item)) {
    return 0;
  }
  return static_cast<std::size_t>(std::count_if(item.cards.begin(), item.cards.end(),
                                                [](std::uint32_t card) { return card != 0; }));
}

std::size_t ItemCardsInSlots(const InventoryItem& item, std::size_t slots) noexcept {
  if (ItemIsForged(item)) {
    return 0;
  }
  const std::size_t checked = std::min(slots, item.cards.size());
  return static_cast<std::size_t>(
      std::count_if(item.cards.begin(), item.cards.begin() + static_cast<std::ptrdiff_t>(checked),
                    [](std::uint32_t card) { return card != 0; }));
}

void Inventory::BeginFill(std::string name) {
  items_.clear();
  name_ = std::move(name);
  filling_ = true;
  ++revision_;
}

void Inventory::EndFill() {
  filling_ = false;
  ++revision_;
}

void Inventory::Put(InventoryItem item) {
  const auto at = LowerBoundBySlot(items_, item.client_slot);
  if (at != items_.end() && at->client_slot == item.client_slot) {
    *at = std::move(item);
  } else {
    items_.insert(at, std::move(item));
  }
  ++revision_;
}

bool Inventory::Take(std::uint16_t client_slot, std::int16_t amount) {
  const auto at = LowerBoundBySlot(items_, client_slot);
  if (at == items_.end() || at->client_slot != client_slot) {
    return false;
  }
  // ⚠ UNE QUANTITÉ NULLE NE RETIRE RIEN, et surtout ne vide pas la case. Le
  // serveur dit toujours combien il retire ; traiter zéro comme « tout » ferait
  // disparaître une pile entière sur un paquet mal lu.
  if (amount <= 0) {
    return false;
  }
  if (amount >= at->amount) {
    items_.erase(at);
  } else {
    at->amount = static_cast<std::int16_t>(at->amount - amount);
  }
  ++revision_;
  return true;
}

bool Inventory::SetFavorite(std::uint16_t client_slot, bool favorite) {
  const auto found = LowerBoundBySlot(items_, client_slot);
  if (found == items_.end() || found->client_slot != client_slot) {
    return false;
  }
  if (found->favorite == favorite) {
    return true;
  }
  found->favorite = favorite;
  ++revision_;
  return true;
}

namespace {

// Les six mutations ci-dessous ne different que par ce qu'elles ecrivent : la
// recherche, le refus d'une case vide et le compte de revisions sont les memes.
template <typename Change>
[[nodiscard]] bool ChangeSlot(std::vector<InventoryItem>& items, std::uint16_t client_slot,
                              std::uint32_t& revision, Change change) {
  const auto found = LowerBoundBySlot(items, client_slot);
  if (found == items.end() || found->client_slot != client_slot) {
    return false;
  }
  change(*found);
  ++revision;
  return true;
}

}  // namespace

bool Inventory::Wear(std::uint16_t client_slot, std::uint32_t worn_at, std::uint16_t view_id) {
  return ChangeSlot(items_, client_slot, revision_, [worn_at, view_id](InventoryItem& item) {
    item.worn_at = worn_at;
    // ⚠ ZÉRO N'EFFACE PAS L'APPARENCE. Le serveur ne la renvoie que pour ce qui
    // se VOIT ; une arme invisible arrive avec zéro, et l'écraser ferait perdre
    // celle que la liste avait donnée.
    if (view_id != 0) {
      item.view_id = view_id;
    }
  });
}

bool Inventory::TakeOff(std::uint16_t client_slot) {
  return ChangeSlot(items_, client_slot, revision_, [](InventoryItem& item) { item.worn_at = 0; });
}

bool Inventory::SetAmount(std::uint16_t client_slot, std::int16_t amount) {
  if (amount <= 0) {
    // Plus rien : la ligne disparait, comme apres un retrait complet.
    const auto found = LowerBoundBySlot(items_, client_slot);
    if (found == items_.end() || found->client_slot != client_slot) {
      return false;
    }
    items_.erase(found);
    ++revision_;
    return true;
  }
  return ChangeSlot(items_, client_slot, revision_,
                    [amount](InventoryItem& item) { item.amount = amount; });
}

bool Inventory::InsertCard(std::uint16_t equip_slot, std::uint16_t card_slot) {
  const InventoryItem* card = Find(card_slot);
  if (card == nullptr || Find(equip_slot) == nullptr) {
    return false;
  }
  const std::uint32_t card_id = card->item_id;
  // ⚠ L'ÉCRITURE AVANT LE RETRAIT : retirer la dernière carte efface sa ligne,
  // et le vecteur déplace alors ce qui la suit — pièce comprise.
  static_cast<void>(ChangeSlot(items_, equip_slot, revision_, [card_id](InventoryItem& equip) {
    const auto empty = std::ranges::find(equip.cards, std::uint32_t{0});
    if (empty != equip.cards.end()) {
      *empty = card_id;
    }
  }));
  return Take(card_slot, 1);
}

bool Inventory::Identify(std::uint16_t client_slot) {
  return ChangeSlot(items_, client_slot, revision_,
                    [](InventoryItem& item) { item.identified = true; });
}

bool Inventory::Bind(std::uint16_t client_slot) {
  return ChangeSlot(items_, client_slot, revision_,
                    [](InventoryItem& item) { item.bind_on_equip = kItemBoundToOwner; });
}

bool Inventory::MarkWornDamaged(std::uint32_t worn_at) {
  if (worn_at == 0) {
    return false;
  }
  bool marked = false;
  for (InventoryItem& item : items_) {
    // ⚠ TOUT CE QUI OCCUPE CET EMPLACEMENT, et il peut y en avoir plusieurs :
    // une arme a deux mains couvre les deux mains, et une armure d'ombre partage
    // son numero avec sa jumelle.
    if ((item.worn_at & worn_at) != 0) {
      item.damaged = true;
      marked = true;
    }
  }
  if (marked) {
    ++revision_;
  }
  return marked;
}

namespace {

// LE CONTRAT DES DEUX JOURNAUX, écrit une fois : fait de la place dans l'anneau
// et rend le rang de l'évènement qui arrive.
//
// L'ANNEAU ÉCRÊTE PAR LE DÉBUT, ET LE RANG NE LE SUIT PAS : il compte les
// évènements, pas les entrées gardées. C'est ce qui permet à un témoin de
// savoir qu'il a manqué un tour plutôt que de relire deux fois le même rang.
//
// Le rang part à UN : zéro veut dire « rien n'est arrivé », et c'est ce qui
// rend un témoin à zéro inoffensif.
template <typename Entry>
[[nodiscard]] std::uint64_t NextRankInRing(std::vector<Entry>& journal, std::size_t kept,
                                           std::uint64_t& seen) {
  if (journal.size() >= kept) {
    journal.erase(journal.begin());
  }
  ++seen;
  return seen;
}

}  // namespace

void Inventory::RecordObtained(const InventoryItem& picked, std::uint32_t at_ms) {
  const std::uint64_t rank = NextRankInRing(obtained_, kObtainedItemsKept, obtained_seen_);
  ++pickup_arrivals_;
  obtained_.push_back(ObtainedItem{
      .seq = rank, .arrival = pickup_arrivals_, .obtained_ms = at_ms, .picked = picked});
}

std::span<const ObtainedItem> Inventory::obtained() const noexcept { return obtained_; }

std::uint64_t Inventory::last_obtained_seq() const noexcept { return obtained_seen_; }

void Inventory::RecordRefused(std::uint8_t result, std::uint32_t at_ms) {
  // ⚠ LE CODE ENTRE TEL QUEL, sans tri ni traduction : `sim` ne sait pas
  // lesquels ce serveur émet, et le choix d'en taire un appartient à la couche
  // qui écrit la ligne de conversation.
  const std::uint64_t rank = NextRankInRing(refused_, kRefusedPickupsKept, refused_seen_);
  ++pickup_arrivals_;
  refused_.push_back(RefusedPickup{
      .seq = rank, .arrival = pickup_arrivals_, .refused_ms = at_ms, .result = result});
}

std::span<const RefusedPickup> Inventory::refused() const noexcept { return refused_; }

std::uint64_t Inventory::last_refused_seq() const noexcept { return refused_seen_; }

void Inventory::RecordDropped(const InventoryItem& dropped, std::uint32_t at_ms) {
  const std::uint64_t rank = NextRankInRing(dropped_, kDroppedItemsKept, dropped_seen_);
  ++pickup_arrivals_;
  dropped_.push_back(DroppedItem{
      .seq = rank, .arrival = pickup_arrivals_, .dropped_ms = at_ms, .dropped = dropped});
}

std::span<const DroppedItem> Inventory::dropped() const noexcept { return dropped_; }

std::uint64_t Inventory::last_dropped_seq() const noexcept { return dropped_seen_; }

void Inventory::SetCapacity(int slots) {
  if (slots == capacity_) {
    return;
  }
  capacity_ = slots;
  ++revision_;
}

void Inventory::SetLoad(std::int64_t weight, std::int64_t max_weight) {
  if (weight == weight_ && max_weight == max_weight_) {
    return;
  }
  weight_ = weight;
  max_weight_ = max_weight;
  ++revision_;
}

void Inventory::Clear() {
  // ⚠ LE CONTENU SEUL. Ce que le contenant PEUT porter est annoncé par un autre
  // paquet, souvent AVANT la liste : l'effacer ici perdrait la capacité à chaque
  // remplissage, et le pied afficherait un maximum tombé à zéro.
  //
  // ⚠ ET LES DEUX JOURNAUX RESTENT — gains ET refus —, `Forget()` compris :
  // ils disent ce qui EST ARRIVÉ, pas ce que le contenant porte. Le serveur
  // reliste le sac à chaque entrée sur une carte, et les effacer là ferait
  // disparaître le bandeau d'un objet ramassé la seconde d'avant, comme la
  // ligne rouge d'un ramassage refusé dans la foulée.
  items_.clear();
  name_.clear();
  filling_ = false;
  ++revision_;
}

void Inventory::Forget() {
  // Le contenant lui-même a disparu — une charrette qu'on rend. Sa capacité s'en
  // va avec lui, à la différence d'un coffre qu'on referme et qui existe encore.
  Clear();
  capacity_ = 0;
  weight_ = 0;
  max_weight_ = 0;
}

const InventoryItem* Inventory::Find(std::uint16_t client_slot) const {
  const auto at = std::lower_bound(
      items_.begin(), items_.end(), client_slot,
      [](const InventoryItem& item, std::uint16_t slot) { return item.client_slot < slot; });
  if (at == items_.end() || at->client_slot != client_slot) {
    return nullptr;
  }
  return &*at;
}

const InventoryItem* Inventory::WornAt(std::uint32_t location) const {
  if (location == 0) {
    return nullptr;
  }
  for (const InventoryItem& item : items_) {
    if ((item.worn_at & location) == location) {
      return &item;
    }
  }
  return nullptr;
}

}  // namespace ro::sim
