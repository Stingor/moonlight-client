#include "text_table.h"

#include <charconv>
#include <cstdint>
#include <string>
#include <system_error>

namespace ro::vfs {
namespace {

// Le séparateur de champs de ces tables : `4001#Lucky#`.
constexpr char kFieldSeparator = '#';

}  // namespace

std::vector<std::string_view> SplitLines(std::string_view text) {
  std::vector<std::string_view> lines;
  std::size_t start = 0;
  while (start <= text.size()) {
    const std::size_t stop = text.find('\n', start);
    std::string_view line =
        text.substr(start, stop == std::string_view::npos ? std::string_view::npos : stop - start);
    if (!line.empty() && line.back() == '\r') {
      line.remove_suffix(1);
    }
    lines.push_back(line);
    if (stop == std::string_view::npos) {
      break;
    }
    start = stop + 1;
  }
  return lines;
}

std::string_view AsText(const std::vector<std::uint8_t>& bytes) {
  return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

core::Result<std::vector<std::uint8_t>> ReadOptionalFile(Vfs& vfs, std::string_view path) {
  auto bytes = vfs.Read(std::string(path));
  if (!bytes && bytes.error().code == core::ErrorCode::kNotFound) {
    return std::vector<std::uint8_t>{};
  }
  return bytes;
}

bool ParseTableId(std::string_view field, std::uint32_t& out) {
  if (field.empty()) {
    return false;
  }
  // ⚠ `from_chars` ET NON UNE BOUCLE À LA MAIN : il refuse le débordement, là
  // où la boucle repliait un numéro trop long sur celui d'une autre ligne.
  std::uint32_t value = 0;
  const auto [end, error] = std::from_chars(field.data(), field.data() + field.size(), value);
  if (error != std::errc{} || end != field.data() + field.size()) {
    return false;
  }
  out = value;
  return true;
}

bool SplitTableRow(std::string_view line, std::uint32_t& id, std::string_view& field) {
  const std::size_t separator = line.find(kFieldSeparator);
  if (separator == std::string_view::npos) {
    return false;
  }
  if (!ParseTableId(line.substr(0, separator), id)) {
    return false;
  }
  std::string_view rest = line.substr(separator + 1);
  // ⚠ LE SECOND DIÈSE FERME LE CHAMP, et il peut manquer : une ligne tronquée
  // rend alors tout ce qui reste plutôt que rien.
  const std::size_t end = rest.find(kFieldSeparator);
  field = end == std::string_view::npos ? rest : rest.substr(0, end);
  return true;
}

}  // namespace ro::vfs
