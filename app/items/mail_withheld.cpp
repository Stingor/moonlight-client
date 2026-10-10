#include "items/mail_withheld.h"

#include <algorithm>

namespace moonclient {

void WithholdMailedCells(std::vector<ro::ui::InventoryCellView>& cells,
                         const std::function<std::int32_t(std::uint16_t)>& withheld) {
  if (!withheld) {
    return;
  }
  for (ro::ui::InventoryCellView& cell : cells) {
    const std::int32_t held = withheld(cell.client_slot);
    if (held > 0) {
      cell.amount = std::max(cell.amount - held, 0);
    }
  }
  std::erase_if(cells, [&withheld](const ro::ui::InventoryCellView& cell) {
    return cell.amount <= 0 && withheld(cell.client_slot) > 0;
  });
}

}  // namespace moonclient
