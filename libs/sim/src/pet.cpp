// Voir l'en-tête.

#include "ro/sim/pet.h"

#include "ro/sim/pet_egg.h"

namespace ro::sim {
namespace {

// Les paliers de la faim : au moins cette valeur pour atteindre le rang.
constexpr int kHungerRank1 = 11;
constexpr int kHungerRank2 = 26;
constexpr int kHungerRank3 = 76;
constexpr int kHungerRank4 = 91;

// Les paliers de l'intimité : PLUS que cette valeur pour atteindre le rang —
// et les mêmes bornes, incluses, pour le mot de la fiche.
constexpr int kIntimacyRank1Above = 100;
constexpr int kIntimacyRank2Above = 250;
constexpr int kIntimacyRank3Above = 750;
constexpr int kIntimacyRank4Above = 900;

// Les montures Doram, et leur barème de haine sous le premier palier.
constexpr std::uint32_t kDoramMountFirstClass = 4050;
constexpr std::uint32_t kDoramMountLastClass = 4052;
constexpr int kHateWithPassionAtMost = 20;
constexpr int kHateAtMost = 40;

// Les rangs d'intimité d'un œuf.
constexpr int kEggRankAwkward = 1;
constexpr int kEggRankShy = 2;
constexpr int kEggRankNeutral = 3;
constexpr int kEggRankCordial = 4;
constexpr int kEggRankLoyal = 5;

}  // namespace

int PetHungerRank(int hunger) noexcept {
  if (hunger >= kHungerRank4) {
    return 4;
  }
  if (hunger >= kHungerRank3) {
    return 3;
  }
  if (hunger >= kHungerRank2) {
    return 2;
  }
  return hunger >= kHungerRank1 ? 1 : 0;
}

int PetIntimacyRank(int intimacy) noexcept {
  if (intimacy > kIntimacyRank4Above) {
    return 4;
  }
  if (intimacy > kIntimacyRank3Above) {
    return 3;
  }
  if (intimacy > kIntimacyRank2Above) {
    return 2;
  }
  return intimacy > kIntimacyRank1Above ? 1 : 0;
}

PetIntimacyWord PetIntimacyWordOf(int intimacy, std::uint32_t pet_class) noexcept {
  const bool doram_mount = pet_class >= kDoramMountFirstClass && pet_class <= kDoramMountLastClass;
  if (doram_mount && intimacy <= kIntimacyRank1Above) {
    if (intimacy <= kHateWithPassionAtMost) {
      return PetIntimacyWord::kHateWithPassion;
    }
    if (intimacy <= kHateAtMost) {
      return PetIntimacyWord::kHate;
    }
    return PetIntimacyWord::kAwkward;
  }
  if (intimacy <= kIntimacyRank1Above) {
    return PetIntimacyWord::kAwkward;
  }
  if (intimacy <= kIntimacyRank2Above) {
    return PetIntimacyWord::kShy;
  }
  if (intimacy <= kIntimacyRank3Above) {
    return PetIntimacyWord::kNeutral;
  }
  if (intimacy <= kIntimacyRank4Above) {
    return PetIntimacyWord::kCordial;
  }
  if (intimacy <= kPetIntimacyMax) {
    return PetIntimacyWord::kLoyal;
  }
  return PetIntimacyWord::kUnknown;
}

std::optional<PetIntimacyWord> PetEggRankWord(int rank) noexcept {
  switch (rank) {
    case kEggRankAwkward:
      return PetIntimacyWord::kAwkward;
    case kEggRankShy:
      return PetIntimacyWord::kShy;
    case kEggRankNeutral:
      return PetIntimacyWord::kNeutral;
    case kEggRankCordial:
      return PetIntimacyWord::kCordial;
    case kEggRankLoyal:
      return PetIntimacyWord::kLoyal;
    default:
      return std::nullopt;
  }
}

PetNameRefusal CheckPetName(std::string_view wire_name) noexcept {
  if (wire_name.empty()) {
    return PetNameRefusal::kEmpty;
  }
  return wire_name.size() > kPetNameMaxBytes ? PetNameRefusal::kTooLong : PetNameRefusal::kNone;
}

PetEvolutionRefusal PetEvolutionRefusalOf(int intimacy, bool wears_accessory,
                                          bool has_materials) noexcept {
  if (intimacy < kPetEvolutionServerMinimum) {
    return PetEvolutionRefusal::kIntimacy;
  }
  if (wears_accessory) {
    return PetEvolutionRefusal::kAccessory;
  }
  return has_materials ? PetEvolutionRefusal::kNone : PetEvolutionRefusal::kMaterials;
}

std::uint32_t HatchedEggOf(const Inventory& bag) noexcept {
  for (const InventoryItem& item : bag.items()) {
    if (IsPetEgg(item.item_id) && item.damaged) {
      return item.item_id;
    }
  }
  return 0;
}

bool IsPetAccessoryItem(const InventoryItem& item) noexcept {
  return item.type == kPetAccessoryItemType && item.wearable_at == 0;
}

void Pet::ApplyProperty(const PetProperty& property) {
  name_ = property.name;
  renameable_ = !property.renamed;
  level_ = property.level;
  previous_hunger_ = hunger_;
  hunger_ = property.hunger;
  intimacy_ = property.intimacy;
  accessory_item_id_ = property.accessory_item_id;
  wears_accessory_ = property.accessory_item_id != 0;
  pet_class_ = property.pet_class;
  gone_ = false;
}

bool Pet::ApplyStateChange(PetStateChange change, std::uint32_t actor_id, std::int32_t value) {
  if (change == PetStateChange::kBirth) {
    actor_id_ = actor_id;
    gone_ = false;
    absent_since_ms_.reset();
    accessory_view_ = 0;
    return true;
  }
  // Les autres ne regardent que le nôtre : l'apparence et le spectacle partent
  // aussi vers le voisinage.
  if (actor_id_ != 0 && actor_id != actor_id_) {
    return false;
  }
  switch (change) {
    case PetStateChange::kIntimacy:
      NoteIntimacy(value);
      return true;
    case PetStateChange::kHunger:
      previous_hunger_ = hunger_;
      hunger_ = value;
      return true;
    case PetStateChange::kEgg:
      // Zéro : le familier n'existe plus. Sinon, l'œuf vient de changer en sac,
      // et la liste d'éclosion a fini son office.
      if (value == 0) {
        pet_class_ = 0;
      }
      CloseEggOffer();
      return true;
    case PetStateChange::kAccessory:
      // LA VALEUR EST UNE APPARENCE, pas l'item : non nulle, un accessoire est
      // porté. La fiche du serveur ne suit pas ; il faut la redemander pour
      // connaître l'item, une fois par apparence nouvelle.
      wears_accessory_ = value != 0;
      if (value == 0) {
        accessory_item_id_ = 0;
      } else if (value != accessory_view_) {
        sheet_wanted_ = true;
      }
      accessory_view_ = value;
      return true;
    case PetStateChange::kBirth:
    case PetStateChange::kPerformance:
    case PetStateChange::kHairstyle:
      return false;
  }
  return false;
}

void Pet::NoteIntimacy(int intimacy) {
  intimacy_ = intimacy;
  const bool low = intimacy < kPetLowIntimacyBelow && PetIntimacyRank(intimacy) == 0;
  if (!low) {
    low_intimacy_warned_id_ = 0;
    return;
  }
  if (low_intimacy_warned_id_ != actor_id_) {
    low_intimacy_warned_id_ = actor_id_;
    low_intimacy_warning_ = true;
  }
}

void Pet::AnnounceAutoFeed(bool enabled) {
  auto_feed_ = enabled;
  auto_feed_echo_ = enabled;
}

void Pet::OfferEggs(std::vector<std::uint16_t> client_slots) {
  offered_egg_slots_ = std::move(client_slots);
  eggs_offered_ = true;
  egg_offer_opened_ = true;
}

void Pet::ObservePresence(bool actor_seen, std::uint32_t map_entries, std::uint32_t now_ms) {
  seen_ = actor_seen;
  if (map_entries != map_entries_) {
    map_entries_ = map_entries;
    absent_since_ms_.reset();
    awaiting_return_ = true;
  }
  if (actor_seen || pet_class_ == 0 || actor_id_ == 0) {
    absent_since_ms_.reset();
    if (actor_seen) {
      gone_ = false;
      awaiting_return_ = false;
    }
    return;
  }
  // ⚠ APRÈS UNE ENTRÉE SUR CARTE, L'ABSENCE NE COMPTE PAS tant que le familier
  // n'a pas reparu : le chargement et les apparitions de la carte suivante
  // peuvent durer plus que le délai.
  if (awaiting_return_) {
    return;
  }
  if (!absent_since_ms_.has_value()) {
    absent_since_ms_ = now_ms;
    return;
  }
  if (now_ms - *absent_since_ms_ >= kPetGoneAfterMs) {
    gone_ = true;
  }
}

bool Pet::TakeSheetWanted() noexcept { return std::exchange(sheet_wanted_, false); }

bool Pet::TakeLowIntimacyWarning() noexcept { return std::exchange(low_intimacy_warning_, false); }

std::optional<std::uint32_t> Pet::TakeEvolutionResult() noexcept {
  return std::exchange(evolution_result_, std::nullopt);
}

std::optional<bool> Pet::TakeAutoFeedEcho() noexcept {
  return std::exchange(auto_feed_echo_, std::nullopt);
}

bool Pet::TakeEggOfferOpened() noexcept { return std::exchange(egg_offer_opened_, false); }

}  // namespace ro::sim
