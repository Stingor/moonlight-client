#include "survivors/survivors_book.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_meta.h"
#include "ro/sim/survivors_records.h"
#include "ro/sim/survivors_score.h"
#include "ro/sim/survivors_state.h"

namespace moonclient {
namespace {

// Sight se débloque par une partie survécue cinq minutes.
constexpr std::uint64_t kSightSurvivedMs = 5ULL * 60 * 1000;
// Frost Diver se débloque par 500 glaçages au cumul.
constexpr std::uint64_t kFrostDiverChills = 500;

constexpr std::array<SurvivorsUnlock, 4> kUnlocks{{
    {.key = "golem_card", .condition = UnlockCondition::kCardFound},
    {.key = "mastering_card", .condition = UnlockCondition::kCardFound},
    {.key = "sight", .condition = UnlockCondition::kFeat},
    {.key = "frost_diver", .condition = UnlockCondition::kFeat},
}};

constexpr std::array<SurvivorsFeat, 2> kFeats{{
    {.key = "sight", .counter = FeatCounter::kBestSurvivedMs, .threshold = kSightSurvivedMs},
    {.key = "frost_diver", .counter = FeatCounter::kChills, .threshold = kFrostDiverChills},
}};

[[nodiscard]] std::uint64_t SaturatedAdd(std::uint64_t total, std::uint64_t amount) noexcept {
  constexpr std::uint64_t kMost = std::numeric_limits<std::uint64_t>::max();
  return amount > kMost - total ? kMost : total + amount;
}

[[nodiscard]] std::int64_t FloorDivide(std::int64_t value, std::int64_t by) noexcept {
  const std::int64_t quotient = value / by;
  return (value % by != 0 && (value < 0) != (by < 0)) ? quotient - 1 : quotient;
}

[[nodiscard]] bool HasCard(const SurvivorsSave& save, std::string_view key) {
  // Une carte d'une autre forme, qu'un client plus récent a écrite, a été
  // trouvée elle aussi.
  return std::ranges::any_of(
             save.album.cards,
             [key](const SurvivorsAlbumCard& card) { return card.key == key && card.found > 0; }) ||
         std::ranges::any_of(save.album.foreign_cards, [key](const SurvivorsForeignEntry& entry) {
           return entry.key == key;
         });
}

[[nodiscard]] bool HasFeat(const SurvivorsSave& save, std::string_view key) {
  return std::ranges::find(save.album.feats, key) != save.album.feats.end() ||
         std::ranges::any_of(save.album.foreign_feats, [key](const SurvivorsForeignEntry& entry) {
           return entry.key == key;
         });
}

// L'Id d'un skill ou d'une carte de la réserve de la classe, par sa clé.
[[nodiscard]] std::optional<std::uint16_t> PoolIdOf(const ro::sim::SurvivorsRules& rules,
                                                    const ro::sim::ClassRule& class_rule,
                                                    std::string_view key) {
  for (const std::uint16_t skill_id : class_rule.skill_pool) {
    const ro::sim::SurvivorsSkillRule* skill = ro::sim::FindSkill(rules, skill_id);
    if (skill != nullptr && key == skill->key) return skill_id;
  }
  for (const std::uint16_t card_item_id : class_rule.card_pool) {
    const ro::sim::CardRule* card = ro::sim::FindCard(rules, card_item_id);
    if (card != nullptr && key == card->key) return card_item_id;
  }
  return std::nullopt;
}

[[nodiscard]] std::uint64_t CounterOf(const SurvivorsAlbumCounters& counters,
                                      FeatCounter counter) noexcept {
  switch (counter) {
    case FeatCounter::kBestSurvivedMs:
      return counters.best_survived_ms;
    case FeatCounter::kChills:
      return counters.chills;
  }
  return 0;
}

// Les cartes ramassées entrent dans l'Album ; rend celles qui y entrent pour
// la première fois.
[[nodiscard]] std::vector<std::uint16_t> FileCards(const ro::sim::SurvivorsRules& rules,
                                                   SurvivorsAlbum& album,
                                                   std::span<const std::uint16_t> cards_found,
                                                   std::int64_t now_utc_s) {
  std::vector<std::uint16_t> new_cards;
  for (const std::uint16_t card_item_id : cards_found) {
    const ro::sim::CardRule* rule = ro::sim::FindCard(rules, card_item_id);
    if (rule == nullptr) continue;
    const std::string_view key = rule->key;
    auto card = std::ranges::find_if(
        album.cards, [key](const SurvivorsAlbumCard& kept) { return kept.key == key; });
    if (card == album.cards.end()) {
      album.cards.push_back(SurvivorsAlbumCard{.key = std::string(key)});
      card = std::prev(album.cards.end());
    }
    if (card->found == 0) {
      card->first_found_day = FloorDivide(now_utc_s, ro::sim::kSecondsPerDay);
      new_cards.push_back(card_item_id);
    }
    if (card->found < std::numeric_limits<std::uint32_t>::max()) ++card->found;
  }
  return new_cards;
}

void CountRun(const ro::sim::SurvivorsRules& rules, SurvivorsAlbumCounters& counters,
              const SurvivorsEnding& ending) {
  counters.chills = SaturatedAdd(counters.chills, ending.recap.chills);
  for (const ro::sim::SurvivorsCount& kills : ending.recap.kills_by_foe) {
    const ro::sim::FoeRule* foe = ro::sim::FindFoe(rules, kills.id);
    if (foe != nullptr && foe->element == ro::sim::SurvivorsElement::kWind) {
      counters.wind_kills = SaturatedAdd(counters.wind_kills, kills.value);
    }
  }
  counters.best_level = std::max(counters.best_level, ending.tally.level);
  const std::uint64_t survived_ms =
      std::uint64_t{ending.tally.survived_steps} * ro::sim::kSurvivorsStepMs;
  counters.best_survived_ms = static_cast<std::uint32_t>(std::max<std::uint64_t>(
      counters.best_survived_ms,
      std::min<std::uint64_t>(survived_ms, std::numeric_limits<std::uint32_t>::max())));
  if (counters.runs < std::numeric_limits<std::uint32_t>::max()) ++counters.runs;
}

[[nodiscard]] std::vector<std::string> ReachFeats(SurvivorsSave& save) {
  std::vector<std::string> reached;
  for (const SurvivorsFeat& feat : kFeats) {
    if (HasFeat(save, feat.key)) continue;
    if (CounterOf(save.album.counters, feat.counter) < feat.threshold) continue;
    save.album.feats.emplace_back(feat.key);
    reached.emplace_back(feat.key);
  }
  return reached;
}

// Le tableau de mêmes clés, ajouté vide s'il manque.
[[nodiscard]] SurvivorsBoard& BoardOf(SurvivorsSave& save, std::string_view class_key,
                                      std::string_view arena_key, std::string_view board_key,
                                      const ro::sim::IsoWeek& week) {
  auto board = std::ranges::find_if(save.records, [&](const SurvivorsBoard& kept) {
    return kept.class_key == class_key && kept.arena_key == arena_key &&
           kept.board_key == board_key && kept.week == week;
  });
  if (board != save.records.end()) return *board;
  save.records.push_back(SurvivorsBoard{.class_key = std::string(class_key),
                                        .arena_key = std::string(arena_key),
                                        .board_key = std::string(board_key),
                                        .week = week});
  return save.records.back();
}

// Q11 : une semaine passée ne garde que sa meilleure partie.
void TrimPastChallengeWeeks(SurvivorsSave& save, const ro::sim::IsoWeek& current_week) {
  for (SurvivorsBoard& board : save.records) {
    if (board.board_key == kChallengeBoardKey && board.week != current_week &&
        board.runs.size() > 1) {
      board.runs.resize(1);
    }
  }
}

[[nodiscard]] RecordVerdict ChallengeVerdict(const ro::sim::SurvivorsRules& rules,
                                             const ro::sim::SurvivorsStart& start,
                                             const ro::sim::IsoWeek& week) {
  if (!ro::sim::IsNeutralStart(start)) return RecordVerdict::kNotNeutral;
  const ro::sim::ClassRule* class_rule = ro::sim::FindClass(rules, start.class_id);
  if (week.week == 0 || class_rule == nullptr ||
      start.seed != ro::sim::SeedOfWeek(week.year, week.week) ||
      start.first_skill_id != ChallengeFirstSkill(*class_rule, week)) {
    return RecordVerdict::kNotThisWeek;
  }
  return RecordVerdict::kPlaced;
}

// LA PARTIE AUX RECORDS : son verdict, son rang quand elle est inscrite, et le
// score à battre quand elle reste dehors. Les semaines passées du défi sont
// ensuite ramenées à leur meilleure partie ; une partie qui y entre derrière
// cette meilleure n'y reste pas.
[[nodiscard]] RecordVerdict Inscribe(const ro::sim::SurvivorsRules& rules, SurvivorsSave& save,
                                     const SurvivorsEnding& ending,
                                     const ro::sim::SurvivorsStart& start, std::int64_t now_utc_s,
                                     SurvivorsCredit& credit) {
  const std::uint64_t score = credit.score;
  std::optional<std::size_t>& rank = credit.record_rank;
  const ro::sim::IsoWeek current_week = IsoWeekOfUtc(now_utc_s);
  TrimPastChallengeWeeks(save, current_week);
  const ro::sim::SurvivorsTally& tally = ending.tally;
  if (tally.outcome == ro::sim::SurvivorsOutcome::kAbandoned) return RecordVerdict::kAbandoned;
  const bool challenge = ending.board.kind == SurvivorsBoardKind::kChallenge;
  const bool ranked = ending.board.kind == SurvivorsBoardKind::kRanked;
  if (challenge) {
    const RecordVerdict verdict = ChallengeVerdict(rules, start, ending.board.week);
    if (verdict != RecordVerdict::kPlaced) return verdict;
  }
  // Le tableau local des parties classées (Q8) ne garde que des départs neutres.
  if (ranked && !ro::sim::IsNeutralStart(start)) return RecordVerdict::kNotNeutral;
  const ro::sim::ClassRule* class_rule = ro::sim::FindClass(rules, start.class_id);
  const ro::sim::ArenaRule* arena_rule = ro::sim::FindArena(rules, start.arena_id);
  const ro::sim::SurvivorsSkillRule* first_skill = ro::sim::FindSkill(rules, start.first_skill_id);
  const ro::sim::IsoWeek week = challenge ? ending.board.week : ro::sim::IsoWeek{};
  const ro::sim::SurvivorsRecord record{
      .score = score,
      .survived_ms = static_cast<std::uint32_t>(
          std::min<std::uint64_t>(std::uint64_t{tally.survived_steps} * ro::sim::kSurvivorsStepMs,
                                  std::numeric_limits<std::uint32_t>::max())),
      .level = tally.level,
      .outcome = tally.outcome,
      .kills = tally.kills,
      .cursed_water = start.cursed_water,
      .ruleset = rules.ruleset_version,
      .first_skill_key = first_skill != nullptr ? first_skill->key : "",
      .seed = start.seed,
      .played_at_utc_s = now_utc_s,
      .week = week};
  const std::string_view board_key = challenge ? kChallengeBoardKey
                                     : ranked  ? kRankedBoardKey
                                               : kFreeBoardKey;
  SurvivorsBoard& board = BoardOf(save, class_rule != nullptr ? class_rule->key : "",
                                  arena_rule != nullptr ? arena_rule->key : "", board_key, week);
  rank = ro::sim::PlaceRecord(board.runs, record);
  TrimPastChallengeWeeks(save, current_week);
  if (rank.has_value() && *rank >= board.runs.size()) rank.reset();
  if (rank.has_value()) return RecordVerdict::kPlaced;
  if (!board.runs.empty()) credit.score_to_beat = board.runs.back().score;
  return RecordVerdict::kBelowBoard;
}

}  // namespace

std::span<const SurvivorsUnlock> ShippedSurvivorsUnlocks() { return kUnlocks; }

std::span<const SurvivorsFeat> ShippedSurvivorsFeats() { return kFeats; }

std::uint64_t FeatProgressOf(const SurvivorsSave& save, const SurvivorsFeat& feat) {
  return CounterOf(save.album.counters, feat.counter);
}

bool FeatReached(const SurvivorsSave& save, const SurvivorsFeat& feat) {
  return HasFeat(save, feat.key);
}

const SurvivorsBoard* FindBoard(const SurvivorsSave& save, std::string_view class_key,
                                std::string_view arena_key, std::string_view board_key,
                                const ro::sim::IsoWeek& week) {
  const auto board = std::ranges::find_if(save.records, [&](const SurvivorsBoard& kept) {
    return kept.class_key == class_key && kept.arena_key == arena_key &&
           kept.board_key == board_key && kept.week == week;
  });
  return board != save.records.end() ? &*board : nullptr;
}

bool IsUnlocked(const SurvivorsSave& save, const SurvivorsUnlock& unlock) {
  switch (unlock.condition) {
    case UnlockCondition::kCardFound:
      return HasCard(save, unlock.key);
    case UnlockCondition::kFeat:
      return HasFeat(save, unlock.key);
  }
  return false;
}

std::vector<std::uint16_t> LockedIdsOf(const ro::sim::SurvivorsRules& rules,
                                       const ro::sim::ClassRule& class_rule,
                                       const SurvivorsSave& save) {
  std::vector<std::uint16_t> locked;
  for (const SurvivorsUnlock& unlock : kUnlocks) {
    if (IsUnlocked(save, unlock)) continue;
    const std::optional<std::uint16_t> id = PoolIdOf(rules, class_rule, unlock.key);
    if (id.has_value() && std::ranges::find(locked, *id) == locked.end()) locked.push_back(*id);
  }
  std::ranges::sort(locked);
  return locked;
}

ro::sim::SurvivorsStart FreeStartOf(const ro::sim::SurvivorsRules& rules, const SurvivorsSave& save,
                                    std::uint64_t seed, std::uint16_t first_skill_id) {
  ro::sim::SurvivorsStart start{
      .seed = seed,
      .class_id = rules.classes.empty() ? std::uint16_t{0} : rules.classes.front().class_id,
      .arena_id = rules.arenas.empty() ? std::uint16_t{0} : rules.arenas.front().arena_id,
      .first_skill_id = first_skill_id,
      .loadout = {}};
  // Un carnet plus généreux que les règles ne fait pas refuser la partie : la
  // Cursed Water se ramène au plus haut rang que les règles connaissent.
  const std::size_t most_cursed_water =
      rules.cursed_water.empty() ? 0 : rules.cursed_water.size() - 1;
  start.cursed_water =
      static_cast<std::uint8_t>(std::min<std::size_t>(save.meta.cursed_water, most_cursed_water));
  if (rules.classes.empty()) return start;
  const ro::sim::ClassRule& class_rule = rules.classes.front();
  const ro::sim::SurvivorsMetaStart meta =
      ro::sim::LoadoutOf(ro::sim::ShippedSurvivorsMeta(), save.meta.ranks, class_rule);
  start.loadout = meta.loadout;
  start.lives = meta.lives;
  start.locked_ids = LockedIdsOf(rules, class_rule, save);
  return start;
}

ro::sim::IsoWeek IsoWeekOfUtc(std::int64_t utc_s) noexcept {
  return ro::sim::IsoWeekOfDay(FloorDivide(utc_s, ro::sim::kSecondsPerDay));
}

std::uint16_t ChallengeFirstSkill(const ro::sim::ClassRule& class_rule,
                                  const ro::sim::IsoWeek& week) noexcept {
  const std::span<const std::uint16_t> bolts = class_rule.starter_skills;
  if (bolts.empty()) return 0;
  const auto count = static_cast<std::int64_t>(bolts.size());
  const std::int64_t ordinal = ro::sim::IsoWeekOrdinal(week);
  return bolts[static_cast<std::size_t>(ordinal - FloorDivide(ordinal, count) * count)];
}

ro::sim::SurvivorsStart ChallengeStartOf(const ro::sim::SurvivorsRules& rules,
                                         const ro::sim::IsoWeek& week) {
  const ro::sim::ClassRule* class_rule = rules.classes.empty() ? nullptr : &rules.classes.front();
  return ro::sim::SurvivorsStart{
      .seed = ro::sim::SeedOfWeek(week.year, week.week),
      .class_id = class_rule == nullptr ? std::uint16_t{0} : class_rule->class_id,
      .arena_id = rules.arenas.empty() ? std::uint16_t{0} : rules.arenas.front().arena_id,
      .first_skill_id =
          class_rule == nullptr ? std::uint16_t{0} : ChallengeFirstSkill(*class_rule, week),
      .loadout = {}};
}

SurvivorsCredit CreditRun(const ro::sim::SurvivorsRules& rules, SurvivorsSave& save,
                          const SurvivorsEnding& ending, const ro::sim::SurvivorsStart& start,
                          std::int64_t now_utc_s) {
  const ro::sim::SurvivorsTally& tally = ending.tally;
  SurvivorsCredit credit;
  credit.zeny = ro::sim::CursedWaterZeny(ro::sim::RunZeny(tally, ending.zeny, tally.outcome),
                                         start.cursed_water);
  save.meta.zeny = SaturatedAdd(save.meta.zeny, credit.zeny);
  save.meta.zeny_earned = SaturatedAdd(save.meta.zeny_earned, credit.zeny);
  credit.zeny_balance = save.meta.zeny;
  credit.score = ro::sim::FreeRunScore(tally, start.cursed_water);
  credit.new_cards = FileCards(rules, save.album, ending.recap.cards_found, now_utc_s);
  CountRun(rules, save.album.counters, ending);
  credit.new_feats = ReachFeats(save);

  credit.played_at_utc_s = now_utc_s;
  credit.record = Inscribe(rules, save, ending, start, now_utc_s, credit);
  return credit;
}

bool CreditSightings(SurvivorsSave& save, const SurvivorsCodexSeen& sightings) {
  return Merge(save.codex, sightings);
}

}  // namespace moonclient
