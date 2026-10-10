#include "ro/vfs/random_option_names.h"

#include <array>
#include <limits>
#include <optional>
#include <string>

#include "ro/formats/lua_table.h"
#include "ro/vfs/encoding.h"
#include "ro/vfs/lua_chain.h"

namespace ro::vfs {
namespace {

constexpr std::string_view kTableGlobal = "NameTable_VAR";

}  // namespace

std::string FillOptionTemplate(std::string_view pattern, int value) {
  std::string out;
  out.reserve(pattern.size() + 8);
  bool value_written = false;
  for (std::size_t at = 0; at < pattern.size(); ++at) {
    const char c = pattern[at];
    if (c != '%') {
      out.push_back(c);
      continue;
    }
    if (at + 1 >= pattern.size()) {
      return {};
    }
    const char spec = pattern[at + 1];
    ++at;
    if (spec == '%') {
      out.push_back('%');
    } else if (spec == 'd' && !value_written) {
      out += std::to_string(value);
      value_written = true;
    } else {
      return {};  // une conversion qu'on ne sait pas remplir sans risque
    }
  }
  return out;
}

std::string RandomOptionNames::Format(std::uint32_t option_id, int value) const {
  const auto found = templates.find(option_id);
  if (found == templates.end()) {
    return {};
  }
  return FillOptionTemplate(found->second, value);
}

core::Result<RandomOptionNames> ReadRandomOptionNames(Vfs& vfs) {
  RandomOptionNames names;
  const std::array<std::string_view, 2> chain = {kEnumVarPath, kRandomOptionNamesPath};
  auto globals = ReadLuaChain(vfs, chain);
  if (!globals) {
    if (globals.error().code == core::ErrorCode::kNotFound) {
      return names;
    }
    return std::unexpected(globals.error());
  }
  const formats::LuaValue* table = globals->Find(kTableGlobal);
  if (table == nullptr || table->kind != formats::LuaValue::Kind::kTable || !table->table) {
    return names;
  }
  for (const auto& [key, value] : table->table->by_index) {
    const std::optional<std::uint32_t> id =
        formats::LuaWholeNumber(key, std::numeric_limits<std::uint32_t>::max());
    if (!id.has_value() || value.kind != formats::LuaValue::Kind::kString) {
      continue;
    }
    // Les tables du client sont dans sa page de codes.
    names.templates[*id] = Cp949ToUtf8Lossy(value.text);
  }
  return names;
}

}  // namespace ro::vfs
