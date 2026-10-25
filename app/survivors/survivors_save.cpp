#include "survivors/survivors_save.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <format>
#include <iterator>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <yaml-cpp/yaml.h>

#include "ro/sim/survivors_week.h"

namespace moonclient {
namespace {

// LES CLÉS DU FICHIER : en changer une rendrait illisibles les carnets déjà
// écrits chez le joueur.
constexpr const char* kOptionsSection = "options";
constexpr const char* kReducedEffectsField = "reduced_effects";
constexpr const char* kShowMeasuresField = "show_measures";
constexpr const char* kHordeSunShadowsField = "horde_sun_shadows";
constexpr const char* kFirstSkillIdField = "first_skill_id";

constexpr const char* kMetaSection = "meta";
constexpr const char* kZenyField = "zeny";
constexpr const char* kZenyEarnedField = "zeny_earned";
constexpr const char* kChurchField = "church";
constexpr const char* kKafraField = "kafra";
constexpr const char* kSpentField = "spent";
constexpr const char* kRanksField = "ranks";
constexpr const char* kCursedWaterField = "cursed_water";

constexpr const char* kAlbumSection = "album";
constexpr const char* kCardsField = "cards";
constexpr const char* kFoundField = "found";
constexpr const char* kFirstFoundField = "first_found";
constexpr const char* kFeatsField = "feats";
constexpr const char* kCountersField = "counters";
constexpr const char* kChillsField = "chills";
constexpr const char* kWindKillsField = "wind_kills";
constexpr const char* kBestLevelField = "best_level";
constexpr const char* kBestSurvivedField = "best_survived_ms";
constexpr const char* kRunsCountField = "runs";

constexpr const char* kRecordsSection = "records";
constexpr const char* kClassField = "class";
constexpr const char* kArenaField = "arena";
constexpr const char* kBoardField = "board";
constexpr const char* kRunsField = "runs";
constexpr const char* kScoreField = "score";
constexpr const char* kSurvivedField = "survived_ms";
constexpr const char* kLevelField = "level";
constexpr const char* kOutcomeField = "outcome";
constexpr const char* kKillsField = "kills";
constexpr const char* kRulesetField = "ruleset";
constexpr const char* kFirstSkillField = "first_skill";
constexpr const char* kSeedField = "seed";
constexpr const char* kDateField = "date";
constexpr const char* kWeekField = "week";

constexpr const char* kPendingSection = "pending_results";

constexpr std::array<const char*, 5> kKnownSections{kOptionsSection, kMetaSection, kAlbumSection,
                                                    kRecordsSection, kPendingSection};

// Les issues d'une partie, par leur clé.
struct OutcomeKey {
  ro::sim::SurvivorsOutcome outcome;
  const char* key;
};
constexpr std::array<OutcomeKey, 5> kOutcomeKeys{{
    {ro::sim::SurvivorsOutcome::kRunning, "running"},
    {ro::sim::SurvivorsOutcome::kFell, "fell"},
    {ro::sim::SurvivorsOutcome::kSurvived, "survived"},
    {ro::sim::SurvivorsOutcome::kVictory, "victory"},
    {ro::sim::SurvivorsOutcome::kAbandoned, "abandoned"},
}};

constexpr const char* kFileHeader =
    "# Le carnet du mode Survivants de Midgard de moonclient.\n"
    "#\n"
    "# options : effets reduits, mesures affichees, ombres du soleil de la horde,\n"
    "# et l'identifiant de competence du dernier premier sort choisi.\n"
    "# meta : les Zeny du mode, les rangs de l'Eglise et de la Kafra.\n"
    "# album : les cartes trouvees, les exploits, les compteurs.\n"
    "# records : les dix meilleures parties de chaque tableau.\n"
    "# pending_results : les parties classees dont le serveur n'a pas encore\n"
    "# rendu le verdict.\n";

using ro::sim::kSecondsPerDay;
using ro::sim::kSecondsPerHour;
using ro::sim::kSecondsPerMinute;
constexpr std::int64_t kHoursPerDay = 24;
constexpr std::int64_t kMinutesPerHour = 60;
constexpr std::uint8_t kMostIsoWeek = 53;

// ── LIRE ────────────────────────────────────────────────────────────────────

// Un entier non signé, ou rien : champ absent, d'un mauvais type, négatif ou
// au-delà de `most`.
[[nodiscard]] std::optional<std::uint64_t> CountOf(const YAML::Node& value, std::uint64_t most) {
  if (!value || !value.IsScalar()) return std::nullopt;
  const std::string& text = value.Scalar();
  std::uint64_t read = 0;
  const auto [end, failure] = std::from_chars(text.data(), text.data() + text.size(), read);
  if (failure != std::errc() || end != text.data() + text.size() || read > most) {
    return std::nullopt;
  }
  return read;
}

template <typename Count>
[[nodiscard]] Count CountIn(const YAML::Node& section, const char* field, Count fallback) {
  return static_cast<Count>(
      CountOf(section[field], std::numeric_limits<Count>::max()).value_or(fallback));
}

// Un rang : au-delà du maximum, il se ramène au maximum.
[[nodiscard]] std::uint8_t RankIn(const YAML::Node& value, std::uint8_t most) {
  const std::optional<std::uint64_t> read =
      CountOf(value, std::numeric_limits<std::uint64_t>::max());
  if (!read.has_value()) return 0;
  return static_cast<std::uint8_t>(std::min<std::uint64_t>(*read, most));
}

[[nodiscard]] bool FlagIn(const YAML::Node& section, const char* field, bool fallback) {
  const YAML::Node value = section[field];
  if (!value || !value.IsScalar()) return fallback;
  return value.as<bool>(fallback);
}

[[nodiscard]] std::string TextIn(const YAML::Node& section, const char* field) {
  const YAML::Node value = section[field];
  if (!value || !value.IsScalar()) return {};
  return value.Scalar();
}

[[nodiscard]] YAML::Node MapIn(const YAML::Node& section, const char* field) {
  const YAML::Node value = section[field];
  return value && value.IsMap() ? value : YAML::Node();
}

// Les chiffres de `text[from, from + count)`, ou rien.
[[nodiscard]] std::optional<std::int64_t> DigitsAt(std::string_view text, std::size_t from,
                                                   std::size_t count) {
  if (from + count > text.size()) return std::nullopt;
  std::int64_t read = 0;
  for (std::size_t index = from; index < from + count; ++index) {
    if (text[index] < '0' || text[index] > '9') return std::nullopt;
  }
  const auto [end, failure] = std::from_chars(text.data() + from, text.data() + from + count, read);
  if (failure != std::errc()) return std::nullopt;
  return read;
}

// « AAAA-MM-JJ » : le jour depuis le 1970-01-01, ou rien.
constexpr std::size_t kDateLength = 10;
[[nodiscard]] std::optional<std::int64_t> DayOfText(std::string_view text) {
  constexpr std::size_t kMonthAt = 5;
  constexpr std::size_t kDayAt = 8;
  constexpr std::size_t kYearDigits = 4;
  constexpr std::size_t kTwoDigits = 2;
  if (text.size() < kDateLength || text[kMonthAt - 1] != '-' || text[kDayAt - 1] != '-') {
    return std::nullopt;
  }
  const std::optional<std::int64_t> year = DigitsAt(text, 0, kYearDigits);
  const std::optional<std::int64_t> month = DigitsAt(text, kMonthAt, kTwoDigits);
  const std::optional<std::int64_t> day = DigitsAt(text, kDayAt, kTwoDigits);
  if (!year || !month || !day) return std::nullopt;
  const ro::sim::CivilDate date{.year = static_cast<std::int32_t>(*year),
                                .month = static_cast<std::uint8_t>(*month),
                                .day = static_cast<std::uint8_t>(*day)};
  if (!ro::sim::CivilDateExists(date)) return std::nullopt;
  return ro::sim::DayOfCivilDate(date);
}

// « AAAA-MM-JJTHH:MM:SSZ » : les secondes depuis le 1970-01-01 UTC, ou rien.
[[nodiscard]] std::optional<std::int64_t> InstantOfText(std::string_view text) {
  constexpr std::size_t kInstantLength = 20;
  constexpr std::size_t kHourAt = 11;
  constexpr std::size_t kMinuteAt = 14;
  constexpr std::size_t kSecondAt = 17;
  constexpr std::size_t kTwoDigits = 2;
  if (text.size() != kInstantLength || text[kDateLength] != 'T' || text[kMinuteAt - 1] != ':' ||
      text[kSecondAt - 1] != ':' || text.back() != 'Z') {
    return std::nullopt;
  }
  const std::optional<std::int64_t> day = DayOfText(text.substr(0, kDateLength));
  const std::optional<std::int64_t> hour = DigitsAt(text, kHourAt, kTwoDigits);
  const std::optional<std::int64_t> minute = DigitsAt(text, kMinuteAt, kTwoDigits);
  const std::optional<std::int64_t> second = DigitsAt(text, kSecondAt, kTwoDigits);
  if (!day || !hour || !minute || !second || *hour >= kHoursPerDay || *minute >= kMinutesPerHour ||
      *second >= kSecondsPerMinute) {
    return std::nullopt;
  }
  return *day * kSecondsPerDay + *hour * kSecondsPerHour + *minute * kSecondsPerMinute + *second;
}

// « AAAA-Wss » : une semaine ISO, ou {0, 0}.
[[nodiscard]] ro::sim::IsoWeek WeekOfText(std::string_view text) {
  constexpr std::size_t kWeekLength = 8;
  constexpr std::size_t kYearDigits = 4;
  constexpr std::size_t kWeekAt = 6;
  constexpr std::size_t kTwoDigits = 2;
  if (text.size() != kWeekLength || text[kYearDigits] != '-' || text[kYearDigits + 1] != 'W') {
    return {};
  }
  const std::optional<std::int64_t> year = DigitsAt(text, 0, kYearDigits);
  const std::optional<std::int64_t> week = DigitsAt(text, kWeekAt, kTwoDigits);
  if (!year || !week || *week < 1 || *week > kMostIsoWeek) return {};
  return {.year = static_cast<std::int32_t>(*year), .week = static_cast<std::uint8_t>(*week)};
}

[[nodiscard]] std::string YamlText(const YAML::Node& node) {
  YAML::Emitter out;
  out << node;
  return out.c_str();
}

// L'identifiant de skill des réglages suit la conversion entière de yaml-cpp,
// qui accepte une saisie à la main en hexadécimal (`0x0E`) ou signée (`+14`),
// et non `from_chars` comme les autres comptes du carnet. 0 hors de 0..65535
// ou d'un mauvais type.
[[nodiscard]] std::uint16_t SkillIdIn(const YAML::Node& section, const char* field) {
  const YAML::Node value = section[field];
  if (!value || !value.IsScalar()) return 0;
  const long long read = value.as<long long>(0);
  if (read < 0 || read > std::numeric_limits<std::uint16_t>::max()) return 0;
  return static_cast<std::uint16_t>(read);
}

// Une valeur que ce client ne sait pas lire, gardée par sa clé.
[[nodiscard]] SurvivorsForeignEntry ForeignOf(const YAML::Node& key, const YAML::Node& value) {
  return SurvivorsForeignEntry{.key = key.Scalar(), .yaml = YamlText(value)};
}

void ReadOptions(const YAML::Node& section, SurvivorsOptions& options) {
  options.reduced_effects = FlagIn(section, kReducedEffectsField, options.reduced_effects);
  options.show_measures = FlagIn(section, kShowMeasuresField, options.show_measures);
  options.horde_sun_shadows = FlagIn(section, kHordeSunShadowsField, options.horde_sun_shadows);
  options.first_skill_id = SkillIdIn(section, kFirstSkillIdField);
}

// Où se range une clé du catalogue : sa place dans les rangs, et son rang
// maximal.
struct RankSlot {
  std::size_t index = 0;
  std::uint8_t most = 0;
};

[[nodiscard]] std::optional<RankSlot> BlessingSlot(const std::string& key) {
  const ro::sim::SurvivorsMeta& meta = ro::sim::ShippedSurvivorsMeta();
  for (const ro::sim::BlessingRule& rule : meta.blessings) {
    if (key == rule.key) {
      return RankSlot{.index = static_cast<std::size_t>(rule.blessing),
                      .most = ro::sim::MostRank(meta, rule.blessing)};
    }
  }
  return std::nullopt;
}

[[nodiscard]] std::optional<RankSlot> KafraSlot(const std::string& key) {
  const ro::sim::SurvivorsMeta& meta = ro::sim::ShippedSurvivorsMeta();
  for (const ro::sim::KafraRule& rule : meta.kafra) {
    if (key == rule.key) {
      return RankSlot{.index = static_cast<std::size_t>(rule.service),
                      .most = ro::sim::MostRank(meta, rule.service)};
    }
  }
  return std::nullopt;
}

// Les rangs d'un lieu, rangés par clé : une clé connue prend sa place dans
// `ranks`, une inconnue se garde dans `foreign`.
template <typename SlotOf>
void ReadRanks(const YAML::Node& place, std::span<std::uint8_t> ranks, std::uint64_t& spent,
               std::vector<SurvivorsForeignEntry>& foreign, SlotOf slot_of) {
  spent = CountIn<std::uint64_t>(place, kSpentField, 0);
  const YAML::Node listed = MapIn(place, kRanksField);
  for (const auto& entry : listed) {
    if (!entry.first.IsScalar()) continue;
    const std::optional<RankSlot> slot = slot_of(entry.first.Scalar());
    if (!slot.has_value() || slot->index >= ranks.size()) {
      foreign.push_back(ForeignOf(entry.first, entry.second));
      continue;
    }
    ranks[slot->index] = RankIn(entry.second, slot->most);
  }
}

void ReadMeta(const YAML::Node& section, SurvivorsSaveMeta& meta) {
  meta.zeny = CountIn<std::uint64_t>(section, kZenyField, 0);
  meta.zeny_earned = CountIn<std::uint64_t>(section, kZenyEarnedField, 0);
  ReadRanks(MapIn(section, kChurchField), meta.ranks.church, meta.ranks.church_spent,
            meta.foreign_church_ranks, BlessingSlot);
  ReadRanks(MapIn(section, kKafraField), meta.ranks.kafra, meta.ranks.kafra_spent,
            meta.foreign_kafra_ranks, KafraSlot);
  meta.cursed_water = RankIn(section[kCursedWaterField], ro::sim::kMostCursedWater);
}

void ReadAlbum(const YAML::Node& section, SurvivorsAlbum& album) {
  for (const auto& entry : MapIn(section, kCardsField)) {
    if (!entry.first.IsScalar()) continue;
    if (!entry.second.IsMap() && !entry.second.IsNull()) {
      album.foreign_cards.push_back(ForeignOf(entry.first, entry.second));
      continue;
    }
    SurvivorsAlbumCard card{.key = entry.first.Scalar()};
    if (entry.second.IsMap()) {
      card.found = CountIn<std::uint32_t>(entry.second, kFoundField, 0);
      card.first_found_day = DayOfText(TextIn(entry.second, kFirstFoundField)).value_or(0);
    }
    album.cards.push_back(std::move(card));
  }
  // Un exploit vaut `true` quand il est atteint ; `false` ne se garde pas.
  for (const auto& entry : MapIn(section, kFeatsField)) {
    if (!entry.first.IsScalar()) continue;
    bool reached = false;
    if (!entry.second.IsScalar() || !YAML::convert<bool>::decode(entry.second, reached)) {
      album.foreign_feats.push_back(ForeignOf(entry.first, entry.second));
      continue;
    }
    if (reached) album.feats.push_back(entry.first.Scalar());
  }
  const YAML::Node counters = MapIn(section, kCountersField);
  album.counters.chills = CountIn<std::uint64_t>(counters, kChillsField, 0);
  album.counters.wind_kills = CountIn<std::uint64_t>(counters, kWindKillsField, 0);
  album.counters.best_level = CountIn<std::uint16_t>(counters, kBestLevelField, 0);
  album.counters.best_survived_ms = CountIn<std::uint32_t>(counters, kBestSurvivedField, 0);
  album.counters.runs = CountIn<std::uint32_t>(counters, kRunsCountField, 0);
}

[[nodiscard]] ro::sim::SurvivorsOutcome OutcomeOfKey(const std::string& key) {
  for (const OutcomeKey& known : kOutcomeKeys) {
    if (key == known.key) return known.outcome;
  }
  return ro::sim::SurvivorsOutcome::kRunning;
}

[[nodiscard]] const char* KeyOfOutcome(ro::sim::SurvivorsOutcome outcome) {
  for (const OutcomeKey& known : kOutcomeKeys) {
    if (outcome == known.outcome) return known.key;
  }
  return kOutcomeKeys.front().key;
}

[[nodiscard]] ro::sim::SurvivorsRecord ReadRun(const YAML::Node& run) {
  constexpr std::uint16_t kFirstLevel = 1;
  ro::sim::SurvivorsRecord record;
  record.score = CountIn<std::uint64_t>(run, kScoreField, 0);
  record.survived_ms = CountIn<std::uint32_t>(run, kSurvivedField, 0);
  record.level = CountIn<std::uint16_t>(run, kLevelField, kFirstLevel);
  record.outcome = OutcomeOfKey(TextIn(run, kOutcomeField));
  record.kills = CountIn<std::uint32_t>(run, kKillsField, 0);
  record.cursed_water = RankIn(run[kCursedWaterField], ro::sim::kMostCursedWater);
  record.ruleset = CountIn<std::uint16_t>(run, kRulesetField, 0);
  record.first_skill_key = TextIn(run, kFirstSkillField);
  record.seed = CountIn<std::uint64_t>(run, kSeedField, 0);
  record.played_at_utc_s = InstantOfText(TextIn(run, kDateField)).value_or(0);
  record.week = WeekOfText(TextIn(run, kWeekField));
  return record;
}

// Chaque partie passe par PlaceRecord : un tableau corrigé à la main se
// relit rangé et ramené à dix lignes, et deux tableaux de mêmes clés se
// fondent.
void ReadRecords(const YAML::Node& section, std::vector<SurvivorsBoard>& boards) {
  if (!section || !section.IsSequence()) return;
  for (const YAML::Node& listed : section) {
    if (!listed.IsMap()) continue;
    SurvivorsBoard read{.class_key = TextIn(listed, kClassField),
                        .arena_key = TextIn(listed, kArenaField),
                        .board_key = TextIn(listed, kBoardField),
                        .week = WeekOfText(TextIn(listed, kWeekField))};
    auto board = std::ranges::find_if(boards, [&read](const SurvivorsBoard& kept) {
      return kept.class_key == read.class_key && kept.arena_key == read.arena_key &&
             kept.board_key == read.board_key && kept.week == read.week;
    });
    if (board == boards.end()) {
      boards.push_back(std::move(read));
      board = std::prev(boards.end());
    }
    const YAML::Node runs = listed[kRunsField];
    if (!runs || !runs.IsSequence()) continue;
    for (const YAML::Node& run : runs) {
      if (run.IsMap()) ro::sim::PlaceRecord(board->runs, ReadRun(run));
    }
  }
}

// ── ÉCRIRE ──────────────────────────────────────────────────────────────────

[[nodiscard]] std::string CivilDateText(const ro::sim::CivilDate& date) {
  return std::format("{:04}-{:02}-{:02}", date.year, unsigned{date.month}, unsigned{date.day});
}

[[nodiscard]] std::string DateText(std::int64_t day) {
  return CivilDateText(ro::sim::CivilDateOfDay(day));
}

[[nodiscard]] std::string InstantText(std::int64_t utc_s) {
  const ro::sim::UtcInstant instant = ro::sim::UtcInstantOf(utc_s);
  return std::format("{}T{:02}:{:02}:{:02}Z", CivilDateText(instant.date), unsigned{instant.hour},
                     unsigned{instant.minute}, unsigned{instant.second});
}

// Un texte YAML gardé tel quel ; rien s'il ne se relit plus.
[[nodiscard]] std::optional<YAML::Node> KeptNode(const std::string& yaml) {
  try {
    return YAML::Load(yaml);
  } catch (const YAML::Exception&) {
    return std::nullopt;
  }
}

// Une entrée gardée, réécrite sous sa clé ; rien si son texte ne se relit plus.
void WriteKept(YAML::Emitter& out, const SurvivorsForeignEntry& kept) {
  if (const std::optional<YAML::Node> node = KeptNode(kept.yaml)) {
    out << YAML::Key << kept.key << YAML::Value << *node;
  }
}

// Les entrées gardées dont ce client n'a pas rangé la clé depuis.
template <typename Has>
void WriteKeptUnless(YAML::Emitter& out, const std::vector<SurvivorsForeignEntry>& kept, Has has) {
  for (const SurvivorsForeignEntry& entry : kept) {
    if (!has(entry.key)) WriteKept(out, entry);
  }
}

[[nodiscard]] std::string WeekText(const ro::sim::IsoWeek& week) {
  return std::format("{:04}-W{:02}", week.year, unsigned{week.week});
}

// Un rang ou un compte d'un octet : l'émetteur écrirait un `uint8_t` comme un
// caractère.
[[nodiscard]] unsigned Number(std::uint8_t value) { return value; }

void WriteOptions(YAML::Emitter& out, const SurvivorsOptions& options) {
  out << YAML::Key << kOptionsSection << YAML::Value << YAML::BeginMap;
  out << YAML::Key << kReducedEffectsField << YAML::Value << options.reduced_effects;
  out << YAML::Key << kShowMeasuresField << YAML::Value << options.show_measures;
  out << YAML::Key << kHordeSunShadowsField << YAML::Value << options.horde_sun_shadows;
  out << YAML::Key << kFirstSkillIdField << YAML::Value << options.first_skill_id;
  out << YAML::EndMap;
}

// Un lieu : ses dépenses, puis chaque rang du catalogue, zéros compris, et
// les clés inconnues à la suite.
template <typename Rule, typename RankOf>
void WritePlace(YAML::Emitter& out, const char* field, std::uint64_t spent,
                std::span<const Rule> catalog, RankOf rank_of,
                const std::vector<SurvivorsForeignEntry>& foreign) {
  out << YAML::Key << field << YAML::Value << YAML::BeginMap;
  out << YAML::Key << kSpentField << YAML::Value << spent;
  out << YAML::Key << kRanksField << YAML::Value << YAML::BeginMap;
  for (const Rule& rule : catalog) {
    out << YAML::Key << rule.key << YAML::Value << Number(rank_of(rule));
  }
  for (const SurvivorsForeignEntry& kept : foreign) WriteKept(out, kept);
  out << YAML::EndMap << YAML::EndMap;
}

void WriteMeta(YAML::Emitter& out, const SurvivorsSaveMeta& meta) {
  const ro::sim::SurvivorsMeta& catalog = ro::sim::ShippedSurvivorsMeta();
  out << YAML::Key << kMetaSection << YAML::Value << YAML::BeginMap;
  out << YAML::Key << kZenyField << YAML::Value << meta.zeny;
  out << YAML::Key << kZenyEarnedField << YAML::Value << meta.zeny_earned;
  WritePlace(
      out, kChurchField, meta.ranks.church_spent, catalog.blessings,
      [&meta](const ro::sim::BlessingRule& rule) { return meta.ranks.Of(rule.blessing); },
      meta.foreign_church_ranks);
  WritePlace(
      out, kKafraField, meta.ranks.kafra_spent, catalog.kafra,
      [&meta](const ro::sim::KafraRule& rule) { return meta.ranks.Of(rule.service); },
      meta.foreign_kafra_ranks);
  out << YAML::Key << kCursedWaterField << YAML::Value << Number(meta.cursed_water);
  out << YAML::EndMap;
}

void WriteAlbum(YAML::Emitter& out, const SurvivorsAlbum& album) {
  out << YAML::Key << kAlbumSection << YAML::Value << YAML::BeginMap;
  out << YAML::Key << kCardsField << YAML::Value << YAML::BeginMap;
  for (const SurvivorsAlbumCard& card : album.cards) {
    out << YAML::Key << card.key << YAML::Value << YAML::Flow << YAML::BeginMap;
    out << YAML::Key << kFoundField << YAML::Value << card.found;
    if (card.found > 0) {
      out << YAML::Key << kFirstFoundField << YAML::Value << DateText(card.first_found_day);
    }
    out << YAML::EndMap;
  }
  WriteKeptUnless(out, album.foreign_cards, [&album](const std::string& key) {
    return std::ranges::any_of(album.cards,
                               [&key](const SurvivorsAlbumCard& card) { return card.key == key; });
  });
  out << YAML::EndMap;
  out << YAML::Key << kFeatsField << YAML::Value << YAML::BeginMap;
  for (const std::string& feat : album.feats) {
    out << YAML::Key << feat << YAML::Value << true;
  }
  WriteKeptUnless(out, album.foreign_feats, [&album](const std::string& key) {
    return std::ranges::find(album.feats, key) != album.feats.end();
  });
  out << YAML::EndMap;
  const SurvivorsAlbumCounters& counters = album.counters;
  out << YAML::Key << kCountersField << YAML::Value << YAML::BeginMap;
  out << YAML::Key << kChillsField << YAML::Value << counters.chills;
  out << YAML::Key << kWindKillsField << YAML::Value << counters.wind_kills;
  out << YAML::Key << kBestLevelField << YAML::Value << counters.best_level;
  out << YAML::Key << kBestSurvivedField << YAML::Value << counters.best_survived_ms;
  out << YAML::Key << kRunsCountField << YAML::Value << counters.runs;
  out << YAML::EndMap << YAML::EndMap;
}

void WriteRun(YAML::Emitter& out, const ro::sim::SurvivorsRecord& record) {
  out << YAML::Flow << YAML::BeginMap;
  out << YAML::Key << kScoreField << YAML::Value << record.score;
  out << YAML::Key << kSurvivedField << YAML::Value << record.survived_ms;
  out << YAML::Key << kLevelField << YAML::Value << record.level;
  out << YAML::Key << kOutcomeField << YAML::Value << KeyOfOutcome(record.outcome);
  out << YAML::Key << kKillsField << YAML::Value << record.kills;
  out << YAML::Key << kCursedWaterField << YAML::Value << Number(record.cursed_water);
  out << YAML::Key << kRulesetField << YAML::Value << record.ruleset;
  out << YAML::Key << kFirstSkillField << YAML::Value << record.first_skill_key;
  out << YAML::Key << kSeedField << YAML::Value << record.seed;
  out << YAML::Key << kDateField << YAML::Value << InstantText(record.played_at_utc_s);
  if (record.week.week != 0) {
    out << YAML::Key << kWeekField << YAML::Value << WeekText(record.week);
  }
  out << YAML::EndMap;
}

void WriteRecords(YAML::Emitter& out, const std::vector<SurvivorsBoard>& boards) {
  out << YAML::Key << kRecordsSection << YAML::Value << YAML::BeginSeq;
  for (const SurvivorsBoard& board : boards) {
    out << YAML::BeginMap;
    out << YAML::Key << kClassField << YAML::Value << board.class_key;
    out << YAML::Key << kArenaField << YAML::Value << board.arena_key;
    out << YAML::Key << kBoardField << YAML::Value << board.board_key;
    if (board.week.week != 0) {
      out << YAML::Key << kWeekField << YAML::Value << WeekText(board.week);
    }
    out << YAML::Key << kRunsField << YAML::Value << YAML::BeginSeq;
    for (const ro::sim::SurvivorsRecord& record : board.runs) WriteRun(out, record);
    out << YAML::EndSeq << YAML::EndMap;
  }
  out << YAML::EndSeq;
}

// Les résultats en attente : ceux de ce client, puis les étrangers.
struct PendingPart {
  const std::vector<SurvivorsPendingResult>& typed;
  const std::vector<std::string>& foreign;
};

[[nodiscard]] bool IsBlank(const PendingPart& pending) {
  return pending.typed.empty() && pending.foreign.empty();
}

void WritePending(YAML::Emitter& out, const PendingPart& pending) {
  out << YAML::Key << kPendingSection << YAML::Value;
  if (IsBlank(pending)) {
    out << YAML::Flow << YAML::BeginSeq << YAML::EndSeq;
    return;
  }
  out << YAML::BeginSeq;
  for (const SurvivorsPendingResult& entry : pending.typed) {
    if (const std::optional<YAML::Node> kept = KeptNode(WriteSurvivorsPendingResult(entry))) {
      out << *kept;
    }
  }
  for (const std::string& entry : pending.foreign) {
    if (const std::optional<YAML::Node> kept = KeptNode(entry)) out << *kept;
  }
  out << YAML::EndSeq;
}

[[nodiscard]] bool IsKnownSection(const std::string& key) {
  return std::ranges::find(kKnownSections, key) != kKnownSections.end();
}

// La forme qu'attend ce client : une table, ou une liste pour `records` et
// `pending_results`. Une section vide (`records:`) compte comme absente.
[[nodiscard]] bool HasKnownShape(const std::string& key, const YAML::Node& value) {
  if (value.IsNull()) return true;
  const bool listed = key == kRecordsSection || key == kPendingSection;
  return listed ? value.IsSequence() : value.IsMap();
}

// UNE SECTION CONNUE : celle de ce client, sauf quand elle est restée à son
// défaut et que le fichier en portait une d'une autre forme, réécrite alors
// telle quelle.
template <typename Part, typename WritePart>
void WriteKnown(YAML::Emitter& out, const char* section, const Part& part,
                const std::vector<SurvivorsForeignEntry>& foreign, WritePart write_part) {
  if (part == Part{}) {
    const auto kept = std::ranges::find_if(
        foreign, [section](const SurvivorsForeignEntry& entry) { return entry.key == section; });
    if (kept != foreign.end() && KeptNode(kept->yaml).has_value()) {
      WriteKept(out, *kept);
      return;
    }
  }
  write_part(out, part);
}

}  // namespace

ro::core::Result<SurvivorsSave> ReadSurvivorsSave(std::string_view text) {
  SurvivorsSave save;
  try {
    const YAML::Node whole = YAML::Load(std::string(text));
    if (whole.IsNull()) return save;
    if (!whole.IsMap()) {
      return ro::core::Fail(ro::core::ErrorCode::kCorruptData, "la racine n'est pas une table");
    }
    ReadOptions(MapIn(whole, kOptionsSection), save.options);
    ReadMeta(MapIn(whole, kMetaSection), save.meta);
    ReadAlbum(MapIn(whole, kAlbumSection), save.album);
    ReadRecords(whole[kRecordsSection], save.records);
    if (const YAML::Node pending = whole[kPendingSection]; pending && pending.IsSequence()) {
      for (const YAML::Node& entry : pending) {
        std::string entry_text = YamlText(entry);
        if (std::optional<SurvivorsPendingResult> typed = ReadSurvivorsPendingResult(entry_text)) {
          save.pending_results.push_back(std::move(*typed));
        } else {
          save.foreign_pending.push_back(std::move(entry_text));
        }
      }
    }
    for (const auto& entry : whole) {
      if (!entry.first.IsScalar()) continue;
      const std::string& key = entry.first.Scalar();
      if (IsKnownSection(key) && HasKnownShape(key, entry.second)) continue;
      save.foreign_sections.push_back(ForeignOf(entry.first, entry.second));
    }
  } catch (const YAML::Exception& broken) {
    return ro::core::Fail(ro::core::ErrorCode::kCorruptData, broken.what());
  }
  return save;
}

std::string WriteSurvivorsSave(const SurvivorsSave& save) {
  YAML::Emitter out;
  out << YAML::BeginMap;
  const std::vector<SurvivorsForeignEntry>& foreign = save.foreign_sections;
  WriteKnown(out, kOptionsSection, save.options, foreign, WriteOptions);
  WriteKnown(out, kMetaSection, save.meta, foreign, WriteMeta);
  WriteKnown(out, kAlbumSection, save.album, foreign, WriteAlbum);
  WriteKnown(out, kRecordsSection, save.records, foreign, WriteRecords);
  const PendingPart pending{.typed = save.pending_results, .foreign = save.foreign_pending};
  if (IsBlank(pending)) {
    WriteKnown(out, kPendingSection, save.pending_results, foreign,
               [&pending](YAML::Emitter& into, const std::vector<SurvivorsPendingResult>&) {
                 WritePending(into, pending);
               });
  } else {
    WritePending(out, pending);
  }
  WriteKeptUnless(out, foreign, IsKnownSection);
  out << YAML::EndMap;
  return std::string(kFileHeader) + out.c_str() + "\n";
}

std::string SurvivorsInstantText(std::int64_t utc_s) { return InstantText(utc_s); }

std::optional<std::int64_t> SurvivorsInstantOfText(std::string_view text) {
  return InstantOfText(text);
}

}  // namespace moonclient
