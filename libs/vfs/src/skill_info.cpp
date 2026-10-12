// Le texte de `skill_info.yml`. Voir l'en-tête.

#include "ro/vfs/skill_info.h"

#include <cstdint>
#include <expected>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "strict_text.h"

namespace ro::vfs {
namespace {

constexpr std::string_view kFileLabel = "skill_info.yml";
constexpr std::string_view kVersionKey = "version";
constexpr std::string_view kSkillsKey = "skills";
constexpr std::string_view kNameField = "name";
constexpr std::string_view kLabelField = "label";
constexpr std::string_view kSpField = "sp";
constexpr std::string_view kRangeField = "range";
constexpr std::string_view kScaleField = "scale";
constexpr std::string_view kSeparateField = "separate_levels";
constexpr std::string_view kDescriptionField = "description";
constexpr std::uint32_t kMostSixteenBits = std::numeric_limits<std::uint16_t>::max();
// Une zone s'écrit [x, y].
constexpr std::size_t kScaleSides = 2;

// Une suite `[…]` d'entiers de seize bits.
[[nodiscard]] bool ReadLevels(StrictText& text, std::vector<std::uint16_t>& levels) {
  std::vector<std::uint32_t> read;
  if (!text.ReadWholeList(kMostSixteenBits, read)) {
    return false;
  }
  levels.reserve(read.size());
  for (const std::uint32_t value : read) {
    levels.push_back(static_cast<std::uint16_t>(value));
  }
  return true;
}

// `[[x, y], …]`.
[[nodiscard]] bool ReadScales(StrictText& text, std::vector<SkillInfoScale>& scales) {
  if (!text.Take('[')) {
    return false;
  }
  if (text.Take(']')) {
    return true;
  }
  do {
    std::vector<std::uint32_t> sides;
    if (!text.ReadWholeList(kMostSixteenBits, sides) || sides.size() != kScaleSides) {
      return false;
    }
    scales.push_back(SkillInfoScale{.x = static_cast<std::uint16_t>(sides[0]),
                                    .y = static_cast<std::uint16_t>(sides[1])});
  } while (text.Take(','));
  return text.Take(']');
}

// Un champ, après son nom et ses deux-points.
[[nodiscard]] bool ReadField(StrictText& text, std::string_view field, SkillInfoRecord& record) {
  if (field == kNameField) {
    return text.ReadQuoted(record.name);
  }
  if (field == kLabelField) {
    return text.ReadQuoted(record.label);
  }
  if (field == kSpField) {
    return ReadLevels(text, record.sp_by_level);
  }
  if (field == kRangeField) {
    return ReadLevels(text, record.range_by_level);
  }
  if (field == kScaleField) {
    return ReadScales(text, record.scale_by_level);
  }
  if (field == kSeparateField) {
    return text.ReadBoolean(record.separate_levels);
  }
  if (field == kDescriptionField) {
    return text.ReadQuotedList(record.description);
  }
  return false;
}

// `{<champ>: <valeur>, …}`.
[[nodiscard]] core::Result<void> ReadRecord(StrictText& text, SkillInfoRecord& record) {
  if (!text.Take('{')) {
    return std::unexpected(text.Refused("une fiche s'ouvre par « { »"));
  }
  if (text.Take('}')) {
    return {};
  }
  do {
    std::string_view field;
    if (!text.ReadKey(field) || !text.Take(':')) {
      return std::unexpected(text.Refused("nom de champ attendu"));
    }
    if (!ReadField(text, field, record)) {
      return std::unexpected(
          text.Refused("champ « " + std::string(field) + " » inconnu ou mal formé"));
    }
  } while (text.Take(','));
  if (!text.Take('}')) {
    return std::unexpected(text.Refused("une fiche se ferme par « } »"));
  }
  return {};
}

}  // namespace

core::Result<std::vector<SkillInfoRecord>> ParseSkillInfo(std::string_view source) {
  StrictText text(source, kFileLabel);
  std::string_view key;
  std::uint32_t version = 0;
  if (!text.ReadKey(key) || key != kVersionKey || !text.Take(':') ||
      !text.ReadWhole(std::numeric_limits<std::uint32_t>::max(), version)) {
    return std::unexpected(text.Refused("« version » attendue en tête"));
  }
  if (version != kSkillInfoVersion) {
    return std::unexpected(text.Refused("version non gérée"));
  }
  if (!text.ReadKey(key) || key != kSkillsKey || !text.Take(':') || !text.Take('{')) {
    return std::unexpected(text.Refused("« skills: { » attendu"));
  }
  std::vector<SkillInfoRecord> records;
  std::vector<bool> seen(static_cast<std::size_t>(kMostSixteenBits) + 1, false);
  bool closed = text.Take('}');
  while (!closed) {
    SkillInfoRecord record;
    std::uint32_t skill_id = 0;
    if (!text.ReadWhole(kMostSixteenBits, skill_id) || skill_id == 0 || !text.Take(':')) {
      return std::unexpected(text.Refused("numéro de compétence attendu"));
    }
    if (seen[skill_id]) {
      return std::unexpected(text.Refused("compétence " + std::to_string(skill_id) + " répétée"));
    }
    seen[skill_id] = true;
    record.skill_id = static_cast<std::uint16_t>(skill_id);
    if (auto read = ReadRecord(text, record); !read) {
      return std::unexpected(read.error());
    }
    records.push_back(std::move(record));
    // Une virgule avant la fermeture est du YAML valide : elle se tolère.
    const bool more = text.Take(',');
    closed = text.Take('}');
    if (!more && !closed) {
      return std::unexpected(text.Refused("« , » ou « } » attendu après une fiche"));
    }
  }
  if (!text.AtEnd()) {
    return std::unexpected(text.Refused("texte après la fermeture de « skills »"));
  }
  return records;
}

core::Result<std::vector<SkillInfoRecord>> LoadSkillInfo(Vfs& vfs) {
  auto bytes = vfs.Read(std::string(kSkillInfoPath));
  if (!bytes) {
    return std::unexpected(bytes.error());
  }
  return ParseSkillInfo(
      std::string_view(reinterpret_cast<const char*>(bytes->data()), bytes->size()));
}

}  // namespace ro::vfs
