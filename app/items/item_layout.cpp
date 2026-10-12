#include "items/item_layout.h"

#include <algorithm>
#include <cstddef>

namespace moonclient {

using ro::ui::InventoryCellView;

int ItemLayout::CellOf(std::uint32_t item_id) const {
  const auto found = places_.find(item_id);
  return found == places_.end() ? kNowhere : found->second;
}

void ItemLayout::Put(std::uint32_t item_id, int cell) {
  if (item_id == 0 || cell < 0 || cell >= kFarthestCell) {
    return;
  }
  places_[item_id] = cell;
}

void ItemLayout::Place(std::uint32_t item_id, int cell, std::uint32_t sitting_there) {
  if (item_id == 0 || cell < 0 || cell >= kFarthestCell) {
    return;
  }
  const int leaving = CellOf(item_id);
  if (sitting_there != 0 && sitting_there != item_id) {
    if (leaving == kNowhere) {
      // ⚠ RIEN À LUI DONNER : l'objet tiré remplissait un trou. L'occupant
      // redevient libre et retombe dans les trous — le garder sur une case qu'un
      // autre occupe désormais ferait deux objets sur la même.
      places_.erase(sitting_there);
    } else {
      places_[sitting_there] = leaving;
    }
  }
  places_[item_id] = cell;
}

void ItemLayout::Clear() noexcept { places_.clear(); }

void ArrangeCells(std::vector<InventoryCellView>& cells, const ItemLayout& layout) {
  if (cells.empty()) {
    return;
  }
  // QUI A UNE CASE, ET LAQUELLE.
  //
  // ⚠ UNE CASE PRISE DEUX FOIS NE FAIT PAS DISPARAÎTRE LE SECOND : il retombe
  // dans les trous. Cela ne peut venir que d'un fichier corrigé à la main, et
  // c'est précisément le genre de chose qui ne doit pas perdre un objet.
  std::map<int, std::size_t> settled;
  std::vector<std::size_t> loose;
  loose.reserve(cells.size());
  for (std::size_t rank = 0; rank < cells.size(); ++rank) {
    const int cell = layout.CellOf(cells[rank].item_id);
    if (cell != ItemLayout::kNowhere && settled.emplace(cell, rank).second) {
      continue;
    }
    loose.push_back(rank);
  }
  if (settled.empty()) {
    return;
  }

  // La grille va jusqu'à la plus lointaine case occupée, et jamais moins loin
  // qu'il n'y a d'objets à loger.
  const auto farthest = static_cast<std::size_t>(settled.rbegin()->first) + 1;
  const std::size_t size = std::max(cells.size(), farthest);
  std::vector<InventoryCellView> ranked(size);
  for (const auto& [cell, rank] : settled) {
    ranked[static_cast<std::size_t>(cell)] = cells[rank];
  }
  // 🔴 IL RESTE TOUJOURS ASSEZ DE TROUS : la grille compte au moins autant de
  // cases que d'objets, et les objets casés en occupent exactement autant qu'ils
  // sont. Ce qui reste — les trous — ne peut donc pas être moins nombreux que
  // les objets libres, et aucun ne se perd faute de place.
  auto next = loose.begin();
  for (std::size_t cell = 0; cell < size && next != loose.end(); ++cell) {
    if (ranked[cell].client_slot != 0) {
      continue;
    }
    ranked[cell] = cells[*next];
    ++next;
  }
  cells.swap(ranked);
}

}  // namespace moonclient
