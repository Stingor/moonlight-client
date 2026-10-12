// Le texte d'`item_info.yml`. Voir l'en-tête.

#include "ro/vfs/item_info_text.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>

#include "strict_text.h"

namespace ro::vfs {
namespace {

// Le nom de la table dans les refus.
constexpr std::string_view kFileLabel = "item_info.yml";
constexpr std::string_view kVersionKey = "version";
constexpr std::string_view kItemsKey = "items";
constexpr std::string_view kNameField = "name";
constexpr std::string_view kResourceField = "resource";
constexpr std::string_view kSlotsField = "slots";
constexpr std::string_view kViewField = "view";
constexpr std::string_view kDescriptionField = "description";
// Les deux champs que seuls les faits du serveur portent.
constexpr std::string_view kTypeField = "type";
constexpr std::string_view kLocationsField = "locations";

// Un champ d'objet, après son nom et ses deux-points.
[[nodiscard]] bool ReadField(StrictText& table, std::string_view field, ItemInfoRecord& record) {
  std::uint32_t number = 0;
  if (field == kNameField) {
    return table.ReadQuoted(record.name);
  }
  if (field == kResourceField) {
    return table.ReadQuoted(record.resource);
  }
  if (field == kSlotsField) {
    const bool read = table.ReadWhole(std::numeric_limits<std::uint8_t>::max(), number);
    record.slots = static_cast<std::uint8_t>(number);
    return read;
  }
  if (field == kViewField) {
    const bool read = table.ReadWhole(std::numeric_limits<std::uint16_t>::max(), number);
    record.view = static_cast<std::uint16_t>(number);
    return read;
  }
  if (field == kDescriptionField) {
    return table.ReadQuotedList(record.description);
  }
  return false;
}

// Quels champs le texte peut porter.
enum class TextForm : std::uint8_t {
  kTable,        // la table : ses seuls champs
  kServerFacts,  // les faits du serveur : ceux de la table, plus `type` et `locations`
};

// Un champ des faits, ou de la table si ce n'en est pas un.
[[nodiscard]] bool ReadAnyField(StrictText& table, TextForm form, std::string_view field,
                                ItemServerFactsRecord& record) {
  if (form == TextForm::kServerFacts) {
    if (field == kTypeField) {
      return table.ReadQuoted(record.type);
    }
    if (field == kLocationsField) {
      return table.ReadQuotedList(record.locations);
    }
  }
  return ReadField(table, field, record.sheet);
}

// `{<champ>: <valeur>, …}`.
[[nodiscard]] core::Result<void> ReadRecord(StrictText& table, TextForm form,
                                            ItemServerFactsRecord& record) {
  if (!table.Take('{')) {
    return std::unexpected(table.Refused("un objet s'ouvre par « { »"));
  }
  if (table.Take('}')) {
    return {};
  }
  do {
    std::string_view field;
    if (!table.ReadKey(field) || !table.Take(':')) {
      return std::unexpected(table.Refused("nom de champ attendu"));
    }
    if (!ReadAnyField(table, form, field, record)) {
      return std::unexpected(
          table.Refused("champ « " + std::string(field) + " » inconnu ou mal formé"));
    }
  } while (table.Take(','));
  if (!table.Take('}')) {
    return std::unexpected(table.Refused("un objet se ferme par « } »"));
  }
  return {};
}

// Le texte entier, dans la forme dite : chaque objet à `take`.
[[nodiscard]] core::Result<void> ReadText(
    std::string_view text, TextForm form,
    const std::function<core::Result<void>(ItemServerFactsRecord&&)>& take) {
  StrictText table(text, kFileLabel);
  std::string_view key;
  std::uint32_t version = 0;
  if (!table.ReadKey(key) || key != kVersionKey || !table.Take(':') ||
      !table.ReadWhole(std::numeric_limits<std::uint32_t>::max(), version)) {
    return std::unexpected(table.Refused("« version » attendue en tête"));
  }
  if (version != kItemInfoVersion) {
    return std::unexpected(table.Refused("version non gérée"));
  }
  if (!table.ReadKey(key) || key != kItemsKey || !table.Take(':') || !table.Take('{')) {
    return std::unexpected(table.Refused("« items: { » attendu"));
  }
  bool closed = table.Take('}');
  while (!closed) {
    ItemServerFactsRecord record;
    if (!table.ReadWhole(std::numeric_limits<std::uint32_t>::max(), record.sheet.item_id) ||
        !table.Take(':')) {
      return std::unexpected(table.Refused("identifiant d'objet attendu"));
    }
    if (auto read = ReadRecord(table, form, record); !read) {
      return read;
    }
    if (auto taken = take(std::move(record)); !taken) {
      return taken;
    }
    // Une virgule avant la fermeture est du YAML valide : elle se tolère.
    const bool more = table.Take(',');
    closed = table.Take('}');
    if (!more && !closed) {
      return std::unexpected(table.Refused("« , » ou « } » attendu après un objet"));
    }
  }
  if (!table.AtEnd()) {
    return std::unexpected(table.Refused("texte après la fermeture de « items »"));
  }
  return {};
}

}  // namespace

core::Result<void> ReadItemInfoText(
    std::string_view text, const std::function<core::Result<void>(ItemInfoRecord&&)>& take) {
  return ReadText(text, TextForm::kTable, [&take](ItemServerFactsRecord&& record) {
    return take(std::move(record.sheet));
  });
}

core::Result<void> ReadItemServerFactsText(
    std::string_view text, const std::function<core::Result<void>(ItemServerFactsRecord&&)>& take) {
  return ReadText(text, TextForm::kServerFacts, take);
}

std::string_view ItemInfoHeader(std::string_view text) {
  std::size_t at = 0;
  while (at < text.size()) {
    const std::size_t end = text.find('\n', at);
    const std::size_t next = end == std::string_view::npos ? text.size() : end + 1;
    const std::string_view line = text.substr(at, next - at);
    const std::size_t first = line.find_first_not_of(" \t\r\n");
    if (first != std::string_view::npos && line[first] != '#') {
      break;
    }
    at = next;
  }
  return text.substr(0, at);
}

namespace {

// Le dernier caractère de contrôle : en deçà, tout s'écrit en `\u00xx`.
constexpr unsigned char kLastControl = 0x1F;
constexpr std::string_view kHexDigitsLower = "0123456789abcdef";
constexpr unsigned kNibbleBits = 4;
constexpr unsigned kNibbleMask = 0xF;

// Un texte entre guillemets, comme `json.dumps(…, ensure_ascii=False)` l'écrit.
void AppendQuoted(std::string_view text, std::string& out) {
  out.push_back('"');
  for (const char c : text) {
    switch (c) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      case '\b':
        out += "\\b";
        break;
      case '\f':
        out += "\\f";
        break;
      default:
        if (static_cast<unsigned char>(c) <= kLastControl) {
          const auto code = static_cast<unsigned>(static_cast<unsigned char>(c));
          out += "\\u00";
          out.push_back(kHexDigitsLower[(code >> kNibbleBits) & kNibbleMask]);
          out.push_back(kHexDigitsLower[code & kNibbleMask]);
        } else {
          out.push_back(c);
        }
    }
  }
  out.push_back('"');
}

// Un objet, `{…}` compris, ses champs non nuls dans l'ordre du script.
void AppendRecord(const ItemInfoRecord& record, std::string& out) {
  bool first = true;
  const auto field = [&out, &first](std::string_view key) {
    out += first ? "" : ", ";
    first = false;
    out += key;
    out += ": ";
  };
  out.push_back('{');
  if (!record.name.empty()) {
    field(kNameField);
    AppendQuoted(record.name, out);
  }
  if (!record.resource.empty()) {
    field(kResourceField);
    AppendQuoted(record.resource, out);
  }
  if (record.slots != 0) {
    field(kSlotsField);
    out += std::to_string(record.slots);
  }
  if (record.view != 0) {
    field(kViewField);
    out += std::to_string(record.view);
  }
  if (!record.description.empty()) {
    field(kDescriptionField);
    out.push_back('[');
    for (std::size_t rank = 0; rank < record.description.size(); ++rank) {
      out += rank == 0 ? "" : ", ";
      AppendQuoted(record.description[rank], out);
    }
    out.push_back(']');
  }
  out.push_back('}');
}

}  // namespace

std::string WriteItemInfoText(std::string_view header, std::span<const ItemInfoRecord> records) {
  std::vector<const ItemInfoRecord*> ordered;
  ordered.reserve(records.size());
  for (const ItemInfoRecord& record : records) {
    ordered.push_back(&record);
  }
  std::ranges::stable_sort(ordered, {},
                           [](const ItemInfoRecord* record) { return record->item_id; });
  std::string out(header);
  out += kVersionKey;
  out += ": ";
  out += std::to_string(kItemInfoVersion);
  out += "\n";
  out += kItemsKey;
  out += ": {\n";
  for (std::size_t rank = 0; rank < ordered.size(); ++rank) {
    out += "  ";
    out += std::to_string(ordered[rank]->item_id);
    out += ": ";
    AppendRecord(*ordered[rank], out);
    out += rank + 1 == ordered.size() ? "\n" : ",\n";
  }
  out += "}\n";
  return out;
}

}  // namespace ro::vfs
