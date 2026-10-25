#include "survivors/survivors_pending.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <yaml-cpp/yaml.h>

#include "survivors/survivors_save.h"

namespace moonclient {
namespace {

// LES CLÉS D'UNE ENTRÉE : en changer une rendrait illisibles les entrées déjà
// écrites chez le joueur.
constexpr const char* kServerField = "server";
constexpr const char* kCharacterIdField = "character_id";
constexpr const char* kCharacterField = "character";
constexpr const char* kTicketField = "ticket";
constexpr const char* kIssuedAtField = "issued_at";
constexpr const char* kRulesetField = "ruleset";
constexpr const char* kArenaField = "arena";
constexpr const char* kClassField = "class";
constexpr const char* kBoardField = "board";
constexpr const char* kSeedField = "seed";
constexpr const char* kSurvivedStepsField = "survived_steps";
constexpr const char* kKillPointsField = "kill_points";
constexpr const char* kKillsField = "kills";
constexpr const char* kBossKillsField = "boss_kills";
constexpr const char* kLevelField = "level";
constexpr const char* kOutcomeField = "outcome";
constexpr const char* kJournalBytesField = "journal_bytes";
constexpr const char* kScoreField = "score";
constexpr const char* kEndedAtField = "ended_at";
constexpr const char* kSendsField = "sends";
constexpr const char* kLastSentField = "last_sent";

// Les issues qu'`END` porte, par leur clé : celles des records.
struct SendableOutcome {
  ro::sim::SurvivorsOutcome outcome;
  const char* key;
};
constexpr std::array<SendableOutcome, 3> kSendableOutcomes{{
    {ro::sim::SurvivorsOutcome::kFell, "fell"},
    {ro::sim::SurvivorsOutcome::kSurvived, "survived"},
    {ro::sim::SurvivorsOutcome::kVictory, "victory"},
}};

[[nodiscard]] std::optional<ro::sim::SurvivorsOutcome> OutcomeOfKey(std::string_view key) {
  for (const SendableOutcome& known : kSendableOutcomes) {
    if (key == known.key) return known.outcome;
  }
  return std::nullopt;
}

[[nodiscard]] std::optional<const char*> KeyOfOutcome(ro::sim::SurvivorsOutcome outcome) {
  for (const SendableOutcome& known : kSendableOutcomes) {
    if (outcome == known.outcome) return known.key;
  }
  return std::nullopt;
}

// Un entier non signé qui tient dans `Count`, ou rien : champ absent, d'un
// mauvais type, négatif ou trop grand.
template <typename Count>
[[nodiscard]] std::optional<Count> CountIn(const YAML::Node& entry, const char* field) {
  const YAML::Node value = entry[field];
  if (!value || !value.IsScalar()) return std::nullopt;
  const std::string& text = value.Scalar();
  std::uint64_t read = 0;
  const auto [end, failure] = std::from_chars(text.data(), text.data() + text.size(), read);
  if (failure != std::errc() || end != text.data() + text.size() ||
      read > std::numeric_limits<Count>::max()) {
    return std::nullopt;
  }
  return static_cast<Count>(read);
}

[[nodiscard]] std::optional<std::string> TextIn(const YAML::Node& entry, const char* field) {
  const YAML::Node value = entry[field];
  if (!value || !value.IsScalar()) return std::nullopt;
  return value.Scalar();
}

[[nodiscard]] std::optional<std::int64_t> InstantIn(const YAML::Node& entry, const char* field) {
  const std::optional<std::string> text = TextIn(entry, field);
  if (!text.has_value()) return std::nullopt;
  return SurvivorsInstantOfText(*text);
}

// Les composantes, toutes obligées.
[[nodiscard]] std::optional<ro::sim::SurvivorsEndComponents> ComponentsIn(const YAML::Node& entry) {
  const std::optional<std::uint32_t> survived_steps =
      CountIn<std::uint32_t>(entry, kSurvivedStepsField);
  const std::optional<std::uint32_t> kill_points = CountIn<std::uint32_t>(entry, kKillPointsField);
  const std::optional<std::uint32_t> kills = CountIn<std::uint32_t>(entry, kKillsField);
  const std::optional<std::uint16_t> boss_kills = CountIn<std::uint16_t>(entry, kBossKillsField);
  const std::optional<std::uint16_t> level = CountIn<std::uint16_t>(entry, kLevelField);
  const std::optional<std::string> outcome_key = TextIn(entry, kOutcomeField);
  const std::optional<std::uint32_t> journal_bytes =
      CountIn<std::uint32_t>(entry, kJournalBytesField);
  if (!survived_steps || !kill_points || !kills || !boss_kills || !level || !outcome_key ||
      !journal_bytes) {
    return std::nullopt;
  }
  const std::optional<ro::sim::SurvivorsOutcome> outcome = OutcomeOfKey(*outcome_key);
  if (!outcome.has_value()) return std::nullopt;
  return ro::sim::SurvivorsEndComponents{.survived_steps = *survived_steps,
                                         .kill_points = *kill_points,
                                         .kills = *kills,
                                         .boss_kills = *boss_kills,
                                         .level = *level,
                                         .outcome = *outcome,
                                         .journal_bytes = *journal_bytes};
}

[[nodiscard]] std::optional<SurvivorsPendingResult> PendingOfNode(const YAML::Node& entry) {
  if (!entry.IsMap()) return std::nullopt;
  const std::optional<std::string> server = TextIn(entry, kServerField);
  const std::optional<std::uint32_t> character_id =
      CountIn<std::uint32_t>(entry, kCharacterIdField);
  const std::optional<std::uint32_t> ticket = CountIn<std::uint32_t>(entry, kTicketField);
  const std::optional<std::int64_t> issued_at = InstantIn(entry, kIssuedAtField);
  const std::optional<std::uint16_t> ruleset = CountIn<std::uint16_t>(entry, kRulesetField);
  const std::optional<std::string> arena = TextIn(entry, kArenaField);
  const std::optional<std::string> class_key = TextIn(entry, kClassField);
  const std::optional<std::string> board = TextIn(entry, kBoardField);
  const std::optional<std::uint64_t> seed = CountIn<std::uint64_t>(entry, kSeedField);
  const std::optional<ro::sim::SurvivorsEndComponents> components = ComponentsIn(entry);
  if (!server || !character_id || !ticket || !issued_at || !ruleset || !arena || !class_key ||
      !board || !seed || !components) {
    return std::nullopt;
  }
  SurvivorsPendingResult read{.server_key = *server,
                              .character_id = *character_id,
                              .character_name = TextIn(entry, kCharacterField).value_or(""),
                              .ticket = *ticket,
                              .issued_at_utc_s = *issued_at,
                              .ruleset = *ruleset,
                              .arena_key = *arena,
                              .class_key = *class_key,
                              .board_key = *board,
                              .seed = *seed,
                              .components = *components,
                              .local_score = CountIn<std::uint64_t>(entry, kScoreField).value_or(0),
                              .ended_at_utc_s = InstantIn(entry, kEndedAtField).value_or(0),
                              .send_count = CountIn<std::uint32_t>(entry, kSendsField).value_or(0),
                              .last_sent_utc_s = InstantIn(entry, kLastSentField).value_or(0)};
  if (!IsSendablePendingResult(read)) return std::nullopt;
  return read;
}

}  // namespace

bool IsSendablePendingResult(const SurvivorsPendingResult& entry) {
  const bool known_board =
      entry.board_key == kRankedBoardKey || entry.board_key == kChallengeBoardKey;
  return !entry.server_key.empty() && entry.character_id != 0 && entry.seed != 0 && known_board &&
         !entry.arena_key.empty() && !entry.class_key.empty() &&
         KeyOfOutcome(entry.components.outcome).has_value();
}

bool IsPendingResultOf(const SurvivorsPendingResult& entry, std::string_view server_key,
                       std::uint32_t character_id, std::uint32_t ticket) {
  return entry.server_key == server_key && entry.character_id == character_id &&
         entry.ticket == ticket;
}

std::optional<SurvivorsPendingResult> ReadSurvivorsPendingResult(std::string_view yaml) {
  try {
    return PendingOfNode(YAML::Load(std::string(yaml)));
  } catch (const YAML::Exception&) {
    return std::nullopt;
  }
}

std::string WriteSurvivorsPendingResult(const SurvivorsPendingResult& entry) {
  const ro::sim::SurvivorsEndComponents& parts = entry.components;
  YAML::Emitter out;
  out << YAML::Flow << YAML::BeginMap;
  out << YAML::Key << kServerField << YAML::Value << entry.server_key;
  out << YAML::Key << kCharacterIdField << YAML::Value << entry.character_id;
  out << YAML::Key << kCharacterField << YAML::Value << entry.character_name;
  out << YAML::Key << kTicketField << YAML::Value << entry.ticket;
  out << YAML::Key << kIssuedAtField << YAML::Value << SurvivorsInstantText(entry.issued_at_utc_s);
  out << YAML::Key << kRulesetField << YAML::Value << entry.ruleset;
  out << YAML::Key << kArenaField << YAML::Value << entry.arena_key;
  out << YAML::Key << kClassField << YAML::Value << entry.class_key;
  out << YAML::Key << kBoardField << YAML::Value << entry.board_key;
  out << YAML::Key << kSeedField << YAML::Value << entry.seed;
  out << YAML::Key << kSurvivedStepsField << YAML::Value << parts.survived_steps;
  out << YAML::Key << kKillPointsField << YAML::Value << parts.kill_points;
  out << YAML::Key << kKillsField << YAML::Value << parts.kills;
  out << YAML::Key << kBossKillsField << YAML::Value << parts.boss_kills;
  out << YAML::Key << kLevelField << YAML::Value << parts.level;
  // Une issue qu'`END` ne porte pas s'écrit sans clé : l'entrée se relira
  // étrangère, jamais envoyée.
  out << YAML::Key << kOutcomeField << YAML::Value << KeyOfOutcome(parts.outcome).value_or("");
  out << YAML::Key << kJournalBytesField << YAML::Value << parts.journal_bytes;
  out << YAML::Key << kScoreField << YAML::Value << entry.local_score;
  out << YAML::Key << kEndedAtField << YAML::Value << SurvivorsInstantText(entry.ended_at_utc_s);
  out << YAML::Key << kSendsField << YAML::Value << entry.send_count;
  if (entry.last_sent_utc_s != 0) {
    out << YAML::Key << kLastSentField << YAML::Value
        << SurvivorsInstantText(entry.last_sent_utc_s);
  }
  out << YAML::EndMap;
  return out.c_str();
}

SurvivorsSaveDelta AddPendingResult(SurvivorsPendingResult entry) {
  return [entry = std::move(entry)](SurvivorsSave& save) {
    if (!IsSendablePendingResult(entry)) return false;
    const auto kept =
        std::ranges::find_if(save.pending_results, [&entry](const SurvivorsPendingResult& listed) {
          return IsPendingResultOf(listed, entry.server_key, entry.character_id, entry.ticket);
        });
    if (kept == save.pending_results.end()) {
      save.pending_results.push_back(entry);
      return true;
    }
    if (*kept == entry) return false;
    *kept = entry;
    return true;
  };
}

SurvivorsSaveDelta RemovePendingResult(std::string server_key, std::uint32_t character_id,
                                       std::uint32_t ticket) {
  return [server_key = std::move(server_key), character_id, ticket](SurvivorsSave& save) {
    return std::erase_if(save.pending_results, [&](const SurvivorsPendingResult& listed) {
             return IsPendingResultOf(listed, server_key, character_id, ticket);
           }) > 0;
  };
}

}  // namespace moonclient
