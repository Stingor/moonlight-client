#include "ro/sim/npc_shop_saved_carts.h"

#include <algorithm>

#include "ro/core/text.h"
#include "ro/sim/npc_shop.h"

namespace ro::sim {
namespace {

[[nodiscard]] bool IsBlank(char letter) {
  return letter == ' ' || letter == '\t' || letter == '\n' || letter == '\r';
}

}  // namespace

std::string TidySavedCartName(std::string_view typed) {
  while (!typed.empty() && IsBlank(typed.front())) {
    typed.remove_prefix(1);
  }
  while (!typed.empty() && IsBlank(typed.back())) {
    typed.remove_suffix(1);
  }
  constexpr std::size_t kRoom = kSavedCartNameBytes - 1;
  // La coupe tombe au début d'un caractère : un octet orphelin, ni l'écran ni
  // le fichier ne savent le lire.
  return std::string(core::Utf8Prefix(typed, kRoom));
}

std::vector<SavedCartLine> TidySavedCartLines(std::span<const SavedCartLine> lines) {
  std::vector<SavedCartLine> tidy;
  for (const SavedCartLine& line : lines) {
    if (line.item_id == 0 || line.amount <= 0) {
      continue;
    }
    const auto same = std::ranges::find(tidy, line.item_id, &SavedCartLine::item_id);
    if (same == tidy.end()) {
      SavedCartLine kept = line;
      kept.amount = std::min(line.amount, kNpcShopMostPerStack);
      tidy.push_back(std::move(kept));
    } else {
      same->amount = std::min(same->amount + line.amount, kNpcShopMostPerStack);
    }
  }
  return tidy;
}

SavedCartRefusal SavedCartBook::Save(std::string_view name, std::span<const SavedCartLine> lines) {
  std::string tidy = TidySavedCartName(name);
  if (tidy.empty()) {
    return SavedCartRefusal::kEmptyName;
  }
  std::vector<SavedCartLine> kept_lines = TidySavedCartLines(lines);
  if (kept_lines.empty()) {
    return SavedCartRefusal::kEmptyCart;
  }
  for (SavedCart& kept : carts_) {
    if (kept.name == tidy) {
      kept.lines = std::move(kept_lines);
      return SavedCartRefusal::kNone;
    }
  }
  if (carts_.size() >= kSavedCartLimit) {
    return SavedCartRefusal::kFull;
  }
  carts_.push_back(SavedCart{.name = std::move(tidy), .lines = std::move(kept_lines)});
  return SavedCartRefusal::kNone;
}

SavedCartRefusal SavedCartBook::Rename(std::size_t index, std::string_view name) {
  if (index >= carts_.size()) {
    return SavedCartRefusal::kUnknown;
  }
  std::string tidy = TidySavedCartName(name);
  if (tidy.empty()) {
    return SavedCartRefusal::kEmptyName;
  }
  for (std::size_t other = 0; other < carts_.size(); ++other) {
    if (other != index && carts_[other].name == tidy) {
      return SavedCartRefusal::kNameTaken;
    }
  }
  carts_[index].name = std::move(tidy);
  return SavedCartRefusal::kNone;
}

SavedCartRefusal SavedCartBook::Remove(std::size_t index) {
  if (index >= carts_.size()) {
    return SavedCartRefusal::kUnknown;
  }
  carts_.erase(carts_.begin() + static_cast<std::ptrdiff_t>(index));
  return SavedCartRefusal::kNone;
}

}  // namespace ro::sim
