#include "ro/vfs/card_illustrations.h"

#include <string>

#include "ro/core/text.h"

#include "text_table.h"

namespace ro::vfs {

std::string_view CardIllustrations::Find(std::uint32_t item_id) const {
  const auto found = by_id.find(item_id);
  return found != by_id.end() ? std::string_view(found->second) : std::string_view();
}

core::Result<CardIllustrations> ReadCardIllustrations(Vfs& vfs) {
  CardIllustrations cards;
  auto bytes = ReadOptionalFile(vfs, kCardIllustrationTablePath);
  if (!bytes) {
    return std::unexpected(bytes.error());
  }
  for (const std::string_view line : SplitLines(core::AsText(*bytes))) {
    std::uint32_t item_id = 0;
    std::string_view resource;
    if (!SplitTableRow(line, item_id, resource) || resource.empty()) {
      continue;
    }
    cards.by_id[item_id] = std::string(resource);
  }
  return cards;
}

}  // namespace ro::vfs
