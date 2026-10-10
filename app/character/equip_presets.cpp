#include "character/equip_presets.h"

#include <algorithm>
#include <bit>
#include <cstdio>
#include <utility>

#include <SDL3/SDL_keycode.h>

#include "ro/core/i18n.h"
#include "ro/sim/equip_location.h"

namespace moonclient {
namespace {

using ro::sim::InventoryItem;

// L'ordre de la rangée d'icônes : les dix places dans l'ordre de leurs bits, puis
// les places d'apparat — haut, bas, milieu, cape.
constexpr std::array<std::uint32_t, 10> kEquipmentPlaces = {
    ro::sim::kEquipHeadLow,        ro::sim::kEquipWeapon,        ro::sim::kEquipGarment,
    ro::sim::kEquipAccessoryRight, ro::sim::kEquipArmor,         ro::sim::kEquipShield,
    ro::sim::kEquipShoes,          ro::sim::kEquipAccessoryLeft, ro::sim::kEquipHeadTop,
    ro::sim::kEquipHeadMid};
constexpr std::array<std::uint32_t, 4> kCostumePlaces = {
    ro::sim::kEquipCostumeHeadTop, ro::sim::kEquipCostumeHeadLow, ro::sim::kEquipCostumeHeadMid,
    ro::sim::kEquipCostumeGarment};

constexpr std::uint32_t kEquipmentMask = [] {
  std::uint32_t mask = 0;
  for (const std::uint32_t place : kEquipmentPlaces) {
    mask |= place;
  }
  return mask;
}();

// Un accessoire qui déclare les deux côtés, une coiffe d'apparat qui déclare les
// trois étages : le serveur leur choisit UNE place, la première libre.
constexpr std::uint32_t kEitherAccessory =
    ro::sim::kEquipAccessoryLeft | ro::sim::kEquipAccessoryRight;
constexpr std::uint32_t kEitherCostumeHead =
    ro::sim::kEquipCostumeHeadTop | ro::sim::kEquipCostumeHeadMid | ro::sim::kEquipCostumeHeadLow;

[[nodiscard]] bool GoesOnOnePlaceOfSeveral(std::uint32_t position) {
  return (position & kEitherAccessory) == kEitherAccessory ||
         (position & kEitherCostumeHead) == kEitherCostumeHead;
}

// L'objet peut-il occuper plusieurs places à la fois ? Porté, il le dit par ce
// qu'il occupe ; rangé, par ce qu'il déclare.
[[nodiscard]] bool MaySpanSeveralPlaces(const InventoryItem& item) {
  const std::uint32_t footprint = item.worn_at != 0 ? item.worn_at : item.wearable_at;
  return std::popcount(footprint) > 1 && !GoesOnOnePlaceOfSeveral(footprint);
}

// Rangée, une arme qui déclare les deux mains peut être une dague : le serveur
// annonce ainsi les armes à une main de qui peut en porter deux.
[[nodiscard]] bool SurelySpansSeveralPlaces(const InventoryItem& item) {
  return MaySpanSeveralPlaces(item) &&
         (item.worn_at != 0 ||
          (item.wearable_at & ro::sim::kEquipBothHands) != ro::sim::kEquipBothHands);
}

[[nodiscard]] std::uint32_t PlacesOf(PresetFamily family) {
  switch (family) {
    case PresetFamily::kEquipment:
      return kEquipmentMask;
    case PresetFamily::kCostume:
      return ro::sim::kEquipCostumeMask;
    case PresetFamily::kAmmo:
      return ro::sim::kEquipAmmo;
  }
  return 0;
}

// La famille d'un objet se lit sur les places où il peut aller : un bit
// d'apparat pour un costume, celui de la munition, ou aucun des deux.
[[nodiscard]] bool FitsFamily(PresetFamily family, std::uint32_t wearable_at) {
  switch (family) {
    case PresetFamily::kEquipment:
      return (wearable_at & (ro::sim::kEquipCostumeMask | ro::sim::kEquipAmmo)) == 0;
    case PresetFamily::kCostume:
      return (wearable_at & ro::sim::kEquipCostumeMask) != 0;
    case PresetFamily::kAmmo:
      return (wearable_at & ro::sim::kEquipAmmo) != 0;
  }
  return false;
}

[[nodiscard]] bool WornAs(const InventoryItem& item, PresetFamily family) {
  return (item.worn_at & PlacesOf(family)) != 0;
}

[[nodiscard]] bool SameCopy(const InventoryItem& item, const PresetPiece& piece) {
  return item.refine_level == piece.refine_level && item.grade == piece.grade &&
         item.cards == piece.cards;
}

[[nodiscard]] bool SameEntry(const PresetPiece& left, const PresetPiece& right) {
  return left.item_id == right.item_id && left.refine_level == right.refine_level &&
         left.grade == right.grade && left.cards == right.cards && left.family == right.family;
}

// L'objet du sac que cette pièce désigne, parmi ceux qu'aucune autre n'a pris :
// l'exemplaire exact, sinon le premier du même objet — et à chaque fois celui
// qu'on porte d'abord. `exact_only` écarte les autres exemplaires.
[[nodiscard]] std::optional<std::size_t> Choose(const PresetPiece& piece,
                                                std::span<const InventoryItem> items,
                                                const std::vector<bool>& taken, bool exact_only) {
  std::optional<std::size_t> exact;
  std::optional<std::size_t> worn;
  std::optional<std::size_t> first;
  for (std::size_t rank = 0; rank < items.size(); ++rank) {
    const InventoryItem& item = items[rank];
    if (taken[rank] || piece.item_id == 0 || item.item_id != piece.item_id ||
        !FitsFamily(piece.family, item.wearable_at)) {
      continue;
    }
    const bool same = SameCopy(item, piece);
    if (exact_only && !same) {
      continue;
    }
    const bool on = WornAs(item, piece.family);
    if (same && on) {
      return rank;
    }
    if (same && !exact.has_value()) {
      exact = rank;
    }
    if (on && !worn.has_value()) {
      worn = rank;
    }
    if (!first.has_value()) {
      first = rank;
    }
  }
  if (exact.has_value()) {
    return exact;
  }
  return worn.has_value() ? worn : first;
}

// L'objet d'une entrée précédente identique, quand il peut occuper plusieurs
// places : l'entrée courante est peut-être cette même pièce, retenue deux fois.
[[nodiscard]] std::optional<std::size_t> EarlierTwin(
    std::span<const PresetPiece> pieces, std::size_t entry,
    std::span<const std::optional<std::size_t>> designated, std::span<const InventoryItem> items) {
  for (std::size_t before = 0; before < entry; ++before) {
    const std::optional<std::size_t> item = designated[before];
    if (item.has_value() && SameEntry(pieces[before], pieces[entry]) &&
        MaySpanSeveralPlaces(items[*item])) {
      return item;
    }
  }
  return std::nullopt;
}

// Une arme se demande dans une main nommée : demandée dans « l'une des deux »,
// une dague irait dans la main libre, et le serveur pose de toute façon une arme
// à deux mains sur les deux. Le reste se demande là où il déclare aller.
[[nodiscard]] std::uint32_t WantedPosition(const PresetPiece& piece, const InventoryItem& item) {
  if (piece.family == PresetFamily::kEquipment && (item.wearable_at & ro::sim::kEquipWeapon) != 0) {
    return piece.left_hand ? ro::sim::kEquipShield : ro::sim::kEquipWeapon;
  }
  return item.wearable_at;
}

}  // namespace

std::vector<PresetPiece> WornPresetPieces(const ro::sim::Inventory& bag) {
  std::vector<PresetPiece> pieces;
  std::vector<std::uint16_t> seen;
  const auto keep = [&](std::uint32_t place, PresetFamily family) {
    const InventoryItem* piece = bag.WornAt(place);
    if (piece == nullptr || std::ranges::find(seen, piece->client_slot) != seen.end()) {
      return;
    }
    seen.push_back(piece->client_slot);
    PresetPiece kept{.item_id = piece->item_id, .family = family};
    // La munition ne se retient que par son objet : c'est une pile qui s'use.
    if (family != PresetFamily::kAmmo) {
      kept.refine_level = piece->refine_level;
      kept.grade = piece->grade;
      kept.cards = piece->cards;
    }
    // Une arme à deux mains a déjà été retenue à la place de l'arme, qui passe
    // avant celle du bouclier.
    kept.left_hand =
        place == ro::sim::kEquipShield && (piece->wearable_at & ro::sim::kEquipWeapon) != 0;
    pieces.push_back(kept);
  };
  for (const std::uint32_t place : kEquipmentPlaces) {
    keep(place, PresetFamily::kEquipment);
  }
  for (const std::uint32_t place : kCostumePlaces) {
    keep(place, PresetFamily::kCostume);
  }
  keep(ro::sim::kEquipAmmo, PresetFamily::kAmmo);
  return pieces;
}

PresetPlan PlanPreset(const EquipPreset& preset, const ro::sim::Inventory& bag) {
  PresetPlan plan;
  const std::span<const InventoryItem> items = bag.items();
  std::vector<bool> taken(items.size(), false);
  std::vector<std::optional<std::size_t>> designated(preset.pieces.size());
  // Les poses dont le serveur respecte la place, et celles dont il la choisit.
  std::vector<PresetWear> named;
  std::vector<PresetWear> chosen_by_server;
  struct Kept {
    std::size_t rank = 0;
    std::uint32_t position = 0;
  };
  std::vector<Kept> kept;

  for (std::size_t entry = 0; entry < preset.pieces.size(); ++entry) {
    const PresetPiece& piece = preset.pieces[entry];
    // Ni remis, ni comptés manquants : un costume rangé au coffre ne rend pas
    // incomplet un preset de combat.
    if (piece.family == PresetFamily::kCostume && !preset.with_costumes) {
      continue;
    }
    std::optional<std::size_t> chosen;
    // Une entrée identique à une précédente dont la pièce peut occuper plusieurs
    // places — un preset peut retenir une arme à deux mains une fois par main —
    // ne prend qu'un exemplaire exact et distinct, et n'est jamais manquante :
    // l'autre exemplaire du sac remplacerait celui qu'on porte.
    if (const std::optional<std::size_t> twin =
            EarlierTwin(preset.pieces, entry, designated, items)) {
      if (SurelySpansSeveralPlaces(items[*twin])) {
        continue;
      }
      chosen = Choose(piece, items, taken, /*exact_only=*/true);
      if (!chosen.has_value()) {
        continue;
      }
    } else {
      chosen = Choose(piece, items, taken, /*exact_only=*/false);
      if (!chosen.has_value()) {
        ++plan.missing;
        continue;
      }
    }
    taken[*chosen] = true;
    designated[entry] = chosen;
    const InventoryItem& item = items[*chosen];
    const std::uint32_t position = WantedPosition(piece, item);
    if ((item.worn_at & position) != 0) {
      kept.push_back(Kept{.rank = *chosen, .position = position});
      continue;
    }
    // Portée ailleurs — une dague dans l'autre main —, elle se retire d'abord :
    // le serveur refuse de poser une pièce déjà portée.
    if (item.worn_at != 0) {
      plan.take_off.push_back(item.client_slot);
    }
    (GoesOnOnePlaceOfSeveral(position) ? chosen_by_server : named)
        .push_back(PresetWear{.client_slot = item.client_slot, .position = position});
  }

  // Le serveur retire la pièce ENTIÈRE qui occupait une place qu'il remplit : la
  // retirer à part ferait en plus tomber la munition avec l'arc.
  std::uint32_t replaced = 0;
  for (const PresetWear& wear : named) {
    replaced |= wear.position;
  }
  for (std::size_t rank = 0; rank < items.size(); ++rank) {
    const InventoryItem& item = items[rank];
    if (taken[rank] || item.worn_at == 0) {
      continue;
    }
    // Ni la munition, ni l'équipement d'ombre, ni les costumes que le preset ne
    // pilote pas.
    const bool equipment = (item.worn_at & kEquipmentMask) != 0;
    const bool costume = (item.worn_at & ro::sim::kEquipCostumeMask) != 0;
    if (!equipment && !(costume && preset.with_costumes)) {
      continue;
    }
    if ((item.worn_at & replaced) != 0) {
      continue;
    }
    plan.take_off.push_back(item.client_slot);
  }
  // Une pièce gardée qu'une pose nommée déloge se redemande, quand elle peut
  // aller ailleurs.
  for (const Kept& piece : kept) {
    const InventoryItem& item = items[piece.rank];
    if ((item.worn_at & replaced) != 0 && GoesOnOnePlaceOfSeveral(piece.position)) {
      chosen_by_server.push_back(
          PresetWear{.client_slot = item.client_slot, .position = piece.position});
    }
  }
  // Les poses nommées d'abord : sinon l'une d'elles pourrait déloger une pièce
  // à qui le serveur vient de choisir la même place.
  plan.wear = std::move(named);
  plan.wear.insert(plan.wear.end(), chosen_by_server.begin(), chosen_by_server.end());
  return plan;
}

std::vector<std::uint16_t> PlanUndress(const ro::sim::Inventory& bag, bool with_costumes) {
  std::vector<std::uint16_t> slots;
  for (const InventoryItem& item : bag.items()) {
    const bool equipment = (item.worn_at & kEquipmentMask) != 0;
    const bool costume = (item.worn_at & ro::sim::kEquipCostumeMask) != 0;
    if (equipment || (with_costumes && costume)) {
      slots.push_back(item.client_slot);
    }
  }
  return slots;
}

bool PresetKeyCapturable(std::uint32_t key) {
  return (key >= SDLK_A && key <= SDLK_Z) || (key >= SDLK_0 && key <= SDLK_9) ||
         (key >= SDLK_F1 && key <= SDLK_F12) || key == SDLK_SPACE;
}

const EquipPreset* EquipPresetBook::At(std::size_t rank) const noexcept {
  return rank < presets_.size() ? &presets_[rank] : nullptr;
}

EquipPresetBook::Saved EquipPresetBook::Save(std::string_view name,
                                             std::vector<PresetPiece> pieces) {
  for (EquipPreset& kept : presets_) {
    if (kept.name == name) {
      kept.pieces = std::move(pieces);
      ++revision_;
      return Saved::kReplaced;
    }
  }
  if (presets_.size() >= kPresetsPerCharacter) {
    return Saved::kFull;
  }
  presets_.push_back(EquipPreset{.name = std::string(name), .pieces = std::move(pieces)});
  ++revision_;
  return Saved::kAdded;
}

bool EquipPresetBook::Erase(std::size_t rank) {
  if (rank >= presets_.size()) {
    return false;
  }
  presets_.erase(presets_.begin() + static_cast<std::ptrdiff_t>(rank));
  ++revision_;
  return true;
}

bool EquipPresetBook::SetWithCostumes(std::size_t rank, bool with_costumes) {
  if (rank >= presets_.size() || presets_[rank].with_costumes == with_costumes) {
    return false;
  }
  presets_[rank].with_costumes = with_costumes;
  ++revision_;
  return true;
}

bool EquipPresetBook::SetHotkey(std::size_t rank, Binding hotkey) {
  if (rank >= presets_.size() || presets_[rank].hotkey == hotkey) {
    return false;
  }
  presets_[rank].hotkey = hotkey;
  ++revision_;
  return true;
}

std::optional<std::size_t> EquipPresetBook::Holding(Binding hotkey,
                                                    std::optional<std::size_t> except) const {
  if (!hotkey.bound()) {
    return std::nullopt;
  }
  for (std::size_t rank = 0; rank < presets_.size(); ++rank) {
    if (rank != except && presets_[rank].hotkey == hotkey) {
      return rank;
    }
  }
  return std::nullopt;
}

std::optional<std::size_t> EquipPresetBook::Release(Binding hotkey) {
  const std::optional<std::size_t> holder = Holding(hotkey);
  if (holder.has_value()) {
    presets_[*holder].hotkey = Binding{};
    ++revision_;
  }
  return holder;
}

std::string PresetOwnerLabel(std::string_view name) {
  std::array<char, 160> named{};
  std::snprintf(named.data(), named.size(), ro::core::Tr("le preset « %s »"),
                std::string(name).c_str());
  return std::string(named.data());
}

std::optional<std::string> PresetKeyHolder(const Hotkeys& table, const EquipPresetBook& book,
                                           std::optional<std::size_t> self, Binding wanted) {
  if (!wanted.bound()) {
    return std::nullopt;
  }
  if (const std::optional<std::size_t> rank = book.Holding(wanted, self)) {
    return PresetOwnerLabel(book.presets()[*rank].name);
  }
  if (const std::optional<ActionRef> action =
          table.ActionFor(wanted.key, wanted.ctrl, wanted.shift, wanted.alt)) {
    std::array<char, 160> named{};
    std::snprintf(named.data(), named.size(), ro::core::Tr("l'action « %s »"),
                  NameOfAction(*action).c_str());
    return std::string(named.data());
  }
  return std::nullopt;
}

}  // namespace moonclient
