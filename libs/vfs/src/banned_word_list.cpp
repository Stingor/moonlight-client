#include "ro/vfs/banned_word_list.h"

#include "install_table.h"

namespace ro::vfs {

core::Result<std::vector<std::string>> ParseBannedWordList(std::string_view yaml_text) {
  std::vector<std::string> words;
  auto read = VisitTableSection(yaml_text, kBannedWordsPath, "words", YAML::NodeType::Sequence,
                                [&words](const YAML::Node& entries) {
                                  for (const YAML::Node& word : entries) {
                                    words.push_back(word.as<std::string>());
                                  }
                                });
  if (!read) {
    return std::unexpected(read.error());
  }
  return words;
}

core::Result<std::vector<std::string>> ReadBannedWordList(Vfs& vfs) {
  auto text = ReadOptionalTable(vfs, kBannedWordsPath);
  if (!text) {
    return std::unexpected(text.error());
  }
  return text->has_value() ? ParseBannedWordList(**text) : std::vector<std::string>{};
}

}  // namespace ro::vfs
