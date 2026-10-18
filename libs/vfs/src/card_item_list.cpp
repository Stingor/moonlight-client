#include "ro/vfs/card_item_list.h"

#include "install_table.h"

namespace ro::vfs {

core::Result<CardItemList> ParseCardItemList(std::string_view yaml_text) {
  CardItemList cards;
  auto read = VisitTableSection(yaml_text, kCardItemListPath, "cards", YAML::NodeType::Sequence,
                                [&cards](const YAML::Node& entries) {
                                  for (const YAML::Node& card : entries) {
                                    cards.item_ids.insert(ItemIdOf(card));
                                  }
                                });
  if (!read) {
    return std::unexpected(read.error());
  }
  return cards;
}

core::Result<CardItemList> ReadCardItemList(Vfs& vfs) {
  auto text = ReadOptionalTable(vfs, kCardItemListPath);
  if (!text) {
    return std::unexpected(text.error());
  }
  return text->has_value() ? ParseCardItemList(**text) : CardItemList{};
}

}  // namespace ro::vfs
