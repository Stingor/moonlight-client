// Le libellé d'une case de filtre, traduit à la lecture. Voir chat_type_labels.h.

#include <cstddef>

#include "ro/core/i18n.h"

#include "chat/chat_tabs.h"
#include "chat/chat_type_labels.h"

namespace moonclient {

const char* ChatTypeLabel(ro::sim::ChatType type) {
  const auto rank = static_cast<std::size_t>(type);
  return rank < kChatTypeLabels.size() ? ro::core::Tr(kChatTypeLabels[rank]) : "";
}

}  // namespace moonclient
