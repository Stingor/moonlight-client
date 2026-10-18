#include "ro/vfs/card_illustrations.h"

#include <string>

#include "install_table.h"

namespace ro::vfs {

std::string_view CardIllustrations::Find(std::uint32_t item_id) const {
  const auto found = by_id.find(item_id);
  return found != by_id.end() ? std::string_view(found->second) : std::string_view();
}

core::Result<CardIllustrations> ParseCardIllustrations(std::string_view yaml_text) {
  CardIllustrations cards;
  auto read = VisitTableSection(yaml_text, kCardIllustrationTablePath, "cards", YAML::NodeType::Map,
                                [&cards](const YAML::Node& entries) {
                                  for (const auto& pair : entries) {
                                    cards.by_id[ItemIdOf(pair.first)] = ArchivedText(pair.second);
                                  }
                                });
  if (!read) {
    return std::unexpected(read.error());
  }
  return cards;
}

core::Result<CardIllustrations> ReadCardIllustrations(Vfs& vfs) {
  auto text = ReadOptionalTable(vfs, kCardIllustrationTablePath);
  if (!text) {
    return std::unexpected(text.error());
  }
  return text->has_value() ? ParseCardIllustrations(**text) : CardIllustrations{};
}

}  // namespace ro::vfs
