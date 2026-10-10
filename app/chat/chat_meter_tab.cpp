#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "ro/core/i18n.h"

#include "chat/chat_tab_view.h"

namespace moonclient {

bool ChatTabsView::IsMeter(std::uint32_t group, int local) const {
  const auto found = shown_.find(group);
  if (found == shown_.end() || local < 0 ||
      static_cast<std::size_t>(local) >= found->second.ranks.size()) {
    return false;
  }
  return tabs_.tabs()[found->second.ranks[static_cast<std::size_t>(local)]].content ==
         ChatTabContent::kDamageMeter;
}

void ChatTabsView::DockMeter(std::uint32_t group, int local) {
  // ⚠ LE RANG SE TRADUIT DANS LA BANDE TELLE QUE LE JOUEUR LA VOYAIT : celui de
  // la languette qui occupait la place visée, ou juste après la dernière.
  std::size_t before = tabs_.count();
  if (const auto found = shown_.find(group); found != shown_.end()) {
    const std::vector<std::size_t>& ranks = found->second.ranks;
    if (local >= 0 && static_cast<std::size_t>(local) < ranks.size()) {
      before = ranks[static_cast<std::size_t>(local)];
    } else if (!ranks.empty()) {
      before = ranks.back() + 1;
    }
  }
  tabs_.DockMeter(std::string(ro::core::Tr("DPS")), group, before);
  SaveIfNeeded();
}

void ChatTabsView::UndockMeter() {
  tabs_.UndockMeter();
  SaveIfNeeded();
}

}  // namespace moonclient
