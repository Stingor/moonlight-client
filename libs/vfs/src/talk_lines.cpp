#include "ro/vfs/talk_lines.h"

#include <string>

#include "install_table.h"

namespace ro::vfs {

std::string_view TalkLines::Line(std::size_t number) const {
  if (number == 0 || number > lines.size()) {
    return {};
  }
  return lines[number - 1];
}

core::Result<TalkLines> ParseTalkLines(std::string_view yaml_text, std::string_view table_path) {
  TalkLines talk;
  auto read = VisitTableSection(yaml_text, table_path, "lines", YAML::NodeType::Sequence,
                                [&talk](const YAML::Node& entries) {
                                  for (const YAML::Node& line : entries) {
                                    talk.lines.push_back(line.as<std::string>());
                                  }
                                });
  if (!read) {
    return std::unexpected(read.error());
  }
  return talk;
}

core::Result<TalkLines> ReadTalkLines(Vfs& vfs, std::string_view table_path) {
  auto text = ReadOptionalTable(vfs, table_path);
  if (!text) {
    return std::unexpected(text.error());
  }
  if (!text->has_value()) {
    return core::Fail(core::ErrorCode::kNotFound, std::string(table_path) + " absente");
  }
  return ParseTalkLines(**text, table_path);
}

}  // namespace ro::vfs
